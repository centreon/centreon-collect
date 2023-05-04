/*
** Copyright 2020 Centreon
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

#ifndef __TABLE_MAX_SIZE_HH__
#define __TABLE_MAX_SIZE_HH__

#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

enum acl_actions_cols {
  acl_actions_acl_action_name,
  acl_actions_acl_action_description,
};
constexpr static uint32_t acl_actions_size[]{
    255,
    255,
};
constexpr uint32_t get_acl_actions_col_size(acl_actions_cols const& col) {
  return acl_actions_size[col];
}

enum acl_actions_rules_cols {
  acl_actions_rules_acl_action_name,
};
constexpr static uint32_t acl_actions_rules_size[]{
    255,
};
constexpr uint32_t get_acl_actions_rules_col_size(
    acl_actions_rules_cols const& col) {
  return acl_actions_rules_size[col];
}

enum acl_groups_cols {
  acl_groups_acl_group_name,
  acl_groups_acl_group_alias,
};
constexpr static uint32_t acl_groups_size[]{
    255,
    255,
};
constexpr uint32_t get_acl_groups_col_size(acl_groups_cols const& col) {
  return acl_groups_size[col];
}

enum acl_resources_cols {
  acl_resources_acl_res_name,
  acl_resources_acl_res_alias,
  acl_resources_acl_res_comment,
};
constexpr static uint32_t acl_resources_size[]{
    255,
    255,
    65534,
};
constexpr uint32_t get_acl_resources_col_size(acl_resources_cols const& col) {
  return acl_resources_size[col];
}

enum acl_topology_cols {
  acl_topology_acl_topo_name,
  acl_topology_acl_topo_alias,
  acl_topology_acl_comments,
};
constexpr static uint32_t acl_topology_size[]{
    255,
    255,
    65534,
};
constexpr uint32_t get_acl_topology_col_size(acl_topology_cols const& col) {
  return acl_topology_size[col];
}

enum auth_ressource_cols {
  auth_ressource_ar_name,
  auth_ressource_ar_description,
  auth_ressource_ar_type,
};
constexpr static uint32_t auth_ressource_size[]{
    255,
    255,
    50,
};
constexpr uint32_t get_auth_ressource_col_size(auth_ressource_cols const& col) {
  return auth_ressource_size[col];
}

enum auth_ressource_host_cols {
  auth_ressource_host_host_address,
};
constexpr static uint32_t auth_ressource_host_size[]{
    255,
};
constexpr uint32_t get_auth_ressource_host_col_size(
    auth_ressource_host_cols const& col) {
  return auth_ressource_host_size[col];
}

enum auth_ressource_info_cols {
  auth_ressource_info_ari_name,
  auth_ressource_info_ari_value,
};
constexpr static uint32_t auth_ressource_info_size[]{
    100,
    1024,
};
constexpr uint32_t get_auth_ressource_info_col_size(
    auth_ressource_info_cols const& col) {
  return auth_ressource_info_size[col];
}

enum cb_field_cols {
  cb_field_fieldname,
  cb_field_displayname,
  cb_field_description,
  cb_field_fieldtype,
  cb_field_external,
};
constexpr static uint32_t cb_field_size[]{
    100, 100, 255, 255, 255,
};
constexpr uint32_t get_cb_field_col_size(cb_field_cols const& col) {
  return cb_field_size[col];
}

enum cb_fieldgroup_cols {
  cb_fieldgroup_groupname,
  cb_fieldgroup_displayname,
};
constexpr static uint32_t cb_fieldgroup_size[]{
    100,
    255,
};
constexpr uint32_t get_cb_fieldgroup_col_size(cb_fieldgroup_cols const& col) {
  return cb_fieldgroup_size[col];
}

enum cb_fieldset_cols {
  cb_fieldset_fieldset_name,
};
constexpr static uint32_t cb_fieldset_size[]{
    255,
};
constexpr uint32_t get_cb_fieldset_col_size(cb_fieldset_cols const& col) {
  return cb_fieldset_size[col];
}

enum cb_list_cols {
  cb_list_default_value,
};
constexpr static uint32_t cb_list_size[]{
    255,
};
constexpr uint32_t get_cb_list_col_size(cb_list_cols const& col) {
  return cb_list_size[col];
}

enum cb_list_values_cols {
  cb_list_values_value_name,
  cb_list_values_value_value,
};
constexpr static uint32_t cb_list_values_size[]{
    255,
    255,
};
constexpr uint32_t get_cb_list_values_col_size(cb_list_values_cols const& col) {
  return cb_list_values_size[col];
}

enum cb_module_cols {
  cb_module_name,
  cb_module_libname,
};
constexpr static uint32_t cb_module_size[]{
    50,
    50,
};
constexpr uint32_t get_cb_module_col_size(cb_module_cols const& col) {
  return cb_module_size[col];
}

enum cb_tag_cols {
  cb_tag_tagname,
};
constexpr static uint32_t cb_tag_size[]{
    50,
};
constexpr uint32_t get_cb_tag_col_size(cb_tag_cols const& col) {
  return cb_tag_size[col];
}

enum cb_type_cols {
  cb_type_type_name,
  cb_type_type_shortname,
};
constexpr static uint32_t cb_type_size[]{
    50,
    50,
};
constexpr uint32_t get_cb_type_col_size(cb_type_cols const& col) {
  return cb_type_size[col];
}

enum cb_type_field_relation_cols {
  cb_type_field_relation_jshook_name,
  cb_type_field_relation_jshook_arguments,
};
constexpr static uint32_t cb_type_field_relation_size[]{
    255,
    255,
};
constexpr uint32_t get_cb_type_field_relation_col_size(
    cb_type_field_relation_cols const& col) {
  return cb_type_field_relation_size[col];
}

enum cfg_centreonbroker_cols {
  cfg_centreonbroker_config_name,
  cfg_centreonbroker_config_filename,
  cfg_centreonbroker_command_file,
  cfg_centreonbroker_cache_directory,
};
constexpr static uint32_t cfg_centreonbroker_size[]{
    100,
    255,
    255,
    255,
};
constexpr uint32_t get_cfg_centreonbroker_col_size(
    cfg_centreonbroker_cols const& col) {
  return cfg_centreonbroker_size[col];
}

enum cfg_centreonbroker_info_cols {
  cfg_centreonbroker_info_config_key,
  cfg_centreonbroker_info_config_value,
  cfg_centreonbroker_info_config_group,
};
constexpr static uint32_t cfg_centreonbroker_info_size[]{
    50,
    255,
    50,
};
constexpr uint32_t get_cfg_centreonbroker_info_col_size(
    cfg_centreonbroker_info_cols const& col) {
  return cfg_centreonbroker_info_size[col];
}

enum cfg_nagios_cols {
  cfg_nagios_nagios_name,
  cfg_nagios_log_file,
  cfg_nagios_cfg_dir,
  cfg_nagios_temp_file,
  cfg_nagios_status_file,
  cfg_nagios_check_result_path,
  cfg_nagios_max_check_result_file_age,
  cfg_nagios_nagios_user,
  cfg_nagios_nagios_group,
  cfg_nagios_log_rotation_method,
  cfg_nagios_log_archive_path,
  cfg_nagios_command_check_interval,
  cfg_nagios_command_file,
  cfg_nagios_downtime_file,
  cfg_nagios_comment_file,
  cfg_nagios_lock_file,
  cfg_nagios_state_retention_file,
  cfg_nagios_sleep_time,
  cfg_nagios_service_inter_check_delay_method,
  cfg_nagios_host_inter_check_delay_method,
  cfg_nagios_service_interleave_factor,
  cfg_nagios_low_service_flap_threshold,
  cfg_nagios_high_service_flap_threshold,
  cfg_nagios_low_host_flap_threshold,
  cfg_nagios_high_host_flap_threshold,
  cfg_nagios_host_perfdata_file,
  cfg_nagios_service_perfdata_file,
  cfg_nagios_host_perfdata_file_template,
  cfg_nagios_service_perfdata_file_template,
  cfg_nagios_date_format,
  cfg_nagios_illegal_object_name_chars,
  cfg_nagios_illegal_macro_output_chars,
  cfg_nagios_admin_email,
  cfg_nagios_admin_pager,
  cfg_nagios_nagios_comment,
  cfg_nagios_event_broker_options,
  cfg_nagios_debug_file,
  cfg_nagios_debug_level_opt,
  cfg_nagios_cfg_file,
};
constexpr static uint32_t cfg_nagios_size[]{
    255, 255,   255,   255, 255, 255, 255, 255, 255,   255, 255, 255, 255,
    255, 255,   255,   255, 10,  255, 255, 255, 255,   255, 255, 255, 255,
    255, 65534, 65534, 255, 255, 255, 255, 255, 65534, 255, 255, 200, 255,
};
constexpr uint32_t get_cfg_nagios_col_size(cfg_nagios_cols const& col) {
  return cfg_nagios_size[col];
}

enum cfg_nagios_broker_module_cols {
  cfg_nagios_broker_module_broker_module,
};
constexpr static uint32_t cfg_nagios_broker_module_size[]{
    255,
};
constexpr uint32_t get_cfg_nagios_broker_module_col_size(
    cfg_nagios_broker_module_cols const& col) {
  return cfg_nagios_broker_module_size[col];
}

enum cfg_resource_cols {
  cfg_resource_resource_name,
  cfg_resource_resource_line,
  cfg_resource_resource_comment,
};
constexpr static uint32_t cfg_resource_size[]{
    255,
    255,
    255,
};
constexpr uint32_t get_cfg_resource_col_size(cfg_resource_cols const& col) {
  return cfg_resource_size[col];
}

enum command_cols {
  command_command_name,
  command_command_line,
  command_command_example,
  command_command_comment,
};
constexpr static uint32_t command_size[]{
    200,
    65534,
    254,
    65534,
};
constexpr uint32_t get_command_col_size(command_cols const& col) {
  return command_size[col];
}

enum command_arg_description_cols {
  command_arg_description_macro_name,
  command_arg_description_macro_description,
};
constexpr static uint32_t command_arg_description_size[]{
    255,
    255,
};
constexpr uint32_t get_command_arg_description_col_size(
    command_arg_description_cols const& col) {
  return command_arg_description_size[col];
}

enum command_categories_cols {
  command_categories_category_name,
  command_categories_category_alias,
};
constexpr static uint32_t command_categories_size[]{
    255,
    255,
};
constexpr uint32_t get_command_categories_col_size(
    command_categories_cols const& col) {
  return command_categories_size[col];
}

enum connector_cols {
  connector_name,
  connector_description,
  connector_command_line,
};
constexpr static uint32_t connector_size[]{
    255,
    255,
    512,
};
constexpr uint32_t get_connector_col_size(connector_cols const& col) {
  return connector_size[col];
}

enum contact_cols {
  contact_contact_name,
  contact_contact_alias,
  contact_contact_passwd,
  contact_contact_lang,
  contact_contact_host_notification_options,
  contact_contact_service_notification_options,
  contact_contact_email,
  contact_contact_pager,
  contact_contact_address1,
  contact_contact_address2,
  contact_contact_address3,
  contact_contact_address4,
  contact_contact_address5,
  contact_contact_address6,
  contact_contact_comment,
  contact_contact_auth_type,
  contact_contact_ldap_dn,
  contact_contact_acl_group_list,
  contact_contact_autologin_key,
  contact_contact_charset,
};
constexpr static uint32_t contact_size[]{
    200, 200, 255, 255, 200,   200, 200,   200, 200, 200,
    200, 200, 200, 200, 65534, 255, 65534, 255, 255, 255,
};
constexpr uint32_t get_contact_col_size(contact_cols const& col) {
  return contact_size[col];
}

enum contact_feature_cols {
  contact_feature_feature,
  contact_feature_feature_version,
};
constexpr static uint32_t contact_feature_size[]{
    255,
    50,
};
constexpr uint32_t get_contact_feature_col_size(
    contact_feature_cols const& col) {
  return contact_feature_size[col];
}

enum contact_param_cols {
  contact_param_cp_key,
  contact_param_cp_value,
};
constexpr static uint32_t contact_param_size[]{
    255,
    255,
};
constexpr uint32_t get_contact_param_col_size(contact_param_cols const& col) {
  return contact_param_size[col];
}

enum contactgroup_cols {
  contactgroup_cg_name,
  contactgroup_cg_alias,
  contactgroup_cg_comment,
  contactgroup_cg_type,
  contactgroup_cg_ldap_dn,
};
constexpr static uint32_t contactgroup_size[]{
    200, 200, 65534, 10, 255,
};
constexpr uint32_t get_contactgroup_col_size(contactgroup_cols const& col) {
  return contactgroup_size[col];
}

enum cron_operation_cols {
  cron_operation_name,
  cron_operation_command,
};
constexpr static uint32_t cron_operation_size[]{
    254,
    254,
};
constexpr uint32_t get_cron_operation_col_size(cron_operation_cols const& col) {
  return cron_operation_size[col];
}

enum css_color_menu_cols {
  css_color_menu_css_name,
};
constexpr static uint32_t css_color_menu_size[]{
    255,
};
constexpr uint32_t get_css_color_menu_col_size(css_color_menu_cols const& col) {
  return css_color_menu_size[col];
}

enum custom_views_cols {
  custom_views_name,
  custom_views_layout,
};
constexpr static uint32_t custom_views_size[]{
    255,
    255,
};
constexpr uint32_t get_custom_views_col_size(custom_views_cols const& col) {
  return custom_views_size[col];
}

enum dependency_cols {
  dependency_dep_name,
  dependency_dep_description,
  dependency_execution_failure_criteria,
  dependency_notification_failure_criteria,
  dependency_dep_comment,
};
constexpr static uint32_t dependency_size[]{
    255, 255, 255, 255, 65534,
};
constexpr uint32_t get_dependency_col_size(dependency_cols const& col) {
  return dependency_size[col];
}

enum downtime_cols {
  downtime_dt_name,
  downtime_dt_description,
};
constexpr static uint32_t downtime_size[]{
    100,
    255,
};
constexpr uint32_t get_downtime_col_size(downtime_cols const& col) {
  return downtime_size[col];
}

enum downtime_cache_cols {
  downtime_cache_start_hour,
  downtime_cache_end_hour,
};
constexpr static uint32_t downtime_cache_size[]{
    255,
    255,
};
constexpr uint32_t get_downtime_cache_col_size(downtime_cache_cols const& col) {
  return downtime_cache_size[col];
}

enum downtime_period_cols {
  downtime_period_dtp_day_of_week,
  downtime_period_dtp_month_cycle,
  downtime_period_dtp_day_of_month,
};
constexpr static uint32_t downtime_period_size[]{
    15,
    100,
    100,
};
constexpr uint32_t get_downtime_period_col_size(
    downtime_period_cols const& col) {
  return downtime_period_size[col];
}

enum escalation_cols {
  escalation_esc_name,
  escalation_esc_alias,
  escalation_escalation_options1,
  escalation_escalation_options2,
  escalation_esc_comment,
};
constexpr static uint32_t escalation_size[]{
    255, 255, 255, 255, 65534,
};
constexpr uint32_t get_escalation_col_size(escalation_cols const& col) {
  return escalation_size[col];
}

enum extended_host_information_cols {
  extended_host_information_ehi_notes,
  extended_host_information_ehi_notes_url,
  extended_host_information_ehi_action_url,
  extended_host_information_ehi_icon_image_alt,
  extended_host_information_ehi_2d_coords,
  extended_host_information_ehi_3d_coords,
};
constexpr static uint32_t extended_host_information_size[]{
    65534, 65534, 65534, 200, 200, 200,
};
constexpr uint32_t get_extended_host_information_col_size(
    extended_host_information_cols const& col) {
  return extended_host_information_size[col];
}

enum extended_service_information_cols {
  extended_service_information_esi_notes,
  extended_service_information_esi_notes_url,
  extended_service_information_esi_action_url,
  extended_service_information_esi_icon_image_alt,
};
constexpr static uint32_t extended_service_information_size[]{
    65534,
    65534,
    65534,
    200,
};
constexpr uint32_t get_extended_service_information_col_size(
    extended_service_information_cols const& col) {
  return extended_service_information_size[col];
}

enum giv_components_template_cols {
  giv_components_template_name,
  giv_components_template_ds_name,
  giv_components_template_ds_color_line,
  giv_components_template_ds_color_area,
  giv_components_template_ds_color_area_warn,
  giv_components_template_ds_color_area_crit,
  giv_components_template_ds_transparency,
  giv_components_template_ds_legend,
  giv_components_template_comment,
};
constexpr static uint32_t giv_components_template_size[]{
    255, 200, 255, 255, 14, 14, 254, 200, 65534,
};
constexpr uint32_t get_giv_components_template_col_size(
    giv_components_template_cols const& col) {
  return giv_components_template_size[col];
}

enum giv_graphs_template_cols {
  giv_graphs_template_name,
  giv_graphs_template_vertical_label,
  giv_graphs_template_comment,
};
constexpr static uint32_t giv_graphs_template_size[]{
    200,
    200,
    65534,
};
constexpr uint32_t get_giv_graphs_template_col_size(
    giv_graphs_template_cols const& col) {
  return giv_graphs_template_size[col];
}

enum host_cols {
  host_command_command_id_arg1,
  host_command_command_id_arg2,
  host_host_name,
  host_host_alias,
  host_host_address,
  host_display_name,
  host_flap_detection_options,
  host_host_notification_options,
  host_host_stalking_options,
  host_host_snmp_community,
  host_host_snmp_version,
  host_host_comment,
  host_geo_coords,
};
constexpr static uint32_t host_size[]{
    65534, 65534, 200, 200, 255, 255, 255, 200, 200, 255, 255, 65534, 32,
};
constexpr uint32_t get_host_col_size(host_cols const& col) {
  return host_size[col];
}

enum hostcategories_cols {
  hostcategories_hc_name,
  hostcategories_hc_alias,
  hostcategories_hc_comment,
};
constexpr static uint32_t hostcategories_size[]{
    200,
    200,
    65534,
};
constexpr uint32_t get_hostcategories_col_size(hostcategories_cols const& col) {
  return hostcategories_size[col];
}

enum hostgroup_cols {
  hostgroup_hg_name,
  hostgroup_hg_alias,
  hostgroup_hg_notes,
  hostgroup_hg_notes_url,
  hostgroup_hg_action_url,
  hostgroup_geo_coords,
  hostgroup_hg_comment,
};
constexpr static uint32_t hostgroup_size[]{
    200, 200, 255, 255, 255, 32, 65534,
};
constexpr uint32_t get_hostgroup_col_size(hostgroup_cols const& col) {
  return hostgroup_size[col];
}

enum informations_cols {
  informations_key,
  informations_value,
};
constexpr static uint32_t informations_size[]{
    25,
    255,
};
constexpr uint32_t get_informations_col_size(informations_cols const& col) {
  return informations_size[col];
}

enum locale_cols {
  locale_locale_short_name,
  locale_locale_long_name,
  locale_locale_img,
};
constexpr static uint32_t locale_size[]{
    3,
    255,
    255,
};
constexpr uint32_t get_locale_col_size(locale_cols const& col) {
  return locale_size[col];
}

enum meta_service_cols {
  meta_service_meta_name,
  meta_service_meta_display,
  meta_service_notification_options,
  meta_service_regexp_str,
  meta_service_metric,
  meta_service_warning,
  meta_service_critical,
  meta_service_meta_comment,
  meta_service_geo_coords,
};
constexpr static uint32_t meta_service_size[]{
    254, 254, 255, 254, 255, 254, 254, 65534, 32,
};
constexpr uint32_t get_meta_service_col_size(meta_service_cols const& col) {
  return meta_service_size[col];
}

enum meta_service_relation_cols {
  meta_service_relation_msr_comment,
};
constexpr static uint32_t meta_service_relation_size[]{
    65534,
};
constexpr uint32_t get_meta_service_relation_col_size(
    meta_service_relation_cols const& col) {
  return meta_service_relation_size[col];
}

enum mod_bam_cols {
  mod_bam_name,
  mod_bam_description,
  mod_bam_notification_options,
  mod_bam_event_handler_args,
  mod_bam_graph_style,
  mod_bam_comment,
};
constexpr static uint32_t mod_bam_size[]{
    254, 254, 255, 254, 254, 65534,
};
constexpr uint32_t get_mod_bam_col_size(mod_bam_cols const& col) {
  return mod_bam_size[col];
}

enum mod_bam_ba_groups_cols {
  mod_bam_ba_groups_ba_group_name,
  mod_bam_ba_groups_ba_group_description,
};
constexpr static uint32_t mod_bam_ba_groups_size[]{
    255,
    255,
};
constexpr uint32_t get_mod_bam_ba_groups_col_size(
    mod_bam_ba_groups_cols const& col) {
  return mod_bam_ba_groups_size[col];
}

enum mod_bam_boolean_cols {
  mod_bam_boolean_name,
  mod_bam_boolean_expression,
  mod_bam_boolean_comments,
};
constexpr static uint32_t mod_bam_boolean_size[]{
    255,
    65534,
    65534,
};
constexpr uint32_t get_mod_bam_boolean_col_size(
    mod_bam_boolean_cols const& col) {
  return mod_bam_boolean_size[col];
}

enum mod_bam_impacts_cols {
  mod_bam_impacts_color,
};
constexpr static uint32_t mod_bam_impacts_size[]{
    7,
};
constexpr uint32_t get_mod_bam_impacts_col_size(
    mod_bam_impacts_cols const& col) {
  return mod_bam_impacts_size[col];
}

enum mod_bam_kpi_cols {
  mod_bam_kpi_comment,
};
constexpr static uint32_t mod_bam_kpi_size[]{
    65534,
};
constexpr uint32_t get_mod_bam_kpi_col_size(mod_bam_kpi_cols const& col) {
  return mod_bam_kpi_size[col];
}

enum mod_bam_user_preferences_cols {
  mod_bam_user_preferences_pref_key,
  mod_bam_user_preferences_pref_value,
};
constexpr static uint32_t mod_bam_user_preferences_size[]{
    255,
    255,
};
constexpr uint32_t get_mod_bam_user_preferences_col_size(
    mod_bam_user_preferences_cols const& col) {
  return mod_bam_user_preferences_size[col];
}

enum mod_ppm_categories_cols {
  mod_ppm_categories_name,
  mod_ppm_categories_slug,
  mod_ppm_categories_icon,
};
constexpr static uint32_t mod_ppm_categories_size[]{
    100,
    100,
    100,
};
constexpr uint32_t get_mod_ppm_categories_col_size(
    mod_ppm_categories_cols const& col) {
  return mod_ppm_categories_size[col];
}

enum mod_ppm_information_url_cols {
  mod_ppm_information_url_url,
};
constexpr static uint32_t mod_ppm_information_url_size[]{
    255,
};
constexpr uint32_t get_mod_ppm_information_url_col_size(
    mod_ppm_information_url_cols const& col) {
  return mod_ppm_information_url_size[col];
}

enum mod_ppm_pluginpack_cols {
  mod_ppm_pluginpack_name,
  mod_ppm_pluginpack_slug,
  mod_ppm_pluginpack_version,
  mod_ppm_pluginpack_status,
  mod_ppm_pluginpack_status_message,
  mod_ppm_pluginpack_changelog,
  mod_ppm_pluginpack_monitoring_procedure,
};
constexpr static uint32_t mod_ppm_pluginpack_size[]{
    255, 255, 50, 20, 255, 65534, 65534,
};
constexpr uint32_t get_mod_ppm_pluginpack_col_size(
    mod_ppm_pluginpack_cols const& col) {
  return mod_ppm_pluginpack_size[col];
}

enum mod_ppm_pluginpack_host_cols {
  mod_ppm_pluginpack_host_discovery_arguments,
};
constexpr static uint32_t mod_ppm_pluginpack_host_size[]{
    255,
};
constexpr uint32_t get_mod_ppm_pluginpack_host_col_size(
    mod_ppm_pluginpack_host_cols const& col) {
  return mod_ppm_pluginpack_host_size[col];
}

enum mod_ppm_pluginpack_service_cols {
  mod_ppm_pluginpack_service_discovery_command_options,
  mod_ppm_pluginpack_service_monitoring_protocol,
};
constexpr static uint32_t mod_ppm_pluginpack_service_size[]{
    100,
    100,
};
constexpr uint32_t get_mod_ppm_pluginpack_service_col_size(
    mod_ppm_pluginpack_service_cols const& col) {
  return mod_ppm_pluginpack_service_size[col];
}

enum mod_ppm_resultset_tpl_cols {
  mod_ppm_resultset_tpl_resultset_tpl,
};
constexpr static uint32_t mod_ppm_resultset_tpl_size[]{
    65534,
};
constexpr uint32_t get_mod_ppm_resultset_tpl_col_size(
    mod_ppm_resultset_tpl_cols const& col) {
  return mod_ppm_resultset_tpl_size[col];
}

enum modules_informations_cols {
  modules_informations_name,
  modules_informations_rname,
  modules_informations_mod_release,
  modules_informations_infos,
  modules_informations_author,
};
constexpr static uint32_t modules_informations_size[]{
    255, 255, 255, 65534, 255,
};
constexpr uint32_t get_modules_informations_col_size(
    modules_informations_cols const& col) {
  return modules_informations_size[col];
}

enum nagios_macro_cols {
  nagios_macro_macro_name,
};
constexpr static uint32_t nagios_macro_size[]{
    255,
};
constexpr uint32_t get_nagios_macro_col_size(nagios_macro_cols const& col) {
  return nagios_macro_size[col];
}

enum nagios_server_cols {
  nagios_server_name,
  nagios_server_ns_ip_address,
  nagios_server_init_script,
  nagios_server_init_system,
  nagios_server_monitoring_engine,
  nagios_server_nagios_bin,
  nagios_server_nagiostats_bin,
  nagios_server_nagios_perfdata,
  nagios_server_centreonbroker_cfg_path,
  nagios_server_centreonbroker_module_path,
  nagios_server_centreonconnector_path,
  nagios_server_ssh_private_key,
  nagios_server_init_script_centreontrapd,
  nagios_server_snmp_trapd_path_conf,
  nagios_server_engine_name,
  nagios_server_engine_version,
  nagios_server_centreonbroker_logs_path,
};
constexpr static uint32_t nagios_server_size[]{
    40,  255, 255, 255, 20,  255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255,
};
constexpr uint32_t get_nagios_server_col_size(nagios_server_cols const& col) {
  return nagios_server_size[col];
}

enum ods_view_details_cols {
  ods_view_details_metric_id,
  ods_view_details_rnd_color,
};
constexpr static uint32_t ods_view_details_size[]{
    12,
    7,
};
constexpr uint32_t get_ods_view_details_col_size(
    ods_view_details_cols const& col) {
  return ods_view_details_size[col];
}

enum on_demand_macro_command_cols {
  on_demand_macro_command_command_macro_name,
  on_demand_macro_command_command_macro_desciption,
};
constexpr static uint32_t on_demand_macro_command_size[]{
    255,
    65534,
};
constexpr uint32_t get_on_demand_macro_command_col_size(
    on_demand_macro_command_cols const& col) {
  return on_demand_macro_command_size[col];
}

enum on_demand_macro_host_cols {
  on_demand_macro_host_host_macro_name,
  on_demand_macro_host_host_macro_value,
  on_demand_macro_host_description,
};
constexpr static uint32_t on_demand_macro_host_size[]{
    255,
    4096,
    65534,
};
constexpr uint32_t get_on_demand_macro_host_col_size(
    on_demand_macro_host_cols const& col) {
  return on_demand_macro_host_size[col];
}

enum on_demand_macro_service_cols {
  on_demand_macro_service_svc_macro_name,
  on_demand_macro_service_svc_macro_value,
  on_demand_macro_service_description,
};
constexpr static uint32_t on_demand_macro_service_size[]{
    255,
    4096,
    65534,
};
constexpr uint32_t get_on_demand_macro_service_col_size(
    on_demand_macro_service_cols const& col) {
  return on_demand_macro_service_size[col];
}

enum options_cols {
  options_key,
  options_value,
};
constexpr static uint32_t options_size[]{
    255,
    255,
};
constexpr uint32_t get_options_col_size(options_cols const& col) {
  return options_size[col];
}

enum remote_servers_cols {
  remote_servers_ip,
  remote_servers_app_key,
  remote_servers_version,
  remote_servers_centreon_path,
};
constexpr static uint32_t remote_servers_size[]{
    16,
    40,
    16,
    255,
};
constexpr uint32_t get_remote_servers_col_size(remote_servers_cols const& col) {
  return remote_servers_size[col];
}

enum service_cols {
  service_service_description,
  service_service_alias,
  service_display_name,
  service_service_notification_options,
  service_service_stalking_options,
  service_service_comment,
  service_geo_coords,
  service_command_command_id_arg,
  service_command_command_id_arg2,
};
constexpr static uint32_t service_size[]{
    200, 255, 255, 200, 200, 65534, 32, 65534, 65534,
};
constexpr uint32_t get_service_col_size(service_cols const& col) {
  return service_size[col];
}

enum service_categories_cols {
  service_categories_sc_name,
  service_categories_sc_description,
};
constexpr static uint32_t service_categories_size[]{
    255,
    255,
};
constexpr uint32_t get_service_categories_col_size(
    service_categories_cols const& col) {
  return service_categories_size[col];
}

enum servicegroup_cols {
  servicegroup_sg_name,
  servicegroup_sg_alias,
  servicegroup_sg_comment,
  servicegroup_geo_coords,
};
constexpr static uint32_t servicegroup_size[]{
    200,
    200,
    65534,
    32,
};
constexpr uint32_t get_servicegroup_col_size(servicegroup_cols const& col) {
  return servicegroup_size[col];
}

enum session_cols {
  session_session_id,
  session_ip_address,
};
constexpr static uint32_t session_size[]{
    256,
    45,
};
constexpr uint32_t get_session_col_size(session_cols const& col) {
  return session_size[col];
}

enum task_cols {
  task_type,
  task_status,
};
constexpr static uint32_t task_size[]{
    40,
    40,
};
constexpr uint32_t get_task_col_size(task_cols const& col) {
  return task_size[col];
}

enum timeperiod_cols {
  timeperiod_tp_name,
  timeperiod_tp_alias,
  timeperiod_tp_sunday,
  timeperiod_tp_monday,
  timeperiod_tp_tuesday,
  timeperiod_tp_wednesday,
  timeperiod_tp_thursday,
  timeperiod_tp_friday,
  timeperiod_tp_saturday,
};
constexpr static uint32_t timeperiod_size[]{
    200, 200, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
};
constexpr uint32_t get_timeperiod_col_size(timeperiod_cols const& col) {
  return timeperiod_size[col];
}

enum timeperiod_exceptions_cols {
  timeperiod_exceptions_days,
  timeperiod_exceptions_timerange,
};
constexpr static uint32_t timeperiod_exceptions_size[]{
    255,
    255,
};
constexpr uint32_t get_timeperiod_exceptions_col_size(
    timeperiod_exceptions_cols const& col) {
  return timeperiod_exceptions_size[col];
}

enum timezone_cols {
  timezone_timezone_name,
  timezone_timezone_offset,
  timezone_timezone_dst_offset,
  timezone_timezone_description,
};
constexpr static uint32_t timezone_size[]{
    200,
    200,
    200,
    255,
};
constexpr uint32_t get_timezone_col_size(timezone_cols const& col) {
  return timezone_size[col];
}

enum topology_cols {
  topology_topology_name,
  topology_topology_url,
  topology_topology_url_opt,
  topology_topology_style_class,
  topology_topology_style_id,
  topology_topology_OnClick,
};
constexpr static uint32_t topology_size[]{
    255, 255, 255, 255, 255, 255,
};
constexpr uint32_t get_topology_col_size(topology_cols const& col) {
  return topology_size[col];
}

enum topology_JS_cols {
  topology_JS_o,
  topology_JS_PathName_js,
  topology_JS_Init,
};
constexpr static uint32_t topology_JS_size[]{
    12,
    65534,
    65534,
};
constexpr uint32_t get_topology_JS_col_size(topology_JS_cols const& col) {
  return topology_JS_size[col];
}

enum traps_cols {
  traps_traps_name,
  traps_traps_oid,
  traps_traps_args,
  traps_traps_execution_command,
  traps_traps_routing_value,
  traps_traps_routing_filter_services,
  traps_traps_output_transform,
  traps_traps_customcode,
  traps_traps_comments,
};
constexpr static uint32_t traps_size[]{
    255, 255, 65534, 255, 255, 255, 255, 65534, 65534,
};
constexpr uint32_t get_traps_col_size(traps_cols const& col) {
  return traps_size[col];
}

enum traps_group_cols {
  traps_group_traps_group_name,
};
constexpr static uint32_t traps_group_size[]{
    255,
};
constexpr uint32_t get_traps_group_col_size(traps_group_cols const& col) {
  return traps_group_size[col];
}

enum traps_matching_properties_cols {
  traps_matching_properties_tmo_regexp,
  traps_matching_properties_tmo_string,
};
constexpr static uint32_t traps_matching_properties_size[]{
    255,
    255,
};
constexpr uint32_t get_traps_matching_properties_col_size(
    traps_matching_properties_cols const& col) {
  return traps_matching_properties_size[col];
}

enum traps_preexec_cols {
  traps_preexec_tpe_string,
};
constexpr static uint32_t traps_preexec_size[]{
    512,
};
constexpr uint32_t get_traps_preexec_col_size(traps_preexec_cols const& col) {
  return traps_preexec_size[col];
}

enum traps_vendor_cols {
  traps_vendor_name,
  traps_vendor_alias,
  traps_vendor_description,
};
constexpr static uint32_t traps_vendor_size[]{
    254,
    254,
    65534,
};
constexpr uint32_t get_traps_vendor_col_size(traps_vendor_cols const& col) {
  return traps_vendor_size[col];
}

enum view_img_cols {
  view_img_img_name,
  view_img_img_path,
  view_img_img_comment,
};
constexpr static uint32_t view_img_size[]{
    255,
    255,
    65534,
};
constexpr uint32_t get_view_img_col_size(view_img_cols const& col) {
  return view_img_size[col];
}

enum view_img_dir_cols {
  view_img_dir_dir_name,
  view_img_dir_dir_alias,
  view_img_dir_dir_comment,
};
constexpr static uint32_t view_img_dir_size[]{
    255,
    255,
    65534,
};
constexpr uint32_t get_view_img_dir_col_size(view_img_dir_cols const& col) {
  return view_img_dir_size[col];
}

enum virtual_metrics_cols {
  virtual_metrics_vmetric_name,
  virtual_metrics_rpn_function,
  virtual_metrics_unit_name,
  virtual_metrics_comment,
};
constexpr static uint32_t virtual_metrics_size[]{
    255,
    255,
    32,
    65534,
};
constexpr uint32_t get_virtual_metrics_col_size(
    virtual_metrics_cols const& col) {
  return virtual_metrics_size[col];
}

enum widget_models_cols {
  widget_models_title,
  widget_models_description,
  widget_models_url,
  widget_models_version,
  widget_models_directory,
  widget_models_author,
  widget_models_email,
  widget_models_website,
  widget_models_keywords,
  widget_models_screenshot,
  widget_models_thumbnail,
};
constexpr static uint32_t widget_models_size[]{
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
};
constexpr uint32_t get_widget_models_col_size(widget_models_cols const& col) {
  return widget_models_size[col];
}

enum widget_parameters_cols {
  widget_parameters_parameter_name,
  widget_parameters_parameter_code_name,
  widget_parameters_default_value,
  widget_parameters_header_title,
  widget_parameters_require_permission,
};
constexpr static uint32_t widget_parameters_size[]{
    255, 255, 255, 255, 255,
};
constexpr uint32_t get_widget_parameters_col_size(
    widget_parameters_cols const& col) {
  return widget_parameters_size[col];
}

enum widget_parameters_field_type_cols {
  widget_parameters_field_type_ft_typename,
};
constexpr static uint32_t widget_parameters_field_type_size[]{
    50,
};
constexpr uint32_t get_widget_parameters_field_type_col_size(
    widget_parameters_field_type_cols const& col) {
  return widget_parameters_field_type_size[col];
}

enum widget_parameters_multiple_options_cols {
  widget_parameters_multiple_options_option_name,
  widget_parameters_multiple_options_option_value,
};
constexpr static uint32_t widget_parameters_multiple_options_size[]{
    255,
    255,
};
constexpr uint32_t get_widget_parameters_multiple_options_col_size(
    widget_parameters_multiple_options_cols const& col) {
  return widget_parameters_multiple_options_size[col];
}

enum widget_preferences_cols {
  widget_preferences_preference_value,
};
constexpr static uint32_t widget_preferences_size[]{
    255,
};
constexpr uint32_t get_widget_preferences_col_size(
    widget_preferences_cols const& col) {
  return widget_preferences_size[col];
}

enum widget_views_cols {
  widget_views_widget_order,
};
constexpr static uint32_t widget_views_size[]{
    255,
};
constexpr uint32_t get_widget_views_col_size(widget_views_cols const& col) {
  return widget_views_size[col];
}

enum widgets_cols {
  widgets_title,
};
constexpr static uint32_t widgets_size[]{
    255,
};
constexpr uint32_t get_widgets_col_size(widgets_cols const& col) {
  return widgets_size[col];
}

enum ws_token_cols {
  ws_token_token,
};
constexpr static uint32_t ws_token_size[]{
    100,
};
constexpr uint32_t get_ws_token_col_size(ws_token_cols const& col) {
  return ws_token_size[col];
}

enum acknowledgements_cols {
  acknowledgements_author,
  acknowledgements_comment_data,
};
constexpr static uint32_t acknowledgements_size[]{
    64,
    255,
};
constexpr uint32_t get_acknowledgements_col_size(
    acknowledgements_cols const& col) {
  return acknowledgements_size[col];
}

enum comments_cols {
  comments_author,
  comments_data,
};
constexpr static uint32_t comments_size[]{
    64,
    65534,
};
constexpr uint32_t get_comments_col_size(comments_cols const& col) {
  return comments_size[col];
}

enum config_cols {
  config_RRDdatabase_path,
  config_RRDdatabase_status_path,
  config_RRDdatabase_nagios_stats_path,
  config_nagios_log_file,
};
constexpr static uint32_t config_size[]{
    255,
    255,
    255,
    255,
};
constexpr uint32_t get_config_col_size(config_cols const& col) {
  return config_size[col];
}

enum customvariables_cols {
  customvariables_name,
  customvariables_default_value,
  customvariables_value,
};
constexpr static uint32_t customvariables_size[]{
    255,
    255,
    255,
};
constexpr uint32_t get_customvariables_col_size(
    customvariables_cols const& col) {
  return customvariables_size[col];
}

enum downtimes_cols {
  downtimes_author,
  downtimes_comment_data,
};
constexpr static uint32_t downtimes_size[]{
    64,
    65534,
};
constexpr uint32_t get_downtimes_col_size(downtimes_cols const& col) {
  return downtimes_size[col];
}

enum eventhandlers_cols {
  eventhandlers_command_args,
  eventhandlers_command_line,
  eventhandlers_output,
};
constexpr static uint32_t eventhandlers_size[]{
    255,
    255,
    255,
};
constexpr uint32_t get_eventhandlers_col_size(eventhandlers_cols const& col) {
  return eventhandlers_size[col];
}

enum hostgroups_cols {
  hostgroups_name,
};
constexpr static uint32_t hostgroups_size[]{
    255,
};
constexpr uint32_t get_hostgroups_col_size(hostgroups_cols const& col) {
  return hostgroups_size[col];
}

enum hosts_cols {
  hosts_name,
  hosts_action_url,
  hosts_address,
  hosts_alias,
  hosts_check_command,
  hosts_check_period,
  hosts_command_line,
  hosts_display_name,
  hosts_event_handler,
  hosts_icon_image,
  hosts_icon_image_alt,
  hosts_notes,
  hosts_notes_url,
  hosts_notification_period,
  hosts_output,
  hosts_perfdata,
  hosts_statusmap_image,
  hosts_timezone,
};
constexpr static uint32_t hosts_size[]{
    255, 2048, 75,  100,  65534, 75,    65534, 100, 255,
    255, 255,  512, 2048, 75,    65534, 65534, 255, 64,
};
constexpr uint32_t get_hosts_col_size(hosts_cols const& col) {
  return hosts_size[col];
}

enum hosts_hosts_dependencies_cols {
  hosts_hosts_dependencies_dependency_period,
  hosts_hosts_dependencies_execution_failure_options,
  hosts_hosts_dependencies_notification_failure_options,
};
constexpr static uint32_t hosts_hosts_dependencies_size[]{
    75,
    15,
    15,
};
constexpr uint32_t get_hosts_hosts_dependencies_col_size(
    hosts_hosts_dependencies_cols const& col) {
  return hosts_hosts_dependencies_size[col];
}

enum index_data_cols {
  index_data_host_name,
  index_data_service_description,
};
constexpr static uint32_t index_data_size[]{
    255,
    255,
};
constexpr uint32_t get_index_data_col_size(index_data_cols const& col) {
  return index_data_size[col];
}

enum instances_cols {
  instances_name,
  instances_address,
  instances_description,
  instances_engine,
  instances_global_host_event_handler,
  instances_global_service_event_handler,
  instances_version,
};
constexpr static uint32_t instances_size[]{
    255, 128, 128, 64, 65534, 65534, 16,
};
constexpr uint32_t get_instances_col_size(instances_cols const& col) {
  return instances_size[col];
}

enum log_action_cols {
  log_action_object_type,
  log_action_object_name,
  log_action_action_type,
};
constexpr static uint32_t log_action_size[]{
    255,
    255,
    255,
};
constexpr uint32_t get_log_action_col_size(log_action_cols const& col) {
  return log_action_size[col];
}

enum log_action_modification_cols {
  log_action_modification_field_name,
  log_action_modification_field_value,
};
constexpr static uint32_t log_action_modification_size[]{
    255,
    65534,
};
constexpr uint32_t get_log_action_modification_col_size(
    log_action_modification_cols const& col) {
  return log_action_modification_size[col];
}

enum log_archive_last_status_cols {
  log_archive_last_status_host_name,
  log_archive_last_status_service_description,
  log_archive_last_status_status,
};
constexpr static uint32_t log_archive_last_status_size[]{
    255,
    255,
    255,
};
constexpr uint32_t get_log_archive_last_status_col_size(
    log_archive_last_status_cols const& col) {
  return log_archive_last_status_size[col];
}

enum log_traps_cols {
  log_traps_host_name,
  log_traps_ip_address,
  log_traps_agent_host_name,
  log_traps_agent_ip_address,
  log_traps_trap_oid,
  log_traps_trap_name,
  log_traps_vendor,
  log_traps_severity_name,
  log_traps_output_message,
};
constexpr static uint32_t log_traps_size[]{
    255, 255, 255, 255, 512, 255, 255, 255, 2048,
};
constexpr uint32_t get_log_traps_col_size(log_traps_cols const& col) {
  return log_traps_size[col];
}

enum log_traps_args_cols {
  log_traps_args_arg_oid,
  log_traps_args_arg_value,
};
constexpr static uint32_t log_traps_args_size[]{
    255,
    255,
};
constexpr uint32_t get_log_traps_args_col_size(log_traps_args_cols const& col) {
  return log_traps_args_size[col];
}

enum logs_cols {
  logs_host_name,
  logs_instance_name,
  logs_notification_cmd,
  logs_notification_contact,
  logs_output,
  logs_service_description,
};
constexpr static uint32_t logs_size[]{
    255, 255, 255, 255, 65534, 255,
};
constexpr uint32_t get_logs_col_size(logs_cols const& col) {
  return logs_size[col];
}

enum metrics_cols {
  metrics_metric_name,
  metrics_unit_name,
};
constexpr static uint32_t metrics_size[]{
    255,
    32,
};
constexpr uint32_t get_metrics_col_size(metrics_cols const& col) {
  return metrics_size[col];
}

enum modules_cols {
  modules_args,
  modules_filename,
};
constexpr static uint32_t modules_size[]{
    255,
    255,
};
constexpr uint32_t get_modules_col_size(modules_cols const& col) {
  return modules_size[col];
}

enum nagios_stats_cols {
  nagios_stats_stat_key,
  nagios_stats_stat_value,
  nagios_stats_stat_label,
};
constexpr static uint32_t nagios_stats_size[]{
    255,
    255,
    255,
};
constexpr uint32_t get_nagios_stats_col_size(nagios_stats_cols const& col) {
  return nagios_stats_size[col];
}

enum notifications_cols {
  notifications_ack_author,
  notifications_ack_data,
  notifications_command_name,
  notifications_contact_name,
  notifications_output,
};
constexpr static uint32_t notifications_size[]{
    255, 65534, 255, 255, 65534,
};
constexpr uint32_t get_notifications_col_size(notifications_cols const& col) {
  return notifications_size[col];
}

enum schemaversion_cols {
  schemaversion_software,
};
constexpr static uint32_t schemaversion_size[]{
    128,
};
constexpr uint32_t get_schemaversion_col_size(schemaversion_cols const& col) {
  return schemaversion_size[col];
}

enum servicegroups_cols {
  servicegroups_name,
};
constexpr static uint32_t servicegroups_size[]{
    255,
};
constexpr uint32_t get_servicegroups_col_size(servicegroups_cols const& col) {
  return servicegroups_size[col];
}

enum services_cols {
  services_description,
  services_action_url,
  services_check_command,
  services_check_period,
  services_command_line,
  services_display_name,
  services_event_handler,
  services_failure_prediction_options,
  services_icon_image,
  services_icon_image_alt,
  services_notes,
  services_notes_url,
  services_notification_period,
  services_output,
  services_perfdata,
};
constexpr static uint32_t services_size[]{
    255, 2048, 65534, 75,   65534, 160,   255,   64,
    255, 255,  512,   2048, 75,    65534, 65534,
};
constexpr uint32_t get_services_col_size(services_cols const& col) {
  return services_size[col];
}

enum services_services_dependencies_cols {
  services_services_dependencies_dependency_period,
  services_services_dependencies_execution_failure_options,
  services_services_dependencies_notification_failure_options,
};
constexpr static uint32_t services_services_dependencies_size[]{
    75,
    15,
    15,
};
constexpr uint32_t get_services_services_dependencies_col_size(
    services_services_dependencies_cols const& col) {
  return services_services_dependencies_size[col];
}

enum severities_cols {
  severities_name,
};
constexpr static uint32_t severities_size[]{
    255,
};
constexpr uint32_t get_severities_col_size(severities_cols const& col) {
  return severities_size[col];
}

enum tags_cols {
  tags_name,
};
constexpr static uint32_t tags_size[]{
    255,
};
constexpr uint32_t get_tags_col_size(tags_cols const& col) {
  return tags_size[col];
}

enum resources_cols {
  resources_name,
  resources_alias,
  resources_address,
  resources_parent_name,
  resources_notes_url,
  resources_notes,
  resources_action_url,
  resources_output,
};
constexpr static uint32_t resources_size[]{
    255, 255, 255, 255, 2048, 512, 2048, 65534,
};
constexpr uint32_t get_resources_col_size(resources_cols const& col) {
  return resources_size[col];
}

enum mod_bam_reporting_cols {
  mod_bam_reporting_host_name,
  mod_bam_reporting_service_description,
};
constexpr static uint32_t mod_bam_reporting_size[]{
    255,
    255,
};
constexpr uint32_t get_mod_bam_reporting_col_size(
    mod_bam_reporting_cols const& col) {
  return mod_bam_reporting_size[col];
}

enum mod_bam_reporting_status_cols {
  mod_bam_reporting_status_host_name,
  mod_bam_reporting_status_service_description,
  mod_bam_reporting_status_status,
};
constexpr static uint32_t mod_bam_reporting_status_size[]{
    255,
    255,
    255,
};
constexpr uint32_t get_mod_bam_reporting_status_col_size(
    mod_bam_reporting_status_cols const& col) {
  return mod_bam_reporting_status_size[col];
}

enum mod_bam_kpi_logs_cols {
  mod_bam_kpi_logs_output,
  mod_bam_kpi_logs_kpi_name,
  mod_bam_kpi_logs_perfdata,
};
constexpr static uint32_t mod_bam_kpi_logs_size[]{
    255,
    255,
    65534,
};
constexpr uint32_t get_mod_bam_kpi_logs_col_size(
    mod_bam_kpi_logs_cols const& col) {
  return mod_bam_kpi_logs_size[col];
}

enum mod_bam_reporting_bv_cols {
  mod_bam_reporting_bv_bv_name,
  mod_bam_reporting_bv_bv_description,
};
constexpr static uint32_t mod_bam_reporting_bv_size[]{
    255,
    65534,
};
constexpr uint32_t get_mod_bam_reporting_bv_col_size(
    mod_bam_reporting_bv_cols const& col) {
  return mod_bam_reporting_bv_size[col];
}

enum mod_bam_logs_cols {
  mod_bam_logs_status,
};
constexpr static uint32_t mod_bam_logs_size[]{
    255,
};
constexpr uint32_t get_mod_bam_logs_col_size(mod_bam_logs_cols const& col) {
  return mod_bam_logs_size[col];
}

enum mod_bam_reporting_ba_cols {
  mod_bam_reporting_ba_ba_name,
  mod_bam_reporting_ba_ba_description,
};
constexpr static uint32_t mod_bam_reporting_ba_size[]{
    254,
    65534,
};
constexpr uint32_t get_mod_bam_reporting_ba_col_size(
    mod_bam_reporting_ba_cols const& col) {
  return mod_bam_reporting_ba_size[col];
}

enum mod_bam_reporting_kpi_cols {
  mod_bam_reporting_kpi_kpi_name,
  mod_bam_reporting_kpi_ba_name,
  mod_bam_reporting_kpi_host_name,
  mod_bam_reporting_kpi_service_description,
  mod_bam_reporting_kpi_kpi_ba_name,
  mod_bam_reporting_kpi_meta_service_name,
  mod_bam_reporting_kpi_boolean_name,
};
constexpr static uint32_t mod_bam_reporting_kpi_size[]{
    255, 254, 255, 255, 254, 254, 255,
};
constexpr uint32_t get_mod_bam_reporting_kpi_col_size(
    mod_bam_reporting_kpi_cols const& col) {
  return mod_bam_reporting_kpi_size[col];
}

enum mod_bam_reporting_kpi_events_cols {
  mod_bam_reporting_kpi_events_first_output,
  mod_bam_reporting_kpi_events_first_perfdata,
};
constexpr static uint32_t mod_bam_reporting_kpi_events_size[]{
    65534,
    45,
};
constexpr uint32_t get_mod_bam_reporting_kpi_events_col_size(
    mod_bam_reporting_kpi_events_cols const& col) {
  return mod_bam_reporting_kpi_events_size[col];
}

enum mod_bam_reporting_timeperiods_exceptions_cols {
  mod_bam_reporting_timeperiods_exceptions_daterange,
  mod_bam_reporting_timeperiods_exceptions_timerange,
};
constexpr static uint32_t mod_bam_reporting_timeperiods_exceptions_size[]{
    255,
    255,
};
constexpr uint32_t get_mod_bam_reporting_timeperiods_exceptions_col_size(
    mod_bam_reporting_timeperiods_exceptions_cols const& col) {
  return mod_bam_reporting_timeperiods_exceptions_size[col];
}

enum mod_bam_reporting_timeperiods_cols {
  mod_bam_reporting_timeperiods_name,
  mod_bam_reporting_timeperiods_sunday,
  mod_bam_reporting_timeperiods_monday,
  mod_bam_reporting_timeperiods_tuesday,
  mod_bam_reporting_timeperiods_wednesday,
  mod_bam_reporting_timeperiods_thursday,
  mod_bam_reporting_timeperiods_friday,
  mod_bam_reporting_timeperiods_saturday,
};
constexpr static uint32_t mod_bam_reporting_timeperiods_size[]{
    200, 200, 200, 200, 200, 200, 200, 200,
};
constexpr uint32_t get_mod_bam_reporting_timeperiods_col_size(
    mod_bam_reporting_timeperiods_cols const& col) {
  return mod_bam_reporting_timeperiods_size[col];
}

CCB_END()

#endif /* __TABLE_MAX_SIZE_HH__ */