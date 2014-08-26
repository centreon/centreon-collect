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

#include <vector>
#include <sstream>
#include <QVariant>
#include <QSqlError>
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/notification/loaders/escalation_loader.hh"

using namespace com::centreon::broker::notification;

escalation_loader::escalation_loader() {}

void escalation_loader::load(QSqlDatabase* db, escalation_builder* output) {
  // If we don't have any db or output, don't do anything.
  if (!db || !output)
    return;

  QSqlQuery query(*db);

  // Performance improvement, as we never go back.
  query.setForwardOnly(true);

  if (!query.exec("SELECT esc_id, esc_name, esc_alias, first_notification, last_notification, notification_interval, escalation_period, escalation_options1, escalation_options2 from escalation"))
    throw (exceptions::msg()
      << "Notification: cannot select escalation in loader: "
      << query.lastError().text());

  while (query.next()) {
    shared_ptr<escalation> esc(new escalation);
    unsigned int id = query.value(0).toUInt();

    esc->set_first_notification(query.value(3).toUInt());
    esc->set_last_notification(query.value(4).toUInt());
    esc->set_notification_interval(query.value(5).toUInt());
    esc->set_escalation_period(query.value(6).toString().toStdString());
    esc->parse_host_escalation_options(query.value(7).toString().toStdString());
    esc->parse_service_escalation_options(query.value(7).toString().toStdString());

    output->add_escalation(id, esc);
  }

  // Load relations
  _load_relations(query, *output);
}

void escalation_loader::_load_relations(QSqlQuery& query,
                                        escalation_builder& output) {
  if (!query.exec("SELECT escalation_esc_id, host_host_id FROM escalation_host_relation"))
    throw (exceptions::msg()
      << "Notification: cannot select escalation_host_relation in loader: "
      << query.lastError().text());
  while (query.next())
    output.connect_escalation_node_id(query.value(0).toUInt(),
                                      node_id(query.value(1).toUInt()));

  if (!query.exec("SELECT escalation_esc_id, host_host_id, service_service_id FROM escalation_service_relation"))
    throw (exceptions::msg()
      << "Notification: cannot select escalation_host_relation in loader: "
      << query.lastError().text());
  while (query.next())
    output.connect_escalation_node_id(query.value(0).toUInt(),
                                      node_id(query.value(1).toUInt(),
                                              query.value(2).toUInt()));

  _load_relation(query,
                 output,
                 "contactgroup_cg_id",
                 "escalation_contactgroup_relation",
                 &escalation_builder::connect_escalation_contactgroup);
  _load_relation(query,
                 output,
                 "hostgroup_hg_id",
                 "escalation_hostgroup_relation",
                 &escalation_builder::connect_escalation_hostgroup);
  _load_relation(query,
                 output,
                 "servicegroup_sg_id",
                 "escalation_servicegroup_relation",
                 &escalation_builder::connect_escalation_servicegroup);
}

void escalation_loader::_load_relation(QSqlQuery& query,
                                       escalation_builder& output,
                                       std::string const& relation_id_name,
                                       std::string const& table,
                                       void (escalation_builder::*register_method)(unsigned int, unsigned int)) {
  std::stringstream ss;
  ss << "SELECT escalation_esc_id, " << relation_id_name << " FROM " << table;
  if (!query.exec(ss.str().c_str()))
    throw (exceptions::msg()
      << "Notification: cannot select " <<  table << " in loader: "
      << query.lastError().text());

  while (query.next()) {
    unsigned int id = query.value(0).toUInt();
    unsigned int associated_id = query.value(1).toUInt();

    (output.*register_method)(id, associated_id);
  }
}
