/*
** Copyright 2011-2014 Merethis
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

#ifndef CCB_NOTIFICATION_BUILDERS_NOTIFICATION_RULE_BY_ID_BUILDER_HH
#  define CCB_NOTIFICATION_BUILDERS_NOTIFICATION_RULE_BY_ID_BUILDER_HH

#  include "com/centreon/broker/namespace.hh"
#  include "com/centreon/broker/notification/objects/node_id.hh"
#  include "com/centreon/broker/notification/objects/notification_rule.hh"
#  include "com/centreon/broker/notification/builders/notification_rule_builder.hh"

CCB_BEGIN()

namespace       notification {
  /**
   *  @class notification_rule_by_id_builder notification_rule_by_id_builder.hh "com/centreon/broker/notification/builders/notification_rule_by_id_builder.hh"
   *  @brief Notification rule by id builder.
   */
  class         notification_rule_by_id_builder
                  : public notification_rule_builder {
  public:
    notification_rule_by_id_builder(
      QHash<unsigned int, objects::notification_rule::ptr>& table);

    void        add_rule(
                  unsigned int id,
                  objects::notification_rule::ptr con);

  private:
    QHash<unsigned int, objects::notification_rule::ptr>&
                  _table;
  };

}

CCB_END()

#endif // !CCB_NOTIFICATION_BUILDERS_NOTIFICATION_RULE_BY_ID_BUILDER_HH
