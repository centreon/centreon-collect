/*
**  Copyright 2009 MERETHIS
**  This file is part of CentreonBroker.
**
**  CentreonBroker is free software: you can redistribute it and/or modify it
**  under the terms of the GNU General Public License as published by the Free
**  Software Foundation, either version 2 of the License, or (at your option)
**  any later version.
**
**  CentreonBroker is distributed in the hope that it will be useful, but
**  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
**  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
**  for more details.
**
**  You should have received a copy of the GNU General Public License along
**  with CentreonBroker.  If not, see <http://www.gnu.org/licenses/>.
**
**  For more information : contact@centreon.com
*/

#include <algorithm>                  // for find
#include <assert.h>
#include <soci.h>
#ifdef USE_MYSQL
# include <soci-mysql.h>
#endif /* USE_MYSQL */
#ifdef USE_ORACLE
# include <soci-oracle.h>
#endif /* USE_ORACLE */
#ifdef USE_POSTGRESQL
# include <soci-postgresql.h>
#endif /* USE_POSTGRESQL */
#include <sstream>
#include <stdlib.h>                   // for abort
#include "events/events.h"
#include "exception.h"
#include "interface/db/destination.h"
#include "interface/db/internal.h"
#include "logging/logging.hh"
#include "mapping.h"
#include "nagios/broker.h"

using namespace Interface::DB;

/**************************************
*                                     *
*           Static Objects            *
*                                     *
**************************************/

// Processing table.
void (Destination::* Destination::processing_table[])(const Events::Event&) =
{
  NULL,                                      // UNKNOWN
  &Destination::ProcessAcknowledgement,      // ACKNOWLEDGEMENT
  &Destination::ProcessComment,              // COMMENT
  &Destination::ProcessCustomVariable,       // CUSTOMVARIABLE
  &Destination::ProcessCustomVariableStatus, // CUSTOMVARIABLESTATUS
  &Destination::ProcessDowntime,             // DOWNTIME
  &Destination::ProcessEventHandler,         // EVENTHANDLER
  &Destination::ProcessFlappingStatus,       // FLAPPINGSTATUS
  &Destination::ProcessHost,                 // HOST
  &Destination::ProcessHostCheck,            // HOSTCHECK
  &Destination::ProcessHostDependency,       // HOSTDEPENDENCY
  &Destination::ProcessHostGroup,            // HOSTGROUP
  &Destination::ProcessHostGroupMember,      // HOSTGROUPMEMBER
  &Destination::ProcessHostParent,           // HOSTPARENT
  &Destination::ProcessHostStatus,           // HOSTSTATUS
  &Destination::ProcessIssue,                // ISSUE
  &Destination::ProcessIssueParent,          // ISSUEPARENT
  &Destination::ProcessLog,                  // LOG
  &Destination::ProcessNotification,         // NOTIFICATION
  &Destination::ProcessProgram,              // PROGRAM
  &Destination::ProcessProgramStatus,        // PROGRAMSTATUS
  &Destination::ProcessService,              // SERVICE
  &Destination::ProcessServiceCheck,         // SERVICECHECK
  &Destination::ProcessServiceDependency,    // SERVICEDEPENDENCY
  &Destination::ProcessServiceGroup,         // SERVICEGROUP
  &Destination::ProcessServiceGroupMember,   // SERVICEGROUPMEMBER
  &Destination::ProcessServiceStatus,        // SERVICESTATUS
  &Destination::ProcessState                 // STATE
};

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  \brief Destination copy constructor.
 *
 *  As Destination is not copiable, the copy constructor is declared private.
 *  Any attempt to use it will result in a call to abort().
 *
 *  \param[in] destination Unused.
 */
Destination::Destination(const Destination& destination)
  : Interface::Destination()
{
  (void)destination;
  assert(false);
  abort();
}

/**
 *  \brief Overload of the assignment operator.
 *
 *  As Destination is not copiable, the assignment operator is declared
 *  private. Any attempt to use it will result in a call to abort().
 *
 *  \param[in] destination Unused.
 *
 *  \return *this
 */
Destination& Destination::operator=(const Destination& destination)
{
  (void)destination;
  assert(false);
  abort();
  return (*this);
}

/**
 *  Clean tables with data associated to the instance.
 */
