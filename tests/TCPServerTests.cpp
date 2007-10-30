// ------------------------------------------------------------------
// pion-net: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2007 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file COPYING or copy at http://www.boost.org/LICENSE_1_0.txt
//

#include <pion/PionConfig.hpp>
#include <pion/PionScheduler.hpp>
#include <pion/net/TCPServer.hpp>
#include <boost/bind.hpp>
#include <boost/thread/xtime.hpp>
#include <boost/thread/thread.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace pion;
using namespace pion::net;
using boost::asio::ip::tcp;

/// sets up logging (run once only)
extern void setup_logging_for_unit_tests(void);


///
/// HelloServer: simple TCP server that sends "Hello there!" after receiving some data
/// 
class HelloServer
	: public pion::net::TCPServer
{
public:
	/// default constructor
	virtual ~HelloServer() {}

	/**
	 * creates a Hello server
	 *
	 * @param tcp_port port number used to listen for new connections (IPv4)
	 */
	HelloServer(const unsigned int tcp_port) : pion::net::TCPServer(tcp_port) {}
	
	/**
	 * handles a new TCP connection
	 * 
	 * @param tcp_conn the new TCP connection to handle
	 */
	virtual void handleConnection(pion::net::TCPConnectionPtr& tcp_conn) {
		static const std::string HELLO_MESSAGE("Hello there!\n");
		tcp_conn->setLifecycle(pion::net::TCPConnection::LIFECYCLE_CLOSE);	// make sure it will get closed
		tcp_conn->async_write(boost::asio::buffer(HELLO_MESSAGE),
							  boost::bind(&HelloServer::handleWrite, this, tcp_conn,
										  boost::asio::placeholders::error));
	}

	
private:
	
	/**
	 * called after the initial greeting has been sent
	 *
     * @param tcp_conn the TCP connection to the server
	 * @param write_error message that explains what went wrong (if anything)
	 */
	void handleWrite(pion::net::TCPConnectionPtr& tcp_conn,
					 const boost::system::error_code& write_error)
	{
		if (write_error) {
			tcp_conn->finish();
		} else {
			tcp_conn->async_read_some(boost::bind(&HelloServer::handleRead, this, tcp_conn,
												  boost::asio::placeholders::error,
												  boost::asio::placeholders::bytes_transferred));
		}
	}
	
	/**
	 * called after the client's greeting has been received
	 *
     * @param tcp_conn the TCP connection to the server
	 * @param read_error message that explains what went wrong (if anything)
	 * @param bytes_read number of bytes read from the client
	 */
	void handleRead(pion::net::TCPConnectionPtr& tcp_conn,
					const boost::system::error_code& read_error,
					std::size_t bytes_read)
	{
		static const std::string HELLO_MESSAGE("Goodbye!\n");
		if (read_error) {
			tcp_conn->finish();
		} else {
			tcp_conn->async_write(boost::asio::buffer(HELLO_MESSAGE),
								  boost::bind(&pion::net::TCPConnection::finish, tcp_conn));
		}
	}
};


///
/// HelloServerTests_F: fixture used for running (Hello) server tests
/// 
class HelloServerTests_F {
public:
	// default constructor and destructor
	HelloServerTests_F()
		: hello_server_ptr(new HelloServer(8080))
	{
		setup_logging_for_unit_tests();
		// start the HTTP server
		hello_server_ptr->start();
	}
	~HelloServerTests_F() {
		hello_server_ptr->stop();
	}
	inline TCPServerPtr& getServerPtr(void) { return hello_server_ptr; }

	/**
	 * put the current thread to sleep for an amount of time
	 *
	 * @param nsec number of nanoseconds (10^-9) to sleep for (default = 0.01 seconds)
	 */
	inline void sleep_briefly(unsigned long nsec = 50000000)
	{
		boost::xtime stop_time;
		boost::xtime_get(&stop_time, boost::TIME_UTC);
		stop_time.nsec += nsec;
		if (stop_time.nsec >= 1000000000) {
			stop_time.sec++;
			stop_time.nsec -= 1000000000;
		}
		boost::thread::sleep(stop_time);
	}
	
private:
	TCPServerPtr	hello_server_ptr;
};


