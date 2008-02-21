// ------------------------------------------------------------------
// pion-net: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2007-2008 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/logic/tribool.hpp>
#include <pion/net/HTTPMessage.hpp>
#include <pion/net/HTTPRequest.hpp>
#include <pion/net/HTTPResponse.hpp>
#include <pion/net/HTTPParser.hpp>
#include <pion/net/TCPConnection.hpp>


namespace pion {	// begin namespace pion
namespace net {		// begin namespace net (Pion Network Library)

// static members of HTTPMessage

const boost::regex		HTTPMessage::REGEX_ICASE_CHUNKED("chunked", boost::regex::icase);


// HTTPRequest member functions

std::size_t HTTPMessage::send(TCPConnection& tcp_conn,
							  boost::system::error_code& ec)
{
	// initialize write buffers for send operation using HTTP headers
	WriteBuffers write_buffers;
	prepareBuffersForSend(write_buffers, tcp_conn.getKeepAlive(), false);

	// append payload content to write buffers (if there is any)
	if (getContentLength() > 0 && getContent() != NULL && getContent()[0] != '\0')
		write_buffers.push_back(boost::asio::buffer(getContent(), getContentLength()));

	// send the message and return the result
	return tcp_conn.write(write_buffers, ec);
}

std::size_t HTTPMessage::receive(TCPConnection& tcp_conn,
								 boost::system::error_code& ec)
{
	static ReceiveError RECEIVE_ERROR;
	// assumption: this can only be either an HTTPRequest or an HTTPResponse
	const bool is_request = (dynamic_cast<HTTPRequest*>(this) != NULL);
	HTTPParser http_parser(is_request);
	std::size_t last_bytes_read = 0;

	// make sure that we start out with an empty message
	clear();

	if (tcp_conn.getPipelined()) {
		// there are pipelined messages available in the connection's read buffer
		const char *read_ptr;
		const char *read_end_ptr;
		tcp_conn.loadReadPosition(read_ptr, read_end_ptr);
		last_bytes_read = (read_end_ptr - read_ptr);
		http_parser.setReadBuffer(read_ptr, last_bytes_read);
	} else {
		// read buffer is empty (not pipelined) -> read some bytes from the connection
		last_bytes_read = tcp_conn.read_some(ec);
		if (ec) return 0;
		http_parser.setReadBuffer(tcp_conn.getReadBuffer().data(), last_bytes_read);
	}

	// incrementally read and parse bytes from the connection
	boost::tribool parse_result;
	while (true) {
		// parse bytes available in the read buffer
		parse_result = http_parser.parseHTTPHeaders(*this);
		if (! boost::indeterminate(parse_result)) break;

		// read more bytes from the connection
		last_bytes_read = tcp_conn.read_some(ec);
		if (ec) return http_parser.getTotalBytesRead();
		http_parser.setReadBuffer(tcp_conn.getReadBuffer().data(), last_bytes_read);
	}
	
	if (parse_result == false) {
		// an error occurred while parsing the message headers
		ec.assign(1, RECEIVE_ERROR);
		return http_parser.getTotalBytesRead();
	}

	bool force_connection_closed = false;
	std::size_t content_bytes_to_read = 0;
	updateTransferCodingUsingHeader();
	
	if (isChunked()) {
		// message content is encoded using chunks
		
		while (true) {
			// parse bytes available in the read buffer
			parse_result = http_parser.parseChunks(m_chunk_buffers);
			if (parse_result == false) {
				// an error occurred while parsing the message headers
				ec.assign(1, RECEIVE_ERROR);
				return http_parser.getTotalBytesRead();
			}
			if (parse_result == true) break;

			// parse_result was indeterminate, so read more bytes from the connection
			last_bytes_read = tcp_conn.read_some(ec);
			if (ec) return http_parser.getTotalBytesRead();
			http_parser.setReadBuffer(tcp_conn.getReadBuffer().data(), last_bytes_read);
		}
		concatenateChunks();
		
	} else if (! isContentLengthImplied()) {
		// we cannot assume that the message has no content
		
		if (hasHeader(HTTPTypes::HEADER_CONTENT_LENGTH)) {
			// message has a content-length header
			
			// set content length & consume any payload content left in the read buffers
			last_bytes_read = http_parser.consumeContent(*this);
			content_bytes_to_read = getContentLength() - last_bytes_read;
			if (content_bytes_to_read > 0) {
				// read remainder of payload content from the connection
				last_bytes_read = tcp_conn.read(boost::asio::buffer(getContent() + last_bytes_read, content_bytes_to_read),
												boost::asio::transfer_at_least(content_bytes_to_read), ec);
				if (ec) return http_parser.getTotalBytesRead();
			}
			
		} else {
			// no content-length specified, and the content length cannot
			// otherwise be determined

			// only if not a request, read through the close of the connection
			if (dynamic_cast<HTTPRequest*>(this) == NULL) {

				force_connection_closed = true;	// make sure lifecycle will be set to close
				content_bytes_to_read = 0;		// note this is used to calculate total bytes read
				m_chunk_buffers.clear();		// clear the chunk buffers before we start

				// read in the remaining data available
				while (true) {
					// use the parser to consume the next chunk
					http_parser.consumeContentAsNextChunk(m_chunk_buffers);

					// read some more data from the connection
					last_bytes_read = tcp_conn.read_some(ec);
					if (ec || last_bytes_read == 0) {
						ec.clear();		// clear error code for connection closed
						break;
					}
					http_parser.setReadBuffer(tcp_conn.getReadBuffer().data(), last_bytes_read);
					content_bytes_to_read += last_bytes_read;
				}
				
				// concatenate the chunks together into a new content buffer
				concatenateChunks();
			} else {
				// the message has no content
				
				setContentLength(0);
				createContentBuffer();
			}
		}
	} else {
		// the message has no content
		
		setContentLength(0);
		createContentBuffer();
	}		

	// the message is valid: finish it (sets valid flag)
	if (is_request)
		http_parser.finishRequest(dynamic_cast<HTTPRequest&>(*this));
	else
		http_parser.finishResponse(dynamic_cast<HTTPResponse&>(*this));
	
	// set the connection's lifecycle type
	if (!force_connection_closed && checkKeepAlive()) {
		if ( http_parser.eof() ) {
			// the connection should be kept alive, but does not have pipelined messages
			tcp_conn.setLifecycle(TCPConnection::LIFECYCLE_KEEPALIVE);
		} else {
			// the connection has pipelined messages
			tcp_conn.setLifecycle(TCPConnection::LIFECYCLE_PIPELINED);
			
			// save the read position as a bookmark so that it can be retrieved
			// by a new HTTP parser, which will be created after the current
			// message has been handled
			const char *read_ptr;
			const char *read_end_ptr;
			http_parser.loadReadPosition(read_ptr, read_end_ptr);
			tcp_conn.saveReadPosition(read_ptr, read_end_ptr);
		}
	} else {
		// default to close the connection
		tcp_conn.setLifecycle(TCPConnection::LIFECYCLE_CLOSE);
	}

	return (http_parser.getTotalBytesRead() + content_bytes_to_read);
}
	
void HTTPMessage::concatenateChunks(void)
{
	std::size_t sumOfChunkSizes = 0;
	ChunkCache::const_iterator i;
	for (i = m_chunk_buffers.begin(); i != m_chunk_buffers.end(); ++i) {
		sumOfChunkSizes += i->size();
	}
	setContentLength(sumOfChunkSizes);
	if (sumOfChunkSizes > 0) {
		char *post_buffer = createContentBuffer();
		for (i = m_chunk_buffers.begin(); i != m_chunk_buffers.end(); ++i) {
			memcpy(post_buffer, &((*i)[0]), i->size());
			post_buffer += i->size();
		}
	}
}
	
}	// end namespace net
}	// end namespace pion