void Destination::CleanTables(int instance_id) {
  // Disable hosts.
  *this->conn_ << "UPDATE " << MappedType<Events::Host>::table
               << " SET enabled=0 "
               << " WHERE instance_id=" << instance_id;

  return ;
}

/**
 *  Insert an object in the DB using its mapping.
 */
template <typename T>
void Destination::Insert(const T& t)
{
  std::string query;

  // Build query string.
  query = "INSERT INTO ";
  query.append(MappedType<T>::table);
  query.append(" (");
  for (typename std::list<std::pair<std::string,
                                    GetterSetter<T> > >::const_iterator
         it = DBMappedType<T>::list.begin(),
         end = DBMappedType<T>::list.end();
       it != end;
       ++it)
    {
      query.append(it->first);
      query.append(", ");
    }
  query.resize(query.size() - 2);
  query.append(") VALUES(");
  for (typename std::list<std::pair<std::string,
                                    GetterSetter<T> > >::const_iterator
         it = DBMappedType<T>::list.begin(),
         end = DBMappedType<T>::list.end();
       it != end;
       ++it)
    {
      query.append(":");
      query.append(it->first);
      query.append(", ");
    }
  query.resize(query.size() - 2);
  query.append(")");
  LOGDEBUG(query.c_str());

  // Execute query.
  *this->conn_ << query, soci::use(t);

  return ;
}

/**
 *  Prepare an insert statement for later execution.
 */
template <typename T>
void Destination::_prepare_insert(std::auto_ptr<soci::statement>& st, T& t)
{
  std::string query;

  // Build query string.
  query = "INSERT INTO ";
  query.append(MappedType<T>::table);
  query.append(" (");
  for (typename std::list<std::pair<std::string,
                                    GetterSetter<T> > >::const_iterator
         it = DBMappedType<T>::list.begin(),
         end = DBMappedType<T>::list.end();
       it != end;
       ++it)
    {
      query.append(it->first);
      query.append(", ");
    }
  query.resize(query.size() - 2);
  query.append(") VALUES(");
  for (typename std::list<std::pair<std::string,
                                    GetterSetter<T> > >::const_iterator
         it = DBMappedType<T>::list.begin(),
         end = DBMappedType<T>::list.end();
       it != end;
       ++it)
    {
      query.append(":");
      query.append(it->first);
      query.append(", ");
    }
  query.resize(query.size() - 2);
  query.append(")");
  LOGDEBUG(query.c_str());

  // Prepare statement.
  st.reset(new soci::statement((this->conn_->prepare << query, soci::use(t))));

  return ;
}

/**
 *  Prepare an update statement for later execution.
 */
template <typename T>
void Destination::PrepareUpdate(std::auto_ptr<soci::statement>& st,
                                T& t,
                                const std::vector<std::string>& id)
{
  std::string query;

  // Build query string.
  query = "UPDATE ";
  query.append(MappedType<T>::table);
  query.append(" SET ");
  for (typename std::list<std::pair<std::string,
                                    GetterSetter<T> > >::const_iterator
         it = DBMappedType<T>::list.begin(),
         end = DBMappedType<T>::list.end();
       it != end;
       ++it)
    if (std::find(id.begin(), id.end(), it->first) == id.end())
      {
        query.append(it->first);
        query.append("=:");
        query.append(it->first);
        query.append(", ");
      }
  query.resize(query.size() - 2);
  query.append(" WHERE ");
  for (std::vector<std::string>::const_iterator
         it = id.begin(),
         end = id.end();
       it != end;
       ++it)
    {
      query.append(*it);
      query.append("=:");
      query.append(*it);
      query.append(" AND ");
    }
  query.resize(query.size() - 5);
  LOGDEBUG(query.c_str());

  // Prepare statement.
  st.reset(new soci::statement((this->conn_->prepare << query, soci::use(t))));

  return ;
}

/**
 *  Process an Acknowledgement event.
 */
void Destination::ProcessAcknowledgement(const Events::Event& event)
{
  const Events::Acknowledgement& ack(
    *static_cast<const Events::Acknowledgement*>(&event));

  LOGDEBUG("Processing Acknowledgement event ...");
  try
    {
      this->Insert(ack);
    }
  catch (const soci::soci_error& se)
    {
      this->acknowledgement_ = ack;
      this->acknowledgement_stmt_->execute(true);
    }
  return ;
}

