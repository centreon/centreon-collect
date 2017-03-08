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

/* Get the socket info */
$socket = $oreon->optGen['broker_socket_path'];

if (trim($socket) !== '') {
    /* Get endpoint */
    $query = "SELECT config_value FROM cfg_centreonbroker_info WHERE config_key = 'name' AND config_id = (SELECT config_id FROM cfg_centreonbroker_info where config_key = 'type' AND config_value = 'db_cfg_reader') AND config_group_id = (SELECT config_group_id FROM cfg_centreonbroker_info where config_key = 'type' AND config_value = 'db_cfg_reader')";
    $res = $pearDB->query($query);
    $row = $res->fetchRow();
    if ($row) {
        $endpoint = $row['config_value'];
        foreach ($tab_server as $host) {
            if ($host['localhost'] == 0) {
                sendCommandBySocket("EXECUTE;" . $endpoint . ";SYNC_CFG_DB;" . $host['id'], 'unix://' . $socket);
            }
        }
    }
}