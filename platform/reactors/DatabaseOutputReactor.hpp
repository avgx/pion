// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2009 Atomic Labs, Inc.  (http://www.atomiclabs.com)
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

#ifndef __PION_DATABASEOUTPUTREACTOR_HEADER__
#define __PION_DATABASEOUTPUTREACTOR_HEADER__

#include <pion/PionConfig.hpp>
#include <pion/PionLogger.hpp>
#include <pion/platform/Event.hpp>
#include <pion/platform/Reactor.hpp>
#include <pion/platform/DatabaseInserter.hpp>


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


///
/// DatabaseOutputReactor: inserts Events into database transaction tables
///
class DatabaseOutputReactor :
	public pion::platform::Reactor
{
public:

	/// constructs a new DatabaseOutputReactor object
	DatabaseOutputReactor(void)
		: pion::platform::Reactor(TYPE_STORAGE),
		m_logger(PION_GET_LOGGER("pion.DatabaseOutputReactor"))
	{}

	/// virtual destructor: this class is meant to be extended
	virtual ~DatabaseOutputReactor() { stop(); }

	/**
	 * sets configuration parameters for this Reactor
	 *
	 * @param v the Vocabulary that this Reactor will use to describe Terms
	 * @param config_ptr pointer to a list of XML nodes containing Reactor
	 *                   configuration parameters
	 */
	virtual void setConfig(const pion::platform::Vocabulary& v, const xmlNodePtr config_ptr);

	/**
	 * this updates the Vocabulary information used by this Reactor; it should
	 * be called whenever the global Vocabulary is updated
	 *
	 * @param v the Vocabulary that this Reactor will use to describe Terms
	 */
	virtual void updateVocabulary(const pion::platform::Vocabulary& v);

	/**
	 * this updates the Databases that are used by this Reactor; it should
	 * be called whenever any Database's configuration is updated
	 */
	virtual void updateDatabases(void);

	/**
	 * processes an Event by writing it to a database and delivering it to the output connections
	 *
	 * @param e pointer to the Event to process
	 */
	virtual void process(const pion::platform::EventPtr& e);

	/**
	 * handle an HTTP query (from QueryService)
	 *
	 * @param out the ostream to write the statistics info into
	 * @param branches URI stem path branches for the HTTP request
	 * @param qp query parameters or pairs passed in the HTTP request
	 *
	 * @return std::string of XML response
	 */
	virtual void query(std::ostream& out, const QueryBranches& branches,
		const QueryParams& qp);

	/// called by the ReactorEngine to start Event processing
	virtual void start(void);

	/// called by the ReactorEngine to stop Event processing
	virtual void stop(void);

	/// sets the logger to be used
	inline void setLogger(PionLogger log_ptr) { 
		m_logger = log_ptr;
		m_inserter->setLogger(log_ptr);
	}

	/// returns the logger currently in use
	inline PionLogger getLogger(void) { return m_logger; }


private:

	/// name of the database element for Pion XML config files
	static const std::string				DATABASE_ELEMENT_NAME;

	/// name of the table element for Pion XML config files
	static const std::string				TABLE_ELEMENT_NAME;

	/// name of the field element for Pion XML config files
	static const std::string				FIELD_ELEMENT_NAME;

	/// name of the events queued element for Pion XML statistics
	static const std::string				EVENTS_QUEUED_ELEMENT_NAME;

	/// name of the KeyCacheSize element for Pion XML statistics
	static const std::string				KEY_CACHE_SIZE_ELEMENT_NAME;

	/// primary logging interface used by this class
	PionLogger								m_logger;

	typedef boost::shared_ptr<pion::platform::DatabaseInserter>		DatabaseInserterPtr;

	/// class that manages insertion of events into the database
	DatabaseInserterPtr						m_inserter;
};


}	// end namespace plugins
}	// end namespace pion

#endif
