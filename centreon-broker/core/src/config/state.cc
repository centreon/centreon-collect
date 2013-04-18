/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/broker/config/state.hh"
#include "com/centreon/broker/logging/file.hh"

using namespace com::centreon::broker::config;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
state::state() {
  clear();
}

/**
 *  Copy constructor.
 *
 *  @param[in] s Object to copy.
 */
state::state(state const& s) {
  _internal_copy(s);
}

/**
 *  Destructor.
 */
state::~state() {}

/**
 *  Assignment operator.
 *
 *  @param[in] s Object to copy.
 *
 *  @return This object.
 */
state& state::operator=(state const& s) {
  if (this != &s)
    _internal_copy(s);
  return (*this);
}

/**
 *  Reset state to default values.
 */
void state::clear() {
  _event_queue_max_size = 0;
  _flush_logs = true;
  _inputs.clear();
  _instance_id = 0;
  _instance_name.clear();
  _log_thread_id = false;
  _log_timestamp
    = com::centreon::broker::logging::file::with_timestamp();
  _loggers.clear();
  _module_dir.clear();
  _module_list.clear();
  _outputs.clear();
  _params.clear();
  _temporary = endpoint();
  return ;
}

/**
 *  Set the maximum limit size of the event queue.
 *
 *  @param[in] val Size limit.
 */
void state::event_queue_max_size(unsigned int val) throw () {
  _event_queue_max_size = val;
}

/**
 *  Get the maximum limit size of the event queue.
 *
 *  @return The size limit.
 */
unsigned int state::event_queue_max_size() const throw () {
  return (_event_queue_max_size);
}

/**
 *  Set if logs must be flushed.
 *
 *  @param[in] flush true to automatically flush log files.
 */
void state::flush_logs(bool flush) throw () {
  _flush_logs = flush;
  return ;
}

/**
 *  Check if logs must be flushed.
 *
 *  @return true if logs must be automatically flushed.
 */
bool state::flush_logs() const throw () {
  return (_flush_logs);
}

/**
 *  Get the list of inputs.
 *
 *  @return Input list.
 */
QList<endpoint>& state::inputs() throw () {
  return (_inputs);
}

/**
 *  Get the list of inputs.
 *
 *  @return Input list.
 */
QList<endpoint> const& state::inputs() const throw () {
  return (_inputs);
}

/**
 *  Set the instance id.
 *
 *  @param[in] id Instance id.
 */
void state::instance_id(unsigned int id) throw () {
  _instance_id = id;
}

/**
 *  Get the instance id.
 *
 *  @return Instance id.
 */
unsigned int state::instance_id() const throw () {
  return (_instance_id);
}

/**
 *  Set the instance name.
 *
 *  @param[in] name Instance name.
 */
void state::instance_name(QString const& name) throw () {
  _instance_name = name;
}

/**
 *  Get the instance name.
 *
 *  @return Instance name.
 */
QString const& state::instance_name() const throw () {
  return (_instance_name);
}

/**
 *  Get the logger list.
 *
 *  @return Logger list.
 */
QList<logger>& state::loggers() throw () {
  return (_loggers);
}

/**
 *  Set whether or not to log thread IDs.
 *
 *  @param[in] log_id true to log thread IDs.
 */
void state::log_thread_id(bool log_id) throw () {
  _log_thread_id = log_id;
  return ;
}

/**
 *  Get whether or not to log thread IDs.
 *
 *  @return true if thread IDs must be logged.
 */
bool state::log_thread_id() const throw () {
  return (_log_thread_id);
}

/**
 *  Set whether or not timestamp logging should be enabled.
 *
 *  @param[in] log_time true to log timestamp.
 */
void state::log_timestamp(bool log_time) throw () {
  _log_timestamp = log_time;
  return ;
}

/**
 *  Get whether or not to log timestamp.
 *
 *  @return true if timestamp must be logged.
 */
bool state::log_timestamp() const throw () {
  return (_log_timestamp);
}

/**
 *  Get the logger list.
 *
 *  @return Logger list.
 */
QList<logger> const& state::loggers() const throw () {
  return (_loggers);
}

/**
 *  Get the module directory.
 *
 *  @return Module directory.
 */
QString const& state::module_directory() const throw () {
  return (_module_dir);
}

/**
 *  Set the module directory.
 *
 *  @param[in] dir Module directory.
 */
void state::module_directory(QString const& dir) {
  _module_dir = dir;
  return ;
}

/**
 *  Get the module list.
 *
 *  @return Modifiable module list.
 */
QList<QString>& state::module_list() throw () {
  return (_module_list);
}

/**
 *  Get the module list.
 *
 *  @return Const module list.
 */
QList<QString> const& state::module_list() const throw () {
  return (_module_list);
}

/**
 *  Get the list of outputs.
 *
 *  @return Output list.
 */
QList<endpoint>& state::outputs() throw () {
  return (_outputs);
}

/**
 *  Get the list of outputs.
 *
 *  @return Output list.
 */
QList<endpoint> const& state::outputs() const throw () {
  return (_outputs);
}

/**
 *  Get the additional parameters.
 *
 *  @return Additional parameters list.
 */
QMap<QString, QString>& state::params() throw () {
  return (_params);
}

/**
 *  Get the additional parameters.
 *
 *  @return Additional parameters list.
 */
QMap<QString, QString> const& state::params() const throw () {
  return (_params);
}

/**
 *  Get temporary.
 *
 *  @return Temporary object.
 */
endpoint& state::temporary() throw () {
  return (_temporary);
}

/**
 *  Get temporary.
 *
 *  @return Temporary object.
 */
endpoint const& state::temporary() const throw () {
  return (_temporary);
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] s Object to copy.
 */
void state::_internal_copy(state const& s) {
  _event_queue_max_size = s._event_queue_max_size;
  _inputs = s._inputs;
  _instance_id = s._instance_id;
  _instance_name = s._instance_name;
  _log_thread_id = s._log_thread_id;
  _log_timestamp = s._log_timestamp;
  _loggers = s._loggers;
  _module_dir = s._module_dir;
  _module_list = s._module_list;
  _outputs = s._outputs;
  _params = s._params;
  _temporary = s._temporary;
  return ;
}
