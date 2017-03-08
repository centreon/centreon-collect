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

function hasGeoLoc() {
    global $pearDB;
    static $hasGeo = null;

    if (is_null($hasGeo)) {
        $utils = new CentreonBam_Utils($pearDB);
        $hasGeo = $utils->tableExists($pearDB, 'mod_map_geo');
    }
    return $hasGeo;
}

function getDowntimeInformation($host_name, $service_description) {
    global $buffer, $pearDBndo, $pearDB;
    static $downtimeData = array();
    
    if (!isset($downtimeData[$host_name])) { 
        $downtimeData[$host_name] = array();
        $res = $pearDBndo->query(
                "SELECT start_time as scheduled_start_time, end_time as scheduled_end_time, internal_id as internal_downtime_id, d.instance_id, s.description " .
                "FROM downtimes d, hosts h, services s " .
                "WHERE h.name = '".$pearDBndo->escape($host_name)."' " .
                "AND h.host_id = s.host_id ".
                "AND s.host_id = d.host_id ".
                "AND s.service_id = d.service_id " .
                "AND d.end_time > '".time()."'" .
                "AND cancelled = 0"
        );
        while ($row = $res->fetchRow()) {
            if (!isset($downtimeData[$host_name][$row['description']])) {
                $downtimeData[$host_name][$row['description']] = array();
            }
            $downtimeData[$host_name][$row['description']][] = $row;
        }
    }
    
    if (isset($downtimeData[$host_name][$service_description])) {
        foreach ($downtimeData[$host_name][$service_description] as $data) {
            $buffer->startElement("downtime");
            $buffer->writeAttribute("id", $data["internal_downtime_id"]);
            $buffer->writeAttribute("timestampbegin", $data["scheduled_start_time"]);
            $buffer->writeAttribute("timestampend", $data["scheduled_end_time"]);
            $buffer->writeAttribute("poller_id", $data['instance_id']);
            $buffer->endElement();
        }
    }
}

function getRadar($request)
{
    global $buffer;

    foreach ($request->children() as $child) {
        if ($child->getName() == "ba") {
            $baId = trim($child['id']);
            $height = trim($child['height']);
            $width = trim($child['width']);
            try {
                $export = new BamExport($baId);
                if (isset($width) && $width) {
                    $export->setWidth($width);
                }
                if (isset($height) && $height) {
                    $export->setHeight($height);
                }
                $export->render();
                exit;
            } catch (Exception $e) {
                $buffer->startElement("error");
                $buffer->writeAttribute("code", "20005");
                $buffer->text("Problem occured while creating : " . $e->getMessage());
                $buffer->endElement();
            }
        }
    }
}

/*
 *  get structure of a specific ba
 */
