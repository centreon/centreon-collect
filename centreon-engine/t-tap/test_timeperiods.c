/*****************************************************************************
 *
 * test_timeperiod.c - Test timeperiod
 *
 * Program: Nagios Core Testing
 * License: GPL
 * Copyright (c) 2009 Nagios Core Development Team and Community Contributors
 * Copyright (c) 1999-2009 Ethan Galstad
 *
 * First Written:   10-08-2009, based on nagios.c
 * Last Modified:   10-08-2009
 *
 * Description:
 *
 * Tests Nagios configuration loading
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/comments.h"
#include "../include/downtime.h"
#include "../include/statusdata.h"
#include "../include/macros.h"
#include "../include/nagios.h"
#include "../include/sretention.h"
#include "../include/perfdata.h"
#include "../include/broker.h"
#include "../include/nebmods.h"
#include "../include/nebmodules.h"
#include "tap.h"

char		*config_file=NULL;
char		*log_file=NULL;
char            *command_file=NULL;
char            *temp_file=NULL;
char            *temp_path=NULL;
char            *check_result_path=NULL;
char            *lock_file=NULL;
char            *log_archive_path=NULL;
char            *p1_file=NULL;    /**** EMBEDDED PERL ****/
char            *auth_file=NULL;  /**** EMBEDDED PERL INTERPRETER AUTH FILE ****/
char            *nagios_user=NULL;
char            *nagios_group=NULL;

extern char     *macro_x[MACRO_X_COUNT];

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

int             daemon_mode=FALSE;
int             daemon_dumps_core=TRUE;

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
unsigned long   update_uid=0L;


extern contact	       *contact_list;
extern contactgroup    *contactgroup_list;
extern hostgroup       *hostgroup_list;
extern command         *command_list;
extern timeperiod      *timeperiod_list;
extern serviceescalation *serviceescalation_list;
extern host 		*host_list;

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


/* Dummy variables */
sched_info scheduling_info;
timed_event event_list_low;
timed_event event_list_high;

/* Dummy functions */
void logit(int data_type, int display, const char *fmt, ...) {}
int my_sendall(int s, char *buf, int *len, int timeout) {}
void free_comment_data(void) {}
int write_to_log(char *buffer, unsigned long data_type, time_t *timestamp) {}
int log_debug_info(int level,int verbosity,const char *fmt, ...) {}

int neb_free_callback_list(void) {}
void broker_program_status(int type, int flags, int attr, struct timeval *timestamp){}
int neb_deinit_modules(void) {}
void broker_program_state(int type, int flags, int attr, struct timeval *timestamp){}
void broker_comment_data(int type, int flags, int attr, int comment_type, int entry_type, char *host_name, char *svc_description, time_t entry_time, char *author_name, char *comment_data, int persistent, int source, int expires, time_t expire_time, unsigned long comment_id, struct timeval *timestamp){}
int neb_unload_all_modules(int flags, int reason){}
int neb_add_module(char *filename,char *args,int should_be_loaded){}
void broker_system_command(int type, int flags, int attr, struct timeval start_time, struct timeval end_time, double exectime, int timeout, int early_timeout, int retcode, char *cmd, char *output, struct timeval *timestamp){}

int schedule_new_event(int event_type, int high_priority, time_t run_time, int recurring, unsigned long event_interval, void *timing_func, int compensate_for_time_change, void *event_data, void *event_args, int event_options){}
int my_tcp_connect(char *host_name, int port, int *sd, int timeout){}
int my_recvall(int s, char *buf, int *len, int timeout){}
int neb_free_module_list(void){}

