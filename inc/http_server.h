#pragma once

#include <string>
#include <mongoose.h>
#include <unordered_map>
#include <functional>
#include <session.h>

/* This is the name of the cookie carrying the session ID. */
#define SESSION_COOKIE_NAME "mgs"
/* In our example sessions are destroyed after 30 seconds of inactivity. */
#define SESSION_TTL 30.0
#define SESSION_CHECK_INTERVAL 5.0

/*最大会话数*/
#define NUM_SESSIONS 10


typedef void OnRspCallback(mg_connection *c, std::string);                                          // 定义http返回callback
using ReqHandler = std::function<void (mg_connection *connection, http_message *http_req)>;         // 定义http请求handler


class HttpServer
{
private:
    HttpServer() {};
public:
    virtual ~HttpServer() {};

    static HttpServer& GetInstance(void);

    void Init(const std::string &port, const bool ssl_enable = false);        // 初始化设置
    bool Start(void);
    bool Stop(void);
    void RegisterHandler(const std::string &url, ReqHandler req_handler);     // 注册url对应的处理方法
    void RemoveHandler(const std::string &url);                               // 删除处理方法

    // struct session *GetSession(struct http_message *hm);
    void DestroySession(struct session *s);
    // struct session *CreateSession(const char *user, const struct http_message *hm);

    static mg_serve_http_opts s_server_option; // web服务器选项
    static std::unordered_map<std::string, ReqHandler> s_handler_map;   // 回调映射表
    static std::shared_ptr<CookieSessions> m_cookie_sessions;           // cookie session列表
    static std::string s_ssl_cert;
    static std::string s_ssl_key;

private:
    static HttpServer *s_instance;

    static void OnHttpEvent(mg_connection *connection, int event_type, void *event_data);
    static void HandleEvent(mg_connection *connection, http_message *http_req);
    static void handleUpload(struct mg_connection *nc, int ev, void *p);

    std::string m_port;     // 端口
    struct mg_mgr m_mgr;    // mongoose 连接管理器
    bool m_ssl_enable;


};