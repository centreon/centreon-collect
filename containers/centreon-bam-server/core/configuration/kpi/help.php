<?php
/*
 * Centreon
 *
 * Source Copyright 2005-2016 Centreon
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@centreon.com
 *
 */

$help = array();
/**
 * regular kpi form
 */
$help['config_mode[config_mode]'] = _("Configuration mode");
$help['kpi_type'] = _("Type of KPI");
$help['warning_impact_regular'] = _("Impact that will be applied when KPI is in Warning state");
$help['critical_impact_regular'] = _("Impact that will be applied when KPI is in Critical state");
$help['unknown_impact_regular'] = _("Impact that will be applied when KPI is in Unknown state");
$help['warning_impact'] = _("Impact that will be applied when KPI is in Warning state");
$help['critical_impact'] = _("Impact that will be applied when KPI is in Critical state");
$help['unknown_impact'] = _("Impact that will be applied when KPI is in Unknown state");
$help['ba_list-f[]'] = _("KPI will be linked to these business activities");

/**
 * multiple kpi form 
 */
$help['obj_type'] = _("Object type to scan");
$help['host_list'] = _("Hosts");
$help['hg_list'] = _("Host groups");
$help['sg_list'] = _("Service groups");

/**
 * CSV load 
 */
$help['filename'] =  _("Upload an existing CSV file");
$help['kpitype[kpi_type]'] =  _("Type of KPI");
$help['manualDef'] =  _("You can define the KPI to import manually");

