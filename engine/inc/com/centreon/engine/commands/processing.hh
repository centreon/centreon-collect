/*
** Copyright 2022 Centreon
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

#ifndef CCE_PROCESSING_HH
#define CCE_PROCESSING_HH

#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/hostgroup.hh"
#include "com/centreon/engine/servicegroup.hh"

namespace com::centreon::engine {

namespace commands {

namespace detail {
struct command_info {
  command_info(int _id = 0,
               int (*_func)(int, time_t, char*) = NULL,
               bool is_thread_safe = false)
      : id(_id), func(_func), thread_safe(is_thread_safe) {}
  ~command_info() throw() {}
  int id;
  int (*func)(int id, time_t entry_time, char* args);
  bool thread_safe;
};

};  // namespace detail

class processing {
 public:
  static bool execute(std::string const& cmd);
  static int execute_with_status(const std::string& cmd, std::string* error);
  static bool is_thread_safe(char const* cmd);

  static void wrapper_enable_host_and_child_notifications(host* hst);
  static void wrapper_disable_host_and_child_notifications(host* hst);

 private:
  static void _wrapper_read_state_information();
  static void _wrapper_save_state_information();
  static void _wrapper_enable_all_notifications_beyond_host(host* hst);
  static void _wrapper_disable_all_notifications_beyond_host(host* hst);
  static void _wrapper_enable_host_svc_notifications(host* hst);
  static void _wrapper_disable_host_svc_notifications(host* hst);
  static void _wrapper_enable_host_svc_checks(host* hst);
  static void _wrapper_disable_host_svc_checks(host* hst);
  static void _wrapper_set_host_notification_number(host* hst, char* args);
  static void _wrapper_send_custom_host_notification(host* hst, char* args);
  static void _wrapper_enable_service_notifications(host* hst);
  static void _wrapper_disable_service_notifications(host* hst);
  static void _wrapper_enable_service_checks(host* hst);
  static void _wrapper_disable_service_checks(host* hst);
  static void _wrapper_enable_passive_service_checks(host* hst);
  static void _wrapper_disable_passive_service_checks(host* hst);
  static void _wrapper_set_service_notification_number(service* svc,
                                                       char* args);
  static void _wrapper_send_custom_service_notification(service* svc,
                                                        char* args);

  static void change_anomaly_detection_sensitivity(anomalydetection* ano,
                                                   char* args);

  template <void (*fptr)()>
  static int _redirector(int id, time_t entry_time, char* args) {
    (void)id;
    (void)entry_time;
    (void)args;
    (*fptr)();
    return 0;
  }

  template <int (*fptr)()>
  static int _redirector(int id, time_t entry_time, char* args) {
    (void)id;
    (void)entry_time;
    (void)args;
    return (*fptr)();
  }

  template <void (*fptr)(int, char*)>
  static int _redirector(int id, time_t entry_time, char* args) {
    (void)entry_time;
    (*fptr)(id, args);
    return 0;
  }

  template <int (*fptr)(int, char*)>
  static int _redirector(int id, time_t entry_time, char* args) {
    (void)entry_time;
    return (*fptr)(id, args);
  }

  template <int (*fptr)(int, time_t, char*)>
  static int _redirector(int id, time_t entry_time, char* args) {
    return (*fptr)(id, entry_time, args);
  }

  template <void (*fptr)(host*)>
  static int _redirector_host(int id, time_t entry_time, char* args);

  template <void (*fptr)(host*, char*)>
  static int _redirector_host(int id, time_t entry_time, char* args);

  template <void (*fptr)(host*)>
  static int _redirector_hostgroup(int id, time_t entry_time, char* args);

  template <void (*fptr)(service*)>
  static int _redirector_service(int id, time_t entry_time, char* args);

  template <void (*fptr)(service*, char*)>
  static int _redirector_service(int id, time_t entry_time, char* args);

  template <void (*fptr)(service*)>
  static int _redirector_servicegroup(int id, time_t entry_time, char* args);

  template <void (*fptr)(host*)>
  static int _redirector_servicegroup(int id, time_t entry_time, char* args);

  template <void (*fptr)(contact*)>
  static int _redirector_contact(int id, time_t entry_time, char* args);

  template <void (*fptr)(char*)>
  static int _redirector_file(int id __attribute__((unused)),
                               time_t entry_time __attribute__((unused)),
                               char* args);

  template <void (*fptr)(contact*)>
  static int _redirector_contactgroup(int id, time_t entry_time, char* args);

  template <void (*fptr)(anomalydetection*, char*)>
  static int _redirector_anomalydetection(int id,
                                           time_t entry_time,
                                           char* args);

  static const std::unordered_map<std::string, detail::command_info>
      _lst_command;
};
}  // namespace commands

}  // namespace com::centreon::engine

#endif  // !CCE_MOD_EXTCMD_PROCESSING_HH