function getBaStructure($ba_id)
{
	global $buffer, $pearDB, $pearDBndo, $child_ba;
	global $hObj, $sObj, $metaObj, $baObj, $mediaObj, $boolObj;
	static $baArr = null;
	static $kpiArr = null;

	$buffer->startElement("ba");
	$utils = new CentreonBam_Utils($pearDB);
	if (is_null($baArr)) {
        $baArr = array();
        if (!hasGeoLoc()) {
            $query = "SELECT ba_id, name, description, NULL as latlng FROM `mod_bam`";
        } else {
            $query = "SELECT ba_id, name, description, latlng FROM mod_bam b LEFT JOIN mod_map_geo g ON (g.resource_id = b.ba_id AND g.resource_type = 6)";
        }
        $DBRES = $pearDB->query($query);
        while ($row = $DBRES->fetchRow()) {
            $baArr[$row['ba_id']] = $row;
        }
	}
	if (isset($baArr[$ba_id])) {
        $row = $baArr[$ba_id];
        $buffer->writeAttribute("id", $row['ba_id']);
        $buffer->writeAttribute("name", htmlentities($row['name']));
        $buffer->writeAttribute("description", htmlentities($row['description']));
        $buffer->writeAttribute("img_id", $baObj->getIconId($ba_id));
        $buffer->writeAttribute("latlng", $row['latlng']);
	}

	if (is_null($kpiArr)) {
        $kpiArr = array();
        $query = "SELECT id_ba, kpi_id, kpi_type, host_id, service_id, id_indicator_ba, meta_id, boolean_id
                FROM `mod_bam_kpi`
                ORDER BY kpi_type ASC";
        $DBRES = $pearDB->query($query);
        while ($row = $DBRES->fetchRow()) {
            if (!isset($kpiArr[$row['id_ba']])) {
                $kpiArr[$row['id_ba']] = array();
            }
            $kpiArr[$row['id_ba']][$row['kpi_id']] = $row;
        }
	}
	if (isset($kpiArr[$ba_id])) {
        foreach ($kpiArr[$ba_id] as $row) {
            $buffer->startElement("kpi");
            switch ($row['kpi_type']) {
                case "0" :
                    $kpi_type = "5";
                    $name = $sObj->getServiceDesc($row['service_id']);
                    $desc = $hObj->getHostName($row['host_id']);
                    $buffer->writeAttribute("resid", $row['host_id']. "_" . $row['service_id']);
                    $buffer->writeAttribute("graph", 1);
                    $imgId = $mediaObj->getServiceImageId($row['service_id']);
                    if ($imgId) {
                            $buffer->writeAttribute('img_id', $imgId);
                    }
                    break;
                case "1" :
                    $kpi_type = "7";
                    $name = $metaObj->getMetaName($row['meta_id']);
                    $desc = $metaObj->getMetaDesc($row['meta_id']);
                    $buffer->writeAttribute("graph", 1);
                    $buffer->writeAttribute("resid", $row['meta_id']);
                    break;
                case "2" :
                    $kpi_type = "6";
                    $child_ba[$row['id_indicator_ba']] = $row['id_indicator_ba'];
                    $name = $baObj->getBA_Name($row['id_indicator_ba']);
                    $desc = $baObj->getBA_Desc($row['id_indicator_ba']);
                    $buffer->writeAttribute("resid", $row['id_indicator_ba']);
                    break;
                case "3":
                    $kpi_type = "5";
                    $boolData = $boolObj->getData($row['boolean_id']);
                    $name = $boolData['name'];
                    $desc = $boolData['comments'];
                    $buffer->writeAttribute("resid", $row['kpi_id']);
                    break;
                default :
                    $kpi_type = "";
                    $name = "";
                    $desc = "";
                    break;
            }
            $buffer->writeAttribute("idkpi", $row['kpi_id']);
            $buffer->writeAttribute("type", $kpi_type);
            $name = str_replace("#S#", "/", $name);
            $name = str_replace("#BS#", "\\", $name);
            $buffer->writeAttribute("name", $name);
            $desc = str_replace("#S#", "/", $desc);
            $desc = str_replace("#BS#", "\\", $desc);
            $buffer->writeAttribute("alias", $desc);
            $buffer->endElement();
        }
	}
	$buffer->endElement();
}

function initHasGraphCache()
{
    global $pearDBO, $serviceGraphed;

    $DBRESULT = $pearDBO->query("SELECT host_id, service_id FROM index_data");
    while ($data = $DBRESULT->fetchRow()) {
        $serviceGraphed[$data["host_id"].'_'.$data["service_id"]] = 1;
    }
}

/*
 *  getTree function
 */
function getTree($request)
{
    global $buffer, $child_ba;

    $parent_ba = array();

    //initHasGraphCache();
    foreach ($request->children() as $balist) {
        $buffer->startElement("bamlist");
        foreach ($balist->children() as $ba) {
            $ba_id = trim($ba['res_id']);
            $parent_ba[$ba_id] = $ba_id;
            getBaStructure($ba_id);
        }
        if (count($child_ba)) {
            foreach ($child_ba as $ba_id) {
                if (!isset($parent_ba[$ba_id])) {
                    getBaStructure($ba_id);
                }
            }
        }
        $buffer->endElement();
    }
}

/**
 * Create <kpi> tags for boolean kpis
 *
 * @param array $booleanData
 */
