/*****************************************************************************
 *
 * XSDDEFAULT.C - Default external status data input routines for Nagios
 *
 * Copyright (c) 2009 Nagios Core Development Team and Community Contributors
 * Copyright (c) 2000-2009 Ethan Galstad (egalstad@nagios.org)
 * Last Modified: 07-31-2009
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


/*********** COMMON HEADER FILES ***********/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/locations.h"
#include "../include/statusdata.h"
#include "../include/comments.h"
#include "../include/downtime.h"
#include "../include/macros.h"
#include "../include/skiplist.h"

#include "../include/nagios.h"

/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/
#include "xsddefault.h"



extern time_t program_start;
extern int nagios_pid;
extern time_t last_command_check;
extern time_t last_log_rotation;
extern int enable_notifications;
extern int execute_service_checks;
extern int accept_passive_service_checks;
extern int execute_host_checks;
extern int accept_passive_host_checks;
extern int enable_event_handlers;
extern int obsess_over_services;
extern int obsess_over_hosts;
extern int check_service_freshness;
extern int check_host_freshness;
extern int enable_flap_detection;
extern int enable_failure_prediction;
extern int process_performance_data;
extern int aggregate_status_updates;
extern int check_external_commands;

extern time_t         last_update_check;
extern char           *last_program_version;
extern int            update_available;
extern char           *last_program_version;
extern char           *new_program_version;

extern int external_command_buffer_slots;
extern circular_buffer external_command_buffer;

extern char *macro_x[MACRO_X_COUNT];

extern host *host_list;
extern service *service_list;
extern contact *contact_list;
extern comment *comment_list;
extern scheduled_downtime *scheduled_downtime_list;

extern skiplist *object_skiplists[NUM_OBJECT_SKIPLISTS];

extern unsigned long  next_comment_id;
extern unsigned long  next_downtime_id;
extern unsigned long  next_event_id;
extern unsigned long  next_problem_id;
extern unsigned long  next_notification_id;

extern unsigned long  modified_host_process_attributes;
extern unsigned long  modified_service_process_attributes;
extern char           *global_host_event_handler;
extern char           *global_service_event_handler;

extern check_stats    check_statistics[MAX_CHECK_STATS_TYPES];


char *xsddefault_status_log=NULL;
char *xsddefault_temp_file=NULL;



/******************************************************************/
/***************** COMMON CONFIG INITIALIZATION  ******************/
/******************************************************************/

/* grab configuration information */
int xsddefault_grab_config_info(char *config_file){
	char *input=NULL;
	mmapfile *thefile;


	/*** CORE PASSES IN MAIN CONFIG FILE, CGIS PASS IN CGI CONFIG FILE! ***/

	/* open the config file for reading */
	if((thefile=mmap_fopen(config_file))==NULL)
		return ERROR;

	/* read in all lines from the main config file */
	while(1){

		/* free memory */
		my_free(input);

		/* read the next line */
		if((input=mmap_fgets_multiline(thefile))==NULL)
			break;

		strip(input);

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]=='\x0')
			continue;

		/* core reads variables directly from the main config file */
		xsddefault_grab_config_directives(input);
	        }

	/* free memory and close the file */
	my_free(input);
	mmap_fclose(thefile);

	/* initialize locations if necessary */
	if(xsddefault_status_log==NULL)
		xsddefault_status_log=(char *)strdup(DEFAULT_STATUS_FILE);
	if(xsddefault_temp_file==NULL)
		xsddefault_temp_file=(char *)strdup(DEFAULT_TEMP_FILE);

	/* make sure we have what we need */
	if(xsddefault_status_log==NULL)
		return ERROR;
	if(xsddefault_temp_file==NULL)
		return ERROR;

	/* save the status file macro */
	my_free(macro_x[MACRO_STATUSDATAFILE]);
	if((macro_x[MACRO_STATUSDATAFILE]=(char *)strdup(xsddefault_status_log)))
		strip(macro_x[MACRO_STATUSDATAFILE]);

	return OK;
        }


