/*
** Copyright 2011-2013,2015 Merethis
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

#ifndef CCB_STORAGE_METRIC_HH
#  define CCB_STORAGE_METRIC_HH

#  include <QString>
#  include "com/centreon/broker/io/data.hh"
#  include "com/centreon/broker/io/event_info.hh"
#  include "com/centreon/broker/io/events.hh"
#  include "com/centreon/broker/namespace.hh"
#  include "com/centreon/broker/timestamp.hh"
#  include "com/centreon/broker/mapping/entry.hh"
#  include "com/centreon/broker/storage/internal.hh"

CCB_BEGIN()

namespace          storage {
  /**
   *  @class metric metric.hh "com/centreon/broker/storage/metric.hh"
   *  @brief Metric information.
   *
   *  Metric information, mainly used to update RRD files.
   */
  class            metric : public io::data {
  public:
                   metric();
                   metric(metric const& m);
                   ~metric();
    metric&        operator=(metric const& m);
    unsigned int   type() const;

    /**
     *  Get the type of this event.
     *
     *  @return  The event type.
     */
    static unsigned int
                   static_type() {
      return (io::events::data_type<
                            io::events::storage,
                            storage::de_metric>::value);
    }


    timestamp      ctime;
    unsigned int   interval;
    bool           is_for_rebuild;
    unsigned int   metric_id;
    QString        name;
    int            rrd_len;
    double         value;
    short          value_type;

    static mapping::entry const
                    entries[];
    static io::event_info::event_operations const
                    operations;

  private:
    void           _internal_copy(metric const& m);
  };
}

CCB_END()

#endif // !CCB_STORAGE_METRIC_HH
