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

/*
 *  This function is used to call classes that might be in Centreon's class folder
 *  If the class does not exist, the function will call an alternative file
 */
function require_class($filename, $alternate)
{
/*	if (file_exists($filename)) {
		require_once ($filename);
	}
	else {*/
		require_once ($alternate);
	//}
}


/*
 *  Returns index from centstorage.index_data
 */
function getMyIndexGraph4Service($host_name = null, $service_description = null, $DB)
{
	if ((!isset($service_description) || !$service_description ) || (!isset($host_name) || !$host_name)) {
		return null;
	}
	$DBRESULT = $DB->query(
        "SELECT id FROM index_data i, metrics m WHERE i.host_name = '".$host_name."' " .
        "AND m.hidden = '0' " .
        "AND i.service_description = '".$service_description."' " .
        "AND i.id = m.index_id"
    );
	if ($DBRESULT->numRows()) {
		$row = $DBRESULT->fetchRow();
		return $row["id"];
	}
	return 0;
}

/*
 *  returns rrd tool path
 */
function getRRDToolPath($pearDBO)
{
    $DBRESULT = $pearDBO->query("SELECT RRDdatabase_path FROM config LIMIT 1");
    $config = $DBRESULT->fetchRow();
    $RRDdatabase_path = $config["RRDdatabase_path"];
    $DBRESULT->free();
    unset($config);
    return $RRDdatabase_path;
}

/*
 *  returns specific date format for reporting's timeline
 */
function create_date_timeline_format($time_unix)
{
    $tab_month = array(
        "01" => "Jan",
        "02" => "Feb",
        "03"=> "Mar",
        "04"=> "Apr",
        "05" => "May",
        "06"=> "Jun",
        "07"=> "Jul",
        "08"=> "Aug",
        "09"=> "Sep",
        "10"=> "Oct",
        "11"=> "Nov",
        "12"=> "Dec"
    );
    $date = $tab_month[date('m', $time_unix)].date(" d Y G:i:s", $time_unix);
    return $date;
}

/*
 * returns string that replaces #S# and #BS#
 */
function slashConvert($str, $flag = null)
{
	if (!isset($flag)) {
		$str = str_replace("#S#", "/", $str);
		$str = str_replace("#BS#", "\\", $str);
	} else {
		$str = str_replace("/", "#S#", $str);
		$str = str_replace("\\", "#BS#", $str);
	}
	return $str;
}

/**
 * Get Bam Default data source
 */
function bam_getDefaultDS($metric_name = null)
{
    global $pearDB;

    $ds = array();
    $filter = "";
    if ($metric_name == "BA_Downtime") {
        $filter = "BA_Downtime";
    } else if ($metric_name == "BA_Level") {
        $filter = "BA_Level";
    } else if ($metric_name == "BA_Acknowledgement") {
        $filter = "BA_Acknowledgement";
    }

    $res = $pearDB->query("SELECT compo_id FROM giv_components_template WHERE ds_name LIKE '".$filter."' LIMIT 1");
    if ($res->numRows()) {
        $ds = $res->fetchRow();
        return $ds["compo_id"];
    } else {
        $res = $pearDB->query("SELECT compo_id FROM giv_components_template WHERE default_tpl1 = '1' LIMIT 1");
        if ($res->numRows()) {
            $ds = $res->fetchRow();
            return $ds["compo_id"];
        }
    }
    return null;
}

/**
 * Get Centreon BA Service Id
 *
 * @param CentreonDB $db
 * @param string $metaName
 * @param int $hostId | bam host id
 * @return int
 */
function getCentreonBaServiceId($db, $baName, $hostId = null)
{
    try {
	    $query = "SELECT service_id FROM service WHERE service_register = '2' AND service_description = '".$db->escape($baName)."'";
        $res = $db->query($query);
        $sid = null;
        if (!$res->numRows()) {
	        $query = "INSERT INTO service (service_description, service_register) VALUES ('".$db->escape($baName)."', '2')";
	        $db->query($query);
            $query = "SELECT MAX(service_id) as sid FROM service WHERE service_description = '".$db->escape($baName)."' AND service_register = '2'";
	        $resId = $db->query($query);
	        if ($resId->numRows()) {
                $row = $resId->fetchRow();
                $sid = $row['sid'];
            }
        } else {
             $row = $res->fetchRow();
             $sid = $row['service_id'];
        }
        if (!isset($sid)) {
            throw new Exception('Service id of BAM Module could not be found');
        }
        if (false === is_null($hostId)) {
            $db->query("DELETE FROM host_service_relation WHERE host_host_id = ".$db->escape($hostId)." AND service_service_id = ".$db->escape($sid));
            $db->query("INSERT INTO host_service_relation (host_host_id, service_service_id) VALUES (".$db->escape($hostId).", ".$db->escape($sid).")");
        }
        return $sid;
    } catch (Exception $e) {
        echo $e->getMessage() . "<br/>";
    }
}

/**
 * Get Centreon BA Host Id
 *
 * @param CentreonDB $db
 * @param int $pollerId The poller for host
 * @return int
 */