/* processes a single directive */
int xsddefault_grab_config_directives(char *input){
	char *temp_ptr=NULL;
	char *varname=NULL;
	char *varvalue=NULL;

	/* get the variable name */
	if((temp_ptr=my_strtok(input,"="))==NULL)
		return ERROR;
	if((varname=(char *)strdup(temp_ptr))==NULL)
		return ERROR;

	/* get the variable value */
	if((temp_ptr=my_strtok(NULL,"\n"))==NULL){
		my_free(varname);
		return ERROR;
	        }
	if((varvalue=(char *)strdup(temp_ptr))==NULL){
		my_free(varname);
		return ERROR;
	        }

	/* status log definition */
	if(!strcmp(varname,"status_file") || !strcmp(varname,"xsddefault_status_log"))
		xsddefault_status_log=(char *)strdup(temp_ptr);

	/* temp file definition */
	else if(!strcmp(varname,"temp_file"))
		xsddefault_temp_file=(char *)strdup(temp_ptr);

	/* free memory */
	my_free(varname);
	my_free(varvalue);

	return OK;
        }



/******************************************************************/
/********************* INIT/CLEANUP FUNCTIONS *********************/
/******************************************************************/


/* initialize status data */
int xsddefault_initialize_status_data(char *config_file){
	int result;

	/* grab configuration data */
	result=xsddefault_grab_config_info(config_file);
	if(result==ERROR)
		return ERROR;

	/* delete the old status log (it might not exist) */
	if(xsddefault_status_log)
		unlink(xsddefault_status_log);

	return OK;
        }


/* cleanup status data before terminating */
int xsddefault_cleanup_status_data(char *config_file, int delete_status_data){

	/* delete the status log */
	if(delete_status_data==TRUE && xsddefault_status_log){
		if(unlink(xsddefault_status_log))
			return ERROR;
	        }

	/* free memory */
	my_free(xsddefault_status_log);
	my_free(xsddefault_temp_file);

	return OK;
        }


/******************************************************************/
/****************** STATUS DATA OUTPUT FUNCTIONS ******************/
/******************************************************************/