function processBooleanStatus($data)
{
    global $buffer, $boolObj, $kpiObj;
    
    $booleanKpiConf = $boolObj->getData($data['boolean_id']);
    $booleanKpiStatus = $boolObj->evalExpr($booleanKpiConf['expression']);
    
    $status = 0;
    $boolstate = "false";
    $impact = 0;
    
    $booleanKpiStatus = $booleanKpiStatus ? "true" : "false";
    $booleanKpiConf['bool_state'] = $booleanKpiConf['bool_state'] ? "true" : "false";
    
    if ($booleanKpiStatus == $booleanKpiConf['bool_state']) {
        $status = 2;
        if ($data['config_type']) {
            $impact = $data['drop_critical_impact'];
        } else {
            $impact = $kpiObj->getCriticityImpact($data['drop_critical_impact_id']);
        }
    }

    if ($booleanKpiStatus == "true") {
        $boolstate = "true";
    }

    $buffer->startElement("kpi");
    $buffer->writeAttribute("id", $data['kpi_id']);
    $buffer->writeAttribute("status", $status);
    $buffer->writeAttribute("output", sprintf(_("Boolean result: %s"), $boolstate));
    
    if ($data['config_type']) {
        $buffer->writeAttribute("critical_impact", $data['drop_critical_impact']);
    } else {
        $buffer->writeAttribute("critical_impact", $kpiObj->getCriticityImpact($data['drop_critical_impact_id']));
    }
    
    $buffer->writeAttribute("impact", $impact);
    $buffer->endElement();
}

/*
 *  get KPI status
 */
