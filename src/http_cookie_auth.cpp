// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2012 Cloudmeter, Inc.  (http://www.cloudmeter.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/algorithm/string.hpp>
#include <pion/algorithm.hpp>
#include <pion/http/cookie_auth.hpp>
#include <pion/http/response_writer.hpp>
#include <pion/http/server.hpp>
#include <ctime>


namespace pion {    // begin namespace pion
namespace http {    // begin namespace http
    
    
// static members of cookie_auth

const unsigned int  cookie_auth::CACHE_EXPIRATION = 3600;    // 1 hour
const unsigned int  cookie_auth::RANDOM_COOKIE_BYTES = 20;
const std::string   cookie_auth::AUTH_COOKIE_NAME = "pion_session_id";   


// cookie_auth member functions

cookie_auth::cookie_auth(PionUserManagerPtr userManager,
                               const std::string& login,
                               const std::string& logout,
                               const std::string& redirect)
    : http::auth(userManager), m_login(login), m_logout(logout), m_redirect(redirect),
    m_random_gen(), m_random_range(0, 255), m_random_die(m_random_gen, m_random_range),
    m_cache_cleanup_time(boost::posix_time::second_clock::universal_time())
{
    // set logger for this class
    setLogger(PION_GET_LOGGER("pion.http.cookie_auth"));

    // Seed random number generator with current time as time_t int value, cast to the required type.
    // (Note that boost::mt19937::result_type is boost::uint32_t, and casting to an unsigned n-bit integer is
    // defined by the standard to keep the lower n bits.  Since ::time() returns seconds since Jan 1, 1970, 
    // it will be a long time before we lose any entropy here, even if time_t is a 64-bit int.)
    m_random_gen.seed(static_cast<boost::mt19937::result_type>(::time(NULL)));

    // generate some random numbers to increase entropy of the rng
    for (unsigned int n = 0; n < 100; ++n)
        m_random_die();
}
    
bool cookie_auth::handleRequest(HTTPRequestPtr& request, tcp::connection_ptr& tcp_conn)
{
    if (processLogin(request,tcp_conn)) {
        return false; // we processed login/logout request, no future processing for this request permitted
    }

    if (!needAuthentication(request)) {
        return true; // this request does not require authentication
    }

    // check if it is redirection page.. If yes, then do not test its credentials ( as used for login)
    if (!m_redirect.empty() && m_redirect==request->getResource()) {
        return true; // this request does not require authentication
    }
    
    // check cache for expiration
    boost::posix_time::ptime time_now(boost::posix_time::second_clock::universal_time());
    expireCache(time_now);

    // if we are here, we need to check if access authorized...
    const std::string auth_cookie(request->getCookie(AUTH_COOKIE_NAME));
    if (! auth_cookie.empty()) {
        // check if this cookie is in user cache
        boost::mutex::scoped_lock cache_lock(m_cache_mutex);
        user_cache_type::iterator user_cache_itr=m_user_cache.find(auth_cookie);
        if (user_cache_itr != m_user_cache.end()) {
            // we find those credential in our cache...
            // we can approve authorization now!
            request->setUser(user_cache_itr->second.second);
            // and update cache timeout
            user_cache_itr->second.first = time_now;
            return true;
        }
    }

    // user not found
    handleUnauthorized(request,tcp_conn);
    return false;
}
    
void cookie_auth::setOption(const std::string& name, const std::string& value) 
{
    if (name=="login")
        m_login = value;
    else if (name=="logout")
        m_logout = value;
    else if (name=="redirect")
        m_redirect = value;
    else
        BOOST_THROW_EXCEPTION( error::bad_arg() << error::errinfo_arg_name(name) );
}

bool cookie_auth::processLogin(HTTPRequestPtr& http_request, tcp::connection_ptr& tcp_conn)
{
    // strip off trailing slash if the request has one
    std::string resource(HTTPServer::stripTrailingSlash(http_request->getResource()));

    if (resource != m_login && resource != m_logout) {
        return false; // no login processing done
    }

    std::string redirect_url = algorithm::url_decode(http_request->getQuery("url"));
    std::string new_cookie;
    bool delete_cookie = false;

    if (resource == m_login) {
        // process login
        // check username
        std::string username = algorithm::url_decode(http_request->getQuery("user"));
        std::string password = algorithm::url_decode(http_request->getQuery("pass"));

        // match username/password
        PionUserPtr user=m_user_manager->getUser(username,password);
        if (!user) { // authentication failed, process as in case of failed authentication...
            handleUnauthorized(http_request,tcp_conn);
            return true;
        }
        // ok we have a new user session, create  a new cookie, add to cache

        // create random cookie
        std::string rand_binary;
        rand_binary.reserve(RANDOM_COOKIE_BYTES);
        for (unsigned int i=0; i<RANDOM_COOKIE_BYTES ; i++) {
            rand_binary += static_cast<unsigned char>(m_random_die());
        }
        algorithm::base64_encode(rand_binary, new_cookie);

        // add new session to cache
        boost::posix_time::ptime time_now(boost::posix_time::second_clock::universal_time());
        boost::mutex::scoped_lock cache_lock(m_cache_mutex);
        m_user_cache.insert(std::make_pair(new_cookie,std::make_pair(time_now,user)));
    } else {
        // process logout sequence
        // if auth cookie presented - clean cache out
        const std::string auth_cookie(http_request->getCookie(AUTH_COOKIE_NAME));
        if (! auth_cookie.empty()) {
            boost::mutex::scoped_lock cache_lock(m_cache_mutex);
            user_cache_type::iterator user_cache_itr=m_user_cache.find(auth_cookie);
            if (user_cache_itr!=m_user_cache.end()) {
                m_user_cache.erase(user_cache_itr);
            }
        }
        // and remove cookie from browser
        delete_cookie = true;
    }
    
    // if redirect defined - send redirect
    if (! redirect_url.empty()) {
        handleRedirection(http_request,tcp_conn,redirect_url,new_cookie,delete_cookie);
    } else {
        // otherwise - OK
        handleOk(http_request,tcp_conn,new_cookie,delete_cookie);
    }

    // yes, we processed login/logout somehow
    return true;
}

void cookie_auth::handleUnauthorized(HTTPRequestPtr& http_request,
    tcp::connection_ptr& tcp_conn)
{
    // if redirection option is used, send redirect
    if (!m_redirect.empty()) {
        handleRedirection(http_request,tcp_conn,m_redirect,"",false);
        return;
    }

    // authentication failed, send 401.....
    static const std::string CONTENT =
        " <!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\""
        "\"http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd\">"
        "<HTML>"
        "<HEAD>"
        "<TITLE>Error</TITLE>"
        "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=ISO-8859-1\">"
        "</HEAD>"
        "<BODY><H1>401 Unauthorized.</H1></BODY>"
        "</HTML> ";
    HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *http_request,
    boost::bind(&connection::finish, tcp_conn)));
    writer->getResponse().setStatusCode(HTTPTypes::RESPONSE_CODE_UNAUTHORIZED);
    writer->getResponse().setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_UNAUTHORIZED);
    writer->writeNoCopy(CONTENT);
    writer->send();
}

