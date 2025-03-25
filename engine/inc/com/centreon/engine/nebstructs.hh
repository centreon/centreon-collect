/*
** Copyright 2003-2007 Ethan Galstad
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

#ifndef CCE_NEBSTRUCTS_HH
#define CCE_NEBSTRUCTS_HH

#include <string>
#include "bbdo/neb.pb.h"
#include "com/centreon/engine/comment.hh"

/* Acknowledgement structure. */
typedef struct nebstruct_acknowledgement_struct {
  int type;
  acknowledgement_resource_type acknowledgement_type;
  uint64_t host_id;
  uint64_t service_id;
  int state;
  const char* author_name;
  const char* comment_data;
  int is_sticky;
  int persistent_comment;
  int notify_contacts;
} nebstruct_acknowledgement_data;

/* Adaptive severity data structure. */
typedef struct nebstruct_adaptive_severity_data_struct {
  int type;
  void* object_ptr;
} nebstruct_adaptive_severity_data;

/* Adaptive tag data structure. */
typedef struct nebstruct_adaptive_tag_data_struct {
  int type;
  void* object_ptr;
} nebstruct_adaptive_tag_data;

/* Adaptive dependency data structure. */
typedef struct nebstruct_adaptive_dependency_data_struct {
  int type;
  void* object_ptr;
} nebstruct_adaptive_dependency_data;

/* Adaptive host data structure. */
typedef struct nebstruct_adaptive_host_data_struct {
  int type;
  int flags;
  int attr;
  unsigned long modified_attribute;
  void* object_ptr;
} nebstruct_adaptive_host_data;

/* Adaptive service data structure. */
typedef struct nebstruct_adaptive_service_data_struct {
  int type;
  int flags;
  int attr;
  unsigned long modified_attribute;
  void* object_ptr;
} nebstruct_adaptive_service_data;

/* Comment data structure. */
typedef struct nebstruct_comment_struct {
  int type;
  com::centreon::engine::comment::type comment_type;
  uint64_t host_id;
  uint64_t service_id;
  time_t entry_time;
  char const* author_name;
  char const* comment_data;
  int persistent;
  com::centreon::engine::comment::src source;
  com::centreon::engine::comment::e_type entry_type;
  int expires;
  time_t expire_time;
  unsigned long comment_id;
} nebstruct_comment_data;

/* Custom variable structure. */
typedef struct nebstruct_custom_variable_struct {
  int type;
  struct timeval timestamp = {};
  std::string_view var_name;
  std::string_view var_value;
  void* object_ptr = nullptr;
} nebstruct_custom_variable_data;

/* Downtime data structure. */
typedef struct nebstruct_downtime_struct {
  int type;
  int attr;
  struct timeval timestamp;
  int downtime_type;
  uint64_t host_id;
  uint64_t service_id;
  time_t entry_time;
  char const* author_name;
  char const* comment_data;
  time_t start_time;
  time_t end_time;
  int fixed;
  unsigned long duration;
  unsigned long triggered_by;
  unsigned long downtime_id;
} nebstruct_downtime_data;

/* Event handler structure. */
typedef struct nebstruct_event_handler_struct {
  int type;
  int eventhandler_type;
  char* host_name;
  char* service_description;
  int state_type;
  int state;
  int timeout;
  std::string command_name;
  std::string command_args;
  char* command_line;
  struct timeval start_time;
  struct timeval end_time;
  int early_timeout;
  double execution_time;
  int return_code;
  char* output;
} nebstruct_event_handler_data;

/* External command data structure. */
typedef struct nebstruct_external_command_struct {
  int type;
  struct timeval timestamp;
  int command_type;
  char* command_args;
} nebstruct_external_command_data;

/* Flapping data structure. */
typedef struct nebstruct_flapping_struct {
  int type;
  struct timeval timestamp;
  int flapping_type;
  uint64_t host_id;
  uint64_t service_id;
  double percent_change;
  double high_threshold;
  double low_threshold;
} nebstruct_flapping_data;

/* Group structure. */
typedef struct nebstruct_group_struct {
  int type;
  void* object_ptr;
} nebstruct_group_data;

/* Group member structure. */
typedef struct nebstruct_group_member_struct {
  int type;
  void* object_ptr;
  void* group_ptr;
} nebstruct_group_member_data;

/* Host check structure. */
typedef struct nebstruct_host_check_struct {
  int type;
  char* host_name;
  int check_type;
  const char* command_line;
  char* output;
  void* object_ptr;
} nebstruct_host_check_data;

/* Host status structure. */
typedef struct nebstruct_host_status_struct {
  int type;
  void* object_ptr;
  uint32_t attributes;
} nebstruct_host_status_data;

/* Log data structure. */
typedef struct nebstruct_log_struct {
  time_t entry_time;
  const char* data;
} nebstruct_log_data;

/* Module data structure. */
typedef struct nebstruct_module_struct {
  int type;
  char* module;
  char* args;
} nebstruct_module_data;

/* Process data structure. */
typedef struct nebstruct_process_struct {
  int type;
  int flags;
} nebstruct_process_data;

/* Program status structure. */
typedef struct nebstruct_program_status_struct {
  time_t last_command_check;
  int notifications_enabled;
  int active_service_checks_enabled;
  int passive_service_checks_enabled;
  int active_host_checks_enabled;
  int passive_host_checks_enabled;
  int event_handlers_enabled;
  int flap_detection_enabled;
  int obsess_over_hosts;
  int obsess_over_services;
  std::string global_host_event_handler;
  std::string global_service_event_handler;
} nebstruct_program_status_data;

/* Relation data structure. */
typedef struct nebstruct_relation_struct {
  int type;
  com::centreon::engine::host* hst;
  com::centreon::engine::service* svc;
  com::centreon::engine::host* dep_hst;
  com::centreon::engine::service* dep_svc;
} nebstruct_relation_data;

/* Service check structure. */
typedef struct nebstruct_service_check_struct {
  int type;
  uint64_t host_id;
  uint64_t service_id;
  int check_type;
  const char* command_line;
  char* output;
  void* object_ptr;
} nebstruct_service_check_data;

/* Service status structure. */
typedef struct nebstruct_service_status_struct {
  int type;
  void* object_ptr;
  uint32_t attributes;
} nebstruct_service_status_data;

typedef struct nebstruct_bench_struct {
  unsigned id;
  std::chrono::system_clock::time_point mess_create;
} nebstruct_bench_data;

struct nebstruct_agent_stats_data {
  struct cumul_data {
    cumul_data(unsigned maj,
               unsigned min,
               unsigned pat,
               bool rev,
               const std::string& operating_system,
               const std::string& os_ver,
               size_t nb_ag)
        : major(maj),
          minor(min),
          patch(pat),
          reverse(rev),
          os(operating_system),
          os_version(os_ver),
          nb_agent(nb_ag) {}

    unsigned major;
    unsigned minor;
    unsigned patch;
    bool reverse;
    std::string os;
    std::string os_version;
    size_t nb_agent;
  };

  std::unique_ptr<std::vector<cumul_data>> data;
};

#endif /* !CCE_NEBSTRUCTS_HH */
