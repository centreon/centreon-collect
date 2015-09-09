/*
** Copyright 2011-2014 Centreon
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

#ifndef CCB_NEB_DOWNTIME_SCHEDULER_HH
#  define CCB_NEB_DOWNTIME_SCHEDULER_HH

#  include <map>
#  include <QThread>
#  include <QMutex>
#  include <QSemaphore>
#  include <QWaitCondition>
#  include "com/centreon/broker/namespace.hh"
#  include "com/centreon/broker/timestamp.hh"
#  include "com/centreon/broker/neb/downtime.hh"

CCB_BEGIN()

namespace             neb {

  /**
   *  @class downtime_scheduler downtime_scheduler.hh "com/centreon/broker/notification/downtime_scheduler.hh"
   *  @brief The downtime scheduler.
   *
   *  Manage a thread that manages downtime end scheduling.
   */
  class        downtime_scheduler : public QThread {
  public:
               downtime_scheduler();

    void       start_and_wait();
    void       quit() throw ();
    void       add_downtime(
                 timestamp start_time,
                 timestamp end_time,
                 downtime const& dwn);
    void       remove_downtime(
                 unsigned int internal_id);

  protected:
    void       run();

  private:
    bool       _should_exit;
    QMutex     _general_mutex;
    QWaitCondition
               _general_condition;
    QSemaphore _started;

    std::multimap<timestamp, unsigned int>
               _downtime_starts;
    std::multimap<timestamp, unsigned int>
               _downtime_ends;
    std::map<unsigned int, downtime>
               _downtimes;

               downtime_scheduler(downtime_scheduler const& obj);
    downtime_scheduler&
               operator=(downtime_scheduler const& obj);

    static timestamp
               _get_first_timestamp(
                 std::multimap<timestamp, unsigned int> const& list);
    void       _process_downtimes();
    static void
               _start_downtime(downtime& dwn, io::stream* stream);
    static void
               _end_downtime(downtime& dwn, io::stream* stream);
  };
}

CCB_END()

#endif // !CCB_NEB_DOWNTIME_SCHEDULER_HH
