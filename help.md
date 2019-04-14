## Mongoose的web服务器
使用C语言的mongoose库 https://github.com/cesanta/mongoose

先根据mongoose提供的非http server example搭建服务器 [HTTP server example](https://github.com/cesanta/mongoose/tree/master/examples/simplest_web_server).  
编译`simplest_web_server.c`,  运行后在浏览器访问会显示当前文件夹下的全部文件。  
针对这个例子提供的功能进行封装：

```c
#include "mongoose.h"

static const char *s_http_port = "8000";
static struct mg_serve_http_opts s_http_server_opts;

static void ev_handler(struct mg_connection *c, int ev, void *p) {
  if (ev == MG_EV_HTTP_REQUEST) {
    mg_serve_http(nc, (struct http_message *) p, s_http_server_opts);
  }
}

int main(void) {
  struct mg_mgr mgr;
  struct mg_connection *c;

  mg_mgr_init(&mgr, NULL);                      // 初始化连接器
  c = mg_bind(&mgr, s_http_port, ev_handler);   // bind并传入有请求后接入后的回调函数ev_handler
  mg_set_protocol_http_websocket(c);            // listen

  for (;;) {
    mg_mgr_poll(&mgr, 1000);                    // accept
  }
  mg_mgr_free(&mgr);                            // free

  return 0;
}
```
  
设计`class HttpServer`
```c++
// http_server.h
class HttpServer
{
public:
    HttpServer() {};
    ~HttpServer() {};

    void Init(const std::string &port);        // 初始化设置
    bool Start(void);                          // 启动服务器
    bool Stop(void);                           // 停止服务器

    static mg_serve_http_opts s_server_option; // web服务器选项
    static std::string s_web_dir;              // 网页根目录

private:
    // 请求接入触发的回调函数
    static void OnHttpEvent(mg_connection *connection, int event_type, void *event_data);

    // 回调函数中根据不同的事件，执行不同的事件处理
    static void HandleEvent(mg_connection *connection, http_message *http_req);

    std::string m_port;     // 端口
    struct mg_mgr m_mgr;    // mongoose 连接管理器
};
```

实现基础的`class HttpServer`
```c++
// http_server.cpp
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

void HttpServer::HandleEvent(mg_connection *connection, http_message *http_req)
{
    string req_str = string(http_req->message.p, http_req->message.len);
    cout << req_str << endl; // 打印Http request
    mg_serve_http(connection, (struct http_message *)http_req, s_server_option);
}
```
***
实现了基本的http server功能后，根据 `URI`的不同请求触发。可以查看打印的`http request`，在输入`127.0.0.1:8000`访问服务后，请求的HTTP方法为`GET`，请求路径`/`
```
GET / HTTP/1.1
Host: 127.0.0.1:8000
User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:66.0) Gecko/20100101 Firefox/66.0
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
Accept-Language: en-US,en;q=0.5
Accept-Encoding: gzip, deflate
Connection: keep-alive
Upgrade-Insecure-Requests: 1
Cache-Control: max-age=0
```
按照需求返回`登录界面`，将上面的`HandleEvent`修改成返回`login.html`内容。
```c++
static void login(mg_connection *connection)
{
    stringstream stringbuffer;
    ifstream file("./web/login.html");

    stringbuffer << file.rdbuf();

    mg_send_head(connection, 200, stringbuffer.str().length(), "Content-type: text/html\r\n\r\n ");
    mg_printf(connection, "%s", stringbuffer.str().c_str());
}

static bool route_check(http_message *http_req, char *route_prefix)
{
    if (mg_vcmp(&http_req->uri, route_prefix)) {
        return true;
    }

    return false;
}

void HttpServer::HandleEvent(mg_connection *connection, http_message *http_req)
{
    if (route_check(http_req, "/")) {
        login(connection);
    }
}
```
