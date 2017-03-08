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
 * boolean form 
 */
$help['name'] =  _("Name of boolean KPI");
$help['expression'] =  _("Expression of the boolean rule. Use the editor to complete the expression and evaluate it with the button. It is also possible to simulate the service statuses.");
$help['bool_state[bool_state]'] =  _("Impact will be applied depending on the result of the expression");
$help['comments'] =  _("Comments regarding the boolean KPI");
$help['activate[activate]'] =  _("Whether or not the boolean KPI is enabled");
