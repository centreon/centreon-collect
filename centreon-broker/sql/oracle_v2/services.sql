--
-- Monitored services.
--
CREATE TABLE services (
  host_id int NOT NULL,
  description varchar(255) NOT NULL,
  service_id int NOT NULL,

  acknowledged char(1) default NULL,
  acknowledgement_type smallint default NULL,
  action_url varchar(255) default NULL,
  active_checks char(1) default NULL,
  check_attempt smallint default NULL,
  check_command clob default NULL,
  check_freshness char(1) default NULL,
  check_interval double precision default NULL,
  check_period varchar(75) default NULL,
  check_type smallint default NULL,
  checked char(1) default NULL,
  command_line clob default NULL,
  default_active_checks char(1) default NULL,
  default_event_handler_enabled char(1) default NULL,
  default_failure_prediction char(1) default NULL,
  default_flap_detection char(1) default NULL,
  default_notifications char(1) default NULL,
  default_passive_checks char(1) default NULL,
  default_process_perfdata char(1) default NULL,
  display_name varchar(160) default NULL,
  enabled char(1) default 1 NOT NULL,
  event_handler varchar(255) default NULL,
  event_handler_enabled char(1) default NULL,
  execution_time double precision default NULL,
  failure_prediction char(1) default NULL,
  failure_prediction_options varchar(64) default NULL,
  first_notification_delay double precision default NULL,
  flap_detection char(1) default NULL,
  flap_detection_on_critical char(1) default NULL,
  flap_detection_on_ok char(1) default NULL,
  flap_detection_on_unknown char(1) default NULL,
  flap_detection_on_warning char(1) default NULL,
  flapping char(1) default NULL,
  freshness_threshold double precision default NULL,
  high_flap_threshold double precision default NULL,
  icon_image varchar(255) default NULL,
  icon_image_alt varchar(255) default NULL,
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
  modified_attributes int default NULL,
  next_check int default NULL,
  next_notification int default NULL,
  no_more_notifications char(1) default NULL,
  notes varchar(255) default NULL,
  notes_url varchar(255) default NULL,
  notification_interval double precision default NULL,
  notification_number smallint default NULL,
  notification_period varchar(75) default NULL,
  notify char(1) default NULL,
  notify_on_critical char(1) default NULL,
  notify_on_downtime char(1) default NULL,
  notify_on_flapping char(1) default NULL,
  notify_on_recovery char(1) default NULL,
  notify_on_unknown char(1) default NULL,
  notify_on_warning char(1) default NULL,
  obsess_over_service char(1) default NULL,
  output clob default NULL,
  passive_checks char(1) default NULL,
  percent_state_change double precision default NULL,
  perfdata clob default NULL,
  process_perfdata char(1) default NULL,
  real_state smallint default NULL,
  retain_nonstatus_information char(1) default NULL,
  retain_status_information char(1) default NULL,
  retry_interval double precision default NULL,
  scheduled_downtime_depth smallint default NULL,
  should_be_scheduled char(1) default NULL,
  stalk_on_critical char(1) default NULL,
  stalk_on_ok char(1) default NULL,
  stalk_on_unknown char(1) default NULL,
  stalk_on_warning char(1) default NULL,
  state smallint default NULL,
  state_type smallint default NULL,
  volatile char(1) default NULL,

  UNIQUE (host_id, service_id),
  INDEX (service_id),
  INDEX (description),
  FOREIGN KEY (host_id) REFERENCES hosts (host_id)
    ON DELETE CASCADE
);