/**
 *  Process a Comment event.
 */
void Destination::ProcessComment(Events::Event const& event) {
  Events::comment const& c(*static_cast<const Events::comment*>(&event));

  LOGDEBUG("Processing comment event ...");
  try {
      this->Insert(c);
    }
  catch (soci::soci_error const& se) {
      _comment = c;
      _comment_stmt->execute(true);
    }

  return ;
}

/**
 *  Process a custom variable event.
 */
void Destination::ProcessCustomVariable(Events::Event const& event) {
  Events::custom_variable const& cv(
    *static_cast<Events::custom_variable const*>(&event));

  LOGDEBUG("Processing custom variable event ...");
  this->Insert(cv);
  return ;
}

/**
 *  Process a custom variable status event.
 */
void Destination::ProcessCustomVariableStatus(Events::Event const& event) {
  Events::custom_variable_status const& cvs(
    *static_cast<Events::custom_variable_status const*>(&event));

  LOGDEBUG("Processing custom variable status event ...");
  _custom_variable_status = cvs;
  _custom_variable_status_stmt->execute(true);
  return ;
}

/**
 *  Process a Downtime event.
 */
void Destination::ProcessDowntime(const Events::Event& event)
{
  const Events::Downtime& downtime(
    *static_cast<const Events::Downtime*>(&event));

  LOGDEBUG("Processing Downtime event ...");
  try
    {
      this->Insert(downtime);
    }
  catch (const soci::soci_error& se)
    {
      this->downtime_ = downtime;
      this->downtime_stmt_->execute(true);
    }
  return ;
}

/**
 *  Process an event handler event.
 */
void Destination::ProcessEventHandler(Events::Event const& event) {
  Events::event_handler const& event_handler(
    *static_cast<Events::event_handler const*>(&event));

  LOGDEBUG("Processing event_handler event ...");
  try {
    Insert(event_handler);
  }
  catch (soci::soci_error const& se) {
    _event_handler = event_handler;
    _event_handler_stmt->execute(true);
  }
  return ;
}

/**
 *  Process a flapping status event.
 */
void Destination::ProcessFlappingStatus(Events::Event const& event) {
  Events::flapping_status const& flapping_status(
    *static_cast<Events::flapping_status const*>(&event));

  LOGDEBUG("Processing flapping status event ...");
  try {
    Insert(flapping_status);
  }
  catch (soci::soci_error const& se) {
    _flapping_status = flapping_status;
    _flapping_status_stmt->execute(true);
  }
  return ;
}

/**
 *  Process an Host event.
 */
void Destination::ProcessHost(Events::Event const& event) {
  Events::Host const& host(*static_cast<Events::Host const*>(&event));

  LOGDEBUG("Processing host event ...");
  try {
    this->Insert(host);
  }
  catch (soci::soci_error const& se) {
    _host = host;
    _host_stmt->execute(true);
  }
  return ;
}

/**
 *  Process an HostCheck event.
 */
void Destination::ProcessHostCheck(const Events::Event& event)
{
  const Events::HostCheck& host_check(
    *static_cast<const Events::HostCheck*>(&event));

  LOGDEBUG("Processing HostCheck event ...");
  this->host_check_ = host_check;
  this->host_check_stmt_->execute(true);
  return ;
}

/**
 *  Process a HostDependency event.
 */
void Destination::ProcessHostDependency(const Events::Event& event)
{
  const Events::HostDependency& hd(
    *static_cast<const Events::HostDependency*>(&event));

  LOGDEBUG("Processing HostDependency event ...");
  this->Insert(hd);
  return ;
}

/**
 *  Process a HostGroup event.
 */
void Destination::ProcessHostGroup(const Events::Event& event)
{
  const Events::HostGroup& hg(*static_cast<const Events::HostGroup*>(&event));

  LOGDEBUG("Processing HostGroup event...");
  this->Insert(hg);
  return ;
}

/**
 *  Process a HostGroupMember event.
 */
