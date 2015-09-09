/*
** Copyright 2015 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/broker/io/event_info.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::io;

/**
 *  Constructor.
 *
 *  @param[in] name     Event name.
 *  @param[in] ops      Event operations (constructor, ...).
 *  @param[in] entries  Event property mapping.
 *  @param[in] table    SQL table of event.
 */
event_info::event_info(
              std::string const& name,
              event_info::event_operations const* ops,
              mapping::entry const* entries,
              std::string const& table)
  : _mapping(entries), _name(name), _ops(ops), _table(table) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
event_info::event_info(event_info const& other)
  : _mapping(other._mapping),
    _name(other._name),
    _ops(other._ops),
    _table(other._table) {}

/**
 *  Destructor.
 */
event_info::~event_info() {}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
event_info& event_info::operator=(event_info const& other) {
  _mapping = other._mapping;
  _name = other._name;
  _ops = other._ops;
  _table = other._table;
  return (*this);
}

/**
 *  Get mapping.
 *
 *  @return Event mapping.
 */
mapping::entry const* event_info::get_mapping() const {
  return (_mapping);
}

/**
 *  Get event name.
 *
 *  @return Event name.
 */
std::string const& event_info::get_name() const {
  return (_name);
}

/**
 *  Get event operations.
 *
 *  @return Event operations.
 */
event_info::event_operations const& event_info::get_operations() const {
  return (*_ops);
}

/**
 *  Get event table.
 *
 *  @return Event SQL table name.
 */
std::string const& event_info::get_table() const {
  return (_table);
}
