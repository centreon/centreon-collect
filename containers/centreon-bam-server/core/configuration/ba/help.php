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
$help['ba_name'] = _("Name of business activity");
$help['ba_desc'] = _("Description of business activity");
$help['ba_warning'] = _("When level is below this threshold, the business activity will be in Warning state");
$help['ba_critical'] = _("When level is below this threshold, the business activity will be in Critical state");
$help['id_reporting_period'] = _("Reporting will be based on this very period");
$help['bam_contact-f[]'] = _("Contact groups that will be notified when business activity goes into Warning/Critical state");
$help['id_notif_period'] = _("Time period during which notification can take place");
$help['notif_interval'] = _("Notification interval length");
$help['notifOpts[r]'] = _("States for which notifications will be sent out");
$help['notifications_enabled[notifications_enabled]'] = _("Whether or not notification is enabled");
$help['bam_comment'] = _("Comments regarding the business activity");
$help['bam_activate[bam_activate]'] = _("Whether or not business activity is enabled");
$help['inherit_kpi_downtimes[inherit_kpi_downtimes]'] = _("(Broker >= 3) Whether or not the business activity has to inherit planned downtimes from its KPI. See documentation for more information on the feature.");
$help['bam_esc-f[]'] = _("Escalations rules that are applied to this business activity");
$help['notification_failure_criteria[o]'] = _("Refer to the official documentation of your monitoring engine");
$help['execution_failure_criteria[o]'] = _("Refer to the official documentation of your monitoring engine");
$help['inherits_parent[inherits_parent]'] = _("Whether or not dependency inheritance is applied");
$help['dep_bamParents-f[]'] = _("Parent business activity");
$help['dep_bamChilds-f[]'] = _("Child business activity");
$help['ba_group_list-f[]'] = _("Business view(s) of this business activity");
$help['icon'] = _("Icon that represents the business activity");
$help['additional_poller'] = _("Possibility to display this Business Activity on poller which have Centreon Poller display module");

$help['reporting_timeperiods-f[]'] = _("Those time periods will be used by Centreon BI reports.");
$help['sla_month_percent_warn'] = _('Percentage of time during which the BA was in a Warning status on a monthly basis');
$help['sla_month_percent_crit'] = _('Percentage of time during which the BA was in a Critical status on a monthly basis');
$help['sla_month_duration_warn'] = _('Amount of time during which the BA was in a Warning status on a monthly basis');
$help['sla_month_duration_crit'] = _('Amount of time during which the BA was in a Critical status on a monthly basis');
