// ------------------------------------------------------------------
// pion-net: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2007-2008 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/test/unit_test.hpp>
#include <pion/PionConfig.hpp>
#include <pion/net/TCPStream.hpp>

using namespace std;
using namespace pion;
using namespace pion::net;
using boost::asio::ip::tcp;

/// sets up logging (run once only)
extern void setup_logging_for_unit_tests(void);


///
/// TCPStreamTests_F: fixture used for performing TCPStream tests
/// 
class TCPStreamTests_F {
public:
	
	/// data type for a function that handles TCPStream connections
	typedef boost::function1<void,TCPStream&>	ConnectionHandler;
	
	
	// default constructor and destructor
	TCPStreamTests_F()
	{
		setup_logging_for_unit_tests();
	}
	virtual ~TCPStreamTests_F() {}
	
	/**
	 * listen for a TCP connection and call the connection handler when connected
	 *
	 * @param conn_handler function to call after a connection is established
	 */
	void acceptConnection(ConnectionHandler conn_handler) {
		// configure the acceptor service
		tcp::acceptor	tcp_acceptor(m_io_service);
		tcp::endpoint	tcp_endpoint(tcp::v4(), 8080);
		tcp_acceptor.open(tcp_endpoint.protocol());
		// allow the acceptor to reuse the address (i.e. SO_REUSEADDR)
		tcp_acceptor.set_option(tcp::acceptor::reuse_address(true));
		tcp_acceptor.bind(tcp_endpoint);
		tcp_acceptor.listen();

		// schedule another thread to listen for a TCP connection
		TCPStream listener_stream(m_io_service);
		boost::system::error_code ec = listener_stream.accept(tcp_acceptor);
		tcp_acceptor.close();
		BOOST_REQUIRE(! ec);
		
		// call the connection handler
		conn_handler(listener_stream);
	}
	
	/// sends a "Hello" to a TCPStream
	static void sendHello(TCPStream& str) {
		str << "Hello" << std::endl;
		str.flush();
	}
	
	/// used to schedule work across multiple threads
	boost::asio::io_service		m_io_service;
};


// TCPStream Test Cases

BOOST_FIXTURE_TEST_SUITE(TCPStreamTests_S, TCPStreamTests_F)

BOOST_AUTO_TEST_CASE(checkTCPConnectToAnotherStream) {
	// schedule another thread to listen for a TCP connection
	ConnectionHandler conn_handler(boost::bind(&TCPStreamTests_F::sendHello, _1));
	boost::thread listener_thread(boost::bind(&TCPStreamTests_F::acceptConnection,
											  this, conn_handler) );
	
	// connect to the listener
	TCPStream client_str(m_io_service);
	boost::system::error_code ec;
	ec = client_str.connect(boost::asio::ip::address::from_string("127.0.0.1"), 8080);
	BOOST_REQUIRE(! ec);
	
	// get the hello message
	std::string response_msg;
	client_str >> response_msg;
	BOOST_CHECK_EQUAL(response_msg, "Hello");
}

BOOST_AUTO_TEST_SUITE_END()

#define BIG_BUF_SIZE (12 * 1024)

///
/// TCPStreamBufferTests_F: fixture that includes a big data buffer used for tests
/// 
class TCPStreamBufferTests_F
	: public TCPStreamTests_F
{
public:
	// default constructor and destructor
	TCPStreamBufferTests_F() {
		// fill the buffer with non-random characters
		for (unsigned long n = 0; n < BIG_BUF_SIZE; ++n) {
			m_big_buf[n] = char(n);
		}
	}
	virtual ~TCPStreamBufferTests_F() {}
	
	/// sends the big buffer contents to a TCPStream
	void sendBigBuffer(TCPStream& str) {
		str.write(m_big_buf, BIG_BUF_SIZE);
		str.flush();
	}
	
	/// big data buffer used for the tests
	char m_big_buf[BIG_BUF_SIZE];
};

	
BOOST_FIXTURE_TEST_SUITE(TCPStreamBufferTests_S, TCPStreamBufferTests_F)

BOOST_AUTO_TEST_CASE(checkSendAndReceiveBiggerThanBuffers) {
	// schedule another thread to listen for a TCP connection
	ConnectionHandler conn_handler(boost::bind(&TCPStreamBufferTests_F::sendBigBuffer, this, _1));
	boost::thread listener_thread(boost::bind(&TCPStreamBufferTests_F::acceptConnection,
											  this, conn_handler) );
	
	// connect to the listener
	TCPStream client_str(m_io_service);
	boost::system::error_code ec;
	ec = client_str.connect(boost::asio::ip::address::from_string("127.0.0.1"), 8080);
	BOOST_REQUIRE(! ec);
	
	// read the big buffer contents
	char another_buf[BIG_BUF_SIZE];
	BOOST_REQUIRE(client_str.read(another_buf, BIG_BUF_SIZE));
	BOOST_CHECK_EQUAL(memcmp(m_big_buf, another_buf, BIG_BUF_SIZE), 0);
}

BOOST_AUTO_TEST_SUITE_END()