/* write all status data to file */
int xsddefault_save_status_data(void){
	char *temp_file=NULL;
	customvariablesmember *temp_customvariablesmember=NULL;
	host *temp_host=NULL;
	service *temp_service=NULL;
	contact *temp_contact=NULL;
	comment *temp_comment=NULL;
	scheduled_downtime *temp_downtime=NULL;
	time_t current_time;
	int fd=0;
	FILE *fp=NULL;
	int used_external_command_buffer_slots=0;
	int high_external_command_buffer_slots=0;
	int result=OK;

	log_debug_info(DEBUGL_FUNCTIONS,0,"save_status_data()\n");

	/* open a safe temp file for output */
	if(xsddefault_temp_file==NULL)
		return ERROR;
	asprintf(&temp_file,"%sXXXXXX",xsddefault_temp_file);
	if(temp_file==NULL)
		return ERROR;

	log_debug_info(DEBUGL_STATUSDATA,2,"Writing status data to temp file '%s'\n",temp_file);

	if((fd=mkstemp(temp_file))==-1){

		/* log an error */
		logit(NSLOG_RUNTIME_ERROR,TRUE,"Error: Unable to create temp file for writing status data!\n");

		/* free memory */
		my_free(temp_file);

		return ERROR;
	        }
	fp=(FILE *)fdopen(fd,"w");
	if(fp==NULL){

		close(fd);
		unlink(temp_file);

		/* log an error */
		logit(NSLOG_RUNTIME_ERROR,TRUE,"Error: Unable to open temp file '%s' for writing status data!\n",temp_file);

		/* free memory */
		my_free(temp_file);

		return ERROR;
	        }

	/* get number of items in the command buffer */
	if(check_external_commands==TRUE){
		pthread_mutex_lock(&external_command_buffer.buffer_lock);
		used_external_command_buffer_slots=external_command_buffer.items;
		high_external_command_buffer_slots=external_command_buffer.high;
		pthread_mutex_unlock(&external_command_buffer.buffer_lock);
		}
	else{
		used_external_command_buffer_slots=0;
		high_external_command_buffer_slots=0;
		}

	/* generate check statistics */
	generate_check_stats();

	/* write version info to status file */
	fprintf(fp,"########################################\n");
	fprintf(fp,"#          NAGIOS STATUS FILE\n");
	fprintf(fp,"#\n");
	fprintf(fp,"# THIS FILE IS AUTOMATICALLY GENERATED\n");
	fprintf(fp,"# BY NAGIOS.  DO NOT MODIFY THIS FILE!\n");
	fprintf(fp,"########################################\n\n");

	time(&current_time);

	/* write file info */
	fprintf(fp,"info {\n");
	fprintf(fp,"\tcreated=%lu\n",current_time);
	fprintf(fp,"\tversion=%s\n",PROGRAM_VERSION);
	fprintf(fp,"\tlast_update_check=%lu\n",last_update_check);
	fprintf(fp,"\tupdate_available=%d\n",update_available);
	fprintf(fp,"\tlast_version=%s\n",(last_program_version==NULL)?"":last_program_version);
	fprintf(fp,"\tnew_version=%s\n",(new_program_version==NULL)?"":new_program_version);
	fprintf(fp,"\t}\n\n");

	/* save program status data */
	fprintf(fp,"programstatus {\n");
	fprintf(fp,"\tmodified_host_attributes=%lu\n",modified_host_process_attributes);
	fprintf(fp,"\tmodified_service_attributes=%lu\n",modified_service_process_attributes);
	fprintf(fp,"\tnagios_pid=%d\n",nagios_pid);
	fprintf(fp,"\tprogram_start=%lu\n",program_start);
	fprintf(fp,"\tlast_command_check=%lu\n",last_command_check);
	fprintf(fp,"\tlast_log_rotation=%lu\n",last_log_rotation);
	fprintf(fp,"\tenable_notifications=%d\n",enable_notifications);
	fprintf(fp,"\tactive_service_checks_enabled=%d\n",execute_service_checks);
	fprintf(fp,"\tpassive_service_checks_enabled=%d\n",accept_passive_service_checks);
	fprintf(fp,"\tactive_host_checks_enabled=%d\n",execute_host_checks);
	fprintf(fp,"\tpassive_host_checks_enabled=%d\n",accept_passive_host_checks);
	fprintf(fp,"\tenable_event_handlers=%d\n",enable_event_handlers);
	fprintf(fp,"\tobsess_over_services=%d\n",obsess_over_services);
	fprintf(fp,"\tobsess_over_hosts=%d\n",obsess_over_hosts);
	fprintf(fp,"\tcheck_service_freshness=%d\n",check_service_freshness);
	fprintf(fp,"\tcheck_host_freshness=%d\n",check_host_freshness);
	fprintf(fp,"\tenable_flap_detection=%d\n",enable_flap_detection);
	fprintf(fp,"\tenable_failure_prediction=%d\n",enable_failure_prediction);
	fprintf(fp,"\tprocess_performance_data=%d\n",process_performance_data);
	fprintf(fp,"\tglobal_host_event_handler=%s\n",(global_host_event_handler==NULL)?"":global_host_event_handler);
	fprintf(fp,"\tglobal_service_event_handler=%s\n",(global_service_event_handler==NULL)?"":global_service_event_handler);
	fprintf(fp,"\tnext_comment_id=%lu\n",next_comment_id);
	fprintf(fp,"\tnext_downtime_id=%lu\n",next_downtime_id);
	fprintf(fp,"\tnext_event_id=%lu\n",next_event_id);
	fprintf(fp,"\tnext_problem_id=%lu\n",next_problem_id);
	fprintf(fp,"\tnext_notification_id=%lu\n",next_notification_id);
	fprintf(fp,"\ttotal_external_command_buffer_slots=%d\n",external_command_buffer_slots);
	fprintf(fp,"\tused_external_command_buffer_slots=%d\n",used_external_command_buffer_slots);
	fprintf(fp,"\thigh_external_command_buffer_slots=%d\n",high_external_command_buffer_slots);
	fprintf(fp,"\tactive_scheduled_host_check_stats=%d,%d,%d\n",check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[0],check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[1],check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp,"\tactive_ondemand_host_check_stats=%d,%d,%d\n",check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[0],check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[1],check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp,"\tpassive_host_check_stats=%d,%d,%d\n",check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[0],check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[1],check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp,"\tactive_scheduled_service_check_stats=%d,%d,%d\n",check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[0],check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[1],check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[2]);
	fprintf(fp,"\tactive_ondemand_service_check_stats=%d,%d,%d\n",check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[0],check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[1],check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[2]);
	fprintf(fp,"\tpassive_service_check_stats=%d,%d,%d\n",check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[0],check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[1],check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[2]);
	fprintf(fp,"\tcached_host_check_stats=%d,%d,%d\n",check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[0],check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[1],check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp,"\tcached_service_check_stats=%d,%d,%d\n",check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[0],check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[1],check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[2]);
	fprintf(fp,"\texternal_command_stats=%d,%d,%d\n",check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[0],check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[1],check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[2]);

	fprintf(fp,"\tparallel_host_check_stats=%d,%d,%d\n",check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[0],check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[1],check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp,"\tserial_host_check_stats=%d,%d,%d\n",check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[0],check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[1],check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp,"\t}\n\n");


	/* save host status data */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		fprintf(fp,"hoststatus {\n");
		fprintf(fp,"\thost_name=%s\n",temp_host->name);

		fprintf(fp,"\tmodified_attributes=%lu\n",temp_host->modified_attributes);
		fprintf(fp,"\tcheck_command=%s\n",(temp_host->host_check_command==NULL)?"":temp_host->host_check_command);
		fprintf(fp,"\tcheck_period=%s\n",(temp_host->check_period==NULL)?"":temp_host->check_period);
		fprintf(fp,"\tnotification_period=%s\n",(temp_host->notification_period==NULL)?"":temp_host->notification_period);
		fprintf(fp,"\tcheck_interval=%f\n",temp_host->check_interval);
		fprintf(fp,"\tretry_interval=%f\n",temp_host->retry_interval);
		fprintf(fp,"\tevent_handler=%s\n",(temp_host->event_handler==NULL)?"":temp_host->event_handler);

		fprintf(fp,"\thas_been_checked=%d\n",temp_host->has_been_checked);
		fprintf(fp,"\tshould_be_scheduled=%d\n",temp_host->should_be_scheduled);
		fprintf(fp,"\tcheck_execution_time=%.3f\n",temp_host->execution_time);
		fprintf(fp,"\tcheck_latency=%.3f\n",temp_host->latency);
		fprintf(fp,"\tcheck_type=%d\n",temp_host->check_type);
		fprintf(fp,"\tcurrent_state=%d\n",temp_host->current_state);
		fprintf(fp,"\tlast_hard_state=%d\n",temp_host->last_hard_state);
		fprintf(fp,"\tlast_event_id=%lu\n",temp_host->last_event_id);
		fprintf(fp,"\tcurrent_event_id=%lu\n",temp_host->current_event_id);
		fprintf(fp,"\tcurrent_problem_id=%lu\n",temp_host->current_problem_id);
		fprintf(fp,"\tlast_problem_id=%lu\n",temp_host->last_problem_id);
		fprintf(fp,"\tplugin_output=%s\n",(temp_host->plugin_output==NULL)?"":temp_host->plugin_output);
		fprintf(fp,"\tlong_plugin_output=%s\n",(temp_host->long_plugin_output==NULL)?"":temp_host->long_plugin_output);
		fprintf(fp,"\tperformance_data=%s\n",(temp_host->perf_data==NULL)?"":temp_host->perf_data);
		fprintf(fp,"\tlast_check=%lu\n",temp_host->last_check);
		fprintf(fp,"\tnext_check=%lu\n",temp_host->next_check);
		fprintf(fp,"\tcheck_options=%d\n",temp_host->check_options);
		fprintf(fp,"\tcurrent_attempt=%d\n",temp_host->current_attempt);
		fprintf(fp,"\tmax_attempts=%d\n",temp_host->max_attempts);
		fprintf(fp,"\tstate_type=%d\n",temp_host->state_type);
		fprintf(fp,"\tlast_state_change=%lu\n",temp_host->last_state_change);
		fprintf(fp,"\tlast_hard_state_change=%lu\n",temp_host->last_hard_state_change);
		fprintf(fp,"\tlast_time_up=%lu\n",temp_host->last_time_up);
		fprintf(fp,"\tlast_time_down=%lu\n",temp_host->last_time_down);
		fprintf(fp,"\tlast_time_unreachable=%lu\n",temp_host->last_time_unreachable);
		fprintf(fp,"\tlast_notification=%lu\n",temp_host->last_host_notification);
		fprintf(fp,"\tnext_notification=%lu\n",temp_host->next_host_notification);
		fprintf(fp,"\tno_more_notifications=%d\n",temp_host->no_more_notifications);
		fprintf(fp,"\tcurrent_notification_number=%d\n",temp_host->current_notification_number);
		fprintf(fp,"\tcurrent_notification_id=%lu\n",temp_host->current_notification_id);
		fprintf(fp,"\tnotifications_enabled=%d\n",temp_host->notifications_enabled);
		fprintf(fp,"\tproblem_has_been_acknowledged=%d\n",temp_host->problem_has_been_acknowledged);
		fprintf(fp,"\tacknowledgement_type=%d\n",temp_host->acknowledgement_type);
		fprintf(fp,"\tactive_checks_enabled=%d\n",temp_host->checks_enabled);
		fprintf(fp,"\tpassive_checks_enabled=%d\n",temp_host->accept_passive_host_checks);
		fprintf(fp,"\tevent_handler_enabled=%d\n",temp_host->event_handler_enabled);
		fprintf(fp,"\tflap_detection_enabled=%d\n",temp_host->flap_detection_enabled);
		fprintf(fp,"\tfailure_prediction_enabled=%d\n",temp_host->failure_prediction_enabled);
		fprintf(fp,"\tprocess_performance_data=%d\n",temp_host->process_performance_data);
		fprintf(fp,"\tobsess_over_host=%d\n",temp_host->obsess_over_host);
		fprintf(fp,"\tlast_update=%lu\n",current_time);
		fprintf(fp,"\tis_flapping=%d\n",temp_host->is_flapping);
		fprintf(fp,"\tpercent_state_change=%.2f\n",temp_host->percent_state_change);
		fprintf(fp,"\tscheduled_downtime_depth=%d\n",temp_host->scheduled_downtime_depth);
		/*
		fprintf(fp,"\tstate_history=");
		for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
			fprintf(fp,"%s%d",(x>0)?",":"",temp_host->state_history[(x+temp_host->state_history_index)%MAX_STATE_HISTORY_ENTRIES]);
		fprintf(fp,"\n");
		*/
		/* custom variables */
		for(temp_customvariablesmember=temp_host->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
			if(temp_customvariablesmember->variable_name)
				fprintf(fp,"\t_%s=%d;%s\n",temp_customvariablesmember->variable_name,temp_customvariablesmember->has_been_modified,(temp_customvariablesmember->variable_value==NULL)?"":temp_customvariablesmember->variable_value);
		        }
		fprintf(fp,"\t}\n\n");
	        }

	/* save service status data */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		fprintf(fp,"servicestatus {\n");
		fprintf(fp,"\thost_name=%s\n",temp_service->host_name);

		fprintf(fp,"\tservice_description=%s\n",temp_service->description);
		fprintf(fp,"\tmodified_attributes=%lu\n",temp_service->modified_attributes);
		fprintf(fp,"\tcheck_command=%s\n",(temp_service->service_check_command==NULL)?"":temp_service->service_check_command);
		fprintf(fp,"\tcheck_period=%s\n",(temp_service->check_period==NULL)?"":temp_service->check_period);
		fprintf(fp,"\tnotification_period=%s\n",(temp_service->notification_period==NULL)?"":temp_service->notification_period);
		fprintf(fp,"\tcheck_interval=%f\n",temp_service->check_interval);
		fprintf(fp,"\tretry_interval=%f\n",temp_service->retry_interval);
		fprintf(fp,"\tevent_handler=%s\n",(temp_service->event_handler==NULL)?"":temp_service->event_handler);

		fprintf(fp,"\thas_been_checked=%d\n",temp_service->has_been_checked);
		fprintf(fp,"\tshould_be_scheduled=%d\n",temp_service->should_be_scheduled);
		fprintf(fp,"\tcheck_execution_time=%.3f\n",temp_service->execution_time);
		fprintf(fp,"\tcheck_latency=%.3f\n",temp_service->latency);
		fprintf(fp,"\tcheck_type=%d\n",temp_service->check_type);
		fprintf(fp,"\tcurrent_state=%d\n",temp_service->current_state);
		fprintf(fp,"\tlast_hard_state=%d\n",temp_service->last_hard_state);
		fprintf(fp,"\tlast_event_id=%lu\n",temp_service->last_event_id);
		fprintf(fp,"\tcurrent_event_id=%lu\n",temp_service->current_event_id);
		fprintf(fp,"\tcurrent_problem_id=%lu\n",temp_service->current_problem_id);
		fprintf(fp,"\tlast_problem_id=%lu\n",temp_service->last_problem_id);
		fprintf(fp,"\tcurrent_attempt=%d\n",temp_service->current_attempt);
		fprintf(fp,"\tmax_attempts=%d\n",temp_service->max_attempts);
		fprintf(fp,"\tstate_type=%d\n",temp_service->state_type);
		fprintf(fp,"\tlast_state_change=%lu\n",temp_service->last_state_change);
		fprintf(fp,"\tlast_hard_state_change=%lu\n",temp_service->last_hard_state_change);
		fprintf(fp,"\tlast_time_ok=%lu\n",temp_service->last_time_ok);
		fprintf(fp,"\tlast_time_warning=%lu\n",temp_service->last_time_warning);
		fprintf(fp,"\tlast_time_unknown=%lu\n",temp_service->last_time_unknown);
		fprintf(fp,"\tlast_time_critical=%lu\n",temp_service->last_time_critical);
		fprintf(fp,"\tplugin_output=%s\n",(temp_service->plugin_output==NULL)?"":temp_service->plugin_output);
		fprintf(fp,"\tlong_plugin_output=%s\n",(temp_service->long_plugin_output==NULL)?"":temp_service->long_plugin_output);
		fprintf(fp,"\tperformance_data=%s\n",(temp_service->perf_data==NULL)?"":temp_service->perf_data);
		fprintf(fp,"\tlast_check=%lu\n",temp_service->last_check);
		fprintf(fp,"\tnext_check=%lu\n",temp_service->next_check);
		fprintf(fp,"\tcheck_options=%d\n",temp_service->check_options);
		fprintf(fp,"\tcurrent_notification_number=%d\n",temp_service->current_notification_number);
		fprintf(fp,"\tcurrent_notification_id=%lu\n",temp_service->current_notification_id);
		fprintf(fp,"\tlast_notification=%lu\n",temp_service->last_notification);
		fprintf(fp,"\tnext_notification=%lu\n",temp_service->next_notification);
		fprintf(fp,"\tno_more_notifications=%d\n",temp_service->no_more_notifications);
		fprintf(fp,"\tnotifications_enabled=%d\n",temp_service->notifications_enabled);
		fprintf(fp,"\tactive_checks_enabled=%d\n",temp_service->checks_enabled);
		fprintf(fp,"\tpassive_checks_enabled=%d\n",temp_service->accept_passive_service_checks);
		fprintf(fp,"\tevent_handler_enabled=%d\n",temp_service->event_handler_enabled);
		fprintf(fp,"\tproblem_has_been_acknowledged=%d\n",temp_service->problem_has_been_acknowledged);
		fprintf(fp,"\tacknowledgement_type=%d\n",temp_service->acknowledgement_type);
		fprintf(fp,"\tflap_detection_enabled=%d\n",temp_service->flap_detection_enabled);
		fprintf(fp,"\tfailure_prediction_enabled=%d\n",temp_service->failure_prediction_enabled);
		fprintf(fp,"\tprocess_performance_data=%d\n",temp_service->process_performance_data);
		fprintf(fp,"\tobsess_over_service=%d\n",temp_service->obsess_over_service);
		fprintf(fp,"\tlast_update=%lu\n",current_time);
		fprintf(fp,"\tis_flapping=%d\n",temp_service->is_flapping);
		fprintf(fp,"\tpercent_state_change=%.2f\n",temp_service->percent_state_change);
		fprintf(fp,"\tscheduled_downtime_depth=%d\n",temp_service->scheduled_downtime_depth);
		/*
		fprintf(fp,"\tstate_history=");
		for(x=0;x<MAX_STATE_HISTORY_ENTRIES;x++)
			fprintf(fp,"%s%d",(x>0)?",":"",temp_service->state_history[(x+temp_service->state_history_index)%MAX_STATE_HISTORY_ENTRIES]);
		fprintf(fp,"\n");
		*/
		/* custom variables */
		for(temp_customvariablesmember=temp_service->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
			if(temp_customvariablesmember->variable_name)
				fprintf(fp,"\t_%s=%d;%s\n",temp_customvariablesmember->variable_name,temp_customvariablesmember->has_been_modified,(temp_customvariablesmember->variable_value==NULL)?"":temp_customvariablesmember->variable_value);
		        }
		fprintf(fp,"\t}\n\n");
	        }

	/* save contact status data */
	for(temp_contact=contact_list;temp_contact!=NULL;temp_contact=temp_contact->next){

		fprintf(fp,"contactstatus {\n");
		fprintf(fp,"\tcontact_name=%s\n",temp_contact->name);

		fprintf(fp,"\tmodified_attributes=%lu\n",temp_contact->modified_attributes);
		fprintf(fp,"\tmodified_host_attributes=%lu\n",temp_contact->modified_host_attributes);
		fprintf(fp,"\tmodified_service_attributes=%lu\n",temp_contact->modified_service_attributes);
		fprintf(fp,"\thost_notification_period=%s\n",(temp_contact->host_notification_period==NULL)?"":temp_contact->host_notification_period);
		fprintf(fp,"\tservice_notification_period=%s\n",(temp_contact->service_notification_period==NULL)?"":temp_contact->service_notification_period);

		fprintf(fp,"\tlast_host_notification=%lu\n",temp_contact->last_host_notification);
		fprintf(fp,"\tlast_service_notification=%lu\n",temp_contact->last_service_notification);
		fprintf(fp,"\thost_notifications_enabled=%d\n",temp_contact->host_notifications_enabled);
		fprintf(fp,"\tservice_notifications_enabled=%d\n",temp_contact->service_notifications_enabled);
		/* custom variables */
		for(temp_customvariablesmember=temp_contact->custom_variables;temp_customvariablesmember!=NULL;temp_customvariablesmember=temp_customvariablesmember->next){
			if(temp_customvariablesmember->variable_name)
				fprintf(fp,"\t_%s=%d;%s\n",temp_customvariablesmember->variable_name,temp_customvariablesmember->has_been_modified,(temp_customvariablesmember->variable_value==NULL)?"":temp_customvariablesmember->variable_value);
		        }
		fprintf(fp,"\t}\n\n");
	        }

	/* save all comments */
	for(temp_comment=comment_list;temp_comment!=NULL;temp_comment=temp_comment->next){

		if(temp_comment->comment_type==HOST_COMMENT)
			fprintf(fp,"hostcomment {\n");
		else
			fprintf(fp,"servicecomment {\n");
		fprintf(fp,"\thost_name=%s\n",temp_comment->host_name);
		if(temp_comment->comment_type==SERVICE_COMMENT)
			fprintf(fp,"\tservice_description=%s\n",temp_comment->service_description);
		fprintf(fp,"\tentry_type=%d\n",temp_comment->entry_type);
		fprintf(fp,"\tcomment_id=%lu\n",temp_comment->comment_id);
		fprintf(fp,"\tsource=%d\n",temp_comment->source);
		fprintf(fp,"\tpersistent=%d\n",temp_comment->persistent);
		fprintf(fp,"\tentry_time=%lu\n",temp_comment->entry_time);
		fprintf(fp,"\texpires=%d\n",temp_comment->expires);
		fprintf(fp,"\texpire_time=%lu\n",temp_comment->expire_time);
		fprintf(fp,"\tauthor=%s\n",temp_comment->author);
		fprintf(fp,"\tcomment_data=%s\n",temp_comment->comment_data);
		fprintf(fp,"\t}\n\n");
	        }

	/* save all downtime */
	for(temp_downtime=scheduled_downtime_list;temp_downtime!=NULL;temp_downtime=temp_downtime->next){

		if(temp_downtime->type==HOST_DOWNTIME)
			fprintf(fp,"hostdowntime {\n");
		else
			fprintf(fp,"servicedowntime {\n");
		fprintf(fp,"\thost_name=%s\n",temp_downtime->host_name);
		if(temp_downtime->type==SERVICE_DOWNTIME)
			fprintf(fp,"\tservice_description=%s\n",temp_downtime->service_description);
		fprintf(fp,"\tdowntime_id=%lu\n",temp_downtime->downtime_id);
		fprintf(fp,"\tentry_time=%lu\n",temp_downtime->entry_time);
		fprintf(fp,"\tstart_time=%lu\n",temp_downtime->start_time);
		fprintf(fp,"\tend_time=%lu\n",temp_downtime->end_time);
		fprintf(fp,"\ttriggered_by=%lu\n",temp_downtime->triggered_by);
		fprintf(fp,"\tfixed=%d\n",temp_downtime->fixed);
		fprintf(fp,"\tduration=%lu\n",temp_downtime->duration);
		fprintf(fp,"\tauthor=%s\n",temp_downtime->author);
		fprintf(fp,"\tcomment=%s\n",temp_downtime->comment);
		fprintf(fp,"\t}\n\n");
	        }


	/* reset file permissions */
	fchmod(fd,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* flush the file to disk */
	fflush(fp);

	/* close the temp file */
	result=fclose(fp);

	/* fsync the file so that it is completely written out before moving it */
	fsync(fd);

	/* save/close was successful */
	if(result==0){

		result=OK;

		/* move the temp file to the status log (overwrite the old status log) */
		if(my_rename(temp_file,xsddefault_status_log)){
			unlink(temp_file);
			logit(NSLOG_RUNTIME_ERROR,TRUE,"Error: Unable to update status data file '%s': %s",xsddefault_status_log,strerror(errno));
			result=ERROR;
			}
		}

	/* a problem occurred saving the file */
	else{

		result=ERROR;

		/* remove temp file and log an error */
		unlink(temp_file);
		logit(NSLOG_RUNTIME_ERROR,TRUE,"Error: Unable to save status file: %s",strerror(errno));
	        }

	/* free memory */
	my_free(temp_file);

	return result;
        }
