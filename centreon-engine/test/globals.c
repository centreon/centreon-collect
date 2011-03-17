#include <stdlib.h>
#include "nagios.h"
#include "broker.h"

char		*config_file=NULL;
char		*log_file=NULL;
char            *command_file=NULL;
char            *temp_file=NULL;
char            *temp_path=NULL;
char            *check_result_path=NULL;
char            *log_archive_path=NULL;
char            *p1_file=NULL;    /**** EMBEDDED PERL ****/
char            *auth_file=NULL;  /**** EMBEDDED PERL INTERPRETER AUTH FILE ****/

char            *global_host_event_handler=NULL;
char            *global_service_event_handler=NULL;
command         *global_host_event_handler_ptr=NULL;
command         *global_service_event_handler_ptr=NULL;

char            *ocsp_command=NULL;
char            *ochp_command=NULL;
command         *ocsp_command_ptr=NULL;
command         *ochp_command_ptr=NULL;

char            *illegal_object_chars=NULL;
char            *illegal_output_chars=NULL;

int             use_regexp_matches=FALSE;
int             use_true_regexp_matching=FALSE;

int		use_syslog=DEFAULT_USE_SYSLOG;
int             log_notifications=DEFAULT_NOTIFICATION_LOGGING;
int             log_service_retries=DEFAULT_LOG_SERVICE_RETRIES;
int             log_host_retries=DEFAULT_LOG_HOST_RETRIES;
int             log_event_handlers=DEFAULT_LOG_EVENT_HANDLERS;
int             log_initial_states=DEFAULT_LOG_INITIAL_STATES;
int             log_external_commands=DEFAULT_LOG_EXTERNAL_COMMANDS;
int             log_passive_checks=DEFAULT_LOG_PASSIVE_CHECKS;

unsigned long   logging_options=0;
unsigned long   syslog_options=0;

int             service_check_timeout=DEFAULT_SERVICE_CHECK_TIMEOUT;
int             host_check_timeout=DEFAULT_HOST_CHECK_TIMEOUT;
int             event_handler_timeout=DEFAULT_EVENT_HANDLER_TIMEOUT;
int             notification_timeout=DEFAULT_NOTIFICATION_TIMEOUT;
int             ocsp_timeout=DEFAULT_OCSP_TIMEOUT;
int             ochp_timeout=DEFAULT_OCHP_TIMEOUT;

double          sleep_time=DEFAULT_SLEEP_TIME;
int             interval_length=DEFAULT_INTERVAL_LENGTH;
int             service_inter_check_delay_method=ICD_SMART;
int             host_inter_check_delay_method=ICD_SMART;
int             service_interleave_factor_method=ILF_SMART;
int             max_host_check_spread=DEFAULT_HOST_CHECK_SPREAD;
int             max_service_check_spread=DEFAULT_SERVICE_CHECK_SPREAD;

int             command_check_interval=DEFAULT_COMMAND_CHECK_INTERVAL;
int             check_reaper_interval=DEFAULT_CHECK_REAPER_INTERVAL;
int             max_check_reaper_time=DEFAULT_MAX_REAPER_TIME;
int             service_freshness_check_interval=DEFAULT_FRESHNESS_CHECK_INTERVAL;
int             host_freshness_check_interval=DEFAULT_FRESHNESS_CHECK_INTERVAL;
int             auto_rescheduling_interval=DEFAULT_AUTO_RESCHEDULING_INTERVAL;

int             check_external_commands=DEFAULT_CHECK_EXTERNAL_COMMANDS;
int             check_orphaned_services=DEFAULT_CHECK_ORPHANED_SERVICES;
int             check_orphaned_hosts=DEFAULT_CHECK_ORPHANED_HOSTS;
int             check_service_freshness=DEFAULT_CHECK_SERVICE_FRESHNESS;
int             check_host_freshness=DEFAULT_CHECK_HOST_FRESHNESS;
int             auto_reschedule_checks=DEFAULT_AUTO_RESCHEDULE_CHECKS;
int             auto_rescheduling_window=DEFAULT_AUTO_RESCHEDULING_WINDOW;

int             additional_freshness_latency=DEFAULT_ADDITIONAL_FRESHNESS_LATENCY;

int             check_for_updates=DEFAULT_CHECK_FOR_UPDATES;
int             bare_update_check=DEFAULT_BARE_UPDATE_CHECK;
time_t          last_update_check=0L;
unsigned long   update_uid=0L;
int             update_available=FALSE;
char            *last_program_version=NULL;
char            *new_program_version=NULL;

time_t          last_command_check=0L;
time_t          last_command_status_update=0L;
time_t          last_log_rotation=0L;

int             use_aggressive_host_checking=DEFAULT_AGGRESSIVE_HOST_CHECKING;
unsigned long   cached_host_check_horizon=DEFAULT_CACHED_HOST_CHECK_HORIZON;
unsigned long   cached_service_check_horizon=DEFAULT_CACHED_SERVICE_CHECK_HORIZON;
int             enable_predictive_host_dependency_checks=DEFAULT_ENABLE_PREDICTIVE_HOST_DEPENDENCY_CHECKS;
int             enable_predictive_service_dependency_checks=DEFAULT_ENABLE_PREDICTIVE_SERVICE_DEPENDENCY_CHECKS;