void Destination::ProcessHostGroupMember(const Events::Event& event)
{
  const Events::HostGroupMember& hgm(
    *static_cast<const Events::HostGroupMember*>(&event));
  int hostgroup_id;

  LOGDEBUG("Processing HostGroupMember event ...");

  // Fetch host group ID.
  *this->conn_ << "SELECT hostgroup_id FROM "
               << MappedType<Events::HostGroup>::table
               << " WHERE instance_id=" << hgm.instance_id
               << " AND name=\"" << hgm.group << "\"",
    soci::into(hostgroup_id);

  // Execute query.
  *this->conn_ << "INSERT INTO "
               << MappedType<Events::HostGroupMember>::table
               << " (host_id, hostgroup_id) VALUES("
               << hgm.host_id << ", "
               << hostgroup_id << ")";

  return ;
}

/**
 *  Process a HostParent event.
 */
void Destination::ProcessHostParent(const Events::Event& event)
{
  const Events::HostParent& hp(
    *static_cast<const Events::HostParent*>(&event));

  LOGDEBUG("Processing HostParent event ...");
  this->Insert(hp);
  return ;
}

/**
 *  Process a HostStatus event.
 */
void Destination::ProcessHostStatus(const Events::Event& event)
{
  const Events::HostStatus& hs(
    *static_cast<const Events::HostStatus*>(&event));

  LOGDEBUG("Processing HostStatus event ...");
  this->host_status_ = hs;
  this->host_status_stmt_->execute(true);
  return ;
}

/**
 *  Process an Issue event.
 */
void Destination::ProcessIssue(const Events::Event& event)
{
  const Events::Issue& issue(*static_cast<const Events::Issue*>(&event));

  LOGDEBUG("Processing Issue event ...");
  try
    {
      this->Insert(issue);
    }
  catch (const soci::soci_error& se)
    {
      this->issue_ = issue;
      this->issue_stmt_->execute(true);
    }
  return ;
}

/**
 *  Process an IssueParent event.
 */
void Destination::ProcessIssueParent(const Events::Event& event)
{
  Events::IssueParent const& ip(
    *static_cast<Events::IssueParent const*>(&event));
  int child_id;
  int parent_id;

  LOGDEBUG("Processing IssueParent event ...");

  // Get child ID.
  {
    std::ostringstream query;
    query << "SELECT issue_id FROM "
          << MappedType<Events::Issue>::table << " WHERE host_id="
          << ip.child_host_id << " AND service_id="
          << ip.child_service_id << " AND start_time="
          << ip.child_start_time;
    logging::debug << logging::LOW << query.str().c_str();
    *this->conn_ << query.str(), soci::into(child_id);
    logging::debug << logging::LOW << "Child issue ID: " << child_id;
  }

  // Get parent ID.
  {
    std::ostringstream query;
    query << "SELECT issue_id FROM "
          << MappedType<Events::Issue>::table << " WHERE host_id="
          << ip.parent_host_id << " AND service_id="
          << ip.parent_service_id << " AND start_time="
          << ip.parent_start_time;
    logging::debug << logging::LOW << query.str().c_str();
    *this->conn_ << query.str(), soci::into(parent_id);
    logging::debug << logging::LOW << "Parent issue ID: " << parent_id;
  }

  if (ip.end_time)
    {
      std::ostringstream query;
      query << "UPDATE "
            << MappedType<Events::IssueParent>::table
            << " SET end_time="
            << ip.end_time << " WHERE child_id="
            << child_id << " AND parent_id="
            << parent_id << " AND start_time="
            << ip.start_time;
      logging::debug << logging::LOW << query.str().c_str();
      *this->conn_ << query.str();
    }
  else
    {
      std::ostringstream query;
      query << "INSERT INTO "
            << MappedType<Events::IssueParent>::table
            << " (child_id, parent_id, start_time) VALUES("
            << child_id << ", "
            << parent_id << ", "
            << ip.start_time << ")";
      logging::debug << logging::LOW << query.str().c_str();
      *this->conn_ << query.str();
    }

  return ;
}

/**
 *  Process a Log event.
 */
