/*
** Copyright 2014 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCB_BAM_TIME_TIMEZONE_MANAGER_HH
#  define CCB_BAM_TIME_TIMEZONE_MANAGER_HH

#  include <stack>
#  include <string>
#  include <QMutex>
#  include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace   bam  {
  namespace time {
  /**
   *  @class timezone_manager timezone_manager.hh "com/centreon/broker/bam/time/timezone_manager.hh"
   *  @brief Manage timezone changes.
   *
   *  This class handle timezone change. This can either be setting a new
   *  timezone or restoring a previous one.
   */
    class                      timezone_manager {
    public:
      static void              load();
      void                     lock();
      void                     pop_timezone();
      void                     push_timezone(char const* tz);
      void                     unlock();
      static void              unload();

    /**
     *  Get class instance.
     *
     *  @return Class instance.
     */
      static timezone_manager& instance() {
        return (*_instance);
      }

    private:
      struct                   tz_info {
        bool                   is_set;
        std::string            tz_name;
      };

                               timezone_manager();
                               timezone_manager(
                                 timezone_manager const& other);
                               ~timezone_manager();
      timezone_manager&        operator=(timezone_manager const& other);
      void                     _fill_tz_info(
                                 tz_info* info,
                                 char const* old_tz);
      void                     _set_timezone(
                                 tz_info const& from,
                                 tz_info const& to);

      tz_info                  _base;
      static timezone_manager* _instance;
      std::stack<tz_info>      _tz;
      QMutex                   _timezone_manager_mutex;
    };
  }
}

CCB_END()

#endif // !CCB_BAM_TIME_TIMEZONE_MANAGER_HH
