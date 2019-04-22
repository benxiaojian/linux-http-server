#include <mysql/mysql.h>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <db_connection.h>
#include <vector>
#include <log.h>
using namespace std;


void DbConnection::Connect(void)
{
    m_mysql = mysql_init(NULL);
    if (!mysql_real_connect(m_mysql, m_host.c_str(), m_user.c_str(), m_password.c_str(), m_database.c_str(), 0, NULL, 0)) {
        showError();
    }
}


void DbConnection::Close(void)
{
    if (m_mysql != nullptr) {
        mysql_close(m_mysql);
        m_mysql = nullptr;
    }
}


void DbConnection::getSelectResult(list<map<string, string>> &result)
{
    MYSQL_RES *mysql_result = mysql_store_result(m_mysql);
    if(mysql_result){
        int row_num, col_num;
        row_num = mysql_num_rows(mysql_result);
        col_num = mysql_num_fields(mysql_result);
        MYSQL_FIELD *fd;
        vector<string> v_name; 

        while (fd = mysql_fetch_field(mysql_result)) {
            v_name.push_back(fd->name);
        }

        MYSQL_ROW sql_row;
        while (sql_row = mysql_fetch_row(mysql_result)) {
            map<string, string> name_value;
            for (int i=0; i<col_num; ++i) {
                name_value[v_name[i]] = sql_row[i];
            }
            result.push_back(name_value);
        }
    }

}


bool DbConnection::Select(const char *query, list<map<string, string>> &result)
{
    if (mysql_real_query(m_mysql, query, strlen(query))) {
        showError();
        return false;
    }

    getSelectResult(result);

    return true;
}




bool DbConnection::Insert(const char *query)
{
    if (mysql_real_query(m_mysql, query, strlen(query))) {
        showError();
        return false;
    }

    return true;
}


void DbConnection::showError(void)
{
    LOG("Error(%d) [%s] \"%s\"", mysql_errno(m_mysql), mysql_sqlstate(m_mysql), mysql_error(m_mysql));

    mysql_close(m_mysql);
    m_mysql = nullptr;
}