void Destination::ProcessLog(const Events::Event& event)
{
  const char* field;
  int issue;
  const Events::Log& log(*static_cast<const Events::Log*>(&event));
  std::string query;

  LOGDEBUG("Processing Log event ...");
  field = "issue_id";
  query = "INSERT INTO ";
  query.append(MappedType<Events::Log>::table);
  query.append("(");
  for (std::list<std::pair<std::string,
                           GetterSetter<Events::Log> > >::const_iterator
         it = DBMappedType<Events::Log>::list.begin(),
         end = DBMappedType<Events::Log>::list.end();
       it != end;
       ++it)
    {
      query.append(it->first);
      query.append(", ");
    }
  query.append(field);
  query.append(") VALUES(");
  for (std::list<std::pair<std::string,
                           GetterSetter<Events::Log> > >::const_iterator
         it = DBMappedType<Events::Log>::list.begin(),
         end = DBMappedType<Events::Log>::list.end();
       it != end;
       ++it)
    {
      query.append(":");
      query.append(it->first);
      query.append(", ");
    }
  query.append(":");
  query.append(field);
  query.append(")");
  LOGDEBUG(query.c_str());

  // Fetch issue ID (if any).
  if (log.issue_start_time)
    *this->conn_ << "SELECT issue_id FROM "
                 << MappedType<Events::Issue>::table
                 << " WHERE host_id=" << log.host_id
                 << " AND service_id=" << log.service_id
                 << " AND start_time=" << log.issue_start_time,
      soci::into(issue);
  else
    issue = 0;

  // Execute query.
  *this->conn_ << query, soci::use(log), soci::use(issue, field);

  return ;
}

/**
 *  Process a notification event.
 */
void Destination::ProcessNotification(Events::Event const& event) {
  Events::notification const& notification(*static_cast<Events::notification const*>(&event));

  LOGDEBUG("Processing notification event ...");
  try {
    Insert(notification);
  }
  catch (soci::soci_error const& se) {
    _notification = notification;
    _notification_stmt->execute(true);
  }
  return ;
}

/**
 *  Process a Program event.
 */
void Destination::ProcessProgram(Events::Event const& event) {
  Events::Program const& program(*static_cast<Events::Program const*>(&event));

  LOGDEBUG("Processing program event ...");
  this->CleanTables(program.instance_id);
  if (!program.program_end) {
    try {
      this->Insert(program);
    }
    catch (soci::soci_error const& se) {
      _program = program;
      _program_stmt->execute(true);
    }
  }
  else {
    _program = program;
    _program_stmt->execute(true);
  }
  return ;
}

/**
 *  Process a ProgramStatus event.
 */
void Destination::ProcessProgramStatus(const Events::Event& event)
{
  const Events::ProgramStatus& ps(
    *static_cast<const Events::ProgramStatus*>(&event));

  LOGDEBUG("Processing ProgramStatus event ...");
  this->program_status_ = ps;
  this->program_status_stmt_->execute(true);
  return ;
}

/**
 *  Process a Service event.
 */
void Destination::ProcessService(Events::Event const& event) {
  Events::Service const& service(
    *static_cast<Events::Service const*>(&event));

  LOGDEBUG("Processing Service event ...");
  try {
    _service = service;
    _service_insert_stmt->execute(true);
  }
  catch (soci::soci_error const& se) {
    _service = service;
    _service_stmt->execute(true);
  }
  return ;
}

/**
 *  Process a ServiceCheck event.
 */
void Destination::ProcessServiceCheck(const Events::Event& event)
{
  const Events::ServiceCheck& service_check(
    *static_cast<const Events::ServiceCheck*>(&event));

  LOGDEBUG("Processing ServiceCheck event ...");
  this->service_check_ = service_check;
  this->service_check_stmt_->execute(true);
  return ;
}

/**
 *  Process a ServiceDependency event.
 */
void Destination::ProcessServiceDependency(const Events::Event& event)
{
  const Events::ServiceDependency& sd(
    *static_cast<const Events::ServiceDependency*>(&event));

  LOGDEBUG("Processing ServiceDependency event ...");
  this->Insert(sd);
  return ;
}

/**
 *  Process a ServiceGroup event.
 */
void Destination::ProcessServiceGroup(const Events::Event& event)
{
  const Events::ServiceGroup& sg(
    *static_cast<const Events::ServiceGroup*>(&event));

  LOGDEBUG("Processing ServiceGroup event ...");
  this->Insert(sg);
  return ;
}

/**
 *  Process a ServiceGroupMember event.
 */
