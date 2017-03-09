<?php
/*
 * Copyright 2016 Centreon.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* init $pearDB if var is not defined */
if (!isset($pearDB)) {
    require_once $centreon_path . 'www/class/CentreonDB.php';
    $pearDB = new CentreonDB();
}

/* retrieve the config id of the central broker */
$sql = "SELECT config_id "
    . "FROM cfg_centreonbroker "
    . "WHERE config_filename = 'central-broker.xml'";
$stmt = $pearDB->query($sql);

/* set config id */
$configId = null;
if ($stmt->numRows()) {
    $row = $stmt->fetchRow();
    $configId = $row['config_id'];
}

/* retrieve tag id for output */
$sql = "SELECT cb_tag_id FROM cb_tag WHERE tagname = 'output'";
$stmt = $pearDB->query($sql);
$row = $stmt->fetchRow();
$outputTagId = $row['cb_tag_id'];

$sql = "SELECT cb_type_id, type_shortname FROM cb_type WHERE type_shortname ='discovery'";
$stmt = $pearDB->query($sql);
$row = $stmt->fetchRow();
$cbTypeId = $row['cb_type_id'];

/* retrieve last group of output */
if (!is_null($configId)) {
    $sql = "SELECT MAX(config_group_id) as last_group 
		FROM cfg_centreonbroker_info 
		WHERE config_group = 'output' 
		AND config_id = {$configId}";
    $stmt = $pearDB->query($sql);
    $row = $stmt->fetchRow();
    $lastGroup = $row['last_group'];

    /* insert configuration keys */
    $configGroupMonitoring = $lastGroup + 1;


    /*broker cfg path */
    $sql = "SELECT `centreonbroker_cfg_path` FROM `nagios_server` where `name` = 'Central'";
    $stmt = $pearDB->query($sql);
    $row = $stmt->fetchRow();
    $brokerCfgPath = $row['centreonbroker_cfg_path'];

    $automationConfigData = array(
        'blockId' => "{$outputTagId}_{$cbTypeId}",
        'name' => 'DiscoveryEngine',
        'type' => 'discovery',
        'cfg_file' => $brokerCfgPath . '/discovery.cfg'
    );

    $sql = "INSERT INTO cfg_centreonbroker_info 
		(config_id, config_key, config_value, config_group, config_group_id) VALUES 
		({$configId}, '%s', '%s', 'output', %s)";

    /* monitoring */
    $tempSql = "SELECT config_id
                FROM cfg_centreonbroker_info
                WHERE config_id = {$configId}
                AND config_key = 'name'
                AND config_value = 'DiscoveryEngine'";
    $tempStmt = $pearDB->query($tempSql);

    if (!$tempStmt->numRows()) {
        foreach ($automationConfigData as $autConfigKey => $autConfigValue) {
            $pearDB->query(
                sprintf(
                    $sql,
                    $autConfigKey,
                    $autConfigValue,
                    $configGroupMonitoring
                )
            );
        }
    }
}
