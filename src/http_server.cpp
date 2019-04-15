#include <http_server.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <db_connection.h>
using namespace std;

void HttpServer::Init(const string &port, const bool ssl_enable)
{
    m_port = port;
    m_ssl_enable = ssl_enable;
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


static bool getUsernamePassword(const char *username, const char *password)
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
    mg_get_http_var(&http_req->body, "username", username, sizeof(username));
    mg_get_http_var(&http_req->body, "password", password, sizeof(password));

    cout << "username = " << username << endl;
    cout << "password = " << password << endl;

    return getUsernamePassword(username, password);
}


void HttpServer::HandleEvent(mg_connection *connection, http_message *http_req)
{
    string req_str = string(http_req->message.p, http_req->message.len);
    cout << req_str << endl;

    if (route_check(http_req, "/")) {
        rootpage(connection);
    } else if (route_check(http_req, "/login")) {
        // if (login(connection, http_req)) {
        if (true) {
            cout << "auth pass" << endl;
            // s_web_dir = "/test_files";
            s_server_option.document_root = "./login";
            s_server_option.enable_directory_listing = "yes";
            mg_serve_http(connection, (struct http_message *)http_req, s_server_option);
            // mg_http_serve_file(connection, http_req, "test_files/", mg_mk_str("text/plain"), mg_mk_str(""));
        }
    }

}