function getKpiStatus($kpi_id_tab)
{
    global $pearDB, $pearDBO, $pearDBndo;
    global $buffer;
    global $hObj, $sObj, $metaObj, $baObj, $kpiObj;

    if (!count($kpi_id_tab)) {
        return 0;
    }
    $kpi_list_str = implode(",", $kpi_id_tab);
        
    if ($kpi_list_str == "") {
        $kpi_list_str = "''";
    }
    
    /* Build hostName */
    $query = "SELECT id FROM nagios_server WHERE localhost = '1'";
    $res = $pearDB->query($query);
    $row = $res->fetchRow();
    $hostname = "_Module_BAM_".$row['id'];

    $query = "SELECT kpi_type, drop_warning, drop_critical, drop_unknown, drop_warning_impact_id, drop_critical_impact_id, drop_unknown_impact_id,
            host_id, service_id, meta_id, id_indicator_ba, kpi_id, NULL as bool_state, NULL as current_bool_state, NULL as bname, boolean_id
            FROM `mod_bam_kpi` WHERE kpi_id IN ($kpi_list_str)";
    $DBRES = $pearDB->query($query);

    $regKpi = array();
	while ($row = $DBRES->fetchRow()) {
        $kpi_tab[$row['kpi_id']]['warning_impact'] = $row['drop_warning'];
        $kpi_tab[$row['kpi_id']]['critical_impact'] = $row['drop_critical'];
        $kpi_tab[$row['kpi_id']]['unknown_impact'] = $row['drop_unknown'];
        $kpi_tab[$row['kpi_id']]['warning_impact_id'] = $row['drop_warning_impact_id'];
        $kpi_tab[$row['kpi_id']]['critical_impact_id'] = $row['drop_critical_impact_id'];
        $kpi_tab[$row['kpi_id']]['unknown_impact_id'] = $row['drop_unknown_impact_id'];
        
        if (isset($row['config_type'])) {
            $kpi_tab[$row['kpi_id']]['config_type'] = $row['config_type'];
        } else {
            $kpi_tab[$row['kpi_id']]['config_type'] = "";
        }
        
        $kpi_tab[$row['kpi_id']]['kpi_type'] = $row['kpi_type'];
        $kpi_tab[$row['kpi_id']]['conf'] = $row;
        switch ($row['kpi_type']) {
            case "0" :
                $hName = $hObj->getHostName($row['host_id']);
                $hName = str_replace("#S#", "/", $hName);
                $hName = str_replace("#BS#", "\\", $hName);
                $sName = $sObj->getServiceDesc($row['service_id']);
                $sName = str_replace("#S#", "/", $sName);
                $sName = str_replace("#BS#", "\\", $sName);
                $regKpi[$row['host_id']] = true;
                break;
            case "1" :
                $hName = "_Module_Meta";
                $sName = $metaObj->getMetaName($row['meta_id']);
                break;
            case "2" :
                $hName = getCentreonBaHostName($ba_id);
                $sName = $baObj->getBA_Name($row['id_indicator_ba']);
                break;
            case "3" :
                processBooleanStatus($row);
                break;
        }

        if ($row['kpi_type'] !== "3") {
            $kpi_tab[$row['kpi_id']]['hName'] = $hName;
            $kpi_tab[$row['kpi_id']]['sName'] = $sName;
        }
	}

	$regKpiStr = implode(',', array_keys($regKpi));
	if ($regKpiStr == '') {
        $regKpiStr = "''";
	}
    $query = "SELECT h.name as hName, s.description as sName, s.output, s.state as current_state, s.last_check, s.next_check," .
    	  "s.last_state_change, s.acknowledged as problem_has_been_acknowledged, s.scheduled_downtime_depth " .
	      "FROM services s, hosts h " .
		  "WHERE s.host_id = h.host_id 
           AND s.enabled = '1' AND h.enabled = '1'
		   AND (h.host_id = '".$hostname."' OR (h.host_id IN (".$regKpiStr."))
		   )";

	$DBRES2 = $pearDBO->query($query);
	while ($row2 = $DBRES2->fetchRow()) {
        foreach ($kpi_id_tab as $value) {
            $value = trim($value);
            if (isset($kpi_tab[$value]) && ($kpi_tab[$value]['kpi_type'] != 3) && $kpi_tab[$value]['hName'] == $row2['hName'] && $kpi_tab[$value]['sName'] == $row2['sName']) {
                $buffer->startElement("kpi");
                $buffer->writeAttribute("id", $value);
                $buffer->writeAttribute("status", $row2['current_state']);
                $buffer->writeAttribute("output", htmlentities($row2['output']));
                $buffer->writeAttribute("lc", $row2['last_check']);
                $buffer->writeAttribute("nc", $row2['next_check']);
                $buffer->writeAttribute("lsc", $row2['last_state_change']);
                $buffer->writeAttribute("duration", time() - $row2['last_state_change']);
                $buffer->writeAttribute("ack", $row2['problem_has_been_acknowledged'] ? "true" : "false");
                $downtime = $row2['scheduled_downtime_depth'] ? "true" : "false";
                $buffer->writeAttribute("downtime", $downtime);
                if ($kpi_tab[$value]['config_type']) {
                    $buffer->writeAttribute("warning_impact", $kpi_tab[$value]['warning_impact']);
                    $buffer->writeAttribute("critical_impact", $kpi_tab[$value]['critical_impact']);
                    $buffer->writeAttribute("unknown_impact", $kpi_tab[$value]['unknown_impact']);
                } else {
                    $buffer->writeAttribute("warning_impact", $kpiObj->getCriticityImpact($kpi_tab[$value]['warning_impact_id']));
                    $buffer->writeAttribute("critical_impact", $kpiObj->getCriticityImpact($kpi_tab[$value]['critical_impact_id']));
                    $buffer->writeAttribute("unknown_impact", $kpiObj->getCriticityImpact($kpi_tab[$value]['unknown_impact_id']));
                }
				$buffer->writeAttribute("impact", $kpiObj->getKpiImpact($kpi_tab[$value]['conf']));
				if ($downtime === "true") {
				    getDowntimeInformation($row2['hName'], $row2['sName']);
				}
				$buffer->endElement();
				unset($kpi_tab[$value]);
			}
		}
	}
}

/*
 *  get BA status
 */
