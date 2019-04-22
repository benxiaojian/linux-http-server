#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <http_server.h>
#include <db_connection.h>
#include <log.h>
using namespace std;

#define WEB_PATH        "./web/"
DbConnection db("localhost", "root", "root", "ses");

static void showHtml(mg_connection *connection, const string &file_name)
{
    stringstream stringbuffer;
    ifstream file(WEB_PATH + file_name);

    stringbuffer << file.rdbuf();

    // mg_printf(connection, "%s", "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n");
    mg_send_head(connection, 200, stringbuffer.str().length(), "Content-type: text/html");
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


/*
* 点击登录按钮后，访问/login，验证身份信息
*/
bool login(mg_connection *connection, http_message *http_req)
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

    shared_ptr<Session> s = HttpServer::GetInstance().m_cookie_sessions->CreateSession(username, http_req);
    char shead[100];
    snprintf(shead, sizeof(shead),
            "Set-Cookie: %s=%" INT64_X_FMT "; path=/", SESSION_COOKIE_NAME,
            s->m_id);
    mg_http_send_redirect(connection, 302, mg_mk_str("/"), mg_mk_str(shead));

    return true;
}


/*
* 根目录访问，如果第一次访问，进入登录界面，并创建cookie seesion
* 有了cookie seesion后，身份认证通过，进入上传页面
*/
void root(mg_connection *connection, http_message *http_req)
{
    shared_ptr<Session> s = HttpServer::GetInstance().m_cookie_sessions->GetSession(http_req);
    if (s == NULL) {
        showHtml(connection, "login.html");
        connection->flags |= MG_F_SEND_AND_CLOSE;
    } else {
        fprintf(stderr, "%s (sid %" INT64_X_FMT ") requested %.*s\n", s->m_user.c_str(),
                s->m_id, (int) http_req->uri.len, http_req->uri.p);
        // connection->user_data = (void*)s;
        showHtml(connection, "my_web_uploadfiles.html");
    }
}


void getTargetFileHtml(ostringstream &os)
{
    os << "<form class=\"form-getfile\" action=\"/get_file\" method=\"post\">";
    os << "<input type=\"text\" required=\"required\" placeholder=\"文件名\" name=\"filename\"></input>";
    os << "<button class=\"sub\" type=\"submit\">查询</button>";
    os << "</form>";
}


/*
* 查询该用户下全部文件
*/
void query(mg_connection *connection, http_message *http_req)
{
    shared_ptr<Session> s = HttpServer::GetInstance().m_cookie_sessions->GetSession(http_req);
    char sql[1024];
    ostringstream os;

    db.Connect();
    list<map<string, string>> result;
    sprintf(sql, "select * from files where user = \"%s\"", s->m_user.c_str());
    db.Select(sql, result);

    os << "<!DOCTYPE html><head><meta charset=\"utf-8\"><title>Files</title></head>";
    os << "<body><h1>文件列表：</h1>";
    getTargetFileHtml(os);
    DbConnection db("localhost", "root", "root", "ses");

    for (auto &it : result) {
        map<string, string> map_it = it;
        os << "<div><a href=\"" << map_it["file_name"] << "\">" << map_it["file_name"] << "</a></div>";
    }

    db.Close();
    os << "</body></html>";
    mg_printf(connection, "%s", "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n");
    mg_printf(connection, "%s", os.str().c_str());
}


void getFile(mg_connection *connection, http_message *http_req)
{
    shared_ptr<Session> s = HttpServer::GetInstance().m_cookie_sessions->GetSession(http_req);
    char sql[1024];
    char filename[1024];
    ostringstream os;
    mg_get_http_var(&http_req->body, "filename", filename, sizeof(filename));

    db.Connect();
    list<map<string, string>> result;
    sprintf(sql, "select * from files where user = \"%s\" and file_name = \"%s\"", s->m_user.c_str(), filename);
    if (!db.Select(sql, result)) {
        LOG("Select sql failure %s", sql);
        mg_printf(connection, "HTTP/1.1 404 \r\n\r\nNot Found 404.\r\n");
        return;
    }

    os << "<!DOCTYPE html><head><meta charset=\"utf-8\"><title>GetFiles</title></head>";
    os << "<body><h1>文件列表：</h1>";
    getTargetFileHtml(os);

    for (auto &it : result) {
        map<string, string> map_it = it;
        os << "<div><a href=\"" << map_it["file_name"] << "\">" << map_it["file_name"] << "</a></div>";
    }

    db.Close();
    os << "</body></html>";
    mg_printf(connection, "%s", "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n");
    mg_printf(connection, "%s", os.str().c_str());
}



int main()
{    
    FILE *log;
    log = fopen("http_server.log", "w+");
    log_set_file(log);

    LOG("hello");

    /* 注册Http Request中uri对应的方法处理 */
    HttpServer::GetInstance().Init("8000", false);
    HttpServer::GetInstance().RegisterHandler("/", root);
    HttpServer::GetInstance().RegisterHandler("/login", login);
    HttpServer::GetInstance().RegisterHandler("/query", query);
    HttpServer::GetInstance().RegisterHandler("/get_file", getFile);

    /* 运行Http Server */
    HttpServer::GetInstance().Start();
    fclose(log);

}