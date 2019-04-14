#include <http_server.h>
#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

void HttpServer::Init(const string &port, const bool ssl_enable)
{
    m_port = port;
    m_ssl_enable = ssl_enable;
    s_server_option.enable_directory_listing = "yes";
    s_server_option.document_root = s_web_dir.c_str();
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
    // Todo
    return true;
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
    // mg_serve_http(connection, (struct http_message *)event_data, s_server_option);


      if (event_type == MG_EV_HTTP_REQUEST) {
    struct http_message *hm = (struct http_message *) event_data;

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


static void login(mg_connection *connection, http_message *http_req)
{
    char username[1024];
    char password[1024];
    // if (strcmp(http_req->method.p, "POST") == 0) {
        mg_get_http_var(&http_req->body, "username", username, sizeof(username));
        mg_get_http_var(&http_req->body, "password", password, sizeof(password));
        cout << "username = " << username << endl;
        cout << "password = " << password << endl;
    // }
}


void HttpServer::HandleEvent(mg_connection *connection, http_message *http_req)
{
    if (route_check(http_req, "/")) {
        rootpage(connection);
    } else if (route_check(http_req, "/login")) {
        login(connection, http_req);
    }

    string req_str = string(http_req->message.p, http_req->message.len);
    cout << req_str << endl;
    // mg_serve_http(connection, (struct http_message *)http_req, s_server_option);
}


string HttpServer::s_web_dir = "./web/";
mg_serve_http_opts HttpServer::s_server_option;

int main()
{
    HttpServer server;
    
    server.Init("8000");
    server.Start();

}