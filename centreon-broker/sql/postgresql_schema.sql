-- ------------------------------------
--                                   --
-- Centreon Broker's database schema --
--                                   --
--                                   --
--          Real-time data           --
--              Logs                 --
--         Performance data          --
-- ------------------------------------

-- rt_acknowledgements
-- rt_customvariables
-- log_data_bin
-- rt_downtimes
-- rt_eventhandlers
-- rt_flappingstatuses
-- rt_hosts
-- rt_hosts_hosts_dependencies
-- rt_hosts_hosts_parents
-- rt_hoststateevents
-- rt_index_data
-- rt_instances
-- rt_issues
-- rt_issues_issues_parents
-- log_logs
-- rt_metrics
-- rt_modules
-- rt_notifications
-- rt_schemaversion
-- rt_services
-- rt_services_services_dependencies
-- rt_servicestateevents


--
-- Holds the current version of the database schema.
--
CREATE TABLE rt_schemaversion (
  software varchar(128) NOT NULL,
  version int NOT NULL
);
INSERT INTO rt_schemaversion (software, version) VALUES ('centreon-broker', 3);


--
-- Store information about Nagios instances.
--
CREATE TABLE rt_instances (
  instance_id int NOT NULL,
  name varchar(255) NOT NULL default 'localhost',

  address varchar(128) default NULL,
  check_hosts_freshness boolean default NULL,
  check_services_freshness boolean default NULL,
  deleted boolean NOT NULL default false,
  description varchar(128) default NULL,
  end_time int default NULL,
  engine varchar(64) default NULL,
  event_handlers boolean default NULL,
  flap_detection boolean default NULL,
  global_host_event_handler text default NULL,
  global_service_event_handler text default NULL,
  last_alive int default NULL,
  last_command_check int default NULL,
  notifications boolean default NULL,
  obsess_over_hosts boolean default NULL,
  obsess_over_services boolean default NULL,
  outdated boolean default NULL,
  pid int default NULL,
  running boolean default NULL,
  start_time int default NULL,
  version varchar(16) default NULL,

  PRIMARY KEY (instance_id)
);


--
-- Monitored hosts.
--
CREATE TABLE rt_hosts (
  host_id int NOT NULL,
  name varchar(255) NOT NULL,
  instance_id int NOT NULL,

  acknowledged boolean default NULL,
  acknowledgement_type smallint default NULL,
  active_checks boolean default NULL,
  address varchar(75) default NULL,
  alias varchar(100) default NULL,
  check_attempt smallint default NULL,
  check_command text default NULL,
  check_freshness boolean default NULL,
  check_interval double precision default NULL,
  check_period varchar(75) default NULL,
  check_type smallint default NULL,
  checked boolean default NULL,
  command_line text default NULL,
  default_active_checks boolean default NULL,
  default_event_handler_enabled boolean default NULL,
  default_flap_detection boolean default NULL,
  default_notify boolean default NULL,
  enabled bool NOT NULL default true,
  event_handler varchar(255) default NULL,
  event_handler_enabled boolean default NULL,
  execution_time double precision default NULL,
  first_notification_delay double precision default NULL,
  flap_detection boolean default NULL,
  flap_detection_on_down boolean default NULL,
  flap_detection_on_unreachable boolean default NULL,
  flap_detection_on_up boolean default NULL,
  flapping boolean default NULL,
  freshness_threshold double precision default NULL,
  high_flap_threshold double precision default NULL,
  last_check int default NULL,
  last_hard_state smallint default NULL,
  last_hard_state_change int default NULL,
  last_notification int default NULL,
  last_state_change int default NULL,
  last_time_down int default NULL,
  last_time_unreachable int default NULL,
  last_time_up int default NULL,
  last_update int default NULL,
  latency double precision default NULL,
  low_flap_threshold double precision default NULL,
  max_check_attempts smallint default NULL,
  next_check int default NULL,
  next_host_notification int default NULL,
  no_more_notifications boolean default NULL,
  notification_interval double precision default NULL,
  notification_number smallint default NULL,
  notification_period varchar(75) default NULL,
  notify boolean default NULL,
  notify_on_down boolean default NULL,
  notify_on_downtime boolean default NULL,
  notify_on_flapping boolean default NULL,
  notify_on_recovery boolean default NULL,
  notify_on_unreachable boolean default NULL,
  obsess_over_host boolean default NULL,
  output text default NULL,
  percent_state_change double precision default NULL,
  perfdata text default NULL,
  real_state smallint default NULL,
  retry_interval double precision default NULL,
  scheduled_downtime_depth smallint default NULL,
  should_be_scheduled boolean default NULL,
  stalk_on_down boolean default NULL,
  stalk_on_unreachable boolean default NULL,
  stalk_on_up boolean default NULL,
  state smallint default NULL,
  state_type smallint default NULL,

  UNIQUE (host_id),
  FOREIGN KEY (instance_id) REFERENCES rt_instances (instance_id)
    ON DELETE CASCADE,
  INDEX (address),
  INDEX (alias),
  INDEX (enabled),
  INDEX (last_hard_state),
  INDEX (last_hard_state_change),
  INDEX (name),
  INDEX (state),
  INDEX (state_type)
);