function getBaStatus($ba_id)
{
	global $pearDB, $pearDBndo;
	global $buffer;
	static $levelArr = null;
	static $baData = null;

    /* Build hostName */
    $query = "SELECT id FROM nagios_server WHERE localhost = '1'";
    $res = $pearDB->query($query);
    $row = $res->fetchRow();
    $hostname = "_Module_BAM_".$row['id'];

	$ba_id = (int)$ba_id;
	if (is_null($baData)) {
		$baData = array();
    	$query = "SELECT  h.name as hName, s.description as sName, s.output, s.state as current_state, s.last_check, s.next_check, s.perfdata, " .
			"s.last_state_change, s.acknowledged as problem_has_been_acknowledged, s.scheduled_downtime_depth, REPLACE(s.description, 'ba_', '') as ba_id " .
			 "FROM services s, hosts h " .
			 "WHERE s.host_id = h.host_id " .
             "AND s.enabled = '1' " .
             "AND h.enabled = '1' " .
			 "AND h.name = '$hostname'";       
		$res = $pearDBndo->query($query);
		while ($row = $res->fetchRow()) {
			$baData[$row['ba_id']] = $row;
		}
	}

	if (is_null($levelArr)) {
        $levelArr = array();
		$DBRES2 = $pearDB->query("SELECT ba_id, level_w, level_c FROM `mod_bam`");
		while ($r2 = $DBRES2->fetchRow()) {
			$levelArr[$r2['ba_id']] = $r2;
		}
	}

	if (isset($baData[$ba_id]) && isset($levelArr[$ba_id])) {
		$row = $baData[$ba_id];
		$buffer->startElement("ba");
		$buffer->writeAttribute("id", $ba_id);
		$buffer->writeAttribute("status", $row['current_state']);
		$buffer->writeAttribute("output", htmlentities($row['output']));
		$buffer->writeAttribute("lc", $row['last_check']);
		$buffer->writeAttribute("nc", $row['next_check']);
		$buffer->writeAttribute("lsc", $row['last_state_change']);
		$buffer->writeAttribute("duration", time() - $row['last_state_change']);
        $buffer->writeAttribute("ack", $row['problem_has_been_acknowledged'] ? "true" : "false");
        $downtime = $row['scheduled_downtime_depth'] ? "true" : "false";
        $buffer->writeAttribute("downtime", $downtime);
		
        preg_match("/BA_Level=([0-9\.]+)\%/", $row['perfdata'], $matches);
		if (isset($matches[1])) {
			$buffer->writeAttribute("level", $matches[1]);
		}
		
        $buffer->writeAttribute("warning_threshold", $levelArr[$ba_id]['level_w']);
		$buffer->writeAttribute("critical_threshold", $levelArr[$ba_id]['level_c']);
        if ($downtime === "true") {
            getDowntimeInformation($row['hName'], $row['sName']);
        }
		$buffer->endElement();
	}
}

/*
 *  getStatus function
 */
function getStatus($request)
{
	global $buffer;

	$buffer->startElement("bamstatuslist");
	foreach ($request->children() as $child) {
		$tmp_tab = array();
		if ($child->getName() == "balist") {
			$buffer->startElement("status_ba");
			foreach ($child->children() as $ba) {
				getBaStatus($ba['id']);
			}
			$buffer->endElement();
		} elseif ($child->getName() == "kpilist") {
			$buffer->startElement("status_kpi");
			foreach ($child->children() as $kpi) {
				$tmp_tab[] = $kpi['id'];
			}
			getKpiStatus($tmp_tab);
			$buffer->endElement();
		}
	}
	$buffer->endElement();
}

/*
 * escape shell chars
 */
function escape_command($command)
{
	return ereg_replace("(\\\$|`)", "", $command);
}

/*
 *  get rrd tool bin path
 */
function getRRDToolBinPath($pearDB)
{
	$DBRES = $pearDB->query("SHOW TABLES LIKE 'general_opt'");
	$str_key = "";
	if ($DBRES->numRows()) {
		$query = "SELECT rrdtool_path_bin FROM general_opt LIMIT 1";
		$str_key = "rrdtool_path_bin";
	}
	$DBRES2 = $pearDB->query("SHOW TABLES LIKE 'options'");
	if ($DBRES2->numRows()) {
		$query = "SELECT `value` FROM options WHERE `key` LIKE 'rrdtool_path_bin' LIMIT 1";
		$str_key = "value";
	}
	if (isset($query)) {
		$DBRESULT = $pearDB->query($query);
		$config = $DBRESULT->fetchRow();
		$RRDdatabase_path = $config[$str_key];
		$DBRESULT->free();
		unset($config);
		return $RRDdatabase_path;
	}
	return null;
}

/*
 * get default graph
 */
