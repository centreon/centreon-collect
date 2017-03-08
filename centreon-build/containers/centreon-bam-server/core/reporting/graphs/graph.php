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

require_once("../../../centreon-bam-server.conf.php");
require_once("../../common/functions.php");

require_once($centreon_path . "www/modules/centreon-bam-server/core/common/header.php");

$color = array();
$color["ok"] = "#88b917";
$color["warning"] = "#ff9a13";
$color["critical"] = "#e00b3d";
$color["undetermined"] = "#bcbdc0";

$pearDB = new CentreonBam_Db();
$ba_acl = new CentreonBam_Acl($pearDB, $oreon->user->user_id);
$user_options = new CentreonBam_Options($pearDB, $oreon->user->user_id);
$opt = $user_options->getUserOptions();

$buffer = new CentreonBam_Xml();
$buffer->startElement("chart");
/* Type of graph */
$buffer->writeElement("license", $xmlswf_lic);
/* The parameters */
	$buffer->writeElement("chart_type", "3D pie");
	/* LEGEND */
	$buffer->startElement("legend");
		$buffer->writeAttribute("bold", "false");
		$buffer->writeAttribute("size", "10");
		$buffer->writeAttribute("margin", "0");
		$buffer->writeAttribute("bullet", "line");
		$buffer->writeAttribute("layout", "hide");
	$buffer->endElement();

	/* SERIES COLOR*/
	$buffer->startElement("series_color");
		$buffer->writeElement("color", substr($color["ok"], 1));
		$buffer->writeElement("color", substr($color["warning"], 1));
		$buffer->writeElement("color", substr($color["critical"], 1));
		$buffer->writeElement("color", substr($color["undetermined"], 1));
	$buffer->endElement();

	/* BORDER THICKNESS */
	$buffer->startElement("chart_border");
		$buffer->writeAttribute("top_thickness", "0");
		$buffer->writeAttribute("bottom_thickness", "0");
		$buffer->writeAttribute("left_thickness", "0");
		$buffer->writeAttribute("right_thickness", "0");
	$buffer->endElement();

	/* AXIS */
	$buffer->startElement("axis_category");
		$buffer->writeAttribute("size", "10");
		$buffer->writeAttribute("color", "FF9900");
		$buffer->writeAttribute("orientation", "horizontal");
	$buffer->endElement();
	$buffer->startElement("axis_value");
		$buffer->writeAttribute("min", "0");
		$buffer->writeAttribute("max", "100");
		$buffer->writeAttribute("size", "9");
		$buffer->writeAttribute("bold", "true");
		$buffer->writeAttribute("alpha", "50");
		$buffer->writeAttribute("show_min", "true");
	$buffer->endElement();
	$buffer->startElement("axis_ticks");
		$buffer->writeAttribute("value_ticks", "false");
		$buffer->writeAttribute("major_thickness", "1");
		$buffer->writeAttribute("minor_thickness", "1");
		$buffer->writeAttribute("major_color", "000000");
		$buffer->writeAttribute("minor_color", "000000");
		$buffer->writeAttribute("minor_count", "1");
		$buffer->writeAttribute("position", "centered");
	$buffer->endElement();

	/* CHART GUIDE */
	if ($opt['display_guide']) {
		$buffer->startElement("chart_guide");
			$buffer->writeAttribute("horizontal", "true");
			$buffer->writeAttribute("vertical", "true");
			$buffer->writeAttribute("thickness", "1");
			$buffer->writeAttribute("color", "ff4400");
			$buffer->writeAttribute("alpha", "75");
			$buffer->writeAttribute("type", "solid");
		$buffer->endElement();
	}

	/* CHART NOTE */
	$buffer->startElement("chart_note");
		$buffer->writeAttribute("type", "flag");
		$buffer->writeAttribute("size", "9");
		$buffer->writeAttribute("background_color", "ff4400");
		$buffer->writeAttribute("background_alpha", "75");
		$buffer->writeAttribute("color", "ffffff");
	$buffer->endElement();

	/* PREFERENCES */
	$buffer->startElement("chart_pref");
		$buffer->writeAttribute("select", "false");
		$buffer->writeAttribute("drag", "true");
		$buffer->writeAttribute("rotation_x", "50");
	$buffer->endElement();

	/* CONTEXT MENU */
	$buffer->startElement("context_menu");
		$buffer->writeAttribute("about", "false");
		$buffer->writeAttribute("print", "true");
		$buffer->writeAttribute("quality", "true");
		$buffer->writeAttribute("jpeg_url", "./modules/centreon-bam-server/core/dashboard/tmp/output_jpg.php");
	$buffer->endElement();

	/* CHART RECT */
	$buffer->startElement("chart_rect");
		$buffer->writeAttribute("x", "60");
	$buffer->endElement();

	/* FILTER */
	$buffer->startElement("filter");
		$buffer->startElement("shadow");
			$buffer->writeAttribute("id", "shadow1");
			$buffer->writeAttribute("distance", "3");
			$buffer->writeAttribute("angle", "45");
			$buffer->writeAttribute("blurX", "5");
			$buffer->writeAttribute("blurY", "5");
			$buffer->writeAttribute("alpha", "50");
		$buffer->endElement();
		$buffer->startElement("bevel");
			$buffer->writeAttribute("id", "bevel1");
			$buffer->writeAttribute("distance", "3");
			$buffer->writeAttribute("angle", "45");
			$buffer->writeAttribute("blurX", "5");
			$buffer->writeAttribute("blurY", "5");
			$buffer->writeAttribute("alpha", "50");
		$buffer->endElement();
	$buffer->endElement();

	/* Transition */
	$buffer->startElement("chart_transition");
		$buffer->writeAttribute("type", "zoom");
		$buffer->writeAttribute("delay", "0.2");
		$buffer->writeAttribute("duration", "0.4");
	$buffer->endElement();

	/* DATA */
	$get_var = str_replace(",", ".", $_GET['var']);
	$var_tab = explode(";", $get_var);
	$ba_name = $var_tab[0];
	$ok_val = $var_tab[1];
	$warning_val = $var_tab[2];
	$critical_val = $var_tab[3];
	$undetermined_val = $var_tab[4];
	$buffer->startElement("chart_data");
		$buffer->startElement("row");
				$buffer->text("<null/>");
				$buffer->writeElement("string", "OK");
				$buffer->writeElement("string", "Warning");
				$buffer->writeElement("string", "Critical");
				$buffer->writeElement("string", "Undetermined");
		$buffer->endElement();
		$buffer->startElement("row");
				$buffer->writeElement("string", $ba_name);
				$buffer->startElement("number");
					$buffer->writeAttribute("label", $ok_val . "%");
					$buffer->writeAttribute("alpha", "80");
					$buffer->text($ok_val);
				$buffer->endElement();
				$buffer->startElement("number");
					$buffer->writeAttribute("label", $warning_val . "%");
					$buffer->writeAttribute("alpha", "80");
					$buffer->text($warning_val);
				$buffer->endElement();
				$buffer->startElement("number");
					$buffer->writeAttribute("label", $critical_val . "%");
					$buffer->writeAttribute("alpha", "80");
					$buffer->text($critical_val);
				$buffer->endElement();
				$buffer->startElement("number");
					$buffer->writeAttribute("label", $undetermined_val . "%");
					$buffer->writeAttribute("alpha", "80");
					$buffer->text($undetermined_val);
				$buffer->endElement();
		$buffer->endElement();
	$buffer->endElement();

	/* BA View Label */
	$buffer->startElement("draw");
		$buffer->startElement("text");
			$buffer->writeAttribute("color", "000000");
			$buffer->writeAttribute("alpha", "25");
			$buffer->writeAttribute("rotation", "0");
			$buffer->writeAttribute("x", "-45");
			$buffer->writeAttribute("y", "0");
			$buffer->writeAttribute("h_align", "center");
			$buffer->writeAttribute("v_align", "top");
			$buffer->writeAttribute("size", "20");
			$buffer->text($ba_name);
		$buffer->endElement();
	$buffer->endElement();

$buffer->endElement();

header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');

$buffer->output();
