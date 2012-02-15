/*
** Copyright 2011-2012 Merethis
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

#ifdef _WIN32
#  include <windows.h>
#else
#  include <sys/types.h>
#  include <unistd.h>
#endif // _WIN32

#include <assert.h>
#include <iomanip>
#include <stdlib.h>
#include <sstream>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"
#include "com/centreon/connector/icmp/check_dispatch.hh"

using namespace com::centreon::concurrency;
using namespace com::centreon::connector::icmp;

/**
 *  Default constructor.
 */
check_dispatch::check_dispatch(check_observer* observer)
  : thread(),
    _build_results(this, &check_dispatch::_process_receive),
    _current_checks(0),
    _host_id(0),
    _id(0),
    _internal_sequence(0),
    _max_concurrent_checks(10),
    _pkt_dispatcher(this),
    _quit(false),
    _observer(observer),
    _t_manager(1) {
  // icmp_id have only 16 bits trunk pid.
#ifdef _WIN32
  _id = GetCurrentProcessId() & 0xFFFF;
#else
  _id = getpid() & 0xFFFF;
#endif
  exec();
}

/**
 *  Default destructor.
 */
check_dispatch::~check_dispatch() throw () {
  {
    locker lock(&_mtx);
    _quit = true;
    _cnd.wake_one();
  }

  wait();

  locker lock(&_mtx);
  for (std::list<check*>::const_iterator
         it(_checks_new.begin()), end(_checks_new.end());
       it != end;
       ++it)
    delete *it;
}

/**
 *  Get the maximum simultaneous checks.
 *
 *  @return The maximum concurrency checks.
 */
unsigned int check_dispatch::get_max_concurrent_checks() const throw () {
  return (_max_concurrent_checks);
}

/**
 *  Set the maximum simultaneous checks.
 *
 *  @param[in] max  The maximum concurrency checks.
 */
void check_dispatch::set_max_concurrent_checks(unsigned int max) {
  if (!max)
    throw (basic_error() << "invalid max concurrent checks" \
           ": value is null");
  _max_concurrent_checks = max;
}

/**
 *  Submit a command line to execute it.
 *
 *  @param[in] command_line  The connector command line.
 */
void check_dispatch::submit(std::string const& command_line) {
  submit(0, command_line);
}

/**
 *  Submit a command line to execute it.
 *
 *  @param[in] command_id    The connector command id.
 *  @param[in] command_line  The connector command line.
 */
void check_dispatch::submit(
                       unsigned int command_id,
                       std::string const& command_line) {
  logging::debug(logging::high)
    << "submit(" << command_id << ", " << command_line << ")";

  locker lock(&_mtx);
  _checks_new.push_back(new check(command_id, command_line));
  _cnd.wake_one();
}

/**
 *  Main loop for check dispatcher.
 */
