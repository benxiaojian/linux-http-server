#include <iostream>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <fstream>
#include <vector>
#include <http_server.h>
#include <db_connection.h>
using namespace std;

#define kUPLOAD_FILE_PATH "MyWebFile/"

string HttpServer::s_web_dir = "./web/";
mg_serve_http_opts HttpServer::s_server_option;
unordered_map<string, ReqHandler> HttpServer::s_handler_map;

/*
 * This example uses a simple in-memory storage for just 10 sessions.
 * A real-world implementation would use persistent storage of some sort.
 */
#define NUM_SESSIONS 10
struct session s_sessions[NUM_SESSIONS];

/*
 * Parses the session cookie and returns a pointer to the session struct
 * or NULL if not found.
 */
struct session *HttpServer::GetSession(struct http_message *hm)
{
    char ssid_buf[21];
    char *ssid = ssid_buf;
    struct session *ret = NULL;
    struct mg_str *cookie_header = mg_get_http_header(hm, "cookie");
    if (cookie_header == NULL)
        return ret;
    if (!mg_http_parse_header2(cookie_header, SESSION_COOKIE_NAME, &ssid,
                               sizeof(ssid_buf)))
    {
        return ret;
    }
    uint64_t sid = strtoull(ssid, NULL, 16);
    int i;
    for (i = 0; i < NUM_SESSIONS; i++)
    {
        if (s_sessions[i].id == sid)
        {
            s_sessions[i].last_used = mg_time();
            ret = &s_sessions[i];
            return ret;
            ;
        }
    }

    return ret;
}

/*
 * Destroys the session state.
 */
void HttpServer::DestroySession(struct session *s)
{
    free(s->user);
    memset(s, 0, sizeof(*s));
}

/*
 * Creates a new session for the user.
 */
struct session *HttpServer::CreateSession(const char *user, const struct http_message *hm)
{
    /* Find first available slot or use the oldest one. */
    struct session *s = NULL;
    struct session *oldest_s = s_sessions;
    int i;
    for (i = 0; i < NUM_SESSIONS; i++)
    {
        if (s_sessions[i].id == 0)
        {
            s = &s_sessions[i];
            break;
        }
        if (s_sessions[i].last_used < oldest_s->last_used)
        {
            oldest_s = &s_sessions[i];
        }
    }
    if (s == NULL)
    {
        DestroySession(oldest_s);
        printf("Evicted %" INT64_X_FMT "/%s\n", oldest_s->id, oldest_s->user);
        s = oldest_s;
    }
    /* Initialize new session. */
    s->created = s->last_used = mg_time();
    s->user = strdup(user);
    s->lucky_number = rand();
    /* Create an ID by putting various volatiles into a pot and stirring. */
    cs_sha1_ctx ctx;
    cs_sha1_init(&ctx);
    cs_sha1_update(&ctx, (const unsigned char *)hm->message.p, hm->message.len);
    cs_sha1_update(&ctx, (const unsigned char *)s, sizeof(*s));
    unsigned char digest[20];
    cs_sha1_final(digest, &ctx);
    s->id = *((uint64_t *)digest);
    return s;
}

struct file_writer_data
{
    FILE *fp;
    size_t bytes_written;
};


string user;
static string getFileName(const char *file_name)
{
    ostringstream file_path;
    file_path << kUPLOAD_FILE_PATH << user << "/" << file_name;
    cout << "full file path " << file_path.str() << endl;
    return file_path.str();
}


static void insertFileToDb(const char *user, const char *file_name)
{
    DbConnection db("localhost", "root", "root", "ses");

    db.Connect();
    char sql[1024];
    list<map<string, string>> res;

    sprintf(sql, "select * from files where user = \"%s\" and file_name = \"%s\"", user, file_name);
    db.Select(sql, res);
    if(!res.empty()) {
        printf("%s is exist\n", file_name);
        return;
    }

    memset(sql, 0, sizeof(sql));
    sprintf(sql, "insert into files (user, file_name) values(\"%s\", \"%s\")", user, file_name);
    if(!db.Insert(sql)) {
        cout << "insert file " << file_name << "failed" << endl;
    }
}