int             soft_state_dependencies=FALSE;

int             retain_state_information=FALSE;
int             retention_update_interval=DEFAULT_RETENTION_UPDATE_INTERVAL;
int             use_retained_program_state=TRUE;
int             use_retained_scheduling_info=FALSE;
int             retention_scheduling_horizon=DEFAULT_RETENTION_SCHEDULING_HORIZON;
unsigned long   modified_host_process_attributes=MODATTR_NONE;
unsigned long   modified_service_process_attributes=MODATTR_NONE;
unsigned long   retained_host_attribute_mask=0L;
unsigned long   retained_service_attribute_mask=0L;
unsigned long   retained_contact_host_attribute_mask=0L;
unsigned long   retained_contact_service_attribute_mask=0L;
unsigned long   retained_process_host_attribute_mask=0L;
unsigned long   retained_process_service_attribute_mask=0L;

unsigned long   next_comment_id=0L;
unsigned long   next_downtime_id=0L;
unsigned long   next_event_id=0L;
unsigned long   next_problem_id=0L;
unsigned long   next_notification_id=0L;

int             log_rotation_method=LOG_ROTATION_NONE;

int             sigshutdown=FALSE;
int             sigrestart=FALSE;
char            *sigs[35]={"EXIT","HUP","INT","QUIT","ILL","TRAP","ABRT","BUS","FPE","KILL","USR1","SEGV","USR2","PIPE","ALRM","TERM","STKFLT","CHLD","CONT","STOP","TSTP","TTIN","TTOU","URG","XCPU","XFSZ","VTALRM","PROF","WINCH","IO","PWR","UNUSED","ZERR","DEBUG",(char *)NULL};
int             caught_signal=FALSE;
int             sig_id=0;

int             restarting=FALSE;

int             verify_config=FALSE;
int             verify_object_relationships=TRUE;
int             verify_circular_paths=TRUE;
int             test_scheduling=FALSE;
int             precache_objects=FALSE;
int             use_precached_objects=FALSE;

int             max_parallel_service_checks=DEFAULT_MAX_PARALLEL_SERVICE_CHECKS;
int             currently_running_service_checks=0;
int             currently_running_host_checks=0;

time_t          program_start=0L;
time_t          event_start=0L;
int             nagios_pid=0;
int             enable_notifications=TRUE;
int             execute_service_checks=TRUE;
int             accept_passive_service_checks=TRUE;
int             execute_host_checks=TRUE;
int             accept_passive_host_checks=TRUE;
int             enable_event_handlers=TRUE;
int             obsess_over_services=FALSE;
int             obsess_over_hosts=FALSE;
int             enable_failure_prediction=TRUE;

int             translate_passive_host_checks=DEFAULT_TRANSLATE_PASSIVE_HOST_CHECKS;
int             passive_host_checks_are_soft=DEFAULT_PASSIVE_HOST_CHECKS_SOFT;

int             aggregate_status_updates=TRUE;
int             status_update_interval=DEFAULT_STATUS_UPDATE_INTERVAL;

int             time_change_threshold=DEFAULT_TIME_CHANGE_THRESHOLD;

unsigned long   event_broker_options=BROKER_NOTHING;

int             process_performance_data=DEFAULT_PROCESS_PERFORMANCE_DATA;

int             enable_flap_detection=DEFAULT_ENABLE_FLAP_DETECTION;

double          low_service_flap_threshold=DEFAULT_LOW_SERVICE_FLAP_THRESHOLD;
double          high_service_flap_threshold=DEFAULT_HIGH_SERVICE_FLAP_THRESHOLD;
double          low_host_flap_threshold=DEFAULT_LOW_HOST_FLAP_THRESHOLD;
double          high_host_flap_threshold=DEFAULT_HIGH_HOST_FLAP_THRESHOLD;

int             use_large_installation_tweaks=DEFAULT_USE_LARGE_INSTALLATION_TWEAKS;
int             enable_environment_macros=TRUE;
int             free_child_process_memory=-1;
int             child_processes_fork_twice=-1;

int             enable_embedded_perl=DEFAULT_ENABLE_EMBEDDED_PERL;
int             use_embedded_perl_implicitly=DEFAULT_USE_EMBEDDED_PERL_IMPLICITLY;
int             embedded_perl_initialized=FALSE;

int             date_format=DATE_FORMAT_US;
char            *use_timezone=NULL;

int             command_file_fd;
FILE            *command_file_fp;
int             command_file_created=FALSE;

notification    *notification_list;

check_result    check_result_info;
check_result    *check_result_list=NULL;
unsigned long	max_check_result_file_age=DEFAULT_MAX_CHECK_RESULT_AGE;

dbuf            check_result_dbuf;

circular_buffer external_command_buffer;
circular_buffer check_result_buffer;
pthread_t       worker_threads[TOTAL_WORKER_THREADS];
int             external_command_buffer_slots=DEFAULT_EXTERNAL_COMMAND_BUFFER_SLOTS;

check_stats     check_statistics[MAX_CHECK_STATS_TYPES];

char            *debug_file;
int             debug_level=DEFAULT_DEBUG_LEVEL;
int             debug_verbosity=DEFAULT_DEBUG_VERBOSITY;
unsigned long   max_debug_file_size=DEFAULT_MAX_DEBUG_FILE_SIZE;
