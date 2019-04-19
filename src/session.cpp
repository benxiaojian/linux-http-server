#include <session.h>
using namespace std;

Session::Session(const char *user, const struct http_message *hm)
{
    /* 初始化session */
    m_created = m_last_used = mg_time();
    m_user = user;
    m_lucky_number = rand();

    /* 根据消息请求创建认证字段 */
    cs_sha1_ctx ctx;
    cs_sha1_init(&ctx);
    cs_sha1_update(&ctx, (const unsigned char *)hm->message.p, hm->message.len);
    cs_sha1_update(&ctx, (const unsigned char *)this, sizeof(*this));
    unsigned char digest[20];
    cs_sha1_final(digest, &ctx);
    m_id = *((uint64_t *)digest);
}


CookieSessions::CookieSessions(int max_sessions) :
m_max_sessions(max_sessions)
{
}

shared_ptr<Session> CookieSessions::CreateSession(const char *user, const struct http_message *hm)
{
    shared_ptr<Session> session = make_shared<Session>(user, hm);
    // Session *session = new Session(user, hm);
    int session_count = m_session_list.size();

    if (session_count < m_max_sessions)
    {
        m_session_list.push_back(session);
        return session;
    }
    
    int old_session_index = 0;
    int i;
    for (i=0; i<m_max_sessions; ++i)
    { 
        if (m_session_list[i]->m_last_used < m_session_list[old_session_index]->m_last_used)
        {
            old_session_index = i;
        }
    }

    m_session_list[i] = session;

    return session;
}


shared_ptr<Session> CookieSessions::GetSession(struct http_message *hm)
{
    char ssid_buf[21];
    char *ssid = ssid_buf;
    shared_ptr<Session> ret;
    struct mg_str *cookie_header = mg_get_http_header(hm, "cookie");
    if (cookie_header == NULL)
        return ret;
    if (!mg_http_parse_header2(cookie_header, SESSION_COOKIE_NAME, &ssid,
                               sizeof(ssid_buf)))
    {
        return ret;
    }
    uint64_t sid = strtoull(ssid, NULL, 16);
    for (auto &s : m_session_list)
    {
        if (s->m_id == sid)
        {
            s->m_last_used = mg_time();
            ret = s;
            return ret;
        }
    }

    return ret;
}