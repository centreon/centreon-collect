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
if (file_exists($centreon_path . "www/class/Session.class.php")) {
    require_once($centreon_path . "www/class/Session.class.php");
}
if (file_exists($centreon_path . "www/class/Oreon.class.php")) {
    require_once($centreon_path . "www/class/Oreon.class.php");
}

require_once($centreon_path . "www/modules/centreon-bam-server/core/common/header.php");

$color = array();
$color["ok"] = $oreon->optGen["color_ok"];
$color["warning"] = $oreon->optGen["color_warning"];
$color["critical"] = $oreon->optGen["color_critical"];

$pearDB = new CentreonBAM_DB();
$pearDBO = new CentreonBAM_DB("centstorage");
$ba_acl = new CentreonBam_Acl($pearDB, $oreon->user->user_id);
$user_options = new CentreonBAM_Options($pearDB, $oreon->user->user_id);
$opt = $user_options->getUserOptions();

$buffer = new CentreonBAM_XML();
$buffer->startElement("chart");
/* Type of graph */
$buffer->writeElement("license", $xmlswf_lic);
/* The parameters */
$buffer->writeElement("chart_type", "Line");
/* LEGEND */
$buffer->startElement("legend");
$buffer->writeAttribute("bold", "false");
$buffer->writeAttribute("size", "10");
$buffer->writeAttribute("margin", "0");
$buffer->writeAttribute("bullet", "line");
$buffer->writeAttribute("layout", "hide");
$buffer->endElement();

/* SERIES COLOR */
$buffer->startElement("series_color");
$buffer->writeElement("color", substr($color["ok"], 1));
$buffer->writeElement("color", substr($color["warning"], 1));
$buffer->writeElement("color", substr($color["critical"], 1));
$buffer->endElement();

/* GRIDS */
$buffer->startElement("chart_grid_h");
$buffer->writeAttribute("alpha", "10");
$buffer->writeAttribute("thickness", "1");
$buffer->writeAttribute("type", "solid");
$buffer->endElement();
$buffer->startElement("chart_grid_v");
$buffer->writeAttribute("alpha", "10");
$buffer->writeAttribute("thickness", "1");
$buffer->writeAttribute("type", "solid");
$buffer->endElement();

/* BORDER THICKNESS */
$buffer->startElement("chart_border");
$buffer->writeAttribute("top_thickness", "1");
$buffer->writeAttribute("bottom_thickness", "1");
$buffer->writeAttribute("left_thickness", "1");
$buffer->writeAttribute("right_thickness", "1");
$buffer->endElement();

/* GUIDE */
$buffer->startElement("chart_guide");
$buffer->writeAttribute("horizontal", "true");
$buffer->writeAttribute("vertical", "true");
$buffer->writeAttribute("thickness", "1");
$buffer->writeAttribute("color", "000000");
$buffer->writeAttribute("alpha", "70");
$buffer->writeAttribute("type", "dashed");
$buffer->writeAttribute("radius", "5");
$buffer->writeAttribute("line_color", "A1A7B0");
$buffer->writeAttribute("line_alpha", "80");
$buffer->writeAttribute("line_thickness", "2");
$buffer->writeAttribute("text_color", "000000");
$buffer->writeAttribute("text_h_alpha", "90");
$buffer->endElement();

/* AXIS */
$buffer->startElement("axis_category");
$buffer->writeAttribute("size", "10");
$buffer->writeAttribute("color", "FF9900");
$buffer->writeAttribute("orientation", "vertical_up");
$buffer->endElement();
$buffer->startElement("axis_value");
$buffer->writeAttribute("min", "0");
$buffer->writeAttribute("max", "110");
$buffer->writeAttribute("size", "9");
$buffer->writeAttribute("bold", "true");
$buffer->writeAttribute("alpha", "50");
$buffer->writeAttribute("shadow", "low");
$buffer->writeAttribute("steps", "13");
$buffer->writeAttribute("show_min", "true");
$buffer->endElement();
$buffer->startElement("axis_ticks");
$buffer->writeAttribute("value_ticks", "true");
$buffer->writeAttribute("category_ticks", "true");
$buffer->writeAttribute("major_thickness", "2");
$buffer->writeAttribute("minor_thickness", "1");
$buffer->writeAttribute("major_color", "000000");
$buffer->writeAttribute("minor_color", "000000");
$buffer->writeAttribute("minor_count", "1");
$buffer->writeAttribute("position", "outside");
$buffer->endElement();

/* CHART NOTE */
$buffer->startElement("chart_note");
$buffer->writeAttribute("type", "lance");
$buffer->writeAttribute("size", "9");
$buffer->writeAttribute("background_color", "ff4400");
$buffer->writeAttribute("background_alpha", "75");
$buffer->writeAttribute("color", "ff4400");
$buffer->writeAttribute("offset_y", "-5");
$buffer->writeAttribute("y", "-15");
$buffer->writeAttribute("x", "0");
$buffer->endElement();

