<?php
/*
 * Copyright 2005-2016 Centreon
 * Centreon is developped by : Julien Mathis and Romain Le Merlus under
 * GPL Licence 2.0.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation ; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses>.
 *
 * Linking this program statically or dynamically with other modules is making a
 * combined work based on this program. Thus, the terms and conditions of the GNU
 * General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this program give Centreon
 * permission to link this program with independent modules to produce an executable,
 * regardless of the license terms of these independent modules, and to copy and
 * distribute the resulting executable under terms of Centreon choice, provided that
 * Centreon also meet, for each linked independent module, the terms  and conditions
 * of the license of that module. An independent module is a module which is not
 * derived from this program. If you modify this program, you may extend this
 * exception to your version of the program, but you are not obliged to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 *
 * For more information : contact@centreon.com
 *
 *
 */

include_once _CENTREON_PATH_ . "/www/class/centreonDB.class.php";
include_once _CENTREON_PATH_ . "/www/class/centreonHosttemplates.class.php";

$pearDB = new CentreonDB('centreon');
$hostTemplateObj = new CentreonHosttemplates($pearDB);

// Lock original host template
$query = 'UPDATE host '
    . 'SET host_locked = 1 '
    . 'WHERE host_id IN ( '
    . 'SELECT host_id FROM mod_pp_host_templates'
    .') ';
$pearDB->query($query);


$query = 'SELECT h.host_id, h.host_name, h.host_alias '
    . 'FROM host h, mod_pp_host_templates ppht '
    . 'WHERE h.host_id = ppht.host_id ';
$res = $pearDB->query($query);

while ($row = $res->fetchRow()) {
    $customHTName = $row['host_name'] . '-custom';

    // Update custom host template name if already created
    $query = 'UPDATE host '
        . 'SET host_name = "' . $customHTName . '-old" '
        . 'WHERE host_name = "' . $customHTName . '" ';
    $pearDB->query($query);

    // Create custom host template
    $customHostTemplate = array(
        'host_name' => $customHTName,
        'host_alias' => $row['host_alias'],
        'host_register' => '0',
        'host_locked' => '0',
        'host_activate' => array(
            'host_activate' => '1'
        )
    );
    $hostTemplateObj->insert($customHostTemplate);

    $query = 'SELECT host_id '
        . 'FROM host '
        . 'WHERE host_name = "' . $customHTName . '" ';
    $res2 = $pearDB->query($query);
    $row2 = $res2->fetchRow();
    $customHTId = $row2['host_id'];

    // Update host to host template relation (from original ht to custom ht)
    $query = 'UPDATE host_template_relation '
        . 'SET host_tpl_id = (SELECT host_id FROM host WHERE host_name = "' . $customHTName . '" LIMIT 1) '
        . 'WHERE host_tpl_id = ' . $row['host_id'] . ' ';
    $pearDB->query($query);

    // Create relation between custom ht and original ht
    $query = 'INSERT INTO host_template_relation (host_host_id, host_tpl_id) '
        . 'VALUES( '. $customHTId . ', "' . $row['host_id'] . '" )';
    $pearDB->query($query);

    // Update service template relations (from original ht to custom ht)
    $query = 'UPDATE host_service_relation '
        . 'SET host_host_id = ' . $customHTId . ' '
        . 'WHERE host_host_id = ' . $row['host_id'] . ' ';
    $pearDB->query($query);
}

$query = 'SELECT s.service_id, s.service_description '
    . 'FROM service s, mod_pp_service_templates ppst '
    . 'WHERE s.service_id = ppst.service_id ';
$res = $pearDB->query($query);

while ($row = $res->fetchRow()) {
    // Update service template names (-Custom to -custom)
    $query = 'UPDATE service '
        . 'SET service_description = "' . $row['service_description'] . '-custom" '
        . 'WHERE service_description = "' . $row['service_description'] . '-Custom" ';
    $pearDB->query($query);
}

?>