int main(int argc, char **argv){
	int result;
	int error=FALSE;
	char *buffer=NULL;
	int display_license=FALSE;
	int display_help=FALSE;
	int c=0;
	struct tm *tm;
	time_t current_time;
	time_t test_time;
	time_t saved_test_time;
	time_t next_valid_time=0L;
	time_t chosen_valid_time=0L;
	char datestring[256];
	host *temp_host=NULL;
	hostgroup *temp_hostgroup=NULL;
	hostsmember *temp_member=NULL;
	timeperiod *temp_timeperiod=NULL;
	int is_valid_time=0;
	int iterations=1000;

	plan_tests(6043);

	/* reset program variables */
	reset_variables();

	printf("Reading configuration data...\n");

	config_file=strdup("smallconfig/nagios.cfg");
	/* read in the configuration files (main config file, resource and object config files) */
	result=read_main_config_file(config_file);
	ok(result==OK, "Read main configuration file okay - if fails, use nagios -v to check");

	result=read_all_object_data(config_file);
	ok(result==OK, "Read all object config files");

	result=pre_flight_check();
	ok(result==OK, "Preflight check okay");

	time(&current_time);
	test_time = current_time;
	saved_test_time = current_time;

	temp_timeperiod = find_timeperiod("none");

	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==ERROR, "No valid time because time period is empty");

	get_next_valid_time( current_time, &next_valid_time, temp_timeperiod);
	ok( current_time == next_valid_time, "There is no valid time due to timeperiod" );

	temp_timeperiod = find_timeperiod("24x7");

	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==OK, "Fine because 24x7" );

	get_next_valid_time( current_time, &next_valid_time, temp_timeperiod);
	ok( current_time == next_valid_time, "Current time should be the next valid time");


	/* 2009-10-25 is the day when clocks go back an hour in Europe. Bug happens during 23:00 to 00:00 */
	/* This is 23:01:01 */
	saved_test_time = 1256511661;
	saved_test_time = saved_test_time-(24*60*60);

	putenv("TZ=UTC");
	tzset();
	test_time = saved_test_time;
	c=0;
	while( c < iterations ) {
		is_valid_time = check_time_against_period(test_time, temp_timeperiod);
		ok( is_valid_time==OK, "Always OK for 24x7 with TZ=UTC, time_t=%lu", test_time );
		chosen_valid_time=0L;
		_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
		ok( test_time==chosen_valid_time, "get_next_valid_time always returns same time" );
		test_time+=1800;
		c++;
	}

	putenv("TZ=Europe/London");
	tzset();
	test_time = saved_test_time;
	c=0;
	while( c < iterations ) {
		is_valid_time = check_time_against_period(test_time, temp_timeperiod);
		ok( is_valid_time==OK, "Always OK for 24x7 with TZ=Europe/London, time_t=%lu", test_time );
		_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
		ok( test_time==chosen_valid_time, "get_next_valid_time always returns same time, time_t=%lu", test_time );
		test_time+=1800;
		c++;
	}

	/* 2009-11-01 is the day when clocks go back an hour in America. Bug happens during 23:00 to 00:00 */
	/* This is 23:01:01 */
	saved_test_time = 1256511661;
	saved_test_time = saved_test_time-(24*60*60);

	putenv("TZ=America/New_York");
	tzset();
	test_time = saved_test_time;
	c=0;
	while( c < iterations ) {
		is_valid_time = check_time_against_period(test_time, temp_timeperiod);
		ok( is_valid_time==OK, "Always OK for 24x7 with TZ=America/New_York, time_t=%lu", test_time );
		_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
		ok( test_time==chosen_valid_time, "get_next_valid_time always returns same time, time_t=%lu", test_time );
		test_time+=1800;
		c++;
	}



	/* Tests around clock change going back for TZ=Europe/London. 1256511661 = Sun Oct  
25 23:01:01 2009 */
	/* A little trip to Paris*/
	putenv("TZ=Europe/Paris");
        tzset();


	/* Timeperiod exclude tests, from Jean Gabes */
	temp_timeperiod = find_timeperiod("Test_exclude");
        ok(temp_timeperiod!=NULL, "ME: Testing Exclude timeperiod");
	test_time=1278939600;
	/* printf("Testing at time %s", ctime(&test_time)); */
        is_valid_time = check_time_against_period(test_time, temp_timeperiod);
        ok( is_valid_time==ERROR, "ME: 12 Jul 2010 15:00:00 - false" );

        _get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	/* printf("JEAN: Got chosent time at %s", ctime(&chosen_valid_time)); */
	todo_start("Bug in exclude");
        ok( chosen_valid_time==1288103400, "ME: Next valid time=Tue Oct 26 16:30:00 2010");
	todo_end();


	temp_timeperiod = find_timeperiod("Test_exclude2");
        ok(temp_timeperiod!=NULL, "ME: Testing Exclude timeperiod 2");
	test_time=1278939600;
	/* printf("Testing at time %s", ctime(&test_time)); */
        is_valid_time = check_time_against_period(test_time, temp_timeperiod);
        ok( is_valid_time==ERROR, "ME: 12 Jul 2010 15:00:00 - false" );
        _get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	/* printf("JEAN: Got chosent time at %s", ctime(&chosen_valid_time)); */
	todo_start("Bug in exclude 2");
        ok( chosen_valid_time==1279058340, "ME: Next valid time=Tue Jul 13 23:59:00 2010");
	todo_end();


	temp_timeperiod = find_timeperiod("Test_exclude3");
        ok(temp_timeperiod!=NULL, "ME: Testing Exclude timeperiod 3");
        test_time=1278939600;
        /* printf("Testing at time %s", ctime(&test_time)); */
        is_valid_time = check_time_against_period(test_time, temp_timeperiod);
        ok( is_valid_time==ERROR, "ME: 12 Jul 2010 15:00:00 - false" );
        _get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
        /* printf("JEAN: Got chosent time at %s", ctime(&chosen_valid_time)); */
	todo_start("Bug in exclude 3");
        ok( chosen_valid_time==1284474600, "ME: Next valid time=Tue Sep 14 16:30:00 2010");
	todo_end();


	temp_timeperiod = find_timeperiod("Test_exclude4");
        ok(temp_timeperiod!=NULL, "ME: Testing Exclude timeperiod 4");
        test_time=1278939600;
        /* printf("Testing at time %s", ctime(&test_time)); */
        is_valid_time = check_time_against_period(test_time, temp_timeperiod);
        ok( is_valid_time==ERROR, "ME: 12 Jul 2010 15:00:00 - false" );
        _get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
        /* printf("JEAN: Got chosent time at %s", ctime(&chosen_valid_time)); */
	todo_start("Bug in exclude 3");
        ok( chosen_valid_time==1283265000, "ME: Next valid time=Tue Aug 31 16:30:00 2010");
	todo_end();


	

	/* Back to New york */
	putenv("TZ=America/New_York");
        tzset();


	temp_timeperiod = find_timeperiod("sunday_only");
	ok(temp_timeperiod!=NULL, "Testing Sunday 00:00-01:15,03:15-22:00");
	putenv("TZ=Europe/London");
	tzset();


	test_time=1256421000;
	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==ERROR, "Sat Oct 24 22:50:00 2009 - false" );
	_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	ok( chosen_valid_time==1256425200, "Next valid time=Sun Oct 25 00:00:00 2009");


	test_time=1256421661;
	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==ERROR, "Sat Oct 24 23:01:01 2009 - false" );
	_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	ok( chosen_valid_time==1256425200, "Next valid time=Sun Oct 25 00:00:00 2009");

	test_time=1256425400;
	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==OK, "Sun Oct 25 00:03:20 2009 - true" );
	_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	ok( chosen_valid_time==test_time, "Next valid time=Sun Oct 25 00:03:20 2009");

	test_time=1256429700;
	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==OK, "Sun Oct 25 01:15:00 2009 - true" );
	_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	ok( chosen_valid_time==test_time, "Next valid time=Sun Oct 25 01:15:00 2009");

	test_time=1256430400;
	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==ERROR, "Sun Oct 25 01:26:40 2009 - false" );
	_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	todo_start("Is a bug in get_next_valid_time for a time that falls in the DST change hour period");
	ok( chosen_valid_time==1256440500, "Next valid time=Sun Oct 25 03:15:00 2009") || printf("chosen_valid_time=%lu\n", chosen_valid_time);
	todo_end();

	test_time=1256440500;
	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==OK, "Sun Oct 25 03:15:00 2009 - true");
	_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	ok( chosen_valid_time==test_time, "Next valid time=Sun Oct 25 03:15:00 2009");
	
	test_time=1256500000;
	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==OK, "Sun Oct 25 19:46:40 2009 - true" );
	_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	ok( chosen_valid_time==1256500000, "Next valid time=Sun Oct 25 19:46:40 2009");

	test_time=1256508000;
	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==OK, "Sun Oct 25 22:00:00 2009 - true" );
	_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	ok( chosen_valid_time==1256508000, "Next valid time=Sun Oct 25 22:00:00 2009");

	test_time=1256508001;
	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==ERROR, "Sun Oct 25 22:00:01 2009 - false" );
	_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	ok( chosen_valid_time==1257033600, "Next valid time=Sun Nov 1 00:00:00 2009");

	test_time=1256513000;
	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==ERROR, "Sun Oct 25 23:23:20 2009 - false" );
	_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	ok( chosen_valid_time==1257033600, "Next valid time=Sun Nov 1 00:00:00 2009");




	temp_timeperiod = find_timeperiod("weekly_complex");
	ok(temp_timeperiod!=NULL, "Testing complex weekly timeperiod definition");
	putenv("TZ=America/New_York");
	tzset();

	test_time=1268109420;
	is_valid_time = check_time_against_period(test_time, temp_timeperiod);
	ok( is_valid_time==ERROR, "Mon Mar  8 23:37:00 2010 - false");
	_get_next_valid_time(test_time, test_time, &chosen_valid_time, temp_timeperiod);
	ok( chosen_valid_time==1268115300, "Next valid time=Tue Mar  9 01:15:00 2010" );





	cleanup();

	my_free(config_file);

	return exit_status();
	}


