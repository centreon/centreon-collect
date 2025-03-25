INSERT IGNORE INTO `nagios_server` (
       id, name, localhost, is_default, last_restart, ns_ip_address,
       ns_activate, ns_status, engine_start_command,
       engine_stop_command,engine_restart_command,
       engine_reload_command, nagios_bin,
       nagiostats_bin, nagios_perfdata,
       broker_reload_command, centreonbroker_cfg_path, centreonbroker_module_path, centreonconnector_path, ssh_port,gorgone_communication_type,gorgone_port,
       init_script_centreontrapd, snmp_trapd_path_conf, engine_name, engine_version, centreonbroker_logs_path, remote_id, remote_server_use_as_proxy,updated)
  VALUES
  (
    1, 'Central', '1', 1, 1711560733, '127.0.0.1',
    '1', '0', 'service centengine start',
    'service centengine stop', 'service centengine restart',
    'service centengine reload', '/usr/sbin/centengine',
    '/usr/sbin/centenginestats', '/var/log/centreon-engine/service-perfdata',
    'service cbd reload', '/etc/centreon-broker',
    '/usr/share/centreon/lib/centreon-broker',
    '/usr/lib64/centreon-connector',
    22, '1', 5556, 'centreontrapd', '/etc/snmp/centreon_traps/',
    NULL, NULL, NULL, NULL, '1', '0'
  ),
  (
    2, 'pushpoller', '0', 0, NULL, '127.0.0.1',
    '1', '0', 'service centengine start',
    'service centengine stop', 'service centengine restart',
    'service centengine reload', '/usr/sbin/centengine',
    '/usr/sbin/centenginestats', '/var/log/centreon-engine/service-perfdata',
    'service cbd reload', '/etc/centreon-broker',
    '/usr/share/centreon/lib/centreon-broker',
    '/usr/lib64/centreon-connector',
    22, '1', 5556, 'centreontrapd', '/etc/snmp/centreon_traps/',
    NULL, NULL, '/var/log/centreon-broker/',
    NULL, '1', '0'
  );

INSERT IGNORE INTO `cfg_nagios` (
       nagios_id, nagios_name, use_timezone, log_file, cfg_dir, status_file, status_update_interval, enable_notifications, execute_service_checks, accept_passive_service_checks, execute_host_checks, accept_passive_host_checks, enable_event_handlers, check_external_commands, external_command_buffer_slots, command_check_interval, command_file, retain_state_information, state_retention_file, retention_update_interval, use_retained_program_state, use_retained_scheduling_info, use_syslog, log_notifications, log_service_retries, log_host_retries, log_event_handlers, log_initial_states, log_external_commands, log_passive_checks, global_host_event_handler, global_service_event_handler, sleep_time, service_inter_check_delay_method, host_inter_check_delay_method, service_interleave_factor, max_concurrent_checks, max_service_check_spread, max_host_check_spread, check_result_reaper_frequency, auto_reschedule_checks, auto_rescheduling_interval, auto_rescheduling_window, enable_flap_detection, low_service_flap_threshold, high_service_flap_threshold, low_host_flap_threshold, high_host_flap_threshold, soft_state_dependencies, service_check_timeout, host_check_timeout, event_handler_timeout, notification_timeout, check_for_orphaned_services, check_for_orphaned_hosts, check_service_freshness, service_freshness_check_interval, freshness_check_interval, check_host_freshness, host_freshness_check_interval, date_format, instance_heartbeat_interval, illegal_object_name_chars, illegal_macro_output_chars, use_regexp_matching, use_true_regexp_matching, admin_email, admin_pager, nagios_comment, nagios_activate, event_broker_options, nagios_server_id, enable_predictive_host_dependency_checks, enable_predictive_service_dependency_checks, cached_host_check_horizon, cached_service_check_horizon, passive_host_checks_are_soft, enable_environment_macros, additional_freshness_latency, debug_file, debug_level, debug_level_opt, debug_verbosity, max_debug_file_size, cfg_file, log_pid, enable_macros_filter, macros_filter, logger_version
       )
  VALUES
  (
    1, 'Centreon Engine Central', NULL,
    '/var/log/centreon-engine/centengine.log',
    '/etc/centreon-engine', '/var/log/centreon-engine/status.dat',
    60, '1', '1', '1', '1', '1', '1', '1',
    4096, '1s', '/var/lib/centreon-engine/rw/centengine.cmd',
    '1', '/var/log/centreon-engine/retention.dat',
    60, '1', '1', '0', '1', '1', '1', '1',
    NULL, '1', '1', NULL, NULL, NULL, 's',
    's', 's', 0, 15, 15, 5, '0', NULL, NULL,
    '0', '25.0', '50.0', '25.0', '50.0',
    '0', 60, 12, 30, 30, '1', '1', '0', NULL,
    NULL, '0', NULL, 'euro', 30, '~!$%^&*\"|\'<>?,()=',
    '`~$^&\"|\'<>', '0', '0', 'admin@localhost',
    'admin@localhost', 'Centreon Engine configuration file for a central instance',
    '1', '-1', 1, '1', '1', 15, 15, NULL,
    '0', 15, '/var/log/centreon-engine/centengine.debug',
    0, '0', '1', 1000000000, 'centengine.cfg',
    '1', '0', '', 'log_v2_enabled'
  ),
  (
    15, 'pushpoller', NULL, '/var/log/centreon-engine/centengine.log',
    '/etc/centreon-engine/', '/var/log/centreon-engine/status.dat',
    60, '1', '1', '1', '1', '1', '1', '1',
    4096, '1s', '/var/lib/centreon-engine/rw/centengine.cmd',
    '1', '/var/log/centreon-engine/retention.dat',
    60, '1', '0', '0', '1', '1', '1', '1',
    '1', '1', '1', NULL, NULL, '0.5', 's',
    's', 's', 0, 15, 15, 5, '0', 30, 180, '0',
    '25.0', '50.0', '25.0', '50.0', '0',
    60, 30, 30, 30, '1', '1', '0', NULL, NULL,
    '0', NULL, 'euro', 30, '~!$%^&*\"|\'<>?,()=',
    '`~$^&\"|\'<>', '0', '0', 'admin@localhost',
    'admin@localhost', 'Centreon Engine config file for a polling instance',
    '1', '-1', 2, '1', '1', 15, 15, NULL,
    '0', 15, '/var/log/centreon-engine/centengine.debug',
    0, '0', '1', 1000000000, 'centengine.cfg',
    '1', '0', '', 'log_v2_enabled'
  );