void check_dispatch::_run() {
  try {
    while (true) {
      _t_manager.execute();

      locker lock(&_mtx);
      if (_quit && !_checks_new.size() && !_checks.size())
        break;

      _process_checks();

      timestamp now(timestamp::now());
      if (!_results.empty())
        _t_manager.add(&_build_results, now, true);

      timestamp next(_t_manager.next_execution_time());
      if (next == timestamp())
        _cnd.wait(&_mtx);
      if (next <= now)
        continue;
      else {
        unsigned long timeout((next - now).to_mseconds());
        _cnd.wait(&_mtx, timeout);
      }
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::low) << e.what();
  }
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
check_dispatch::check_dispatch(check_dispatch const& right)
  : thread(),
    packet_observer(),
    _build_results(this, &check_dispatch::_process_receive) {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
check_dispatch& check_dispatch::operator=(check_dispatch const& right) {
  return (_internal_copy(right));
}

/**
 *  Receive data from packet dispatcher.
 *
 *  @param[in] pkt   The packet receive.
 */
void check_dispatch::emit_receive(packet const& pkt) {
  if (pkt.get_id() != _id) {
    logging::debug(logging::low) << "invalid packet:id";
    return ;
  }

  // This packet is an echo request from us to us.
  // (raw socket on loopback).
  if (pkt.get_type() == packet::icmp_echo)
    return;

  locker lock(&_mtx);
  _results.push_back(pkt);
  _cnd.wake_one();
}

/**
 *  Build response for a specific check.
 *
 *  @param[in] chk Source of data.
 *
 *  @return The formated response.
 */
void check_dispatch::_build_response(check const& chk) {
  int hosts_ok(0);
  int hosts_warning(0);
  std::ostringstream oss;
  unsigned int status(0);
  static char const* status_str[] = {
    "OK",
    "WARNING",
    "CRITICAL",
    "UNKNOWN"
  };

  oss << std::fixed << std::setprecision(3) << " - ";

  std::list<host*> const& hosts(chk.get_hosts());
  for (std::list<host*>::const_iterator
         it(hosts.begin()), end(hosts.end());
       it != end;
       ++it) {
    if (it != hosts.begin())
      oss << " :: ";

    host const& hst(**it);
    unsigned int pl((hst.get_packet_lost() * 100) / hst.get_packet_send());

    if (!hst.get_error().empty()) {
      status = 2;
      oss << hst.get_name() << ": "
          << hst.get_error() << " @ "
          << host::address_to_string(hst.get_address())
          << ". rta nan, lost 100%";
    }
    else {
      oss << hst.get_name() << ": rta ";
      if (pl != 100)
        oss << static_cast<float>(hst.get_roundtrip_avg()) / 1000 << "ms";
      else
        oss << "nan";
      oss << ", lost " << pl << "%";
    }

    if (pl >= chk.get_critical_packet_lost()
        || hst.get_roundtrip_avg() >= chk.get_critical_roundtrip_avg())
      status = 2;
    else if(!status
            && (pl >= chk.get_warning_packet_lost()
                || hst.get_roundtrip_avg() >= chk.get_warning_roundtrip_avg())) {
      status = 1;
      ++hosts_warning;
    }
    else
      ++hosts_ok;
  }

  oss << "|";

  for (std::list<host*>::const_iterator
         it(hosts.begin()), end(hosts.end());
       it != end;
       ++it) {
    host const& hst(**it);
    std::string const& name(hosts.size() > 1 ? hst.get_name() : "");
    oss << name
        << "rta=" << static_cast<float>(hst.get_roundtrip_avg()) / 1000 << "ms;"
        << static_cast<float>(chk.get_warning_roundtrip_avg()) / 1000 << ";"
        << static_cast<float>(chk.get_critical_roundtrip_avg()) / 1000 << ";0; "
        << name << "pl="
        << (hst.get_packet_lost() * 100) / hst.get_packet_send() << "%;"
        << chk.get_warning_packet_lost() << ";"
        << chk.get_critical_packet_lost() << ";; "
        << name << "rtmax="
        << static_cast<float>(hst.get_roundtrip_max()) / 1000 << "ms;;;; "
        << name << "rtmin="
        << static_cast<float>(hst.get_roundtrip_min()) / 1000 << "ms;;;; ";
  }

  if (chk.get_min_hosts_alive() > -1) {
    if (hosts_ok >= chk.get_min_hosts_alive())
      status = 0;
    else if ((hosts_ok + hosts_warning) >= chk.get_min_hosts_alive())
      status = 1;
  }

  std::string new_status(status_str[status] + oss.str());
  logging::debug(logging::low)
    << "build response(" << chk.get_command_id() << ", " << status
    << ", " << new_status << ")";

  if (_observer)
    _observer->emit_check_result(
                 chk.get_command_id(),
                 status,
                 new_status);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
check_dispatch& check_dispatch::_internal_copy(check_dispatch const& right) {
  (void)right;
  assert(!"impossible to copy check_dispatch");
  abort();
  return (*this);
}

/**
 *  Create packet with host and check information and send it.
 *  (Don't forget to lock before calling this methode.)
 *
 *  @param[in] icmp_info  The icmp information.
 */
void check_dispatch::_push_packet(icmp_info* info) {
  unsigned short seq(++_internal_sequence);
  logging::debug(logging::low) << "push packet(" << seq << ")";

  info->hst->has_send_packet();
  info->pkt->set_sequence(seq);
  _pkt_dispatcher.push(*info->pkt);

  timestamp next_timeout(timestamp::now());
  next_timeout.add_useconds(info->chk->get_max_packet_interval());

  _t_manager.add(
               new timeout(
                     this,
                     info->hst->get_id(),
                     info->hst->get_packet_send()),
               next_timeout,
               false,
               true);
  _cnd.wake_one();
}

/**
 *  Build all class checks.
 */
void check_dispatch::_process_checks() {
  while (!_checks_new.empty()
         && _current_checks <= _max_concurrent_checks) {
    check* chk(_checks_new.front());
    _checks_new.pop_front();

    try {
      chk->parse();
      ++_current_checks;
    }
    catch (std::exception const& e) {
      delete chk;
      logging::error(logging::low) << e.what();
      continue;
    }

    // logging::debug(logging::medium) << "build " << *chk;
    std::list<host*> const& hosts(chk->get_hosts());
    for (std::list<host*>::const_iterator
           hst(hosts.begin()), end(hosts.end());
         hst != end;
         ++hst) {

      (*hst)->set_id(_host_id++);
      // logging::debug(logging::medium) << "build " << **hst;

      packet* pkt(new packet(chk->get_packet_data_size()));
      pkt->set_address((*hst)->get_address());
      pkt->set_host_id((*hst)->get_id());
      pkt->set_id(_id);

      icmp_info info(chk, *hst, pkt);

      _checks[(*hst)->get_id()] = info;
      _push_packet(&info);

      timestamp next_timeout(timestamp::now());
      next_timeout.add_useconds(chk->get_max_completion_time());
      _t_manager.add(
                   new timeout(
                         this,
                         (*hst)->get_id()),
                   next_timeout,
                   false,
                   true);
    }
  }
}

/**
 *  Process data receive from the packet dispatcher.
 */
void check_dispatch::_process_receive() {
  locker lock(&_mtx);
  while (!_results.empty()) {
    packet pkt(_results.front());
    _results.pop_front();

    std::map<unsigned int, icmp_info>::iterator
      it(_checks.find(pkt.get_host_id()));
    if (it == _checks.end()) {
      logging::debug(logging::low)
        << "packet drop: " << pkt.get_host_id();
      continue;
    }

    host& hst(*it->second.hst);
    unsigned int elapsed_time
      = (pkt.get_recv_time() - pkt.get_send_time()).to_useconds();
    if (pkt.get_type() == packet::icmp_echoreply) {
      hst.has_recv_packet(elapsed_time);
      // logging::debug(logging::high) << "packet receive " << hst;
    }
    else {
      hst.has_lost_packet(elapsed_time);
      hst.set_error(pkt.get_error());
      // logging::debug(logging::high) << "packet lost " << hst;
    }

    check& chk(*it->second.chk);
    if (hst.get_packet_send() < chk.get_nb_packet())
      _push_packet(&it->second);
    else if (hst.get_packet_recv() + hst.get_packet_lost()
             >= chk.get_nb_packet()) {
      chk.host_was_checked();
      delete it->second.pkt;
      if (chk.get_current_host_check() >= chk.get_hosts().size()) {
        _build_response(chk);
        delete it->second.chk;
        --_current_checks;
      }
      _checks.erase(it);
    }
  }
}

/**
 *  Default constructor.
 *
 *  @param[in] dispatch  The check dispatcher.
 */
check_dispatch::task_runner::task_runner(
                               check_dispatch* dispatcher,
                               void (check_dispatch::*func)())
  : _dispatcher(dispatcher),
    _func(func) {

}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
check_dispatch::task_runner::task_runner(task_runner const& right)
  : task(right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
check_dispatch::task_runner::~task_runner() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
check_dispatch::task_runner& check_dispatch::task_runner::operator=(task_runner const& right) {
  return (_internal_copy(right));
}

/**
 *  Build all new checks and insert the result into the dispatcher.
 */
void check_dispatch::task_runner::run() {
  if (_dispatcher && _func)
    (_dispatcher->*_func)();
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
check_dispatch::task_runner& check_dispatch::task_runner::_internal_copy(task_runner const& right) {
  if (this != &right) {
    _dispatcher = right._dispatcher;
  }
  return (*this);
}

/**
 *  Constructor.
 *
 *  @param[in] dispatcher  The dispatcher to remove data.
 *  @param[in] host_id     Host identifier.
 *  @param[in] id          Packet identifier.
 */
check_dispatch::timeout::timeout(
                           check_dispatch* dispatcher,
                           unsigned int host_id,
                           unsigned int id)
  : _host_id(host_id),
    _dispatcher(dispatcher),
    _remove(id ? &timeout::_delay_push_packet : &timeout::_remove_target),
    _id(id) {

}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
check_dispatch::timeout::timeout(timeout const& right)
  : task(right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
check_dispatch::timeout::~timeout() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
check_dispatch::timeout& check_dispatch::timeout::operator=(timeout const& right) {
  return (_internal_copy(right));
}

/**
 *  Execute timeout.
 */
void check_dispatch::timeout::run() {
  (this->*_remove)();
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
check_dispatch::timeout& check_dispatch::timeout::_internal_copy(timeout const& right) {
  if (this != &right) {
    _dispatcher = right._dispatcher;
    _host_id = right._host_id;
    _remove = right._remove;
    _id = right._id;
  }
  return (*this);
}

/**
 *  Remove packet check.
 */
void check_dispatch::timeout::_delay_push_packet() {
  locker lock(&_dispatcher->_mtx);
  logging::debug(logging::high)
    << "delay push packet hst_id(" << _host_id << ") pkt_id(" << _id << ")";

  std::map<unsigned int, icmp_info>::iterator
    it(_dispatcher->_checks.find(_host_id));
  if (it != _dispatcher->_checks.end()) {
    host& hst(*it->second.hst);
    check& chk(*it->second.chk);
    if (hst.get_packet_recv() + hst.get_packet_lost() < _id
        && hst.get_packet_send() < chk.get_nb_packet())
      _dispatcher->_push_packet(&it->second);
  }
}

/**
 *  Remove target check.
 */
void check_dispatch::timeout::_remove_target() {
  _dispatcher->_process_receive();

  locker lock(&_dispatcher->_mtx);
  std::map<unsigned int, icmp_info>::iterator
    it(_dispatcher->_checks.find(_host_id));
  if (it == _dispatcher->_checks.end())
      return;

  host& hst(*it->second.hst);
  check& chk(*it->second.chk);

  // logging::debug(logging::low) << "remove target " << hst;

  unsigned int nb_packet(hst.get_packet_recv() + hst.get_packet_lost());
  if (nb_packet < chk.get_nb_packet()) {
    unsigned int elapsed_time
      = (chk.get_max_completion_time() - hst.get_total_time_waited())
      / (chk.get_nb_packet() - nb_packet);
    while (nb_packet++ < chk.get_nb_packet()) {
      if (hst.get_packet_send() < chk.get_nb_packet())
        hst.has_send_packet();
      hst.has_lost_packet(elapsed_time);
    }
  }

  chk.host_was_checked();
  if (chk.get_current_host_check() == chk.get_hosts().size()) {
    _dispatcher->_build_response(chk);
    --_dispatcher->_current_checks;
    delete it->second.chk;
  }

  delete it->second.pkt;
  _dispatcher->_checks.erase(it);
}