void Destination::ProcessServiceGroupMember(const Events::Event& event)
{
  int servicegroup_id;
  const Events::ServiceGroupMember& sgm(
    *static_cast<const Events::ServiceGroupMember*>(&event));

  LOGDEBUG("Processing ServiceGroupMember event ...");

  // Fetch service group ID.
  *this->conn_ << "SELECT servicegroup_id FROM "
               << MappedType<Events::ServiceGroup>::table
               << " WHERE instance_id=" << sgm.instance_id
               << " AND name=\"" << sgm.group << "\"",
    soci::into(servicegroup_id);

  // Execute query.
  *this->conn_ << "INSERT INTO "
               << MappedType<Events::ServiceGroupMember>::table
               << " (host_id, service_id, servicegroup_id) VALUES("
               << sgm.host_id << ", "
               << sgm.service_id << ", "
               << servicegroup_id << ")";

  return ;
}

/**
 *  Process a ServiceStatus event.
 */
void Destination::ProcessServiceStatus(const Events::Event& event)
{
  const Events::ServiceStatus& ss(
    *static_cast<const Events::ServiceStatus*>(&event));

  LOGDEBUG("Processing ServiceStatus event ...");
  this->service_status_ = ss;
  this->service_status_stmt_->execute(true);
  return ;
}

/**
 *  Process a state event.
 */
