// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2008 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Pion is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Pion is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
// more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with Pion.  If not, see <http://www.gnu.org/licenses/>.
//

#include <pion/platform/ConfigManager.hpp>
#include "HTTPProtocol.hpp"

using namespace pion::net;
using namespace pion::platform;

namespace pion {	// begin namespace pion
namespace plugins {		// begin namespace plugins


// static members of HTTPProtocol
	
const std::string HTTPProtocol::REQUEST_CONTENT_ELEMENT_NAME = "RequestContent";
const std::string HTTPProtocol::RESPONSE_CONTENT_ELEMENT_NAME = "ResponseContent";
const std::string HTTPProtocol::CONTENT_TYPE_ELEMENT_NAME = "ContentType";
const std::string HTTPProtocol::MAX_SIZE_ELEMENT_NAME = "MaxSize";
	
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CS_BYTES="urn:vocab:clickstream#cs-bytes";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_SC_BYTES="urn:vocab:clickstream#sc-bytes";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_BYTES="urn:vocab:clickstream#bytes";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_STATUS="urn:vocab:clickstream#status";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_COMMENT="urn:vocab:clickstream#comment";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_METHOD="urn:vocab:clickstream#method";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_URI="urn:vocab:clickstream#uri";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_URI_STEM="urn:vocab:clickstream#uri-stem";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_URI_QUERY="urn:vocab:clickstream#uri-query";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_REQUEST="urn:vocab:clickstream#request";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_HOST="urn:vocab:clickstream#host";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_REFERER="urn:vocab:clickstream#referer";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_USERAGENT="urn:vocab:clickstream#useragent";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CS_CONTENT="urn:vocab:clickstream#cs-content";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_SC_CONTENT="urn:vocab:clickstream#sc-content";
const std::string HTTPProtocol::VOCAB_CLICKSTREAM_CACHED="urn:vocab:clickstream#cached";

	
// HTTPProtocol member functions

boost::tribool HTTPProtocol::readNext(bool request, const char *ptr, size_t len, 
									  EventPtr& event_ptr_ref)
{
	// parse the data
	boost::tribool rc;
	if (request) {
		m_request_parser.setReadBuffer(ptr, len);
		rc = m_request_parser.parse(m_request);
	} else {
		m_response_parser.setReadBuffer(ptr, len);
		rc = m_response_parser.parse(m_response);
	}

	// message has been fully parsed, generate an event
	if (rc == true) {
		if (request) {
			// update response to "know" about the request (this influences parsing)
			m_response.updateRequestInfo(m_request);
			 // wait until the response is parsed before generating an event
			rc = boost::indeterminate;
		} else {
			generateEvent(event_ptr_ref); 
		}
	}

	PION_ASSERT((event_ptr_ref.get() != NULL) || (rc != true));
	return rc;
}

boost::shared_ptr<Protocol> HTTPProtocol::clone(void) const
{
	HTTPProtocol* retval = new HTTPProtocol;
	retval->copyProtocol(*this);

	retval->m_cs_bytes_term_ref = m_cs_bytes_term_ref;
	retval->m_sc_bytes_term_ref = m_sc_bytes_term_ref;
	retval->m_bytes_term_ref = m_bytes_term_ref;
	retval->m_status_term_ref = m_status_term_ref;
	retval->m_comment_term_ref = m_comment_term_ref;
	retval->m_method_term_ref = m_method_term_ref;
	retval->m_uri_term_ref = m_uri_term_ref;
	retval->m_uri_stem_term_ref = m_uri_stem_term_ref;
	retval->m_uri_query_term_ref = m_uri_query_term_ref;
	retval->m_request_term_ref = m_request_term_ref;
	retval->m_host_term_ref = m_host_term_ref;
	retval->m_referer_term_ref = m_referer_term_ref;
	retval->m_useragent_term_ref = m_useragent_term_ref;
	retval->m_cs_content_term_ref = m_cs_content_term_ref;
	retval->m_sc_content_term_ref = m_sc_content_term_ref;
	retval->m_cached_term_ref = m_cached_term_ref;

	retval->m_request_content_rule = m_request_content_rule;
	retval->m_response_content_rule = m_response_content_rule;
	
	return ProtocolPtr(retval);
}

void HTTPProtocol::generateEvent(EventPtr& event_ptr_ref)
{
	const Event::EventType event_type(getEventType());

	// create a new event via EventFactory
	m_event_factory.create(event_ptr_ref, event_type);

	// populate some basic fields
	(*event_ptr_ref).setUInt(m_cs_bytes_term_ref, m_request_parser.getTotalBytesRead());
	(*event_ptr_ref).setUInt(m_sc_bytes_term_ref, m_response_parser.getTotalBytesRead());
	(*event_ptr_ref).setUInt(m_bytes_term_ref, m_request_parser.getTotalBytesRead()
							 + m_response_parser.getTotalBytesRead());
	(*event_ptr_ref).setUInt(m_status_term_ref, m_response.getStatusCode());
	(*event_ptr_ref).setString(m_comment_term_ref, m_response.getStatusMessage());
	(*event_ptr_ref).setString(m_method_term_ref, m_request.getMethod());

	// construct uri string
	std::string uri_str(m_request.getResource());
	if (! m_request.getQueryString().empty()) {
		uri_str += '?';
		uri_str += m_request.getQueryString();
	}
	(*event_ptr_ref).setString(m_uri_term_ref, uri_str);

	// populate some more fields...
	(*event_ptr_ref).setString(m_uri_stem_term_ref, m_request.getResource());
	(*event_ptr_ref).setString(m_uri_query_term_ref, m_request.getQueryString());
	(*event_ptr_ref).setString(m_request_term_ref, m_request.getFirstLine());
	(*event_ptr_ref).setString(m_host_term_ref, m_request.getHeader(HTTPTypes::HEADER_HOST));
	(*event_ptr_ref).setString(m_referer_term_ref, m_request.getHeader(HTTPTypes::HEADER_REFERER));
	(*event_ptr_ref).setString(m_useragent_term_ref, m_request.getHeader(HTTPTypes::HEADER_USER_AGENT));
	(*event_ptr_ref).setUInt(m_cached_term_ref,
							 m_response.getStatusCode() == HTTPTypes::RESPONSE_CODE_NOT_MODIFIED
							 ? 1 : 0);
	
	// check if request content should be saved
	checkContentExtraction(event_ptr_ref, m_request_content_rule, m_request, m_cs_content_term_ref);
	
	// check if response content should be saved
	checkContentExtraction(event_ptr_ref, m_response_content_rule, m_response, m_sc_content_term_ref);
}

void HTTPProtocol::parseExtractionRule(ExtractionRule& rule,
									   const std::string& element_name,
									   const xmlNodePtr config_ptr)
{
	// parse extraction configuration parameters
	xmlNodePtr content_node = ConfigManager::findConfigNodeByName(element_name, config_ptr);
	if (content_node != NULL) {
		// get ContentType regex
		std::string type_regex_str;
		if (! ConfigManager::getConfigOption(CONTENT_TYPE_ELEMENT_NAME, type_regex_str,
											 content_node->children))
			type_regex_str = ".*";
		rule.m_type_regex = type_regex_str;
		
		// get MaxSize parameter
		std::string max_size_str;
		if (ConfigManager::getConfigOption(MAX_SIZE_ELEMENT_NAME, max_size_str,
										   content_node->children))
			rule.m_max_size = boost::lexical_cast<boost::uint32_t>(max_size_str);
		else
			rule.m_max_size = boost::uint32_t(-1);
	} else {
		// do not extract content
		rule.m_max_size = 0;
	}
}	

void HTTPProtocol::setConfig(const Vocabulary& v, const xmlNodePtr config_ptr)
{
	Protocol::setConfig(v, config_ptr);
	
	// get RequestContent and ResponseContent

	parseExtractionRule(m_request_content_rule, REQUEST_CONTENT_ELEMENT_NAME, config_ptr);
	parseExtractionRule(m_response_content_rule, RESPONSE_CONTENT_ELEMENT_NAME, config_ptr);

	// initialize references to known Terms:

	m_cs_bytes_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CS_BYTES);
	if (m_cs_bytes_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CS_BYTES);

