#pragma once

#include <string>
#include <mongoose.h>

class HttpServer
{
public:
    HttpServer() {};
    ~HttpServer() {};

    void Init(const std::string &port, const bool ssl_enable = false);        // 初始化设置
    bool Start(void);
    bool Stop(void);

    static mg_serve_http_opts s_server_option; // web服务器选项
    static std::string s_web_dir;              // 网页根目录

private:
    static void OnHttpEvent(mg_connection *connection, int event_type, void *event_data);
    static void HandleEvent(mg_connection *connection, http_message *http_req);

    std::string m_port;     // 端口
    struct mg_mgr m_mgr;    // mongoose 连接管理器
    bool m_ssl_enable;
};