function bam_getDefaultDS($metric_name = null)
{
		global $pearDB;
		$ds = array();
		$filter = "";
		if ($metric_name == "BA_Downtime") {
			$filter = "BA_Downtime";
		}
		else if ($metric_name == "BA_Level") {
			$filter = "BA_Level";
		}
		else if ($metric_name == "BA_Acknowledgement") {
		    $filter = "BA_Acknowledgement";
		}

		$res = $pearDB->query("SELECT compo_id FROM giv_components_template WHERE ds_name LIKE '".$filter."' LIMIT 1");
		if ($res->numRows())	{
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
/*
 * get graph
 */
function getGraph($request)
{
	global $pearDB, $pearDBO;

	include("../centreon-bam-server.conf.php");
	require_once "../../../include/common/common-Func.php";

	foreach ($request->children() as $child) {
		$tmp_tab = array();
		if ($child->getName() == "ba") {
			$ba_id = $child['id'];
			$height = $child['height'];
			$width = $child['width'];
			$nb_day = $child['nbday'];
		}
	}

	if (!isset($ba_id) || !isset($height) || !isset($width) || !isset($nb_day)) {
		exit;
	}

	$DBRESULT = $pearDBO->query("SELECT RRDdatabase_path FROM config LIMIT 1");
	$config = $DBRESULT->fetchRow();
	$RRDdatabase_path = $config["RRDdatabase_path"];
	$DBRESULT->free();
	unset($config);

	$DBRESULT2 = $pearDB->query("SELECT graph_id FROM mod_bam WHERE ba_id = '".$ba_id."' LIMIT 1");
	$qos_tmpl = $DBRESULT2->fetchRow();

	if (isset($qos_tmpl["graph_id"])) {
		$template_id = $qos_tmpl["graph_id"];
	} else {
		$template_id = getDefaultGraph($svc_id, 1);
	}

	$end = time();
	$start = $end - (60 * 60 * 24) * $nb_day;
	$command_line = " graph - --start=".$start. " --end=" . $end;

	/* 
     * get all template infos
     */
	$DBRESULT = $pearDB->query("SELECT * FROM giv_graphs_template WHERE graph_id = '".$template_id."' LIMIT 1");
	$GraphTemplate = $DBRESULT->fetchRow();

	$DBRESULT = $pearDB->query("SELECT name FROM mod_bam WHERE ba_id = '".$ba_id."' LIMIT 1");
	$qos_name_tab = $DBRESULT->fetchRow();
	$qos_name = $qos_name_tab["name"];

	$command_line .= " --interlaced -b 1024 --imgformat PNG --width=".$width." --height=".$height." --title='$qos_name' --vertical-label='".$GraphTemplate["vertical_label"]."' ";

	if (isset($GraphTemplate["lower_limit"]) && $GraphTemplate["lower_limit"] != null) {
		$command_line .= "--lower-limit ".$GraphTemplate["lower_limit"]." ";
	}
	if (isset($GraphTemplate["upper_limit"]) && $GraphTemplate["upper_limit"] != null) {
		$command_line .= "--upper-limit ".$GraphTemplate["upper_limit"]." ";
	}
	if ((isset($GraphTemplate["lower_limit"]) && $GraphTemplate["lower_limit"] != null) || (isset($GraphTemplate["upper_limit"]) && $GraphTemplate["upper_limit"] != null)) {
		$command_line .= "--rigid ";
	}

	# Init DS template For each curv
	$metrics = array();

	$DBRES = $pearDBO->query("SELECT id FROM index_data WHERE host_name LIKE '_Module_BAM%' AND service_description LIKE 'ba_".$ba_id."' LIMIT 1");
	$rowz = $DBRES->fetchRow();

	
	$cpt = 0;
	$metrics = array();
	$DBRESULT = $pearDBO->query("SELECT metric_id, metric_name, unit_name FROM metrics WHERE index_id = '".$rowz["id"]."' ORDER BY metric_name");
    while ($metric = $DBRESULT->fetchRow()){
		$metrics[$metric["metric_id"]]["metric_id"] = $metric["metric_id"];
		$metrics[$metric["metric_id"]]["metric"] = str_replace("/", "", $metric["metric_name"]);
		$metrics[$metric["metric_id"]]["unit"] = $metric["unit_name"];
		$ds = bam_getDefaultDS($metric["metric_name"]);
		$metrics[$metric["metric_id"]]["ds_id"] = $ds;
		
        $res_ds = $pearDB->query("SELECT * FROM giv_components_template WHERE compo_id = '".$ds."'");
		$res_ds->fetchInto($ds_data);
		foreach ($ds_data as $key => $ds_d){
			if ($key == "ds_transparency") {
				$metrics[$metric["metric_id"]][$key] = dechex(255-($ds_d*255)/100);
			} else {
				$metrics[$metric["metric_id"]][$key] = $ds_d;
			}
		}
		$res_ds->free();
		$metrics[$metric["metric_id"]]["legend"] = $ds_data["ds_name"];
		if (strcmp($metric["unit_name"], "")) {
			$metrics[$metric["metric_id"]]["legend"] .= " (".$metric["unit_name"].") ";
		}
		$metrics[$metric["metric_id"]]["legend_len"] = strlen($metrics[$metric["metric_id"]]["legend"]);
		$cpt++;
	}
	$DBRESULT->free();
	$cpt = 0;
	$longer = 0;
	foreach ($metrics as $key => $tm){
		$ds_value = CentreonBam_Utils::formatDsName($pearDB, $metrics[$key]['metric']);
		if (isset($tm["ds_invert"]) && $tm["ds_invert"]) {
			$command_line .= " DEF:va".$cpt."=".$RRDdatabase_path.$key.".rrd:".$ds_value.":AVERAGE CDEF:v".$cpt."=va".$cpt.",-1,* ";
		} else {
			$command_line .= " DEF:v".$cpt."=".$RRDdatabase_path.$key.".rrd:".$ds_value.":AVERAGE ";
		}
		if ($tm["legend_len"] > $longer)
			$longer = $tm["legend_len"];
		$cpt++;
	}

	# Create Legende
	$cpt = 1;
	foreach ($metrics as $key => $tm){
		if ($metrics[$key]["ds_filled"]) {
			$command_line .= " AREA:v".($cpt-1)."".$tm["ds_color_area"].$tm["ds_transparency"]." ";
		}
		$command_line .= " LINE".$tm["ds_tickness"].":v".($cpt-1);
		$command_line .= $tm["ds_color_line"].":\"";
		$command_line .= $metrics[$key]["legend"];
		for ($i = $metrics[$key]["legend_len"]; $i != $longer + 1; $i++) {
			$command_line .= " ";
		}
		$command_line .= "\"";
		if ($tm["ds_average"]) {
			$command_line .= " GPRINT:v".($cpt-1).":AVERAGE:\"Average\:%8.2lf%s";
			$tm["ds_min"] || $tm["ds_max"] || $tm["ds_last"] ? $command_line .= "\"" : $command_line .= "\\l\" ";
		}
		if ($tm["ds_min"]) {
			$command_line .= " GPRINT:v".($cpt-1).":MIN:\"Min\:%8.2lf%s";
			$tm["ds_max"] || $tm["ds_last"] ? $command_line .= "\"" : $command_line .= "\\l\" ";
		}
		if ($tm["ds_max"]) {
			$command_line .= " GPRINT:v".($cpt-1).":MAX:\"Max\:%8.2lf%s";
			$tm["ds_last"] ? $command_line .= "\"" : $command_line .= "\\l\" ";
		}
		if ($tm["ds_last"]) {
			$command_line .= " GPRINT:v".($cpt-1).":LAST:\"Last\:%8.2lf%s\\l\"";
		}
		$cpt++;
	}

	$command_line = getRRDToolBinPath($pearDB).$command_line." 2>&1";
	$command_line = escape_command("$command_line");

	$fp = popen($command_line  , 'r');
	if (isset($fp) && $fp ) {
		$str ='';
		while (!feof ($fp)) {
	  		$buffer = fgets($fp, 4096);
	 		$str = $str . $buffer ;
		}
		print $str;
	}
	exit;
}

/**
 * Get Centreon BA Host Name
 *
 * @param CentreonDB $db
 * @return int
 */
function getCentreonBaHostName($ba_id)
{
    try {
        $db = new CentreonBam_Db("centstorage");
        $query = "SELECT h.name FROM services s, hosts h WHERE h.name like '_Module_BAM_%' AND h.host_id = s.host_id and s.description LIKE 'ba_".$ba_id."'";
        $res = $db->query($query);
        $row = $res->fetchRow();
        if (isset($row['name'])) {
            return $row['name'];
        } else {
            return null;
        }
    } catch (Exception $e) {
        echo $e->getMessage() . "<br/>";
    }
}