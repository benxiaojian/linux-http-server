#include <iostream>
#include <cstdio>
#include <sstream>
#include <http_server.h>
#include <db_connection.h>
using namespace std;

#define kUPLOAD_FILE_PATH         "MyWebFile/"

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
  if (cookie_header == NULL) return ret;
  if (!mg_http_parse_header2(cookie_header, SESSION_COOKIE_NAME, &ssid,
                             sizeof(ssid_buf))) {
    return ret;
  }
  uint64_t sid = strtoull(ssid, NULL, 16);
  int i;
  for (i = 0; i < NUM_SESSIONS; i++) {
    if (s_sessions[i].id == sid) {
      s_sessions[i].last_used = mg_time();
      ret = &s_sessions[i];
      return ret;;
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
  for (i = 0; i < NUM_SESSIONS; i++) {
    if (s_sessions[i].id == 0) {
      s = &s_sessions[i];
      break;
    }
    if (s_sessions[i].last_used < oldest_s->last_used) {
      oldest_s = &s_sessions[i];
    }
  }
  if (s == NULL) {
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
  cs_sha1_update(&ctx, (const unsigned char *) hm->message.p, hm->message.len);
  cs_sha1_update(&ctx, (const unsigned char *) s, sizeof(*s));
  unsigned char digest[20];
  cs_sha1_final(digest, &ctx);
  s->id = *((uint64_t *) digest);
  return s;
}


struct file_writer_data {
  FILE *fp;
  size_t bytes_written;
};


static string getFileName(http_message *http_req, const char *file_name)
{
    struct session *s = HttpServer::GetSession(http_req);
    ostringstream file_path;
    file_path << kUPLOAD_FILE_PATH << s->user << "/" << file_name;
    cout << "full file path " << file_path.str() << endl;
    return file_path.str();
}


static void handle_upload(struct mg_connection *nc, int ev, void *p) {
  struct file_writer_data *data = (struct file_writer_data *) nc->user_data;
  struct mg_http_multipart_part *mp = (struct mg_http_multipart_part *) p;
  cout << "handle upload" << endl;
  cout << "file name = " << mp->file_name << endl;
  switch (ev) {
    case MG_EV_HTTP_PART_BEGIN: {
      if (data == NULL) {
        data = (struct file_writer_data *)calloc(1, sizeof(struct file_writer_data));
        // data->fp = fopen(getFileName((http_message *)p, mp->file_name).c_str(), "w+");
        data->fp = fopen(mp->file_name, "w+");
        data->bytes_written = 0;
        if (data->fp == NULL) {
          mg_printf(nc, "%s",
                    "HTTP/1.1 500 Failed to open a file\r\n"
                    "Content-Length: 0\r\n\r\n");
          nc->flags |= MG_F_SEND_AND_CLOSE;
          free(data);
          return;
        }
        nc->user_data = (void *) data;
      }
      break;
    }
    case MG_EV_HTTP_PART_DATA: {
      if (fwrite(mp->data.p, 1, mp->data.len, data->fp) != mp->data.len) {
        mg_printf(nc, "%s",
                  "HTTP/1.1 500 Failed to write to a file\r\n"
                  "Content-Length: 0\r\n\r\n");
        nc->flags |= MG_F_SEND_AND_CLOSE;
        return;
      }
      data->bytes_written += mp->data.len;
      break;
    }
    case MG_EV_HTTP_PART_END: {
      mg_printf(nc,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n\r\n"
                "Written %ld of POST data to a temp file\n\n",
                (long) ftell(data->fp));
      nc->flags |= MG_F_SEND_AND_CLOSE;
      fclose(data->fp);
      free(data);
      nc->user_data = NULL;
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

    mg_mgr_init(&m_mgr, NULL);      // 初始化连接器

    connection = mg_bind(&m_mgr, m_port.c_str(), OnHttpEvent);  // bind and listen, OnHttpEvent是收到请求后的回调
    if (connection == NULL) {
        cout << "failed to create listener" << endl;
        return -1;
    }

    // mg_register_http_endpoint(connection, "/upload", handle_upload MG_UD_ARG(NULL));
    mg_set_protocol_http_websocket(connection);
    cout << "Starting web server on port " << m_port << endl;
    while (true) {
        mg_mgr_poll(&m_mgr, 500);   // accept
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
    if (s_handler_map.find(url) != s_handler_map.end()) {
        return;
    }

    s_handler_map.insert(make_pair(url, req_handler));
}


void HttpServer::RemoveHandler(const string &url)
{
    auto it = s_handler_map.find(url);
    if (it != s_handler_map.end()) {
        s_handler_map.erase(it);
    }
}


void p(http_message *http_req)
{
      string req_str = string(http_req->message.p, http_req->message.len);
    cout << req_str << endl;
}


void HttpServer::OnHttpEvent(struct mg_connection *connection, int event_type, void *event_data)
{
    http_message *http_req = (http_message *)event_data;

    switch (event_type) {
        case MG_EV_HTTP_REQUEST:
        // p(http_req);
            HandleEvent(connection, http_req);
            break;
        case MG_EV_SSI_CALL:
        // p(http_req);
            break;
        case MG_EV_HTTP_PART_BEGIN:
        case MG_EV_HTTP_PART_DATA:
        case MG_EV_HTTP_PART_END: {
            handle_upload(connection, event_type, event_data);
            break;
        }
        default:
            break;
    }
}


static bool route_check(http_message *http_req, const string route_prefix)
{
    if (mg_vcmp(&http_req->uri, route_prefix.c_str()) == 0) {
        return true;
    }

    return false;
    // TODO: 还可以判断 GET, POST, PUT, DELTE等方法
    //mg_vcmp(&http_msg->method, "GET");
    //mg_vcmp(&http_msg->method, "POST");
    //mg_vcmp(&http_msg->method, "PUT");
    //mg_vcmp(&http_msg->method, "DELETE");
}


void HttpServer::HandleEvent(mg_connection *connection, http_message *http_req)
{
    string url = string(http_req->uri.p, http_req->uri.len);
    auto it = s_handler_map.find(url);
    if (it != s_handler_map.end()) {
        ReqHandler handle_func = it->second;
        handle_func(connection, http_req);
    } else if (route_check(http_req, "/upload")) {
        mg_serve_http(connection, (struct http_message *)http_req, s_server_option);
    } else {
        mg_printf(connection, "HTTP/1.1 404 \r\n\r\nNot Found 404.\r\n");
    }
}
