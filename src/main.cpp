#include <iostream>
#include <fstream>
#include <sstream>
#include <http_server.h>
#include <db_connection.h>
using namespace std;

#define WEB_PATH        "./web/"
HttpServer server;

static void showHtml(mg_connection *connection, const string &file_name)
{
    stringstream stringbuffer;
    ifstream file(WEB_PATH + file_name);

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

    struct session *s = server.CreateSession(username, http_req);
    char shead[100];
    snprintf(shead, sizeof(shead),
            "Set-Cookie: %s=%" INT64_X_FMT "; path=/", SESSION_COOKIE_NAME,
            s->id);
    mg_http_send_redirect(connection, 302, mg_mk_str("/"), mg_mk_str(shead));

    // char cmd[1024];
    // sprintf("%s %s/%s", "mkdir", "MyWebFile", s->user);
    // system(cmd);
    return true;
}


void root(mg_connection *connection, http_message *http_req)
{
    struct session *s = server.GetSession(http_req);
    if (s == NULL) {
        showHtml(connection, "login.html");
        connection->flags |= MG_F_SEND_AND_CLOSE;
    } else {
        fprintf(stderr, "%s (sid %" INT64_X_FMT ") requested %.*s\n", s->user,
                s->id, (int) http_req->uri.len, http_req->uri.p);
        connection->user_data = s;
        showHtml(connection, "my_web_uploadfiles.html");
    }
}


void query(mg_connection *connection, http_message *http_req)
{
    struct session *s = server.GetSession(http_req);

    ostringstream os;
    os << "<!DOCTYPE html><head><meta charset=\"utf-8\"><title>Files</title></head>";
    os << "<body><h1>文件列表：</h1>";
    DbConnection db("localhost", "root", "root", "ses");

    db.Connect();
    list<map<string, string>> result;
    db.Select("select * from files where user = \"test\"", result);

    for (auto &it : result) {
        map<string, string> map_it = it;
        os << "<div>" << map_it["file_name"] << "</div>";
    }

    os << "</body></html>";
    mg_printf(connection, "%s", "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n");
    cout << os.str() << endl;
    mg_printf(connection, "%s", os.str().c_str());
}


int main()
{    
    server.Init("8000");
    server.RegisterHandler("/", root);
    server.RegisterHandler("/login", login);
    server.RegisterHandler("/query", query);

    server.Start();

}