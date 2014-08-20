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

#include <utility>
#include <vector>
#include <QVariant>
#include <QSqlError>
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/notification/objects/dependency.hh"
#include "com/centreon/broker/notification/loaders/dependency_loader.hh"

using namespace com::centreon::broker::notification;

dependency_loader::dependency_loader() {}

void dependency_loader::load(QSqlDatabase* db, dependency_builder* output) {
  // If we don't have any db or output, don't do anything.
  if (!db || !output)
    return;

  // We do not know the type of a dependency until far latter.
  // Cache the options until we know enough to correctly parse them.
  std::vector<std::pair<int, std::string> > dep_execution_failure_options;
  std::vector<std::pair<int, std::string > > dep_notification_failure_options;
  QSqlQuery query(*db);

  if (!query.exec("SELECT dep_id, dep_name, dep_description, inherits_parent, execution_failure_criteria, notification_failure_criteria from dependency"))
    throw (exceptions::msg()
      << "Notification: cannot select dependency in loader: "
      << query.lastError().text());

  while (query.next()) {
    shared_ptr<dependency> dep(new dependency);
    unsigned int id = query.value(0).toUInt();
    dep->set_inherits_parent(query.value(3).toBool());
    dep_execution_failure_options.push_back(
          std::make_pair(id, query.value(4).toString().toStdString()));
    dep_notification_failure_options.push_back(
          std::make_pair(id, query.value(5).toString().toStdString()));

    output->add_dependency(id, dep);
  }

  // Get the type of the dependency and their relations.
  if (!query.exec("SELECT dependency_dep_id, host_host_id from dependency_hostParent_relation"))
    throw (exceptions::msg()
      << "Notification: cannot select dependency_hostParent_relation in loader: "
      << query.lastError().text());

  while (query.next()) {
    unsigned int id = query.value(0).toUInt();
    unsigned int host_id = query.value(1).toUInt();

    output->dependency_host_parent_relation(id, host_id);
  }

  if (!query.exec("SELECT dependency_dep_id, host_host_id from dependency_hostChild_relation"))
    throw (exceptions::msg()
      << "Notification: cannot select dependency_hostChild_relation in loader: "
      << query.lastError().text());

  while (query.next()) {
    unsigned int id = query.value(0).toUInt();
    unsigned int host_id = query.value(1).toUInt();

    output->dependency_host_child_relation(id, host_id);
  }

  if (!query.exec("SELECT dependency_dep_id, service_service_id from dependency_serviceParent_relation"))
    throw (exceptions::msg()
      << "Notification: cannot select dependency_serviceParent_relation in loader: "
      << query.lastError().text());

  while (query.next()) {
    unsigned int id = query.value(0).toUInt();
    unsigned int service_service_id = query.value(1).toUInt();

    output->dependency_service_parent_relation(id, service_service_id);
  }

  if (!query.exec("SELECT dependency_dep_id, service_service_id from dependency_serviceChild_relation"))
    throw (exceptions::msg()
      << "Notification: cannot select dependency_serviceChild_relation in loader: "
      << query.lastError().text());

  while (query.next()) {
    unsigned int id = query.value(0).toUInt();
    unsigned int service_service_id = query.value(1).toUInt();

    output->dependency_service_child_relation(id, service_service_id);
  }

  // We know the types of the dependencies: now we can parse the failure options.
  for (std::vector<std::pair<int, std::string> >::const_iterator
       it(dep_execution_failure_options.begin()),
       end(dep_execution_failure_options.end());
       it != end; ++it)
    output->set_execution_failure_options(it->first, it->second);
  for (std::vector<std::pair<int, std::string> >::const_iterator
       it(dep_notification_failure_options.begin()),
       end(dep_notification_failure_options.end());
       it != end; ++it)
    output->set_notification_failure_options(it->first, it->second);
}
