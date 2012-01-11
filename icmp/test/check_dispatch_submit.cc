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

#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/check_dispatch.hh"
#include "com/centreon/concurrency/condvar.hh"
#include "com/centreon/concurrency/mutex.hh"
#include "com/centreon/logging/engine.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon::concurrency;
using namespace com::centreon;

/**
 *  @class observer
 *  @brief Little implementation of packet_observer
 *         to test check dispatcher.
 */
class                observer : public check_observer {
public:
                     observer() : _command_id(0), _status(0) {
    _mtx.lock();
  }
                     ~observer() throw () {
    _mtx.unlock();
  }
  void               emit_check_result(
                       unsigned int command_id,
                       unsigned int status,
                       std::string const& msg) {
    _command_id = command_id;
    _status = status;
    _msg = msg;

    _mtx.lock();
    _cond.wake_one();
    _mtx.unlock();
  }
  unsigned int       get_command_id() const throw () {
    return (_command_id);
  }
  unsigned int       get_status() const throw () { return (_status); }
  std::string const& get_message() const throw () { return (_msg); }
  void               wait() {
    _cond.wait(&_mtx);
  }

private:
  unsigned int       _command_id;
  unsigned int       _status;
  std::string        _msg;
  mutex              _mtx;
  condvar            _cond;
};

/**
 *  Check check dispatch submit command.
 *
 *  @return 0 on success.
 */
int main() {
  int ret = 0;
  logging::engine::load();
  try {
    observer obs;
    check_dispatch cd(&obs);
    cd.submit(42, "127.0.0.1");
    obs.wait();
    if (obs.get_command_id() != 42
        || obs.get_status() != 0
        || obs.get_message().empty())
      throw (basic_error() << "invalid result for 127.0.0.1");

    cd.submit(24, "localhost");
    obs.wait();
    if (obs.get_command_id() != 24
        || obs.get_status() != 0
        || obs.get_message().empty())
      throw (basic_error() << "invalid result for localhost");

    cd.submit(11, "");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    ret = 1;
  }
  logging::engine::unload();
  return (ret);
}