// HelloServer Test Cases

BOOST_FIXTURE_TEST_SUITE(HelloServerTests_S, HelloServerTests_F)

BOOST_AUTO_TEST_CASE(checkTCPServerIsListening) {
	BOOST_CHECK(getServerPtr()->isListening());
}

BOOST_AUTO_TEST_CASE(checkNumberOfActiveServerConnections) {
	// there should be no connections to start
	// sleep first just in case other tests ran before this one, which are still
	do {
		sleep_briefly();
	} while (PionScheduler::getInstance().getRunningThreads() == 0);	
	BOOST_CHECK_EQUAL(getServerPtr()->getConnections(), static_cast<unsigned long>(0));

	// open a connection
	tcp::endpoint localhost(boost::asio::ip::address::from_string("127.0.0.1"), 8080);
	tcp::iostream tcp_stream_a(localhost);
	// we need to wait for the server to accept the connection since it happens
	// in another thread.  This should always take less than 0.1 seconds
	sleep_briefly();
	BOOST_CHECK_EQUAL(getServerPtr()->getConnections(), static_cast<unsigned long>(1));

	// open a few more connections;
	tcp::iostream tcp_stream_b(localhost);
	sleep_briefly();
	BOOST_CHECK_EQUAL(getServerPtr()->getConnections(), static_cast<unsigned long>(2));

	tcp::iostream tcp_stream_c(localhost);
	sleep_briefly();
	BOOST_CHECK_EQUAL(getServerPtr()->getConnections(), static_cast<unsigned long>(3));

	tcp::iostream tcp_stream_d(localhost);
	sleep_briefly();
	BOOST_CHECK_EQUAL(getServerPtr()->getConnections(), static_cast<unsigned long>(4));
	
	// close connections	
	tcp_stream_a.close();
	sleep_briefly();
	BOOST_CHECK_EQUAL(getServerPtr()->getConnections(), static_cast<unsigned long>(3));

	tcp_stream_b.close();
	sleep_briefly();
	BOOST_CHECK_EQUAL(getServerPtr()->getConnections(), static_cast<unsigned long>(2));

	tcp_stream_c.close();
	sleep_briefly();
	BOOST_CHECK_EQUAL(getServerPtr()->getConnections(), static_cast<unsigned long>(1));

	tcp_stream_d.close();
	sleep_briefly();
	BOOST_CHECK_EQUAL(getServerPtr()->getConnections(), static_cast<unsigned long>(0));
}

BOOST_AUTO_TEST_CASE(checkServerConnectionBehavior) {
	// open a connection
	tcp::endpoint localhost(boost::asio::ip::address::from_string("127.0.0.1"), 8080);
	tcp::iostream tcp_stream_a(localhost);

	// read greeting from the server
	std::string message;
	std::getline(tcp_stream_a, message);
	BOOST_CHECK(message == "Hello there!");

	// open a second connection & read the greeting
	tcp::iostream tcp_stream_b(localhost);
	std::getline(tcp_stream_b, message);
	BOOST_CHECK(message == "Hello there!");

	// send greeting to the first server
	tcp_stream_a << "Hi!\n";
	tcp_stream_a.flush();

	// send greeting to the second server
	tcp_stream_b << "Hi!\n";
	tcp_stream_b.flush();
	
	// receive goodbye from the first server
	std::getline(tcp_stream_a, message);
	tcp_stream_a.close();
	BOOST_CHECK(message == "Goodbye!");

	// receive goodbye from the second server
	std::getline(tcp_stream_b, message);
	tcp_stream_b.close();
	BOOST_CHECK(message == "Goodbye!");
}

BOOST_AUTO_TEST_SUITE_END()
