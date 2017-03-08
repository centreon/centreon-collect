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

/* init $pearDB if var is not defined */
if (!isset($pearDB)) {
	require_once $centreon_path . 'www/modules/centreon-bam-server/core/class/Centreon/Db.php';
	$pearDB = new CentreonBam_Db();
}

/* retrieve the config id of the central broker */
$sql = "SELECT config_id 
	FROM cfg_centreonbroker 
	WHERE config_filename = 'central-broker.xml'";
$stmt = $pearDB->query($sql);

/* set config id */
$configId = null;
if ($stmt->numRows()) {
	$row = $stmt->fetchRow();
	$configId = $row['config_id'];
}

/* insert the new module */
$sql = "SELECT cb_module_id 
        FROM cb_module 
        WHERE name = 'BAM'";
$stmt = $pearDB->query($sql);

if (!$stmt->numRows()) {
    $sql = "INSERT INTO cb_module (name, libname, loading_pos, is_activated) 
            VALUES ('BAM', 'bam.so', 20, 1)";
    $pearDB->query($sql);
}

/* retrieve module id */
$sql = "SELECT MAX(cb_module_id) as last_id FROM cb_module";
$stmt = $pearDB->query($sql);
$row = $stmt->fetchRow();
$bamModuleId = $row['last_id'];

$sql = "SELECT cb_module_id
        FROM cb_type
        WHERE type_shortname = 'bam'";
$stmt = $pearDB->query($sql);

if (!$stmt->numRows()) {
    $sql = "INSERT INTO cb_type (type_name, type_shortname, cb_module_id)
            VALUES ('Monitoring engine (BAM)', 'bam', {$bamModuleId})";
    $pearDB->query($sql);
}

$sql = "SELECT cb_module_id
        FROM cb_type
        WHERE type_shortname = 'bam_bi'";
$stmt = $pearDB->query($sql);

if (!$stmt->numRows()) {
    $sql = "INSERT INTO cb_type (type_name, type_shortname, cb_module_id)
            VALUES ('BI engine (BAM)', 'bam_bi', {$bamModuleId})";
    $pearDB->query($sql);
}

/* retrieve tag id for output */
$sql = "SELECT cb_tag_id FROM cb_tag WHERE tagname = 'output'";
$stmt = $pearDB->query($sql);
$row = $stmt->fetchRow();
$outputTagId = $row['cb_tag_id'];

/* insert new command_file field */
$sql = "SELECT fieldname FROM cb_field WHERE fieldname = 'command_file'";
$stmt = $pearDB->query($sql);

if (!$stmt->numRows()) {
    $sql = "INSERT INTO cb_field (fieldname, displayname, description, fieldtype) VALUES 
            ('command_file', 'Command file path', 'File for external commands', 'text')";
    $pearDB->query($sql);
}

/* insert new filter */
$sql = "SELECT cb_list_id FROM cb_list_values WHERE value_name = 'BAM' AND value_value = 'bam'";
$stmt = $pearDB->query($sql);

if (!$stmt->numRows()) {
    $sql = "INSERT INTO cb_list_values (cb_list_id, value_name, value_value) 
            (SELECT l.cb_list_id, 'BAM', 'bam' 
            FROM cb_list l, cb_field f 
            WHERE l.cb_field_id = f.cb_field_id
            AND f.fieldname = 'category')";
    $pearDB->query($sql);
}


$sql = "SELECT cb_type_id, type_shortname FROM cb_type WHERE type_shortname IN ('bam', 'bam_bi')";
$stmt = $pearDB->query($sql);
while ($row = $stmt->fetchRow()) {
	/* insert tag <-> type relations */
        $sql = "SELECT cb_tag_id FROM cb_tag_type_relation WHERE cb_tag_id = {$outputTagId} AND cb_type_id = {$row['cb_type_id']}";
        $tempStmt = $pearDB->query($sql);

        if (!$tempStmt->numRows()) {
	    $sql = "INSERT INTO cb_tag_type_relation (cb_tag_id, cb_type_id) 
                    VALUES ({$outputTagId}, {$row['cb_type_id']})";
            $pearDB->query($sql);
        }

	/* 
	 * insert command file if output is of bam type
	 * insert category filter if output is of bi type
	 */
	if ($row['type_shortname'] == 'bam') {
		$bamTypeId = $row['cb_type_id'];
		$bamCommandFile = ", 'command_file'";
        $storageDb = ", 'storage_db_name'";
        $cache = ", 'cache'";
	} else {
		$bamBiTypeId = $row['cb_type_id'];
		$bamCommandFile = "";
        $storageDb = "";
        $cache = "";
	}

	/* insert type <-> field relations */
    $sql = "SELECT cb_type_id FROM cb_type_field_relation WHERE cb_type_id = {$row['cb_type_id']}";
    $tempStmt = $pearDB->query($sql);

    if (!$tempStmt->numRows()) {
        $sql = "INSERT cb_type_field_relation (cb_type_id, is_required, cb_field_id, order_display)
                (SELECT {$row['cb_type_id']}, 0, cb_field_id, @rownum := @rownum + 1
                FROM cb_field CROSS JOIN (SELECT @rownum := 0) r
                WHERE fieldname IN ('db_host', 'db_user', 'db_password', 'db_name', 'db_type', 
                'db_port', 'queries_per_transaction', 'category', 'read_timeout', 
                'retry_interval', 'check_replication' {$cache} {$bamCommandFile} {$storageDb})
                ORDER BY fieldname)";
        $pearDB->query($sql);
    }
}