/* PREFERENCES */
$buffer->startElement("chart_pref");
$buffer->writeAttribute("line_thickness", "2");
$buffer->writeAttribute("point_shape", "circle");
$buffer->writeAttribute("point_size", "1");
$buffer->writeAttribute("drag", "true");
$buffer->writeAttribute("fill_shape", "false");
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
$buffer->writeAttribute("x", "110");
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

/* SCROLL */
$buffer->startElement("scroll");
$buffer->writeAttribute("scroll_detail", "65");
$buffer->writeAttribute("x", "120");
$buffer->writeAttribute("y", "50");
$buffer->writeAttribute("width", "760");
$buffer->writeAttribute("height", '20');
$buffer->writeAttribute("url_button_1_idle", 'modules/centreon-bam-server/core/lib/xmlswf/resources/scroll/slider_L.png');
$buffer->writeAttribute("url_button_2_idle", 'modules/centreon-bam-server/core/lib/xmlswf/resources/scroll/slider_R.png');
$buffer->writeAttribute("button_length", '23');
$buffer->writeAttribute("url_button_1_over", 'modules/centreon-bam-server/core/lib/xmlswf/resources/scroll/slider_L.png');
$buffer->writeAttribute("url_button_1_press", 'modules/centreon-bam-server/core/lib/xmlswf/resources/scroll/slider_L.png');
$buffer->writeAttribute("url_button_2_over", 'modules/centreon-bam-server/core/lib/xmlswf/resources/scroll/slider_R.png');
$buffer->writeAttribute("url_button_2_press", 'modules/centreon-bam-server/core/lib/xmlswf/resources/scroll/slider_R.png');
$buffer->writeAttribute("url_slider_body", 'modules/centreon-bam-server/core/lib/xmlswf/resources/scroll/slider_handle.gif');
$buffer->writeAttribute("url_slider_handle_1", 'modules/centreon-bam-server/core/lib/xmlswf/resources/scroll/slider_handle_L.gif');
$buffer->writeAttribute("url_slider_handle_2", 'modules/centreon-bam-server/core/lib/xmlswf/resources/scroll/slider_handle_R.gif');
$buffer->writeAttribute("slider_handle_length", '10');
$buffer->writeAttribute("gap", '2');
isset($_GET['start']) ? $scroll_start = $_GET['start'] : $scroll_start = "10";
isset($_GET['span']) ? $scroll_span = $_GET['span'] : $scroll_span = "100";
$buffer->writeAttribute("start", $scroll_start);
$buffer->writeAttribute("span", $scroll_span);
$buffer->writeAttribute("drag", 'true');
$buffer->writeAttribute("alpha", '100');
$buffer->endElement();
$buffer->startElement("flash_to_javascript");
$buffer->writeAttribute("Scrolled_Chart", "true");
$buffer->endElement();

/* LINK DATA */
$buffer->startElement("link_data");
$buffer->writeAttribute("url", "javascript:displayKpiLogs( _category_ )");
$buffer->writeAttribute("target", "javascript");
$buffer->endElement();


/* DATA */
if (isset($_GET['var'])) {
    $tab = explode(";", $_GET['var']);
    $my_ba_id = $tab[0];
    $my_tp = $tab[1];
} else {
    (isset($_GET['ba_id'])) ? $my_ba_id = $_GET['ba_id'] : $my_ba_id = 0;
    (isset($_GET['tp'])) ? $my_tp = $_GET['tp'] : $my_tp = 'today';
}


$log_meth = new CentreonBAM_Log($pearDB, $pearDBO, $my_ba_id);
$dates = $log_meth->getPeriodToReport($my_tp);
$buffer->writeElement("start", $dates[0]);
$buffer->writeElement("end", $dates[1]);
$logs = $log_meth->retrieveLogs($dates[0], $dates[1]);
$buffer->startElement("chart_data");
$buffer->startElement("row");
$buffer->text("<null/>");
foreach ($logs as $key => $data) {
    $buffer->startElement("string");
    $buffer->text(date("m/d/Y H:i:s", $data['start_time']));
    $buffer->endElement();
}
$buffer->endElement();
$buffer->startElement("row");
$buffer->writeElement("string", "Level");
$counter = 1;
$max = sizeof($logs);

$statusArr = array(
    0 => _('OK'),
    1 => _('Warning'),
    2 => _('Critical'),
    3 => _('Unknown'),
    4 => _('Unreachable')
);
foreach ($logs as $key => $data) {
    $buffer->startElement("number");
    if (isset($_GET['details']) && $_GET['details'] == 'true') {
        $buffer->writeAttribute('note', $statusArr[$data['status']]);
    }
    $buffer->text($data['first_level']);
    $buffer->endElement();
    $counter++;
}
$buffer->endElement();
$buffer->endElement();

$buffer->endElement();

header('Content-Type: text/xml');
header('Pragma: no-cache');
header('Expires: 0');
header('Cache-Control: no-cache, must-revalidate');

$buffer->output();
?>
