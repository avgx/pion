// ------------------------------------------------------------------
// pion-net: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2007 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file COPYING or copy at http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_HTTPRESPONSE_HEADER__
#define __PION_HTTPRESPONSE_HEADER__

#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <pion/PionConfig.hpp>
#include <pion/net/HTTPMessage.hpp>


namespace pion {	// begin namespace pion
namespace net {		// begin namespace net (Pion Network Library)

	
///
/// HTTPResponse: container for HTTP response information
/// 
class HTTPResponse
	: public HTTPMessage
{
public:

	/// constructs a new HTTPResponse object
	HTTPResponse(void)
		: m_status_code(RESPONSE_CODE_OK),
		m_status_message(RESPONSE_MESSAGE_OK) {}
	
	/// copy constructor
	HTTPResponse(const HTTPResponse& http_response)
		: m_status_code(http_response.m_status_code),
		m_status_message(http_response.m_status_message) {}
	
	/**
	 * constructs a new HTTPResponse object for a particular request
	 *
	 * @param http_request the request that this is responding to
	 */
	HTTPResponse(const HTTPMessage& http_request)
		: m_status_code(RESPONSE_CODE_OK),
		m_status_message(RESPONSE_MESSAGE_OK)
	{
		if (http_request.getVersionMajor() == 1 && http_request.getVersionMinor() >= 1)
			setChunksSupported(true);
	}

	/// virtual destructor
	virtual ~HTTPResponse() {}

	/// clears all response data
	virtual void clear(void) {
		HTTPMessage::clear();
		m_first_line.erase();
		m_status_code = RESPONSE_CODE_OK;
		m_status_message = RESPONSE_MESSAGE_OK;
	}

	/// sets the HTTP response status code
	inline void setStatusCode(unsigned int n) { m_status_code = n; }

	/// sets the HTTP response status message
	inline void setStatusMessage(const std::string& msg) { m_status_message = msg; }
	
	/// returns the HTTP response status code
	inline unsigned int getStatusCode(void) const { return m_status_code; }
	
	/// returns the HTTP response status message
	inline const std::string& getStatusMessage(void) const { return m_status_message; }
	

	/**
	 * sets a cookie by adding a Set-Cookie header (see RFC 2109)
	 * the cookie will be discarded by the user-agent when it closes
	 *
	 * @param name the name of the cookie
	 * @param value the value of the cookie
	 */
	inline void setCookie(const std::string& name, const std::string& value) {
		std::string set_cookie_header(make_set_cookie_header(name, value, ""));
		addHeader(HEADER_SET_COOKIE, set_cookie_header);
	}
	
	/**
	 * sets a cookie by adding a Set-Cookie header (see RFC 2109)
	 * the cookie will be discarded by the user-agent when it closes
	 *
	 * @param name the name of the cookie
	 * @param value the value of the cookie
	 * @param path the path of the cookie
	 */
	inline void setCookie(const std::string& name, const std::string& value,
						  const std::string& path)
	{
		std::string set_cookie_header(make_set_cookie_header(name, value, path));
		addHeader(HEADER_SET_COOKIE, set_cookie_header);
	}
	
	/**
	 * sets a cookie by adding a Set-Cookie header (see RFC 2109)
	 *
	 * @param name the name of the cookie
	 * @param value the value of the cookie
	 * @param path the path of the cookie
	 * @param max_age the life of the cookie, in seconds (0 = discard)
	 */
	inline void setCookie(const std::string& name, const std::string& value,
						  const std::string& path, const unsigned long max_age)
	{
		std::string set_cookie_header(make_set_cookie_header(name, value, path, true, max_age));
		addHeader(HEADER_SET_COOKIE, set_cookie_header);
	}
	
	/**
	 * sets a cookie by adding a Set-Cookie header (see RFC 2109)
	 *
	 * @param name the name of the cookie
	 * @param value the value of the cookie
	 * @param max_age the life of the cookie, in seconds (0 = discard)
	 */
	inline void setCookie(const std::string& name, const std::string& value,
						  const unsigned long max_age)
	{
		std::string set_cookie_header(make_set_cookie_header(name, value, "", true, max_age));
		addHeader(HEADER_SET_COOKIE, set_cookie_header);
	}
	
	/// deletes cookie called name by adding a Set-Cookie header (cookie has no path)
	inline void deleteCookie(const std::string& name) {
		std::string set_cookie_header(make_set_cookie_header(name, "", "", true, 0));
		addHeader(HEADER_SET_COOKIE, set_cookie_header);
	}
	
	/// deletes cookie called name by adding a Set-Cookie header (cookie has a path)
	inline void deleteCookie(const std::string& name, const std::string& path) {
		std::string set_cookie_header(make_set_cookie_header(name, "", path, true, 0));
		addHeader(HEADER_SET_COOKIE, set_cookie_header);
	}
	
	/// sets the time that the response was last modified (Last-Modified)
	inline void setLastModified(const unsigned long t) {
		changeHeader(HEADER_LAST_MODIFIED, get_date_string(t));
	}

	
protected:
	
	/// returns a string containing the first line for the HTTP message
	virtual const std::string& getFirstLine(void) const {
		// start out with the HTTP version
		m_first_line = STRING_HTTP_VERSION;
		m_first_line += ' ';
		// append the response status code
		m_first_line +=  boost::lexical_cast<std::string>(m_status_code);
		m_first_line += ' ';
		// append the response status message
		m_first_line += m_status_message;
		// return the first response line
		return m_first_line;
	}
	
	
private:

	/// first line sent in a HTTP response (i.e. "HTTP/1.1 200 OK")
	mutable std::string		m_first_line;

	/// The HTTP response status code
	unsigned int			m_status_code;
	
	/// The HTTP response status message
	std::string				m_status_message;
};


/// data type for a HTTP response pointer
typedef boost::shared_ptr<HTTPResponse>		HTTPResponsePtr;


}	// end namespace net
}	// end namespace pion

#endif
