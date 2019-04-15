#pragma once

#include <mysql/mysql.h>
#include <list>
#include <map>
#include <string>


class DbConnection
{
public:
    DbConnection() {}
    DbConnection(const char *host, const char *user, const char *password, const char *database):
        m_host(host),
        m_user(user),
        m_password(password),
        m_database(database)
    {
    }

    ~DbConnection() 
    {
        if (m_mysql != nullptr) {
            mysql_close(m_mysql);
            m_mysql = nullptr;
        }
    }

    void Connect(void);
    void Close(void);

    bool Select(const char *query, std::list<std::map<std::string, std::string>> &result);
    bool Insert(const char *query);
    // void Delete();
    // void Update();

private:
    void showError(void);
    void getSelectResult(std::list<std::map<std::string, std::string>> &result);

    MYSQL *m_mysql;
    std::string m_host;
    std::string m_user;
    std::string m_password;
    std::string m_database;
};