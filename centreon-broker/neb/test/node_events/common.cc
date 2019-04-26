/*
** Copyright 2011-2015 Centreon
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

#include <cstdio>
#include <ctime>
#include <iostream>
#include "com/centreon/broker/correlation/engine_state.hh"
#include "com/centreon/broker/correlation/state.hh"
#include "com/centreon/broker/correlation/internal.hh"
#include "com/centreon/broker/correlation/issue.hh"
#include "com/centreon/broker/correlation/issue_parent.hh"
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/io/events.hh"
#include "common.hh"

using namespace com::centreon::broker;

/**
 *  Format a command.
 *
 *  @param[in] command  The command.
 *  @param[in] when     Replace the instance of $TIMESTAMP$ in the command by.
 *  @param[in] when2    Replace the instance of $TIMESTAMP2$ in the command by.
 *
 *  @return             The formatted command.
 */
QString format_command(
          QString const& command,
          time_t when,
          time_t when2) {
  char t[64];
  ::snprintf(t, 64, "[%lu] ", (unsigned long)(::time));
  QString cpy = command;
  QString ts = QString::number((unsigned long)when);
  QString ts2 = QString::number((unsigned long)when2);
  cpy.replace("$TIMESTAMP$", ts);
  cpy.replace("$TIMESTAMP2$", ts2);
  cpy.prepend(t);
  return (cpy);
}

/**
 *  Add a downtime.
 */
void add_downtime(
       QList<com::centreon::broker::std::shared_ptr<com::centreon::broker::io::data> >& content,
       time_t start_time,
       time_t end_time,
       time_t duration,
       bool fixed,
       unsigned int host_id,
       unsigned int service_id,
       unsigned int internal_id,
       short downtime_type,
       time_t real_start_time,
       time_t real_end_time) {
  std::shared_ptr<neb::downtime> dwn(new neb::downtime);
  dwn->start_time = start_time;
  dwn->end_time = end_time;
  dwn->duration = duration;
  dwn->fixed = fixed;
  dwn->host_id = host_id;
  dwn->service_id = service_id;
  dwn->internal_id = internal_id;
  dwn->downtime_type = downtime_type;
  dwn->actual_start_time = real_start_time;
  dwn->actual_end_time = real_end_time;
  content.append(dwn);
}

/**
 *  Add an acknowledgement.
 */
void add_acknowledgement(
       QList<com::centreon::broker::std::shared_ptr<com::centreon::broker::io::data> >& content,
       time_t entry_time,
       time_t deletion_time,
       unsigned int host_id,
       unsigned int service_id,
       bool is_sticky,
       short state) {
  std::shared_ptr<neb::acknowledgement> ack(new neb::acknowledgement);
  ack->entry_time = entry_time;
  ack->deletion_time = deletion_time;
  ack->host_id = host_id;
  ack->service_id = service_id;
  ack->is_sticky = is_sticky;
  ack->state = state;
  content.append(ack);
}

/**
 *  Check the content read from a stream.
 *
 *  @param[in] s       Stream.
 *  @param[in] content Content to match against stream.
 *
 *  @return If all content was found and matched. Throw otherwise.
 */
