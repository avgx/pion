// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2012 Cloudmeter, Inc.  (http://www.cloudmeter.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_HTTP_RESPONSE_READER_HEADER__
#define __PION_HTTP_RESPONSE_READER_HEADER__

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/function/function2.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <pion/config.hpp>
#include <pion/http/response.hpp>
#include <pion/http/reader.hpp>


namespace pion {    // begin namespace pion
namespace http {    // begin namespace http


///
/// HTTPResponseReader: asynchronously reads and parses HTTP responses
///
class HTTPResponseReader :
    public HTTPReader,
    public boost::enable_shared_from_this<HTTPResponseReader>
{

public:

    /// function called after the HTTP message has been parsed
    typedef boost::function3<void, HTTPResponsePtr, tcp::connection_ptr,
        const boost::system::error_code&>   FinishedHandler;

    
    // default destructor
    virtual ~HTTPResponseReader() {}
    
    /**
     * creates new HTTPResponseReader objects
     *
     * @param tcp_conn TCP connection containing a new message to parse
     * @param http_request the request we are responding to
     * @param handler function called after the message has been parsed
     */
    static inline boost::shared_ptr<HTTPResponseReader>
        create(tcp::connection_ptr& tcp_conn, const HTTPRequest& http_request,
               FinishedHandler handler)
    {
        return boost::shared_ptr<HTTPResponseReader>
            (new HTTPResponseReader(tcp_conn, http_request, handler));
    }

    /// sets a function to be called after HTTP headers have been parsed
    inline void setHeadersParsedCallback(FinishedHandler& h) { m_parsed_headers = h; }

    
protected:

    /**
     * protected constructor restricts creation of objects (use create())
     *
     * @param tcp_conn TCP connection containing a new message to parse
     * @param http_request the request we are responding to
     * @param handler function called after the message has been parsed
     */
    HTTPResponseReader(tcp::connection_ptr& tcp_conn, const HTTPRequest& http_request,
                       FinishedHandler handler)
        : HTTPReader(false, tcp_conn), m_http_msg(new HTTPResponse(http_request)),
        m_finished(handler)
    {
        m_http_msg->setRemoteIp(tcp_conn->get_remote_ip());
        setLogger(PION_GET_LOGGER("pion.http.HTTPResponseReader"));
    }
        
    /// Reads more bytes from the TCP connection
    virtual void readBytes(void) {
        get_connection()->async_read_some(boost::bind(&HTTPResponseReader::consumeBytes,
                                                        shared_from_this(),
                                                        boost::asio::placeholders::error,
                                                        boost::asio::placeholders::bytes_transferred));
    }

    /// Called after we have finished parsing the HTTP message headers
    virtual void finishedParsingHeaders(const boost::system::error_code& ec) {
        // call the finished headers handler with the HTTP message
        if (m_parsed_headers) m_parsed_headers(m_http_msg, get_connection(), ec);
    }
    
    /// Called after we have finished reading/parsing the HTTP message
    virtual void finishedReading(const boost::system::error_code& ec) {
        // call the finished handler with the finished HTTP message
        if (m_finished) m_finished(m_http_msg, get_connection(), ec);
    }
    
    /// Returns a reference to the HTTP message being parsed
    virtual http::message& getMessage(void) { return *m_http_msg; }

    
    /// The new HTTP message container being created
    HTTPResponsePtr             m_http_msg;

    /// function called after the HTTP message has been parsed
    FinishedHandler             m_finished;

    /// function called after the HTTP message headers have been parsed
    FinishedHandler             m_parsed_headers;
};


/// data type for a HTTPResponseReader pointer
typedef boost::shared_ptr<HTTPResponseReader>   HTTPResponseReaderPtr;


}   // end namespace http
}   // end namespace pion

#endif