void cookie_auth::handleRedirection(HTTPRequestPtr& http_request,
                                        tcp::connection_ptr& tcp_conn,
                                        const std::string &redirection_url,
                                        const std::string &new_cookie,
                                        bool delete_cookie
                                        )
{
    // authentication failed, send 302.....
    static const std::string CONTENT =
        " <!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\""
        "\"http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd\">"
        "<HTML>"
        "<HEAD>"
        "<TITLE>Redirect</TITLE>"
        "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=ISO-8859-1\">"
        "</HEAD>"
        "<BODY><H1>302 Found.</H1></BODY>"
        "</HTML> ";
    HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *http_request,
        boost::bind(&connection::finish, tcp_conn)));
    writer->getResponse().setStatusCode(HTTPTypes::RESPONSE_CODE_FOUND);
    writer->getResponse().setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_FOUND);
    writer->getResponse().addHeader(HTTPTypes::HEADER_LOCATION, redirection_url);
    // Note: use empty pass "" while setting cookies to workaround IE/FF difference
    // It is assumed that request url points to the root
    // ToDo: find a better workaround
    if (delete_cookie) {
        // remove cookie
        writer->getResponse().deleteCookie(AUTH_COOKIE_NAME,"");
    } else if (!new_cookie.empty()) {
        // set up a new cookie
        writer->getResponse().setCookie(AUTH_COOKIE_NAME, new_cookie,"");
    }

    writer->writeNoCopy(CONTENT);
    writer->send();
}

void cookie_auth::handleOk(HTTPRequestPtr& http_request,
                              tcp::connection_ptr& tcp_conn,
                              const std::string &new_cookie,
                              bool delete_cookie
                              )
{
    // send 204 (No Content) response
    HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *http_request,
        boost::bind(&connection::finish, tcp_conn)));
    writer->getResponse().setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
    writer->getResponse().setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
    // Note: use empty pass "" while setting cookies to workaround IE/FF difference
    // It is assumed that request url points to the root
    // ToDo: find a better workaround
    if (delete_cookie) {
        // remove cookie
        writer->getResponse().deleteCookie(AUTH_COOKIE_NAME,"");
    } else if(!new_cookie.empty()) {
        // set up a new cookie
        writer->getResponse().setCookie(AUTH_COOKIE_NAME, new_cookie,"");
    }
    writer->send();
}

void cookie_auth::expireCache(const boost::posix_time::ptime &time_now)
{
    if (time_now > m_cache_cleanup_time + boost::posix_time::seconds(CACHE_EXPIRATION)) {
        // expire cache
        boost::mutex::scoped_lock cache_lock(m_cache_mutex);
        user_cache_type::iterator i;
        user_cache_type::iterator next=m_user_cache.begin();
        while (next!=m_user_cache.end()) {
            i=next;
            ++next;
            if (time_now > i->second.first + boost::posix_time::seconds(CACHE_EXPIRATION)) {
                // ok - this is an old record.. expire it now
                m_user_cache.erase(i);
            }
        }
        m_cache_cleanup_time = time_now;
    }
}

}   // end namespace http
}   // end namespace pion
