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

	//header("Content-Type: image/png");

function escape_command($command) {
	return ereg_replace("(\\\$|`)", "", $command);
}

function getRRDToolBinPath($pearDB){
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
		if (PEAR::isError($DBRESULT)) {
			print "DB Error : ".$DBRESULT->getDebugInfo();
		}
		$config = $DBRESULT->fetchRow();
		$RRDdatabase_path = $config[$str_key];
		$DBRESULT->free();
		unset($config);
		return $RRDdatabase_path;
	}
	return NULL;
}

require_once 'DB.php';
if (is_file("../../../class/Session.class.php")) {
    require_once ("../../../class/Session.class.php");
}

Session::start();
if (isset($_SESSION['oreon'])) {
    $centreon = $_SESSION["oreon"];
} elseif (isset($_SESSION['centreon'])) {
    $centreon = $_SESSION['centreon'];
}

include("../centreon-bam-server.conf.php");
require_once "../../../include/common/common-Func.php";
require_once("./common-Func.php");

# Connect to Centreon DB
$dsn = array(
    'phptype'  => 'mysql',
    'username' => $conf_centreon['user'],
    'password' => $conf_centreon['password'],
    'hostspec' => $conf_centreon['hostCentreon'],
    'database' => $conf_centreon['db'],
);

$options = array(
    'debug'       => 2,
    'portability' => DB_PORTABILITY_ALL ^ DB_PORTABILITY_LOWERCASE,
);

$pearDB = DB::connect($dsn, $options);
if (PEAR::isError($pearDB)) {
    die("Unable to connect : " . $pearDB->getMessage());
}

$pearDB->setFetchMode(DB_FETCHMODE_ASSOC);

$session = $pearDB->query("SELECT * FROM `session` WHERE session_id = '".$_GET["session"]."'");
if (!$session->numRows()){
	exit;
} else {
	$session->free();
	include_once("../../../DBOdsConnect.php");

	$DBRESULT = $pearDBO->query("SELECT RRDdatabase_path FROM config LIMIT 1");
	if (PEAR::isError($DBRESULT)) {
		print "Mysql Error : ".$DBRESULT->getDebugInfo();
	}
	$DBRESULT->fetchInto($config);
	$RRDdatabase_path = $config["RRDdatabase_path"];
	$DBRESULT->free();
	unset($config);

	if (!isset($_GET["template_id"]) || !$_GET["template_id"]){
		$DBRESULT2 = $pearDB->query("SELECT graph_id FROM mod_bam WHERE ba_id = '".$_GET['bam_id']."' LIMIT 1");
		$qos_tmpl = $DBRESULT2->fetchRow();

		if (isset($qos_tmpl["graph_id"])) {
			$template_id = $qos_tmpl["graph_id"];
		} else {
			$template_id = getDefaultGraph($svc_id, 1);
		}
	}

	$end = time();
	$start = $end - (60 * 60 * 24) * $_GET['nbdays'];
	$command_line = " graph - --start=".$start. " --end=" . $end;

	/* get all template infos */
	$DBRESULT = $pearDB->query("SELECT * FROM giv_graphs_template WHERE graph_id = '".$template_id."' LIMIT 1");
	$GraphTemplate = $DBRESULT->fetchRow();

	$DBRESULT = $pearDB->query("SELECT name FROM mod_bam WHERE ba_id = '".$_GET['bam_id']."' LIMIT 1");
	$qos_name_tab = $DBRESULT->fetchRow();
	$qos_name = $qos_name_tab["name"];

	$command_line .= " --interlaced -b 1024 --imgformat PNG --width=".$_GET['graph_width']." --height=".$_GET['graph_height']." --title='$qos_name' --vertical-label='".$GraphTemplate["vertical_label"]."' ";

	if (isset($GraphTemplate["lower_limit"]) && $GraphTemplate["lower_limit"] != NULL) {
		$command_line .= "--lower-limit ".$GraphTemplate["lower_limit"]." ";
	}
	if (isset($GraphTemplate["upper_limit"]) && $GraphTemplate["upper_limit"] != NULL) {
		$command_line .= "--upper-limit ".$GraphTemplate["upper_limit"]." ";
	}
	if ((isset($GraphTemplate["lower_limit"]) && $GraphTemplate["lower_limit"] != NULL) || (isset($GraphTemplate["upper_limit"]) && $GraphTemplate["upper_limit"] != NULL)) {
		$command_line .= "--rigid ";
	}

	# Init DS template For each curv
	$metrics = array();

	$DBRES = $pearDBO->query("SELECT id FROM index_data WHERE host_name LIKE '_Module_BAM_%' AND service_description LIKE 'ba_".$_GET['bam_id']."' LIMIT 1");
	$rowz = $DBRES->fetchRow();

	$DBRESULT = $pearDBO->query("SELECT metric_id, metric_name, unit_name FROM metrics WHERE index_id = '".$rowz["id"]."' ORDER BY metric_name");
	if (PEAR::isError($DBRESULT)) {
		print "Mysql Error : ".$DBRESULT->getDebugInfo();
	}

	$cpt = 0;
	$metrics = array();
	while ($metric = $DBRESULT->fetchRow()){
		$metrics[$metric["metric_id"]]["metric_id"] = $metric["metric_id"];
		$metrics[$metric["metric_id"]]["metric"] = str_replace("/", "", $metric["metric_name"]);
		$metrics[$metric["metric_id"]]["unit"] = $metric["unit_name"];
		
		$ds = bam_getDefaultDS($metric["metric_name"]);
		$metrics[$metric["metric_id"]]["ds_id"] = $ds;
		
		$res_ds = $pearDB->query("SELECT * FROM giv_components_template WHERE compo_id = '".$ds."'");
		$res_ds->fetchInto($ds_data);
		foreach ($ds_data as $key => $ds_d) {
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
	foreach ($metrics as $key => $tm) {
		if (isset($tm["ds_invert"]) && $tm["ds_invert"]) {
			$command_line .= " DEF:va".$cpt."=".$RRDdatabase_path.$key.".rrd:".$metrics[$key]["metric"].":AVERAGE CDEF:v".$cpt."=va".$cpt.",-1,* ";
		} else {
			$command_line .= " DEF:v".$cpt."=".$RRDdatabase_path.$key.".rrd:".$metrics[$key]["metric"].":AVERAGE ";
		}
		if ($tm["legend_len"] > $longer) {
			$longer = $tm["legend_len"];
		}
		$cpt++;
	}

	# Create Legende
	$cpt = 1;
	foreach ($metrics as $key => $tm) {
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
}
