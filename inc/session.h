#pragma once

#include <mongoose.h>
#include <string>
#include <vector>

/* This is the name of the cookie carrying the session ID. */
#define SESSION_COOKIE_NAME "mgs"
/* In our example sessions are destroyed after 30 seconds of inactivity. */
#define SESSION_TTL 30.0
#define SESSION_CHECK_INTERVAL 5.0

class Session
{
public:
    Session(const char *user, const struct http_message *hm);
    ~Session() {}

    uint64_t m_id;
    /*
   * Time when the session was created and time of last activity.
   * Used to clean up stale sessions.
   */
    double m_created;
    double m_last_used; /* Time when the session was last active. */

    /* User name this session is associated with. */
    std::string m_user;
    /* Some state associated with user's session. */
    int m_lucky_number;
};


class CookieSessions
{
public:
    CookieSessions() {}
    CookieSessions(int max_sessions);

    Session* CreateSession(const char *user, const struct http_message *hm);
    void DestroySession(Session *session);
    Session* GetSession(struct http_message *hm);

private:
    int m_max_sessions;
    std::vector<Session*> m_session_list;
};