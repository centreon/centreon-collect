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

#ifndef CCB_SQL_STREAM_HH
#  define CCB_SQL_STREAM_HH

#  include <memory>
#  include <QHash>
#  include <QMap>
#  include <QPair>
#  include <QSqlDatabase>
#  include <QSqlQuery>
#  include <QString>
#  include <QVector>
#  include "com/centreon/broker/io/stream.hh"
#  include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace        sql {
  /**
   *  @class stream stream.hh "com/centreon/broker/sql/stream.hh"
   *  @brief SQL stream.
   *
   *  Stream events into SQL database.
   */
  class          stream : public io::stream {
  public:
                 stream(
                   QString const& type,
                   QString const& host,
                   unsigned short port,
                   QString const& user,
                   QString const& password,
                   QString const& db,
                   unsigned int queries_per_transaction,
                   bool check_replication,
                   bool with_state_events);
                 stream(stream const& s);
                 ~stream();
    static void  initialize();
    void         process(bool in = false, bool out = false);
    void         read(misc::shared_ptr<io::data>& d);
    void         write(misc::shared_ptr<io::data> const& d);

  private:
    stream&      operator=(stream const& s);
    void         _clean_tables(int instance_id);
    void         _execute(QString const& query);
    void         _execute(QSqlQuery& query);
    template     <typename T>
    bool         _insert(T const& t);
    void         _prepare();
    template     <typename T>
    bool         _prepare_insert(std::auto_ptr<QSqlQuery>& st);
    template     <typename T>
    bool         _prepare_update(
                   std::auto_ptr<QSqlQuery>& st,
                   QVector<QPair<QString, bool> > const& id);
    void         _process_acknowledgement(io::data const& e);
    void         _process_comment(io::data const& e);
    void         _process_custom_variable(io::data const& e);
    void         _process_custom_variable_status(io::data const& e);
    void         _process_downtime(io::data const& e);
    void         _process_engine(io::data const& e);
    void         _process_event_handler(io::data const& e);
    void         _process_flapping_status(io::data const& e);
    void         _process_host(io::data const& e);
    void         _process_host_check(io::data const& e);
    void         _process_host_dependency(io::data const& e);
    void         _process_host_group(io::data const& e);
    void         _process_host_group_member(io::data const& e);
    void         _process_host_parent(io::data const& e);
    void         _process_host_state(io::data const& e);
    void         _process_host_status(io::data const& e);
    void         _process_instance(io::data const& e);
    void         _process_instance_status(io::data const& e);
    void         _process_issue(io::data const& e);
    void         _process_issue_parent(io::data const& e);
    void         _process_log(io::data const& e);
    void         _process_module(io::data const& e);
    void         _process_notification(io::data const& e);
    void         _process_service(io::data const& e);
    void         _process_service_check(io::data const& e);
    void         _process_service_dependency(io::data const& e);
    void         _process_service_group(io::data const& e);
    void         _process_service_group_member(io::data const& e);
    void         _process_service_state(io::data const& e);
    void         _process_service_status(io::data const& e);
    void         _unprepare();
    template     <typename T>
    void         _update_on_none_insert(
                   QSqlQuery& ins,
                   QSqlQuery& up,
                   T& t);

    static QHash<QString, void (stream::*)(io::data const&)>
                                _processing_table;
    std::auto_ptr<QSqlQuery>    _acknowledgement_insert;
    std::auto_ptr<QSqlQuery>    _acknowledgement_update;
    std::auto_ptr<QSqlQuery>    _comment_insert;
    std::auto_ptr<QSqlQuery>    _comment_update;
    std::auto_ptr<QSqlQuery>    _custom_variable_insert;
    std::auto_ptr<QSqlQuery>    _custom_variable_update;
    std::auto_ptr<QSqlQuery>    _custom_variable_status_update;
    std::auto_ptr<QSqlQuery>    _downtime_insert;
    std::auto_ptr<QSqlQuery>    _downtime_update;
    std::auto_ptr<QSqlQuery>    _event_handler_insert;
    std::auto_ptr<QSqlQuery>    _event_handler_update;
    std::auto_ptr<QSqlQuery>    _flapping_status_insert;
    std::auto_ptr<QSqlQuery>    _flapping_status_update;
    std::auto_ptr<QSqlQuery>    _host_insert;
    std::auto_ptr<QSqlQuery>    _host_update;
    std::auto_ptr<QSqlQuery>    _host_check_update;
    std::auto_ptr<QSqlQuery>    _host_dependency_insert;
    std::auto_ptr<QSqlQuery>    _host_dependency_update;
    std::auto_ptr<QSqlQuery>    _host_group_insert;
    std::auto_ptr<QSqlQuery>    _host_group_update;
    std::auto_ptr<QSqlQuery>    _host_state_insert;
    std::auto_ptr<QSqlQuery>    _host_state_update;
    std::auto_ptr<QSqlQuery>    _host_status_update;
    std::auto_ptr<QSqlQuery>    _instance_insert;
    std::auto_ptr<QSqlQuery>    _instance_update;
    std::auto_ptr<QSqlQuery>    _instance_status_update;
    std::auto_ptr<QSqlQuery>    _issue_insert;
    std::auto_ptr<QSqlQuery>    _issue_update;
    std::auto_ptr<QSqlQuery>    _issue_parent_insert;
    std::auto_ptr<QSqlQuery>    _issue_parent_update;
    std::auto_ptr<QSqlQuery>    _notification_insert;
    std::auto_ptr<QSqlQuery>    _notification_update;
    std::auto_ptr<QSqlQuery>    _service_insert;
    std::auto_ptr<QSqlQuery>    _service_update;
    std::auto_ptr<QSqlQuery>    _service_check_update;
    std::auto_ptr<QSqlQuery>    _service_dependency_insert;
    std::auto_ptr<QSqlQuery>    _service_dependency_update;
    std::auto_ptr<QSqlQuery>    _service_group_insert;
    std::auto_ptr<QSqlQuery>    _service_group_update;
    std::auto_ptr<QSqlQuery>    _service_state_insert;
    std::auto_ptr<QSqlQuery>    _service_state_update;
    std::auto_ptr<QSqlQuery>    _service_status_update;
    std::auto_ptr<QSqlDatabase> _db;
    bool                        _process_out;
    unsigned int                _queries_per_transaction;
    unsigned int                _transaction_queries;
    bool                        _with_state_events;
  };
}

CCB_END()

#endif // !CCB_SQL_STREAM_HH
