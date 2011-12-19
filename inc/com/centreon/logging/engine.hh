/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CC_LOGGING_ENGINE_HH
#  define CC_LOGGING_ENGINE_HH

#  include <vector>
#  include "com/centreon/concurrency/mutex.hh"
#  include "com/centreon/logging/backend.hh"
#  include "com/centreon/logging/verbosity.hh"
#  include "com/centreon/namespace.hh"

CC_BEGIN()

namespace                      logging {
  typedef unsigned long long   type_flags;
  typedef unsigned int         type_number;
  typedef verbosity            verbosities[sizeof(type_flags) * CHAR_BIT];

  /**
   *  @class engine engine.hh "com/centreon/logging/engine.hh"
   *  @brief Logging object manager.
   *
   *  This is an external access point to logging system. Allow to
   *  register backends and write log message into them.
   */
  class                        engine {
  public:
    enum                       time_precision {
      none = 0,
      microsecond = 1,
      millisecond = 2,
      second = 3
    };

    unsigned long              add(
                                 backend* obj,
                                 type_flags types,
                                 verbosity const& verbose);
    static engine&             instance();
    bool                       is_log(
                                 type_number flag,
                                 verbosity const& verbose) const throw ();
    bool                       get_enable_sync() const throw ();
    bool                       get_show_pid() const throw ();
    time_precision             get_show_timestamp() const throw ();
    bool                       get_show_thread_id() const throw ();
    static void                load();
    void                       log(
                                 type_number flag,
                                 verbosity const& verbose,
                                 char const* msg);
    bool                       remove(unsigned long id);
    unsigned int               remove(backend* obj);
    void                       set_enable_sync(bool enable) throw ();
    void                       set_show_pid(bool enable) throw ();
    void                       set_show_timestamp(
                                 time_precision p) throw ();
    void                       set_show_thread_id(bool enable) throw ();
    static void                unload();

  private:
    struct                     backend_info {
      unsigned long            id;
      backend*                 obj;
      type_flags               types;
      verbosity                verbose;
    };

                               engine();
                               engine(engine const& right);
                               ~engine() throw ();
    engine&                    operator=(engine const& right);
    engine&                    _internal_copy(engine const& right);
    void                       _rebuild_verbosities();

    std::vector<backend_info*> _backends;
    unsigned long              _id;
    static engine*             _instance;
    bool                       _is_sync;
    verbosities                _list_verbose;
    mutable concurrency::mutex _mtx;
    bool                       _show_pid;
    time_precision             _show_timestamp;
    bool                       _show_thread_id;
  };
}

CC_END()

#endif // !CC_LOGGING_ENGINE_HH