static void handle_upload(struct mg_connection *nc, int ev, void *p)
{
    if (ev == MG_EV_HTTP_MULTIPART_REQUEST)
    {
        http_message *http_req = (http_message *)p;
        struct session *s = HttpServer::GetSession(http_req);
        user = s->user;
        return;
    }

    struct file_writer_data *data = (struct file_writer_data *)nc->user_data;
    struct mg_http_multipart_part *mp = (struct mg_http_multipart_part *)p;
    cout << "handle upload" << endl;
    cout << "file name = " << mp->file_name << endl;
    switch (ev)
    {
    case MG_EV_HTTP_PART_BEGIN:
    {
        if (data == NULL)
        {
            data = (struct file_writer_data *)calloc(1, sizeof(struct file_writer_data));
            data->fp = fopen(getFileName(mp->file_name).c_str(), "w+");
            // data->fp = fopen(mp->file_name, "w+");
            data->bytes_written = 0;
            if (data->fp == NULL)
            {
                mg_printf(nc, "%s",
                          "HTTP/1.1 500 Failed to open a file\r\n"
                          "Content-Length: 0\r\n\r\n");
                nc->flags |= MG_F_SEND_AND_CLOSE;
                free(data);
                return;
            }
            nc->user_data = (void *)data;
        }
        break;
    }
    case MG_EV_HTTP_PART_DATA:
    {
        if (fwrite(mp->data.p, 1, mp->data.len, data->fp) != mp->data.len)
        {
            mg_printf(nc, "%s",
                      "HTTP/1.1 500 Failed to write to a file\r\n"
                      "Content-Length: 0\r\n\r\n");
            nc->flags |= MG_F_SEND_AND_CLOSE;
            return;
        }
        data->bytes_written += mp->data.len;
        break;
    }
    case MG_EV_HTTP_PART_END:
    {
        // mg_printf(nc,
        //           "HTTP/1.1 200 OK\r\n"
        //           "Content-Type: text/plain\r\n"
        //           "Connection: close\r\n\r\n",
        //           (long)ftell(data->fp));
        nc->flags |= MG_F_SEND_AND_CLOSE;
        fclose(data->fp);
        free(data);
        nc->user_data = NULL;
        insertFileToDb(user.c_str(), mp->file_name);
        mg_http_send_redirect(nc, 302, mg_mk_str("/"), mg_mk_str(""));
        break;
    }
    }
}


void HttpServer::Init(const string &port, const bool ssl_enable)
{
    m_port = port;
    m_ssl_enable = ssl_enable;
    s_server_option.document_root = ".";
}


bool HttpServer::Start(void)
{
    struct mg_connection *connection;

    mg_mgr_init(&m_mgr, NULL); // 初始化连接器

    connection = mg_bind(&m_mgr, m_port.c_str(), OnHttpEvent); // bind and listen, OnHttpEvent是收到请求后的回调
    if (connection == NULL)
    {
        cout << "failed to create listener" << endl;
        return -1;
    }

    mg_set_protocol_http_websocket(connection);
    cout << "Starting web server on port " << m_port << endl;
    while (true)
    {
        mg_mgr_poll(&m_mgr, 500); // accept
    }
    mg_mgr_free(&m_mgr);

    return true;
}


bool HttpServer::Stop(void)
{
    mg_mgr_free(&m_mgr);

    return true;
}


void HttpServer::RegisterHandler(const string &url, ReqHandler req_handler)
{
    if (s_handler_map.find(url) != s_handler_map.end())
    {
        return;
    }

    s_handler_map.insert(make_pair(url, req_handler));
}

void HttpServer::RemoveHandler(const string &url)
{
    auto it = s_handler_map.find(url);
    if (it != s_handler_map.end())
    {
        s_handler_map.erase(it);
    }
}


