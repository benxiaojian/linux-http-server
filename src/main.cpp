#include <http_server.h>
#include <db_connection.h>
#include <iostream>
using namespace std;
string HttpServer::s_web_dir = "./web/";
mg_serve_http_opts HttpServer::s_server_option;
unordered_map<string, ReqHandler> HttpServer::s_handler_map;


int main()
{
    HttpServer server;
    
    server.Init("8000");
    server.Start();

}