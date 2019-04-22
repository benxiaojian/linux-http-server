#include <iostream>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <fstream>
#include <vector>
#include <http_server.h>
#include <db_connection.h>
#include <log.h>
using namespace std;

#define kUPLOAD_FILE_PATH "MyWebFile/"

mg_serve_http_opts HttpServer::s_server_option;
unordered_map<string, ReqHandler> HttpServer::s_handler_map;
HttpServer* HttpServer::s_instance = NULL;
shared_ptr<CookieSessions> HttpServer::m_cookie_sessions;
string HttpServer::s_ssl_cert = "./ssl/server.pem";
string HttpServer::s_ssl_key = "./ssl/server.key";

HttpServer& HttpServer::GetInstance(void)
{
    if (s_instance == NULL)
    {
        s_instance = new HttpServer();
    }

    return *s_instance;
}


void HttpServer::Init(const string &port, const bool ssl_enable)
{
    m_cookie_sessions = make_shared<CookieSessions>(NUM_SESSIONS);

    m_port = port;
    m_ssl_enable = ssl_enable;
    s_server_option.document_root = ".";
}


bool HttpServer::Start(void)
{
    struct mg_connection *connection;
    struct mg_bind_opts bind_opts;
    const char *err;

    mg_mgr_init(&m_mgr, NULL); // 初始化连接器

    memset(&bind_opts, 0, sizeof(bind_opts));
    bind_opts.ssl_cert = s_ssl_cert.c_str();
    bind_opts.ssl_key = s_ssl_key.c_str();
    bind_opts.error_string = &err;

    if (m_ssl_enable) {
        connection = mg_bind_opt(&m_mgr, m_port.c_str(), OnHttpEvent, bind_opts);
    } else {
        connection = mg_bind(&m_mgr, m_port.c_str(), OnHttpEvent); // bind and listen, OnHttpEvent是收到请求后的回调
    }
    
    if (connection == NULL) {
        LOG("failed to create listener");
        return -1;
    }

    mg_set_protocol_http_websocket(connection);
    LOG("Starting web server on port %s", m_port.c_str());

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


string user;
static string getFileName(const char *file_name)
{
    ostringstream file_path;
    file_path << kUPLOAD_FILE_PATH << user << "/" << file_name;
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
        LOG("%s is exist\n", file_name);
        return;
    }

    memset(sql, 0, sizeof(sql));
    sprintf(sql, "insert into files (user, file_name) values(\"%s\", \"%s\")", user, file_name);
    if(!db.Insert(sql)) {
        LOG("Insert upload file %s failed", file_name);
    }
}


/*
* 文件上传结果
*/
static void uploadResult(struct mg_connection *connection, bool succeed)
{
    ostringstream os;
    mg_printf(connection, "%s", "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n");
    os << "<!DOCTYPE html><head><meta charset=\"utf-8\"><title>Upload</title></head>";
    os << "<body><script type=\"text/javascript\">";
    if (succeed) os << "alert(\"上传成功\")";
    else os << "alert(\"上传失败\")";
    os << "</script></body></html>";

    mg_printf(connection, "%s", os.str().c_str());
}