static void p(http_message *http_req)
{
    cout << http_req->message.p << endl;
}


void HttpServer::OnHttpEvent(struct mg_connection *connection, int event_type, void *event_data)
{
    http_message *http_req = (http_message *)event_data;

    switch (event_type)
    {
    case MG_EV_HTTP_REQUEST:
        cout << "MG_EV_HTTP_REQUEST" << endl;
        p(http_req);
        HandleEvent(connection, http_req);
        break;
    case MG_EV_SSI_CALL:
        cout << "MG_EV_SSI_CALL" << endl;
        p(http_req);
        break;
    case MG_EV_HTTP_MULTIPART_REQUEST:
    case MG_EV_HTTP_PART_BEGIN:
    case MG_EV_HTTP_PART_DATA:
    case MG_EV_HTTP_PART_END:
    {
        handle_upload(connection, event_type, event_data);
        break;
    }
    default:
        break;
    }
}


static bool route_check(http_message *http_req, const string route_prefix)
{
    if (mg_vcmp(&http_req->uri, route_prefix.c_str()) == 0)
    {
        return true;
    }

    return false;
}


static void getAllFilesFromDb(http_message *http_req, vector<string> &files)
{
    files.clear();
    struct session *s = HttpServer::GetSession(http_req);
    DbConnection db("localhost", "root", "root", "ses");

    db.Connect();
    list<map<string, string>> result;
    db.Select("select * from files where user = \"test\"", result);

    for (auto &it : result) {
        map<string, string> map_it = it;
        char tmp[1024];
        sprintf(tmp, "/%s", map_it["file_name"].c_str());
        files.push_back(tmp);
    }

    db.Close();
}


static bool getFile(http_message *http_req, string &file_path)
{
    vector<string> files;
    char tmp_file_path[1024];
    getAllFilesFromDb(http_req, files);

    for (auto &file : files)
    {
        if (route_check(http_req, file)) {
            struct session *s = HttpServer::GetSession(http_req);
            sprintf(tmp_file_path, "%s%s%s", kUPLOAD_FILE_PATH, s->user, file.c_str());
            cout << "\n\n\file name = " << tmp_file_path << endl;
            file_path = tmp_file_path;
            return true;
        }
    }

    return false;
}


static void download(mg_connection *connection, http_message *http_req, string &file_path)
{
    stringstream stringbuffer;
    // ifstream file(file_path, ifstream::binary);

	ifstream i_f_stream(file_path, ifstream::binary);
	// [2]检测，避免路径无效;
	if (!i_f_stream.is_open()) {
		return;
	}
	// [3]定位到流末尾以获得长度;
	i_f_stream.seekg(0, i_f_stream.end);
	int length = i_f_stream.tellg();
	// [4]再定位到开始处进行读取;
	i_f_stream.seekg(0, i_f_stream.beg);
 
	// [5]定义一个buffer;
	char *buffer = new char[length];
	// [6]将数据读入buffer;
	i_f_stream.read(buffer, length);
 
	// [7]记得关闭资源;
	i_f_stream.close();
 
	// cout.write(buffer, length);

    mg_http_serve_file(connection, http_req, file_path.c_str(),
                         mg_mk_str("text/plain"), mg_mk_str(""));
    delete[] buffer;
}


void HttpServer::HandleEvent(mg_connection *connection, http_message *http_req)
{
    string file_path;
    string url = string(http_req->uri.p, http_req->uri.len);
    auto it = s_handler_map.find(url);
    if (it != s_handler_map.end())
    {
        ReqHandler handle_func = it->second;
        handle_func(connection, http_req);
    }
    else if (route_check(http_req, "/upload"))
    {
        mg_serve_http(connection, (struct http_message *)http_req, s_server_option);
    }
    else if (getFile(http_req, file_path))
    {
        download(connection, http_req, file_path);
    }
    else
    {
        mg_printf(connection, "HTTP/1.1 404 \r\n\r\nNot Found 404.\r\n");
    }
}