function getCentreonBaHostId($db, $pollerId)
{
    try {
	    $query = "SELECT host_id FROM host WHERE host_register = '2' AND host_name = '_Module_BAM_" . $pollerId . "'";
        $res = $db->query($query);
        $hid = null;
        if (!$res->numRows()) {
	        $query = "INSERT INTO host (host_name, host_register) VALUES ('_Module_BAM_" . $pollerId . "', '2')";
	        $db->query($query);
            $query = "SELECT MAX(host_id) as hid FROM host WHERE host_name = '_Module_BAM_" . $pollerId . "' AND host_register = '2'";
	        $resId = $db->query($query);
	        if ($resId->numRows()) {
                $row = $resId->fetchRow();
                $hid = $row['hid'];
            }
        } else {
             $row = $res->fetchRow();
             $hid = $row['host_id'];
        }
        if (!isset($hid)) {
            throw new Exception('Host id of BAM Module could not be found');
        }
        return $hid;
    } catch (Exception $e) {
        echo $e->getMessage() . "<br/>";
    }
}

/**
 * Get Centreon BA Host Name
 *
 * @param CentreonDB $db
 * @return int
 */
function getCentreonBaHostName($db)
{
    $hostname = '_Module_BAM';
    try {
        $query = "SELECT host_name "
            . "FROM host h, nagios_server ns, ns_host_relation nsh "
            . "WHERE h.host_register = '2' "
            . "AND h.host_name like '_Module_BAM_%' "
            . "AND nsh.nagios_server_id = ns.id "
            . "AND nsh.host_host_id = h.host_id "
            . "ORDER BY localhost DESC ";
        $res = $db->query($query);
        if ($res->numRows()) {
            $row = $res->fetchRow();
            $hostname = $row['host_name'];
        } else {
            $pearDBO = new CentreonBam_Db("centstorage");
            $query2 = "SELECT name FROM hosts WHERE name like '_Module_BAM_%' LIMIT 1";
            $res2 = $pearDBO->query($query2);
            if ($res2->numRows()) {
                $row2 = $res2->fetchRow();
                $hostname = $row2['name'];
            }
        }
        return $hostname;
    } catch (Exception $e) {
        echo $e->getMessage() . "<br/>";
    }
}

/**
 * Get Centreon BA Host Names with location
 *
 * @param CentreonDB $db
 * @return int
 */
function getCentreonBaHostNamesWithLocation($db)
{
    try {
        $query = "SELECT host_name FROM host WHERE host_register = '2' AND host_name like '_Module_BAM_%'";
        $res = $db->query($query);
        $hosts = array();
        if (!$res->numRows()) {
            $pearDBO = new CentreonBam_Db("centstorage");
            $query = "SELECT name as host_name FROM hosts WHERE name like '_Module_BAM_%'";
            $res = $pearDBO->query($query);
        }
        while ($row = $res->fetchRow()) {
            preg_match('/_Module_BAM_(\d+)/', $row['host_name'], $matches);
            if (isset($matches[1])) {
                $query2 = "SELECT localhost FROM nagios_server WHERE id = " . $matches[1];
                $res2 = $db->query($query2);
                while ($row2 = $res2->fetchRow()) {
                    $hosts[] = array(
                        'host_name' => $row['host_name'],
                        'poller_id' => $matches[1],
                        'localhost' => $row2['localhost']
                    );
                }
            }
        }
        return $hosts;
    } catch (Exception $e) {
        echo $e->getMessage() . "<br/>";
    }
}

/**
 * Get if a poller as BA
 *
 * @param int $pollerId The poller to test
 * @return bool
 */
function pollerAsBa($pollerId, $db)
{
    global $pollerAsBa;
    
    if (false === is_array($pollerAsBa)) {
        $pollerAsBa = array();
        $res = $db->query("SELECT poller_id, COUNT(ba_id) as nb_ba FROM mod_bam_poller_relations GROUP BY poller_id");
        while ($row = $res->fetchRow()) {
            $pollerAsBa[$row['poller_id']] = $row['nb_ba'];
        }
    }
    if (isset($pollerAsBa[$pollerId]) && $pollerAsBa[$pollerId] > 0) {
        return true;
    }
    return false;
}

/*
 *  check version for check injection
 */
function versionCheckInjection($id, $db)
{

// version centreon
    $query = $db->query("SELECT `value` FROM `informations` ");
    $res = $query->fetchRow();
    $dataVersion = explode( '.', $res['value']);
    $curentCentreonVersion = (float) ($dataVersion[0].'.'.$dataVersion[1]);

// check centreon version
    if( $curentCentreonVersion < 2.8 ){
        $result = CentreonDB::check_injection($id);
        return $result;
    } else {
        $result = CentreonDB::checkInjection($id);
        return $result;
    }
}

/**
 * 
 * @param type $db
 * @param type $bv
 */
function notifyAclConfChangedByBv($db, $bv)
{
    $queryGetAclGroups = "SELECT acl_group_id FROM mod_bam_acl WHERE ba_group_id = $bv";
    $resAclGroups = $db->query($queryGetAclGroups);
    if ($resAclGroups->numRows()) {
        while ($row = $resAclGroups->fetchRow()) {
            $queryUpdateAcl = "UPDATE acl_groups SET acl_group_changed = '1' WHERE acl_group_id = $row[acl_group_id]";
            $db->query($queryUpdateAcl);
        }
    }
}