/* retrieve command file for local poller */
$sql = "SELECT command_file 
	FROM cfg_nagios n, nagios_server s
	WHERE n.nagios_server_id = s.id
	AND s.ns_activate = '1'
	AND n.nagios_activate = '1'
	AND s.localhost = '1'";
$stmt = $pearDB->query($sql);
$row = $stmt->fetchRow();
$commandFile = $row['command_file'];

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
	$configGroupBi = $lastGroup + 2;

	$bamMonitoringConfigData = array(
		'blockId' => "{$outputTagId}_{$bamTypeId}",
		'name' => 'centreon-bam-monitoring',
		'type' => 'bam',
        'cache' => 'yes',
		'db_name' => $conf_centreon['db'],
		'db_type' => 'mysql',
		'db_host' => $conf_centreon['hostCentreon'],
		'db_user' => $conf_centreon['user'],
		'db_password' => $conf_centreon['password'],
		'db_port' => (isset($conf_centreon['port']) ? $conf_centreon['port'] : '3306'),
		'storage_db_name' => $conf_centreon['dbcstg'],
		'queries_per_transaction' => '0',
		'command_file' => $commandFile
	);

	$bamBiConfigData = array(
		'blockId' => "{$outputTagId}_{$bamBiTypeId}",
		'name' => 'centreon-bam-reporting',
		'type' => 'bam_bi',
		'db_name' => $conf_centreon['dbcstg'],
		'db_type' => 'mysql',
		'db_host' => $conf_centreon['hostCentstorage'],
		'db_user' => $conf_centreon['user'],
		'db_password' => $conf_centreon['password'],
		'db_port' => (isset($conf_centreon['port']) ? $conf_centreon['port'] : '3306'),
		'queries_per_transaction' => '0'
	);

	$sql = "INSERT INTO cfg_centreonbroker_info 
		(config_id, config_key, config_value, config_group, config_group_id) VALUES 
		({$configId}, '%s', '%s', 'output', %s)";

	/* monitoring */
        $tempSql = "SELECT config_id
                FROM cfg_centreonbroker_info
                WHERE config_id = {$configId}
                AND config_key = 'name'
                AND config_value = 'centreon-bam-monitoring'";
        $tempStmt = $pearDB->query($tempSql);

        if (!$tempStmt->numRows()) {
	    foreach ($bamMonitoringConfigData as $bamConfigKey => $bamConfigValue) {
		$pearDB->query(
			sprintf(
				$sql, 
				$bamConfigKey, 
				$bamConfigValue, 
				$configGroupMonitoring
			)
		);
	    }
        }

	/* reporting */
        $tempSql = "SELECT config_id
                FROM cfg_centreonbroker_info
                WHERE config_id = {$configId}
                AND config_key = 'name'
                AND config_value = 'centreon-bam-reporting'";
        $tempStmt = $pearDB->query($tempSql);

        if (!$tempStmt->numRows()) {
	    foreach ($bamBiConfigData as $bamConfigKey => $bamConfigValue) {
		$pearDB->query(
			sprintf(
				$sql, 
				$bamConfigKey,
				$bamConfigValue,
				$configGroupBi
			)
		);
	    }

            /* category filter */
            $tempSql2 = "INSERT INTO cfg_centreonbroker_info
                    (config_id, config_key, config_value, config_group, config_group_id, grp_level, subgrp_id, parent_grp_id)
                    VALUES ({$configId}, 'filters', '', 'output', {$configGroupBi}, 0, 1, NULL)";
            $pearDB->query($tempSql2);

            $tempSql2 = "INSERT INTO cfg_centreonbroker_info
                    (config_id, config_key, config_value, config_group, config_group_id, grp_level, subgrp_id, parent_grp_id)
                    VALUES ({$configId}, 'category', 'bam', 'output', {$configGroupBi}, 1, NULL, 1)";
            $pearDB->query($tempSql2);
        }
}