void Destination::ProcessState(Events::Event const& event) {
  Events::state const& s(
    *static_cast<Events::state const*>(&event));

  LOGDEBUG("Processing state event ...");
  if (s.end_time)
    {
      this->state_ = s;
      this->state_stmt_->execute(true);
    }
  else
    this->Insert(s);
  return ;
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  \brief Destination default constructor.
 *
 *  Initialize the Destination with default parameters.
 *
 *  \param[in] dbms Type of the database to use.
 */
Destination::Destination()
{
  assert((sizeof(processing_table) / sizeof(*processing_table))
         == Events::Event::EVENT_TYPES_NB);
}

/**
 *  \brief Destination destructor.
 *
 *  Release all previously allocated ressources.
 */
Destination::~Destination()
{
  this->Close();
}

/**
 *  Close the event destination.
 */
void Destination::Close()
{
  this->acknowledgement_stmt_.reset();
  this->_comment_stmt.reset();
  this->_custom_variable_status_stmt.reset();
  this->downtime_stmt_.reset();
  this->_event_handler_stmt.reset();
  this->_flapping_status_stmt.reset();
  this->_host_stmt.reset();
  this->host_check_stmt_.reset();
  this->host_status_stmt_.reset();
  this->issue_stmt_.reset();
  this->_notification_stmt.reset();
  this->_program_stmt.reset();
  this->program_status_stmt_.reset();
  this->_service_stmt.reset();
  this->_service_insert_stmt.reset();
  this->service_check_stmt_.reset();
  this->service_status_stmt_.reset();
  this->state_stmt_.reset();
  this->conn_.reset();
  return ;
}

/**
 *  \brief Process an event.
 *
 *  When an event is poped from the list, it will be processed by this method.
 *  We will determine the true event type and process it accordingly.
 *
 *  \param[in] event Event that should be stored in the database.
 */
void Destination::Event(Events::Event* event)
{
  try
    {
      (this->*this->processing_table[event->GetType()])(*event);
    }
  catch (...)
    {
      // Event self deregistration.
      event->RemoveReader();

      // Rethrow the exception
      throw ;
    }

  // Event self deregistration.
  event->RemoveReader();

  return ;
}

/**
 *  \brief Connect the database destination.
 *
 *  Connect to the specified database using the specified credentials.
 *
 *  \param[in] db_type Database type.
 *  \param[in] host    DB server.
 *  \param[in] db      Database
 *  \param[in] user    User name to use for authentication.
 *  \param[in] pass    Password to use for authentication.
 */
void Destination::Connect(Destination::DB db_type,
                          const std::string& db,
                          const std::string& host,
                          const std::string& user,
                          const std::string& pass)
{
  // Connect to DB.
  {
    std::stringstream ss;

    switch (db_type)
      {
#ifdef USE_MYSQL
       case MYSQL:
        ss << "dbname=" << db
           << " host=" << host
           << " user=" << user
           << " password=" << pass;
        this->conn_.reset(new soci::session(soci::mysql, ss.str()));
        break ;
#endif /* USE_MYSQL */

#ifdef USE_ORACLE
       case ORACLE:
        ss << "service=" << host
           << " user=" << user
           << " password=" << pass;
        this->conn_.reset(new soci::session(soci::oracle, ss.str()));
        break ;
#endif /* USE_ORACLE */

#ifdef USE_POSTGRESQL
       case POSTGRESQL:
        ss << "dbname=" << db
           << " host=" << host
           << " user=" << user
           << " password=" << pass;
        this->conn_.reset(new soci::session(soci::postgresql, ss.str()));
        break ;
#endif /* USE_POSTGRESQL */

      default:
        throw Exception(0, "Unsupported DBMS requested.");
      }
  }

  _prepare_insert<Events::Service>(_service_insert_stmt, _service);

  std::vector<std::string> id;

  id.clear();
  id.push_back("author_name");
  id.push_back("entry_time");
  id.push_back("host_name");
  id.push_back("instance_name");
  id.push_back("service_description");
  this->PrepareUpdate<Events::Acknowledgement>(
    this->acknowledgement_stmt_, this->acknowledgement_, id);

  id.clear();
  id.push_back("entry_time");
  id.push_back("instance_name");
  id.push_back("internal_id");
  this->PrepareUpdate<Events::comment>(
    _comment_stmt, _comment, id);

  id.clear();
  id.push_back("host_id");
  id.push_back("name");
  id.push_back("service_id");
  this->PrepareUpdate<Events::custom_variable_status>(
    _custom_variable_status_stmt, _custom_variable_status, id);

  id.clear();
  id.push_back("entry_time");
  id.push_back("instance_name");
  id.push_back("internal_id");
  this->PrepareUpdate<Events::Downtime>(
    this->downtime_stmt_, this->downtime_, id);

  id.clear();
  id.push_back("host_id");
  id.push_back("service_id");
  id.push_back("start_time");
  this->PrepareUpdate<Events::event_handler>(
    _event_handler_stmt, _event_handler, id);

  id.clear();
  id.push_back("host_id");
  id.push_back("service_id");
  id.push_back("event_time");
  this->PrepareUpdate<Events::flapping_status>(
    _flapping_status_stmt, _flapping_status, id);

  id.clear();
  id.push_back("host_id");
  this->PrepareUpdate<Events::Host>(
    _host_stmt, _host, id);

  id.clear();
  id.push_back("host_id");
  this->PrepareUpdate<Events::HostCheck>(
    this->host_check_stmt_, this->host_check_, id);

  id.clear();
  id.push_back("host_id");
  this->PrepareUpdate<Events::HostStatus>(
    this->host_status_stmt_, this->host_status_, id);

  id.clear();
  id.push_back("host_id");
  id.push_back("service_id");
  id.push_back("start_time");
  this->PrepareUpdate<Events::Issue>(
    this->issue_stmt_, this->issue_, id);

  id.clear();
  id.push_back("host_id");
  id.push_back("service_id");
  id.push_back("start_time");
  this->PrepareUpdate<Events::notification>(
    _notification_stmt, _notification, id);

  id.clear();
  id.push_back("host_id");
  id.push_back("service_id");
  id.push_back("start_time");
  this->PrepareUpdate<Events::state>(
    this->state_stmt_, this->state_, id);

  id.clear();
  id.push_back("instance_id");
  PrepareUpdate<Events::Program>(
    _program_stmt, _program, id);

  id.clear();
  id.push_back("instance_id");
  this->PrepareUpdate<Events::ProgramStatus>(
    this->program_status_stmt_, this->program_status_, id);

  id.clear();
  id.push_back("host_id");
  id.push_back("service_id");
  this->PrepareUpdate<Events::Service>(
    _service_stmt, _service, id);

  id.clear();
  id.push_back("host_id");
  id.push_back("service_id");
  this->PrepareUpdate<Events::ServiceCheck>(
    this->service_check_stmt_, this->service_check_, id);

  id.clear();
  id.push_back("host_id");
  id.push_back("service_id");
  this->PrepareUpdate<Events::ServiceStatus>(
    this->service_status_stmt_, this->service_status_, id);

  return ;
}
