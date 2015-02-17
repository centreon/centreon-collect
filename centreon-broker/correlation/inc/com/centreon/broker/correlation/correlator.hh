/*
** Copyright 2009-2013 Merethis
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

#ifndef CCB_CORRELATION_CORRELATOR_HH
#  define CCB_CORRELATION_CORRELATOR_HH

#  include <QList>
#  include <QMap>
#  include <QPair>
#  include <QString>
#  include "com/centreon/broker/correlation/node.hh"
#  include "com/centreon/broker/misc/shared_ptr.hh"
#  include "com/centreon/broker/multiplexing/hooker.hh"
#  include "com/centreon/broker/neb/host_status.hh"
#  include "com/centreon/broker/neb/log_entry.hh"
#  include "com/centreon/broker/persistent_cache.hh"

namespace                          com {
  namespace                        centreon {
    namespace                      broker {
      namespace                    correlation {
        // Forward declaration.
        class issue;
        class state;

        /**
         *  @class correlator correlator.hh "correlation/correlator.hh"
         *  @brief Create event correlation.
         *
         *  This class aggregate data from multiple events and generates
         *  according issues which are used to group those events.
         */
        class                      correlator : public multiplexing::hooker {
          Q_OBJECT

         public:
                                   correlator(
                                     unsigned int instance_id,
                                     bool is_passive = false);
                                   correlator(correlator const& c);
                                   ~correlator();
          correlator&              operator=(correlator const& c);
          QMap<QPair<unsigned int, unsigned int>, node> const&
                                   get_state() const;
          void                     load(
                                     QString const& correlation_file,
                                     QString const& retention_file);
          void                     read(misc::shared_ptr<io::data>& d);
          void                     set_state(
                                     QMap<QPair<unsigned int, unsigned int>, node> const& state);
          void                     starting();
          void                     stopping();
          unsigned int             write(
                                     misc::shared_ptr<io::data> const& e);

         public slots:
          void                     update();

         private:
          QList<misc::shared_ptr<io::data> >
                                   _events;
          QString                  _correlation_file;
          unsigned int             _instance_id;
          QMap<QPair<unsigned int, unsigned int>, node>
                                   _nodes;
          QString                  _retention_file;
          void                     _correlate_acknowledgement(
                                     misc::shared_ptr<io::data> e);
          void                     _correlate_host_service_status(
                                     misc::shared_ptr<io::data> e,
                                     bool is_host);
          void                     _correlate_host_status(
                                     misc::shared_ptr<io::data> e);
          void                     _correlate_log(
                                     misc::shared_ptr<io::data> e);
          void                     _correlate_service_status(
                                     misc::shared_ptr<io::data> e);
          issue*                   _find_related_issue(node& n);
          void                     _internal_copy(correlator const& c);
          void                     _issue_parenting(node* n, bool full);
          void                     _process_event_on_active(
                                     misc::shared_ptr<io::data> const& e);
          void                     _process_event_on_passive(
                                     misc::shared_ptr<io::data> const& e);
          QMap<QPair<unsigned int, unsigned int>, node>::iterator
                                   _remove_node(
                                     QMap<QPair<unsigned int, unsigned int>, node>::iterator it);
          void                     _update_host_service_state(
                                     misc::shared_ptr<state> s);
          void                     _update_issue(
                                     misc::shared_ptr<issue> i);
          void                     _write_issues();

          std::auto_ptr<persistent_cache>
                                   _cache;

          void                     (correlator::*_process_event)(
                                     misc::shared_ptr<io::data> const&);
        };
      }
    }
  }
}

#endif // !CCB_CORRELATION_CORRELATOR_HH
