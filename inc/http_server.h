#pragma once

#include <string>
#include <mongoose.h>
#include <unordered_map>
#include <functional>

// 定义http返回callback
typedef void OnRspCallback(mg_connection *c, std::string);
// 定义http请求handler
using ReqHandler = std::function<bool (std::string, std::string, mg_connection *c, OnRspCallback)>;

class HttpServer
{
public:
    HttpServer() {};
    ~HttpServer() {};

    void Init(const std::string &port, const bool ssl_enable = false);        // 初始化设置
    bool Start(void);
    bool Stop(void);
    void RegisterHandler(const std::string &url, ReqHandler req_handler);
    void RemoveHandler(const std::string &url);

    static mg_serve_http_opts s_server_option; // web服务器选项
    static std::string s_web_dir;              // 网页根目录
    static std::unordered_map<std::string, ReqHandler> s_handler_map;   // 回调映射表

private:
    static void OnHttpEvent(mg_connection *connection, int event_type, void *event_data);
    static void HandleEvent(mg_connection *connection, http_message *http_req);

    std::string m_port;     // 端口
    struct mg_mgr m_mgr;    // mongoose 连接管理器
    bool m_ssl_enable;
};