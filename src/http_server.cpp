#include <http_server.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <db_connection.h>
using namespace std;

/* This is the name of the cookie carrying the session ID. */
#define SESSION_COOKIE_NAME "mgs"
/* In our example sessions are destroyed after 30 seconds of inactivity. */
#define SESSION_TTL 30.0
#define SESSION_CHECK_INTERVAL 5.0

/* Session information structure. */
struct session {
  /* Session ID. Must be unique and hard to guess. */
  uint64_t id;
  /*
   * Time when the session was created and time of last activity.
   * Used to clean up stale sessions.
   */
  double created;
  double last_used; /* Time when the session was last active. */

  /* User name this session is associated with. */
  char *user;
  /* Some state associated with user's session. */
  int lucky_number;
};

/*
 * This example uses a simple in-memory storage for just 10 sessions.
 * A real-world implementation would use persistent storage of some sort.
 */
#define NUM_SESSIONS 10
struct session s_sessions[NUM_SESSIONS];

/*
 * Password check function.
 * In our example all users have password "password".
 */
static int check_pass(const char *user, const char *pass) {
  (void) user;
  return (strcmp(pass, "password") == 0);
}

/*
 * Parses the session cookie and returns a pointer to the session struct
 * or NULL if not found.
 */
static struct session *get_session(struct http_message *hm) {
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
static void destroy_session(struct session *s) {
  free(s->user);
  memset(s, 0, sizeof(*s));
}

/*
 * Creates a new session for the user.
 */
static struct session *create_session(const char *user,
                                      const struct http_message *hm) {
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
    destroy_session(oldest_s);
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


void HttpServer::OnHttpEvent(struct mg_connection *connection, int event_type, void *event_data)
{
    http_message *http_req = (http_message *)event_data;

    switch (event_type) {
        case MG_EV_HTTP_REQUEST:
            HandleEvent(connection, http_req);
            break;
        case MG_EV_SSI_CALL:
            break;
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


static void rootpage(mg_connection *connection)
{
    stringstream stringbuffer;
    ifstream file("./web/login.html");

    stringbuffer << file.rdbuf();

    mg_printf(connection, "%s", "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n");
    mg_printf(connection, "%s", stringbuffer.str().c_str());
}


static bool checkPassword(const char *username, const char *password)
{
    DbConnection db("localhost", "root", "root", "ses");

    db.Connect();
    list<map<string, string>> result;
    db.Select("select * from user where name = \"test\"", result);

    for (auto &it : result) {
        map<string, string> map_it = it;
        if ((map_it["name"] == username) && (map_it["password"] == password)) {
            return true;
        }
    }

    return false;
}


static bool login(mg_connection *connection, http_message *http_req)
{
    char username[1024];
    char password[1024];
    int user_len = mg_get_http_var(&http_req->body, "username", username, sizeof(username));
    int pass_len = mg_get_http_var(&http_req->body, "password", password, sizeof(password));

    if (user_len<=0 && pass_len<=0) {
        mg_printf(connection, "HTTP/1.0 400 Bad Request\r\n\r\nusername, password required.\r\n");
        return false;
    }

    if (!checkPassword(username, password)) {
        mg_printf(connection, "HTTP/1.0 403 Unauthorized\r\n\r\nWrong password.\r\n");
        return false;
    }

    struct session *s = create_session(username, http_req);
    char shead[100];
    snprintf(shead, sizeof(shead),
            "Set-Cookie: %s=%" INT64_X_FMT "; path=/", SESSION_COOKIE_NAME,
            s->id);
    mg_http_send_redirect(connection, 302, mg_mk_str("/"), mg_mk_str(shead));
    return true;
}


void HttpServer::HandleEvent(mg_connection *connection, http_message *http_req)
{
    string req_str = string(http_req->message.p, http_req->message.len);
    cout << req_str << endl;

    if (route_check(http_req, "/")) {
        struct session *s = get_session(http_req);
        if (s == NULL) {
            rootpage(connection);
            connection->flags |= MG_F_SEND_AND_CLOSE;
        } else {
            fprintf(stderr, "%s (sid %" INT64_X_FMT ") requested %.*s\n", s->user,
                    s->id, (int) http_req->uri.len, http_req->uri.p);
            connection->user_data = s;
            s_server_option.document_root = "./test_files/";
            mg_serve_http(connection, (struct http_message *)http_req, s_server_option);
        }
        
    } else if (route_check(http_req, "/login")) {
            login(connection, http_req);
    }

}
