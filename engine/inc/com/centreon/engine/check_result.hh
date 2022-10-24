/*
** Copyright 2002-2006 Ethan Galstad
** Copyright 2011-2013 Merethis
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

#ifndef CCE_CHECKS_HH
#define CCE_CHECKS_HH

#include <sys/time.h>

#include "com/centreon/engine/checkable.hh"

enum check_source { service_check, host_check };

CCE_BEGIN()
class notifier;
class check_result {
 public:
  using pointer = std::shared_ptr<check_result>;

  check_result();
  check_result(enum check_source object_check_type,
               notifier* notifier,
               enum checkable::check_type check_type,
               unsigned check_options,
               bool reschedule_check,
               double latency,
               struct timeval start_time,
               struct timeval finish_time,
               bool early_timeout,
               bool exited_ok,
               int return_code,
               std::string output);

  inline enum check_source get_object_check_type() const {
    return _object_check_type;
  }
  void set_object_check_type(enum check_source object_check_type);
  inline notifier* get_notifier() { return _notifier; }
  void set_notifier(notifier* notifier);
  inline struct timeval get_finish_time() const { return _finish_time; }
  void set_finish_time(struct timeval finish_time);
  inline struct timeval get_start_time() const { return _start_time; }
  void set_start_time(struct timeval start_time);
  inline int get_return_code() const { return _return_code; }
  void set_return_code(int return_code);
  inline bool get_early_timeout() const { return _early_timeout; }
  void set_early_timeout(bool early_timeout);
  inline const std::string& get_output() const { return _output; }
  void set_output(std::string const& output);
  inline bool get_exited_ok() const { return _exited_ok; }
  void set_exited_ok(bool exited_ok);
  inline bool get_reschedule_check() const { return _reschedule_check; }
  void set_reschedule_check(bool reschedule_check);
  inline enum checkable::check_type get_check_type() const {
    return _check_type;
  };
  void set_check_type(enum checkable::check_type check_type);
  inline double get_latency() const { return _latency; };
  void set_latency(double latency);
  inline unsigned get_check_options() const { return _check_options; };
  void set_check_options(unsigned check_options);

 private:
  enum check_source _object_check_type;  // is this a service or a host check?
  notifier* _notifier;
  // was this an active or passive service check?
  enum checkable::check_type _check_type;
  unsigned _check_options;
  bool _reschedule_check;  // should we reschedule the next check
  double _latency;
  struct timeval _start_time;   // time the service check was initiated
  struct timeval _finish_time;  // time the service check was completed
  bool _early_timeout;          // did the service check timeout?
  bool _exited_ok;              // did the plugin check return okay?
  int _return_code;             // plugin return code
  std::string _output;          // plugin output
};
CCE_END()

#endif  // !CCE_CHECKS_HH