--
-- Hosts dependencies.
--
CREATE TABLE rt_hosts_hosts_dependencies (
  dependent_host_id int NOT NULL,
  host_id int NOT NULL,

  dependency_period varchar(75) default NULL,
  execution_failure_options varchar(15) default NULL,
  inherits_parent boolean default NULL,
  notification_failure_options varchar(15) default NULL,

  FOREIGN KEY (dependent_host_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE,
  FOREIGN KEY (host_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE
);


--
-- Hosts parenting relationships.
--
CREATE TABLE rt_hosts_hosts_parents (
  child_id int NOT NULL,
  parent_id int NOT NULL,

  UNIQUE (child_id, parent_id),
  FOREIGN KEY (child_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE,
  FOREIGN KEY (parent_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE
);


--
-- Monitored services.
--
CREATE TABLE rt_services (
  host_id int NOT NULL,
  description varchar(255) NOT NULL,
  service_id int NOT NULL,

  acknowledged boolean default NULL,
  acknowledgement_type smallint default NULL,
  active_checks boolean default NULL,
  check_attempt smallint default NULL,
  check_command text default NULL,
  check_freshness boolean default NULL,
  check_interval double precision default NULL,
  check_period varchar(75) default NULL,
  check_type smallint default NULL,
  checked boolean default NULL,
  command_line text default NULL,
  default_active_checks boolean default NULL,
  default_event_handler_enabled boolean default NULL,
  default_flap_detection boolean default NULL,
  default_notify boolean default NULL,
  enabled bool NOT NULL default true,
  event_handler varchar(255) default NULL,
  event_handler_enabled boolean default NULL,
  execution_time double precision default NULL,
  first_notification_delay double precision default NULL,
  flap_detection boolean default NULL,
  flap_detection_on_critical boolean default NULL,
  flap_detection_on_ok boolean default NULL,
  flap_detection_on_unknown boolean default NULL,
  flap_detection_on_warning boolean default NULL,
  flapping boolean default NULL,
  freshness_threshold double precision default NULL,
  high_flap_threshold double precision default NULL,
  last_check int default NULL,
  last_hard_state smallint default NULL,
  last_hard_state_change int default NULL,
  last_notification int default NULL,
  last_state_change int default NULL,
  last_time_critical int default NULL,
  last_time_ok int default NULL,
  last_time_unknown int default NULL,
  last_time_warning int default NULL,
  last_update int default NULL,
  latency double precision default NULL,
  low_flap_threshold double precision default NULL,
  max_check_attempts smallint default NULL,
  next_check int default NULL,
  next_notification int default NULL,
  no_more_notifications boolean default NULL,
  notification_interval double precision default NULL,
  notification_number smallint default NULL,
  notification_period varchar(75) default NULL,
  notify boolean default NULL,
  notify_on_critical boolean default NULL,
  notify_on_downtime boolean default NULL,
  notify_on_flapping boolean default NULL,
  notify_on_recovery boolean default NULL,
  notify_on_unknown boolean default NULL,
  notify_on_warning boolean default NULL,
  obsess_over_service boolean default NULL,
  output text default NULL,
  percent_state_change double precision default NULL,
  perfdata text default NULL,
  real_state smallint default NULL,
  retry_interval double precision default NULL,
  scheduled_downtime_depth smallint default NULL,
  should_be_scheduled boolean default NULL,
  stalk_on_critical boolean default NULL,
  stalk_on_ok boolean default NULL,
  stalk_on_unknown boolean default NULL,
  stalk_on_warning boolean default NULL,
  state smallint default NULL,
  state_type smallint default NULL,
  volatile boolean default NULL,

  UNIQUE (host_id, service_id),
  FOREIGN KEY (host_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE,
  INDEX (acknowledged),
  INDEX (enabled),
  INDEX (last_hard_state),
  INDEX (last_hard_state_change),
  INDEX (last_state_change),
  INDEX (scheduled_downtime_depth),
  INDEX (state),
  INDEX (state_type)
);


--
-- Services dependencies.
--
CREATE TABLE rt_services_services_dependencies (
  dependent_host_id int NOT NULL,
  dependent_service_id int NOT NULL,
  host_id int NOT NULL,
  service_id int NOT NULL,

  dependency_period varchar(75) default NULL,
  execution_failure_options varchar(15) default NULL,
  inherits_parent boolean default NULL,
  notification_failure_options varchar(15) default NULL,

  FOREIGN KEY (dependent_host_id, dependent_service_id) REFERENCES rt_services (host_id, service_id)
    ON DELETE CASCADE,
  FOREIGN KEY (host_id, service_id) REFERENCES rt_services (host_id, service_id)
    ON DELETE CASCADE
);


--
-- Holds acknowledgedments information.
--
CREATE TABLE rt_acknowledgements (
  acknowledgement_id serial,
  entry_time int NOT NULL,
  host_id int NOT NULL,
  service_id int default NULL,

  author varchar(64) default NULL,
  comment_data varchar(255) default NULL,
  deletion_time int default NULL,
  instance_id int default NULL,
  notify_contacts boolean default NULL,
  persistent_comment boolean default NULL,
  state smallint default NULL,
  sticky boolean default NULL,
  type smallint default NULL,

  PRIMARY KEY (acknowledgement_id),
  UNIQUE (entry_time, host_id, service_id),
  FOREIGN KEY (host_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE,
  FOREIGN KEY (instance_id) REFERENCES rt_instances (instance_id)
    ON DELETE SET NULL
);


--
-- Custom variables.
--
CREATE TABLE rt_customvariables (
  customvariable_id serial,
  host_id int default NULL,
  name varchar(255) default NULL,
  service_id int default NULL,

  default_value varchar(255) default NULL,
  modified boolean default NULL,
  type smallint default NULL,
  update_time int default NULL,
  value varchar(255) default NULL,

  PRIMARY KEY (customvariable_id),
  UNIQUE (host_id, name, service_id)
);


--
-- Downtimes.
--
CREATE TABLE rt_downtimes (
  downtime_id serial,
  entry_time int NOT NULL,
  host_id int NOT NULL,
  service_id int default NULL,

  actual_end_time int default NULL,
  actual_start_time int default NULL,
  author varchar(64) default NULL,
  cancelled boolean default NULL,
  comment_data text default NULL,
  deletion_time int default NULL,
  duration int default NULL,
  end_time int default NULL,
  fixed boolean default NULL,
  instance_id int default NULL,
  internal_id int default NULL,
  start_time int default NULL,
  started boolean default NULL,
  triggered_by int default NULL,
  type smallint default NULL,

  PRIMARY KEY (downtime_id),
  UNIQUE (entry_time, host_id, service_id),
  UNIQUE (entry_time, host_id, internal_id),
  FOREIGN KEY (host_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE,
  FOREIGN KEY (instance_id) REFERENCES rt_instances (instance_id)
    ON DELETE SET NULL
);


--
-- Event handlers.
--
CREATE TABLE rt_eventhandlers (
  host_id int default NULL,
  service_id int default NULL,
  start_time int default NULL,

  command_args varchar(255) default NULL,
  command_line varchar(255) default NULL,
  early_timeout boolean default NULL,
  end_time int default NULL,
  execution_time double precision default NULL,
  output varchar(255) default NULL,
  return_code smallint default NULL,
  state smallint default NULL,
  state_type smallint default NULL,
  timeout smallint default NULL,
  type smallint default NULL,

  UNIQUE (host_id, service_id, start_time),
  FOREIGN KEY (host_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE
);


--
-- Historization of flapping statuses.
--
CREATE TABLE rt_flappingstatuses (
  flappingstatus_id serial,
  host_id int default NULL,
  service_id int default NULL,
  event_time int default NULL,

  comment_time int default NULL,
  event_type smallint default NULL,
  high_threshold double precision default NULL,
  internal_comment_id int default NULL,
  low_threshold double precision default NULL,
  percent_state_change double precision default NULL,
  reason_type smallint default NULL,
  type smallint default NULL,

  PRIMARY KEY (flappingstatus_id),
  UNIQUE (host_id, service_id, event_time),
  FOREIGN KEY (host_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE
) ENGINE=InnoDB;


--
-- Correlated issues.
--
CREATE TABLE rt_issues (
  issue_id serial,
  host_id int default NULL,
  service_id int default NULL,
  start_time int NOT NULL,

  ack_time int default NULL,
  end_time int default NULL,

  PRIMARY KEY (issue_id),
  UNIQUE (host_id, service_id, start_time),
  FOREIGN KEY (host_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE
);


--
-- Issues parenting.
--
CREATE TABLE rt_issues_issues_parents (
  child_id int NOT NULL,
  end_time int default NULL,
  start_time int NOT NULL,
  parent_id int NOT NULL,

  FOREIGN KEY (child_id) REFERENCES rt_issues (issue_id)
    ON DELETE CASCADE,
  FOREIGN KEY (parent_id) REFERENCES rt_issues (issue_id)
    ON DELETE CASCADE
);


--
-- Nagios logs.
--
CREATE TABLE log_logs (
  log_id serial,

  ctime int default NULL,
  host_id int default NULL,
  host_name varchar(255) default NULL,
  instance_name varchar(255) NOT NULL,
  issue_id default NULL,
  msg_type smallint default NULL,
  notification_cmd varchar(255) default NULL,
  notification_contact varchar(255) default NULL,
  output text default NULL,
  retry int default NULL,
  service_description varchar(255) default NULL,
  service_id int default NULL,
  status smallint default NULL,
  type smallint default NULL,

  PRIMARY KEY (log_id),
  FOREIGN KEY (host_id) REFERENCES rt_hosts (host_id)
    ON DELETE SET NULL
);


--
-- Nagios modules.
--
CREATE TABLE rt_modules (
  module_id serial,
  instance_id int NOT NULL,

  args varchar(255) default NULL,
  filename varchar(255) default NULL,
  loaded boolean default NULL,
  should_be_loaded boolean default NULL,

  PRIMARY KEY (module_id),
  FOREIGN KEY (instance_id) REFERENCES rt_instances (instance_id)
    ON DELETE CASCADE
);


--
--  Notifications.
--
CREATE TABLE rt_notifications (
  notification_id serial,
  host_id int default NULL,
  service_id int default NULL,
  start_time int default NULL,

  ack_author varchar(255) default NULL,
  ack_data text default NULL,
  command_name varchar(255) default NULL,
  contact_name varchar(255) default NULL,
  contacts_notified boolean default NULL,
  end_time int default NULL,
  escalated boolean default NULL,
  output text default NULL,
  reason_type int default NULL,
  state int default NULL,
  type int default NULL,

  PRIMARY KEY (notification_id),
  UNIQUE (host_id, service_id, start_time),
  FOREIGN KEY (host_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE
);


--
--  Host states.
--
CREATE TABLE rt_hoststateevents (
  host_id int NOT NULL,
  start_time int NOT NULL,

  ack_time int default NULL,
  end_time int default NULL,
  in_downtime boolean default NULL,
  state int default NULL,

  UNIQUE (host_id, start_time),
  FOREIGN KEY (host_id) REFERENCES rt_hosts (host_id)
    ON DELETE CASCADE
);


--
--  Service states.
--
CREATE TABLE rt_servicestateevents (
  host_id int NOT NULL,
  service_id int NOT NULL,
  start_time int NOT NULL,

  ack_time int default NULL,
  end_time int default NULL,
  in_downtime boolean default NULL,
  state int default NULL,

  UNIQUE (host_id, service_id, start_time),
  FOREIGN KEY (host_id, service_id) REFERENCES rt_services (host_id, service_id)
    ON DELETE CASCADE
);

--
--  Base performance data index.
--
CREATE TABLE rt_index_data (
  id serial,
  host_id int NOT NULL,
  service_id int default NULL,

  check_interval int default NULL,
  hidden enum('0', '1') NOT NULL default '0',
  host_name varchar(255) default NULL,
  locked enum('0', '1') NOT NULL default '0',
  must_be_rebuild enum('0', '1', '2') NOT NULL default '0',
  rrd_retention int default NULL,
  service_description varchar(255) default NULL,
  special enum('0', '1') NOT NULL default '0',
  storage_type enum('0', '1', '2') NOT NULL default '2',
  to_delete int NOT NULL default 0,
  trashed enum('0', '1') NOT NULL default '0',

  PRIMARY KEY (id),
  UNIQUE (host_id, service_id),
  INDEX (host_id),
  INDEX (host_name),
  INDEX (must_be_rebuild),
  INDEX (service_description),
  INDEX (service_id),
  INDEX (trashed)
);

--
--  Metrics.
--
CREATE TABLE rt_metrics (
  metric_id serial,
  index_id int NOT NULL,
  metric_name varchar(255) NOT NULL,

  crit double default NULL,
  crit_low double default NULL,
  crit_threshold_mode boolean default NULL,
  current_value double default NULL,
  data_source_type enum('0', '1', '2', '3') NOT NULL default '0',
  hidden enum('0', '1') NOT NULL default '0',
  locked enum('0', '1') NOT NULL default '0',
  min double default NULL,
  max double default NULL,
  to_delete int NOT NULL default 0,
  unit_name varchar(32) default NULL,
  warn double default NULL,
  warn_low double default NULL,
  warn_threshold_mode boolean default NULL,

  PRIMARY KEY (metric_id),
  UNIQUE KEY (index_id, metric_name),
  FOREIGN KEY (index_id) REFERENCES rt_index_data (id)
    ON DELETE CASCADE,
  INDEX (index_id)
);

--
--  Performance data.
--
CREATE TABLE log_data_bin (
  id_metric int NOT NULL,
  ctime int NOT NULL,
  status enum('0', '1', '2', '3', '4') NOT NULL default '3',
  value float default NULL,

  FOREIGN KEY (id_metric) REFERENCES rt_metrics (metric_id)
    ON DELETE CASCADE,
  INDEX (id_metric)
);
