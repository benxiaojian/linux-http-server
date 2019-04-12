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


static void login(mg_connection *connection)
{
    stringstream stringbuffer;
    ifstream file("./web/login.html");

    stringbuffer << file.rdbuf();

    mg_send_head(connection, 200, stringbuffer.str().length(), "Content-type: text/html\r\n\r\n ");
    mg_printf(connection, "%s", stringbuffer.str().c_str());
}


void HttpServer::HandleEvent(mg_connection *connection, http_message *http_req)
{
    // string req_str = string(http_req->message.p, http_req->message.len);
    // cout << req_str << endl;
    // mg_serve_http(connection, (struct http_message *)http_req, s_server_option);
    
    login(connection);

}


string HttpServer::s_web_dir = "./web/";
mg_serve_http_opts HttpServer::s_server_option;

int main()
{
    HttpServer server;
    
    server.Init("8000");
    server.Start();

}