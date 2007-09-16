// -----------------------------------------------------------------
// libpion: a C++ framework for building lightweight HTTP interfaces
// -----------------------------------------------------------------
// Copyright (C) 2007 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file COPYING or copy at http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_PIONEXCEPTION_HEADER__
#define __PION_PIONEXCEPTION_HEADER__

#include <libpion/PionConfig.hpp>
#include <exception>
#include <string>


namespace pion {	// begin namespace pion

///
/// PionException: basic exception class that defines a what() function
///
class PionException :
	public std::exception
{
public:
	// virtual destructor does not throw
	virtual ~PionException() throw () {}

	// constructors used for constant messages
	PionException(const char *what_msg) : m_what_msg(what_msg) {}
	PionException(const std::string& what_msg) : m_what_msg(what_msg) {}
	
	// constructors used for messages with a parameter
	PionException(const char *description, const std::string& param)
		: m_what_msg(std::string(description) + param) {}
	PionException(std::string description, const std::string& param)
		: m_what_msg(description + param) {}

	/// returns a desciptive message for the exception
	virtual const char* what() const throw() {
		return m_what_msg.c_str();
	}
	
private:
	
	// message returned by what() function
	const std::string	m_what_msg;
};

}	// end namespace pion

#endif