void check_content(
       io::stream& s,
       QList<std::shared_ptr<io::data> > const& content) {
  unsigned int i(0);
  for (QList<std::shared_ptr<io::data> >::const_iterator
         it(content.begin()),
         end(content.end());
       it != end;) {
    std::shared_ptr<io::data> d;
    s.read(d);
    if (d.isNull())
      throw (exceptions::msg() << "entry #" << i << " is null");
    else if (d->type() == (*it)->type()) {
      if (d->type() == neb::downtime::static_type()) {
        neb::downtime const& dwn1 = d.ref_as<neb::downtime const>();
        neb::downtime const& dwn2 = it->ref_as<neb::downtime const>();
        if (dwn1.start_time != dwn2.start_time
            || dwn1.end_time != dwn2.end_time
            || dwn1.duration != dwn2.duration
            || dwn1.fixed != dwn2.fixed
            || dwn1.host_id != dwn2.host_id
            || dwn1.service_id != dwn2.service_id
            || dwn1.internal_id != dwn2.internal_id
            || dwn1.downtime_type != dwn2.downtime_type
            || dwn1.actual_start_time != dwn2.actual_start_time
            || dwn1.actual_end_time != dwn2.actual_end_time)
          throw (exceptions::msg() << "entry #" << i
                 << " (downtime) mismatch: got (start_time " << dwn1.start_time
                 << ", end_time " << dwn1.end_time
                 << ", duration " << dwn1.duration
                 << ", fixed " << dwn1.fixed
                 << ", host_id " << dwn1.host_id
                 << ", service_id " << dwn1.service_id
                 << ", internal_id " << dwn1.internal_id
                 << ", downtime_type " << dwn1.downtime_type
                 << ", actual_start_time " << dwn1.actual_start_time
                 << ", actual_end_time " << dwn1.actual_end_time
                 << ") expected (start_time " << dwn2.start_time
                 << ", end_time " << dwn2.end_time
                 << ", duration " << dwn2.duration
                 << ", fixed " << dwn2.fixed
                 << ", host_id " << dwn2.host_id
                 << ", service_id " << dwn2.service_id
                 << ", internal_id " << dwn2.internal_id
                 << ", downtime_type " << dwn2.downtime_type
                 << ", actual_start_time " << dwn2.actual_start_time
                 << ", actual_end_time " << dwn2.actual_end_time
                 << ")");
      }
      else if (d->type() == neb::acknowledgement::static_type()) {
        neb::acknowledgement const& ack1 = d.ref_as<neb::acknowledgement>();
        neb::acknowledgement const& ack2 = it->ref_as<neb::acknowledgement>();
        if (ack1.entry_time != ack2.entry_time
            || ack1.deletion_time != ack2.deletion_time
            || ack1.host_id != ack2.host_id
            || ack1.service_id != ack2.service_id
            || ack1.is_sticky != ack2.is_sticky
            || ack1.state != ack2.state)
          throw (exceptions::msg() << "entry #" << i
                 << " (acknowledgemen) mismatch: got (entry_time " << ack1.entry_time
                 << ", deletion_time " << ack1.deletion_time
                 << ", host_id " << ack1.host_id
                 << ", service_id " << ack1.service_id
                 << ", is_sticky " << ack1.is_sticky
                 << ", state " << ack1.state
                 << ") expected (entry_time " << ack2.entry_time
                 << ", deletion_time " << ack2.deletion_time
                 << ", host_id " << ack2.host_id
                 << ", service_id " << ack2.service_id
                 << ", is_sticky " << ack2.is_sticky
                 << ", state " << ack2.state
                 << ")");
      }
      /*if (d->type() == 0) {
        std::shared_ptr<correlation::engine_state>
          es1(d.staticCast<correlation::engine_state>());
        std::shared_ptr<correlation::engine_state>
          es2(it->staticCast<correlation::engine_state>());
        if (es1->started != es2->started)
          throw (exceptions::msg() << "entry #" << i
                 << " (engine_state) mismatch: got (started "
                 << es1->started << "), expected (" << es2->started
                 << ")");
      }*/
      ++it;
      ++i;
    }
  }
  return ;
}

/**
 *  Read.
 *
 *  @param[out] d         Next available event.
 *  @param[in]  deadline  Unused.
 *
 *  @return True.
 */
bool test_stream::read(
       com::centreon::broker::std::shared_ptr<com::centreon::broker::io::data>& d,
       time_t deadline){
  (void)deadline;
  d.clear();
  if (!_events.empty() && _finalized) {
    d = _events.front();
    _events.erase(_events.begin());
  }
  return (true);
}

/**
 *  Write.
 *
 *  @param[in] d  The event.
 *
 *  @return       1.
 */
int test_stream::write(
      com::centreon::broker::std::shared_ptr<com::centreon::broker::io::data> const& d) {
  if (!d.isNull())
    _events.push_back(d);

  return (1);
}

/**
 *  Get all the written events.
 *
 *  @return  The written events.
 */
std::vector<com::centreon::broker::std::shared_ptr<com::centreon::broker::io::data> > const&
  test_stream::get_events() const {
  return (_events);
}

/**
 *  Test stream started.
 */
void test_stream::starting() {
  _finalized = false;
}

/**
 *  Test stream stopped.
 */
void test_stream::stopping() {
}

/**
 *  Finalize the test stream.
 */
void test_stream::finalize() {
  _finalized = true;
}