/*
* client文件上传
*/
struct file_writer_data
{
    FILE *fp;
    size_t bytes_written;
};
void HttpServer::handleUpload(struct mg_connection *nc, int ev, void *p)
{
    if (ev == MG_EV_HTTP_MULTIPART_REQUEST)
    {
        http_message *http_req = (http_message *)p;
        LOG("Recevice Http Multipart %s", http_req->method.p);
        shared_ptr<Session> s= m_cookie_sessions->GetSession(http_req);
        user = s->m_user;
        return;
    }

    struct file_writer_data *data = (struct file_writer_data *)nc->user_data;
    struct mg_http_multipart_part *mp = (struct mg_http_multipart_part *)p;
    switch (ev)
    {
    case MG_EV_HTTP_PART_BEGIN:
    {
        if (data == NULL)
        {
            LOG("Upload file %s", mp->file_name);
            data = (struct file_writer_data *)calloc(1, sizeof(struct file_writer_data));
            data->fp = fopen(getFileName(mp->file_name).c_str(), "w+");
            // data->fp = fopen(mp->file_name, "w+");
            data->bytes_written = 0;
            if (data->fp == NULL)
            {
                LOG("Upload failure, failed to open a file");
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
        nc->flags |= MG_F_SEND_AND_CLOSE;
        fclose(data->fp);
        free(data);
        nc->user_data = NULL;
        insertFileToDb(user.c_str(), mp->file_name);

        uploadResult(nc, true);
        mg_http_send_redirect(nc, 302, mg_mk_str("/"), mg_mk_str(NULL));
        break;
    }
    }
}


/*
* mongoose收到http request的回调方法
*/
void HttpServer::OnHttpEvent(struct mg_connection *connection, int event_type, void *event_data)
{
    http_message *http_req = (http_message *)event_data;

    switch (event_type)
    {
    case MG_EV_HTTP_REQUEST:
        LOG("Recevice Http Request %s", http_req->method.p);
        HandleEvent(connection, http_req);
        break;
    case MG_EV_SSI_CALL:
        LOG("Recevice Http SSI %s", http_req->method.p);
        break;
    case MG_EV_HTTP_MULTIPART_REQUEST:
    case MG_EV_HTTP_PART_BEGIN:
    case MG_EV_HTTP_PART_DATA:
    case MG_EV_HTTP_PART_END:
    {
        handleUpload(connection, event_type, event_data);
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


/*
* 根据cookie获取对应用户的全部文件名字
*/
static void getAllFilesFromDb(http_message *http_req, vector<string> &files)
{
    files.clear();
    shared_ptr<Session> s = HttpServer::GetInstance().m_cookie_sessions->GetSession(http_req);
    DbConnection db("localhost", "root", "root", "ses");
    char sql[1024];

    db.Connect();
    list<map<string, string>> result;
    sprintf(sql, "select * from files where user = \"%s\"", s->m_user.c_str());
    db.Select(sql, result);

    for (auto &it : result) {
        map<string, string> map_it = it;
        char tmp[1024];
        sprintf(tmp, "/%s", map_it["file_name"].c_str());
        files.push_back(tmp);
    }

    db.Close();
}


/*
* 获取用户的全部上传文件
*/
static bool getFile(http_message *http_req, string &file_path)
{
    vector<string> files;
    char tmp_file_path[1024];
    getAllFilesFromDb(http_req, files);

    for (auto &file : files)
    {
        if (route_check(http_req, file)) {
            shared_ptr<Session> s = HttpServer::GetInstance().m_cookie_sessions->GetSession(http_req);
            sprintf(tmp_file_path, "%s%s%s", kUPLOAD_FILE_PATH, s->m_user.c_str(), file.c_str());
            file_path = tmp_file_path;
            return true;
        }
    }

    return false;
}


/*
* client文件下载请求
*/
static void download(mg_connection *connection, http_message *http_req, string &file_path)
{
    stringstream stringbuffer;
	ifstream i_f_stream(file_path, ifstream::binary);

	if (!i_f_stream.is_open()) {            	// 检测，避免路径无效;
		return;
	}
	
	i_f_stream.seekg(0, i_f_stream.end);        // 定位到流末尾以获得长度;
	int length = i_f_stream.tellg();
	
	i_f_stream.seekg(0, i_f_stream.beg);        // 再定位到开始处进行读取;
 
    char *buffer = new char[length];            // 定义一个buffer;
	
	i_f_stream.read(buffer, length);            // 将数据读入buffer;
	i_f_stream.close();                         // 记得关闭资源;
 
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
    else if (getFile(http_req, file_path))
    {
        download(connection, http_req, file_path);
    }
    else
    {
        mg_printf(connection, "HTTP/1.1 404 \r\n\r\nNot Found 404.\r\n");
    }
}

