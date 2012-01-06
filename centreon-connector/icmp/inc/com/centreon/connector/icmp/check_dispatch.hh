/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCC_ICMP_CHECK_DISPATCH_HH
#  define CCC_ICMP_CHECK_DISPATCH_HH

#  include <list>
#  include <string>
#  include "com/centreon/concurrency/mutex.hh"
#  include "com/centreon/concurrency/thread.hh"
#  include "com/centreon/connector/icmp/check.hh"
#  include "com/centreon/connector/icmp/check_observer.hh"
#  include "com/centreon/connector/icmp/icmp_info.hh"
#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/connector/icmp/packet_dispatch.hh"
#  include "com/centreon/connector/icmp/packet_observer.hh"
#  include "com/centreon/task_manager.hh"
#  include "com/centreon/task.hh"
#  include "com/centreon/timestamp.hh"

CCC_ICMP_BEGIN()

/**
 *  @class check_dispatch check_dispatch.hh "com/centreon/connector/icmp/check_dispatch.hh"
 *  @brief Dispatch check.
 */
class                  check_dispatch
  : private concurrency::thread,
    private packet_observer {
public:
                       check_dispatch(check_observer* observer = NULL);
                       ~check_dispatch() throw ();
  unsigned int         get_max_concurrent_checks() const throw ();
  void                 set_max_concurrent_checks(unsigned int max) throw ();
  void                 submit(std::string const& command_line);
  void                 submit(
                         unsigned int command_id,
                         std::string const& command_line);

protected:
  void                 _run();

private:
  class                task_runner : public task {
  public:
                       task_runner(
                         check_dispatch* dispatcher,
                         void (check_dispatch::*func)());
                       task_runner(task_runner const& right);
                       ~task_runner() throw ();
    task_runner&       operator=(task_runner const& right);
    void               run();

  private:
    task_runner&       _internal_copy(task_runner const& right);

    check_dispatch*    _dispatcher;
    void (check_dispatch::*_func)();
  };

  class                timeout : public task {
  public:
                       timeout(
                         check_dispatch* dispatcher,
                         unsigned int host_id,
                         unsigned int id = 0);
                       timeout(timeout const& right);
                       ~timeout() throw ();
    timeout&           operator=(timeout const& right);
    void               run();

  private:
    timeout&           _internal_copy(timeout const& right);
    void               _delay_push_packet();
    void               _remove_target();

    unsigned int       _host_id;
    check_dispatch*    _dispatcher;
    void               (timeout::*_remove)();
    unsigned int       _id;
  };

                       check_dispatch(check_dispatch const& right);
  check_dispatch&      operator=(check_dispatch const& right);
  void                 emit_receive(packet const& pkt);
  void                 _build_response(check const& chk);
  check_dispatch&      _internal_copy(check_dispatch const& right);
  void                 _process_checks();
  void                 _process_receive();
  void                 _push_packet(icmp_info* info);

  task_runner          _build_results;
  std::list<check*>    _checks_new;
  std::map<unsigned int, icmp_info>
                       _checks;
  concurrency::wait_condition
                       _cnd;
  unsigned int         _current_checks;
  unsigned int         _host_id;
  unsigned short       _id;
  unsigned short       _internal_sequence;
  unsigned int         _max_concurrent_checks;
  packet_dispatch      _pkt_dispatcher;
  bool                 _quit;
  std::list<packet>    _results;
  check_observer*      _observer;
  concurrency::mutex   _mtx;
  task_manager         _t_manager;
};

CCC_ICMP_END()

#endif // !CCC_ICMP_CHECK_DISPATCH_HH