	m_sc_bytes_term_ref = v.findTerm(VOCAB_CLICKSTREAM_SC_BYTES);
	if (m_sc_bytes_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_SC_BYTES);

	m_bytes_term_ref = v.findTerm(VOCAB_CLICKSTREAM_BYTES);
	if (m_bytes_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_BYTES);

	m_status_term_ref = v.findTerm(VOCAB_CLICKSTREAM_STATUS);
	if (m_status_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_STATUS);

	m_comment_term_ref = v.findTerm(VOCAB_CLICKSTREAM_COMMENT);
	if (m_comment_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_COMMENT);

	m_method_term_ref = v.findTerm(VOCAB_CLICKSTREAM_METHOD);
	if (m_method_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_METHOD);

	m_uri_term_ref = v.findTerm(VOCAB_CLICKSTREAM_URI);
	if (m_uri_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_URI);

	m_uri_stem_term_ref = v.findTerm(VOCAB_CLICKSTREAM_URI_STEM);
	if (m_uri_stem_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_URI_STEM);

	m_uri_query_term_ref = v.findTerm(VOCAB_CLICKSTREAM_URI_QUERY);
	if (m_uri_query_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_URI_QUERY);

	m_request_term_ref = v.findTerm(VOCAB_CLICKSTREAM_REQUEST);
	if (m_request_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_REQUEST);

	m_host_term_ref = v.findTerm(VOCAB_CLICKSTREAM_HOST);
	if (m_host_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_HOST);

	m_referer_term_ref = v.findTerm(VOCAB_CLICKSTREAM_REFERER);
	if (m_referer_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_REFERER);

	m_useragent_term_ref = v.findTerm(VOCAB_CLICKSTREAM_USERAGENT);
	if (m_useragent_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_USERAGENT);

	m_cs_content_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CS_CONTENT);
	if (m_cs_content_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CS_CONTENT);
	
	m_sc_content_term_ref = v.findTerm(VOCAB_CLICKSTREAM_SC_CONTENT);
	if (m_sc_content_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_SC_CONTENT);
	
	m_cached_term_ref = v.findTerm(VOCAB_CLICKSTREAM_CACHED);
	if (m_cached_term_ref == Vocabulary::UNDEFINED_TERM_REF)
		throw UnknownTermException(VOCAB_CLICKSTREAM_CACHED);

	// TODO: initialize other terms here
}

}	// end namespace plugins
}	// end namespace pion



/// creates new HTTPProtocol objects
extern "C" PION_PLUGIN_API pion::platform::Protocol *pion_create_HTTPProtocol(void) {
	return new pion::plugins::HTTPProtocol();
}

/// destroys HTTPProtocol objects
extern "C" PION_PLUGIN_API void pion_destroy_HTTPProtocol(pion::plugins::HTTPProtocol *protocol_ptr) {
	delete protocol_ptr;
}