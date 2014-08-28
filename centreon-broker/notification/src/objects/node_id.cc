/*
** Copyright 2011-2013 Merethis
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

#include <QHash>
#include <QPair>
#include "com/centreon/broker/notification/objects/node_id.hh"

using namespace com::centreon::broker::notification;

node_id::node_id() :
  _host_id(0),
  _service_id(0) {}

node_id::node_id(node_id const& obj) :
  _host_id(obj._host_id),
  _service_id(obj._service_id) {}

node_id& node_id::operator=(node_id const& obj) {
  if (this != &obj) {
    _host_id = obj._host_id;
    _service_id = obj._service_id;
  }
  return (*this);
}

node_id::node_id(unsigned int host_id,
                 unsigned int service_id) :
  _host_id(host_id),
  _service_id(service_id) {}

bool node_id::operator<(node_id const& obj) const throw() {
  if (_host_id != obj._host_id)
    return (_host_id < obj._host_id);
  else
    return (_service_id < obj._service_id);
}

bool node_id::operator==(node_id const& obj) const throw() {
  return (_host_id == obj._host_id && _service_id == obj._service_id);
}

unsigned int node_id::get_host_id() const throw() {
  return (_host_id);
}

unsigned int node_id::get_service_id() const throw() {
  return (_service_id);
}

bool node_id::has_host() const throw() {
  return (_host_id != 0);
}

bool node_id::has_service() const throw() {
  return (_service_id != 0);
}

// QHash function for hash and sets.
uint ::com::centreon::broker::notification::qHash(node_id id) {
  return (qHash(qMakePair(id.get_host_id(),
                          id.get_service_id())));
}
