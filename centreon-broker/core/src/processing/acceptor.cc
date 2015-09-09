/*
** Copyright 2015 Centreon
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

#include <QMutexLocker>
#include <QCoreApplication>
#include <sstream>
#include <unistd.h>
#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/processing/acceptor.hh"
#include "com/centreon/broker/processing/feeder.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::processing;

/**
 *  Constructor.
 *
 *  @param[in] endp       Endpoint.
 *  @param[in] name       Name of the endpoint.
 */
acceptor::acceptor(
            misc::shared_ptr<io::endpoint> endp,
            std::string const& name)
  : _endp(endp),
    _name(name),
    _retry_interval(30) {}

/**
 *  Destructor.
 */
acceptor::~acceptor() {
  _wait_feeders();
}

/**
 *  Accept a new incoming connection.
 */
void acceptor::accept() {
  static unsigned int connection_id(0);

  // Try to accept connection.
  misc::shared_ptr<io::stream> s(_endp->open());
  if (!s.isNull()) {
    // Create feeder thread.
    std::string name;
    {
      std::ostringstream oss;
      oss << _name << "-" << ++connection_id;
      name = oss.str();
    }
    misc::shared_ptr<processing::feeder>
      f(new processing::feeder(
                          name,
                          s,
                          _read_filters,
                          _write_filters));

    // Run feeder thread.
    f->start();
    _feeders.push_back(f);
  }
  return ;
}

/**
 *  Exit this thread.
 */
void acceptor::exit() {
  thread::exit();
  return ;
}

/**
 *  @brief Main routine.
 *
 *  Acceptors accepts clients and launch feeder threads.
 */
void acceptor::run() {
  // Run as long as no exit request was made.
  while (!should_exit()) {
    try {
      // Try to accept connection.
      accept();
    }
    catch (std::exception const& e) {
      // Log error.
      logging::error(logging::high) << "acceptor: endpoint '"
        << _name << "' could not accept client: " << e.what();

      // Sleep a while before reconnection.
      logging::info(logging::medium)
        << "acceptor: endpoint '" << _name << "' will wait "
        << _retry_interval
        << "s before attempting to accept a new client";
      time_t limit(time(NULL) + _retry_interval);
      while (!should_exit() && (time(NULL) < limit)) {
        QCoreApplication::processEvents();
        ::sleep(1);
      }
    }

    // Check for terminated feeders.
    for (std::list<misc::shared_ptr<processing::feeder> >::iterator
           it(_feeders.begin()),
           end(_feeders.end());
         it != end;)
      if ((*it)->wait(0))
        it = _feeders.erase(it);
      else
        ++it;
  }

  // Cleanup.
  _wait_feeders();

  return ;
}

/**
 *  @brief Set read filters.
 *
 *  This is only useful in input mode.
 *
 *  @param[in] filters  Set of accepted event IDs.
 */
void acceptor::set_read_filters(uset<unsigned int> const& filters) {
  _read_filters = filters;
  return ;
}

/**
 *  @brief Set retry interval of the acceptor.
 *
 *  The retry interval is only used in case of error of the acceptor. In
 *  normal operation mode, connections are accepted as soon as possible.
 *
 *  @param[in] retry_interval  Retry interval between two client
 *                             acception attempts in case of error at
 *                             the first attempt.
 */
void acceptor::set_retry_interval(time_t retry_interval) {
  _retry_interval = retry_interval;
  return ;
}

/**
 *  @brief Set write filters.
 *
 *  This is useful to prevent endpoints of generating some kind of
 *  events.
 */
void acceptor::set_write_filters(uset<unsigned int> const& filters) {
  _write_filters = filters;
  return ;
}

/**
 *  Wait for launched feeders.
 */
void acceptor::_wait_feeders() {
  // Wait for all launched feeders.
  for (std::list<misc::shared_ptr<processing::feeder> >::iterator
         it(_feeders.begin()),
         end(_feeders.end());
       it != end;
       ++it)
    (*it)->exit();
  for (std::list<misc::shared_ptr<processing::feeder> >::iterator
         it(_feeders.begin()),
         end(_feeders.end());
       it != end;
       ++it)
    (*it)->wait();
  _feeders.clear();
  return ;
}
