// ------------------------------------------------------------------
// pion-net: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2007 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file COPYING or copy at http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_HTTPSERVER_HEADER__
#define __PION_HTTPSERVER_HEADER__

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/PionPlugin.hpp>
#include <pion/net/TCPServer.hpp>
#include <pion/net/TCPConnection.hpp>
#include <pion/net/WebService.hpp>
#include <string>
#include <map>


namespace pion {	// begin namespace pion
namespace net {		// begin namespace net (Pion Network Library)

///
/// HTTPServer: a server that handles HTTP connections
///
class PION_NET_API HTTPServer :
	public TCPServer
{

public:

	/// exception thrown if a web service cannot be found
	class ServiceNotFoundException : public PionException {
	public:
		ServiceNotFoundException(const std::string& resource)
			: PionException("No web services are identified by the resource: ", resource) {}
	};

	/// exception thrown if the web service configuration file cannot be found
	class ConfigNotFoundException : public PionException {
	public:
		ConfigNotFoundException(const std::string& file)
			: PionException("Web service configuration file not found: ", file) {}
	};
	
	/// exception thrown if the plug-in file cannot be opened
	class ConfigParsingException : public PionException {
	public:
		ConfigParsingException(const std::string& file)
			: PionException("Unable to parse configuration file: ", file) {}
	};

	/// handler for requests that result in "400 Bad Request"
	typedef boost::function2<void, HTTPRequestPtr&,
		TCPConnectionPtr&>	BadRequestHandler;

	/// handler for requests that result in "404 Not Found"
	typedef boost::function2<void, HTTPRequestPtr&,
		 TCPConnectionPtr&>	NotFoundHandler;

	/// handler for requests that result in "500 Server Error"
	typedef boost::function3<void, HTTPRequestPtr&, TCPConnectionPtr&,
		const std::string&>	ServerErrorHandler;
	
	
	/**
	 * creates new HTTPServer objects
	 *
	 * @param tcp_port port number used to listen for new connections
	 */
	static inline boost::shared_ptr<HTTPServer> create(const unsigned int tcp_port)
	{
		return boost::shared_ptr<HTTPServer>(new HTTPServer(tcp_port));
	}

	/// default destructor
	virtual ~HTTPServer() {}
	
	/**
	 * adds a new web service to the HTTP server
	 *
	 * @param resource the resource name or uri-stem to bind to the web service
	 * @param service_ptr a pointer to the web service
	 */
	void addService(const std::string& resource, WebService *service_ptr);
	
	/**
	 * loads a web service from a shared object file
	 *
	 * @param resource the resource name or uri-stem to bind to the web service
	 * @param service_name the name of the web service to load (searches plug-in
	 *        directories and appends extensions)
	 */
	void loadService(const std::string& resource, const std::string& service_name);
	
	/**
	 * sets a configuration option for the web service associated with resource
	 *
	 * @param resource the resource name or uri-stem that identifies the web service
	 * @param name the name of the configuration option
	 * @param value the value to set the option to
	 */
	void setServiceOption(const std::string& resource,
						  const std::string& name, const std::string& value);
	
	/**
	 * Parses a simple web service configuration file. Each line in the file
	 * starts with one of the following commands:
	 *
	 * path VALUE  :  adds a directory to the web service search path
	 * service RESOURCE FILE  :  loads web service bound to RESOURCE from FILE
	 * option RESOURCE NAME=VALUE  :  sets web service option NAME to VALUE
	 *
	 * Blank lines or lines that begin with # are ignored as comments.
	 *
	 * @param config_name the name of the config file to parse
	 */
	void loadServiceConfig(const std::string& config_name);

	/// clears all the web services that are currently configured
	void clearServices(void);
	
	/// sets the function that handles bad HTTP requests
	inline void setBadRequestHandler(BadRequestHandler h) { m_bad_request_handler = h; }
	
	/// sets the function that handles requests which match no other web services
	inline void setNotFoundHandler(NotFoundHandler h) { m_not_found_handler = h; }
	
	/// sets the function that handles requests which match no other web services
	inline void setServerErrorHandler(ServerErrorHandler h) { m_server_error_handler = h; }

	
protected:
	
	/**
	 * protected constructor restricts creation of objects (use create())
	 * 
	 * @param tcp_port port number used to listen for new connections
	 */
	explicit HTTPServer(const unsigned int tcp_port)
		: TCPServer(tcp_port),
		m_bad_request_handler(HTTPServer::handleBadRequest),
		m_not_found_handler(HTTPServer::handleNotFoundRequest),
		m_server_error_handler(HTTPServer::handleServerError)
	{ 
		setLogger(PION_GET_LOGGER("Pion.HTTPServer"));
	}
	
	/**
	 * handles a new TCP connection
	 * 
	 * @param tcp_conn the new TCP connection to handle
	 */
	virtual void handleConnection(TCPConnectionPtr& tcp_conn);
	
	/**
	 * handles a new HTTP request
	 *
	 * @param http_request the HTTP request to handle
	 * @param tcp_conn TCP connection containing a new request
	 */
	void handleRequest(HTTPRequestPtr& http_request, TCPConnectionPtr& tcp_conn);
		
	/**
	 * used to send responses when a bad HTTP request is made
	 *
     * @param http_request the new HTTP request to handle
     * @param tcp_conn the TCP connection that has the new request
	 */
	 static void handleBadRequest(HTTPRequestPtr& http_request,
								  TCPConnectionPtr& tcp_conn);
	
	/**
	 * used to send responses when no web services can handle the request
	 *
     * @param http_request the new HTTP request to handle
     * @param tcp_conn the TCP connection that has the new request
	 */
	static void handleNotFoundRequest(HTTPRequestPtr& http_request,
									  TCPConnectionPtr& tcp_conn);
	
	/**
	 * used to send responses when a server error occurs
	 *
     * @param http_request the new HTTP request to handle
     * @param tcp_conn the TCP connection that has the new request
	 * @param error_msg message that explains what went wrong
	 */
	static void handleServerError(HTTPRequestPtr& http_request,
								  TCPConnectionPtr& tcp_conn,
								  const std::string& error_msg);
	
	/// called before the TCP server starts listening for new connections
	virtual void beforeStarting(void);
	
	/// called after the TCP server has stopped listening for new connections
	virtual void afterStopping(void);

	
private:
	
	/// used by WebServiceMap to associate web service objects with plug-in libraries
	typedef std::pair<WebService *, PionPluginPtr<WebService> >	PluginPair;
	
	/// data type for a collection of web services
	class WebServiceMap
		: public std::map<std::string, PluginPair>
	{
	public:
		void clear(void);
		virtual ~WebServiceMap() { WebServiceMap::clear(); }
		WebServiceMap(void) {}
	};
	
	
	/// Web services associated with this server
	WebServiceMap			m_services;

	/// mutex to make class thread-safe
	boost::mutex			m_mutex;

	/// points to a function that handles bad HTTP requests
	BadRequestHandler		m_bad_request_handler;
	
	/// points to a function that handles requests which match no web services
	NotFoundHandler			m_not_found_handler;

	/// points to the function that handles server errors
	ServerErrorHandler		m_server_error_handler;
};


/// data type for a HTTP protocol handler pointer
typedef boost::shared_ptr<HTTPServer>		HTTPServerPtr;


}	// end namespace net
}	// end namespace pion

#endif
