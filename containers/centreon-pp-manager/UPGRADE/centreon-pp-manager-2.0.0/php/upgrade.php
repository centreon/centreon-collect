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

$pearDB = new CentreonDB('centreon');

if (isset($pearDB)) {
    $querySelect = "SELECT resource_id FROM cfg_resource WHERE resource_name = '\$CENTREONPLUGINS\$';";
    $res = $pearDB->query($querySelect);

    /* If macro doesn't exist*/
    if ($res->numRows() == 0) {
        $queryInsert = "INSERT INTO cfg_resource(resource_id, resource_name, resource_line, resource_comment, resource_activate) VALUES(NULL, '\$CENTREONPLUGINS\$', '/usr/lib/centreon/plugins/', 'Directory of Centreon Plugins', '1');";
        $pearDB->query($queryInsert);

        $querySelect = "SELECT resource_id FROM cfg_resource WHERE resource_name = '\$CENTREONPLUGINS\$';";
        $res = $pearDB->query($querySelect);
    }
    $row = $res->fetchRow();
    $resource_id = $row['resource_id'];

    /* Insert relation between macro and poller  */
    $querySelect = "SELECT id FROM nagios_server;";
    $res = $pearDB->query($querySelect);
    while ($row = $res->fetchRow()) {
        $queryCheck = "SELECT 1 FROM cfg_resource_instance_relations WHERE resource_id = '" . $resource_id . "' AND instance_id = '" . $row['id'] . "';";
        $result = $pearDB->query($queryCheck);

        /* If relation doesn't exist */
        if ($result->numRows() == 0) {
            $queryUpdate = "INSERT INTO cfg_resource_instance_relations(resource_id, instance_id) VALUES ('" . $resource_id . "', '" . $row['id'] . "');";
            $pearDB->query($queryUpdate);
        }
    }
}

/* Create license directory */
$licenseDir = _CENTREON_ETC_ . '/license.d';
if (is_dir($licenseDir)) {
    /* Create license file */
    $licenseFile = $licenseDir . '/centreon-pp-manager.license';
    if (!file_exists($licenseFile)) {
        touch($licenseFile);
    }
}
