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


require_once ("./modules/centreon-bam-server/core/common/header.php");
require_once ("./modules/centreon-bam-server/centreon-bam-server.conf.php");
require_once ("./modules/centreon-bam-server/core/common/functions.php");

$path = "./modules/centreon-bam-server/core/configuration/kpi/template";

$search = "";
$rows = 0;
$tmp = NULL;

include("./include/common/autoNumLimit.php");

# Smarty template Init
$tpl = new Smarty();
$tpl = initSmartyTpl($path, $tpl);

$pearDB = new CentreonBam_Db();
$iconObj = new CentreonBam_Icon($pearDB);

$versionDesign = version_compare(getCentreonVersion($pearDB), '2.6.999', '<');

if (isset($_POST["o1"]) && isset($_POST["o2"])){
	if ($_POST["o1"] != "") {
		$o = $_POST["o1"];
	}
	if ($_POST["o2"] != "") {
		$o = $_POST["o2"];
	}
}


$searchBA = "";
if (isset($_POST['searchBA'])) {
    $searchBA = $_POST['searchBA'];
} elseif (isset($_SESSION['searchBaKpi']) && $_SESSION['searchBaKpi']) {
    $searchBA = $_SESSION['searchBaKpi'];
} elseif (isset($_GET['search']) && $_GET['search']) {
    $searchBA = $_GET['search'];
}
$_SESSION['searchBaKpi'] = $searchBA;

$searchKPI = "";
if (isset($_POST['searchKPI'])) {
    $searchKPI = $_POST['searchKPI'];
} elseif (isset($_SESSION['searchKpiKpi']) && $_SESSION['searchKpiKpi']) {
    $searchKPI = $_SESSION['searchKpiKpi'];
} elseif (isset($_GET['searchKPI']) && $_GET['searchKPI']) {
    $searchKPI = $_GET['searchKPI'];
}
$_SESSION['searchKpiKpi'] = $searchKPI;


$search_arr = array();

if ($searchBA != "") {
	$search_arr[] = " ba_name LIKE '%".$pearDB->escape($searchBA)."%' ";
}

if ($searchKPI != "") {
    $search_arr[] = " kpi_name LIKE '%".$pearDB->escape($searchKPI)."%' ";
}

$search_str = count($search_arr) ? ' WHERE ' . implode(' AND ', $search_arr) : '';

$rq = 'SELECT SQL_CALC_FOUND_ROWS * FROM '
    . '(SELECT *, concat(kpi_host, " - ", kpi_service) AS kpi_name '
    . 'FROM mod_bam_view_kpi '
    . 'WHERE kpi_host IS NOT NULL '
    . 'AND kpi_service IS NOT NULL '
    . 'UNION '
    . 'SELECT *, kpi_meta AS kpi_name '
    . 'FROM mod_bam_view_kpi '
    . 'WHERE kpi_meta IS NOT NULL '
    . 'UNION '
    . 'SELECT *, kpi_ba AS kpi_name '
    . 'FROM mod_bam_view_kpi '
    . 'WHERE kpi_ba IS NOT NULL '
    . 'UNION '
    . 'SELECT *, kpi_boolean AS kpi_name '
    . 'FROM mod_bam_view_kpi '
    . 'WHERE kpi_boolean IS NOT NULL) AS subquery '
    . $search_str;

$DBRES = $pearDB->query($rq . 'LIMIT '  . ($num * $limit) . ', ' . $limit);
$rows = $pearDB->numberRows();

if (!($DBRES->numRows())) {
    $DBRES = $pearDB->query($rq . 'LIMIT ' . (floor($rows / $limit) * $limit) . ", " . $limit);
}

$form = new HTML_QuickForm('select_form', 'POST', "?p=".$p);

$elemArr = array();
while ($row = $DBRES->fetchRow()) {
    $label = "";
    $type = "";
    if ($row['kpi_type'] == '0') { // Standard KPI service
        $label = slashConvert($row['kpi_host'] . " - " . $row['kpi_service']);
        $type = _("Service");
    } elseif ($row['kpi_type'] == '1') { // meta
        $label = $row['kpi_meta'];
        $type = _("Meta Service");
    } elseif ($row['kpi_type'] == '2') { // business activity
        $label = $row['kpi_ba'];
        $type = _("Business Activity");
        $elemArr[$row['ba_id']][$row['kpi_id']]['kpi_label'] = $ba->getBA_Name($row['id_indicator_ba']);
    } elseif ($row['kpi_type'] == '3') { // boolean rule
        $type = _("Boolean");
        $label = $row['kpi_boolean'];
    }

    $elemArr[$row['ba_id']]['kpi'][$label]['type'] = $type;
    $elemArr[$row['ba_id']]["label"] = sprintf(_("%s [ Warning : %s%% - Critical : %s%% ]"), $row['ba_name'], $row['level_w'], $row['level_c']);
    $elemArr[$row['ba_id']]["icon"] = $iconObj->getFullIconPath($row['icon_id']);
    $elemArr[$row['ba_id']]["nbKPI"] = 1;
    $selectedElements = $form->addElement('checkbox', "select[".$row['kpi_id']."]");
    $elemArr[$row['ba_id']]['kpi'][$label]['select'] = $selectedElements->toHtml();
    $elemArr[$row['ba_id']]['kpi'][$label]['id'] = $row['kpi_id'];
    $elemArr[$row['ba_id']]['kpi'][$label]['url_edit'] = "./main.php?p=".$p."&o=c&kpi_id=".$row['kpi_id'];
    $elemArr[$row['ba_id']]['kpi'][$label]['url_set_enable'] = "./main.php?p=".$p."&o=sa&kpi_id=".$row['kpi_id'];
    $elemArr[$row['ba_id']]['kpi'][$label]['url_set_disable'] = "./main.php?p=".$p."&o=su&kpi_id=".$row['kpi_id'];
    $elemArr[$row['ba_id']]['kpi'][$label]['activate'] = $row['kpi_activate'];
    
    if ($row['config_type']) {
        $elemArr[$row['ba_id']]['kpi'][$label]['drop_warning'] = $row['drop_warning'] . '%';
        $elemArr[$row['ba_id']]['kpi'][$label]['drop_critical'] = $row['drop_critical'] . '%';
        $elemArr[$row['ba_id']]['kpi'][$label]['drop_unknown'] = $row['drop_unknown'] . '%';
    } else {
        $elemArr[$row['ba_id']]['kpi'][$label]['drop_warning'] = $kpi->getCriticityLabel($kpi->getCriticityCode($row['drop_warning_impact_id']));
        $elemArr[$row['ba_id']]['kpi'][$label]['drop_critical'] = $kpi->getCriticityLabel($kpi->getCriticityCode($row['drop_critical_impact_id']));
        $elemArr[$row['ba_id']]['kpi'][$label]['drop_unknown'] = $kpi->getCriticityLabel($kpi->getCriticityCode($row['drop_unknown_impact_id']));
    }
    $elemArr[$row['ba_id']]['kpi'][$label]['typecode'] = $row['kpi_type'];
}

include("./include/common/checkPagination.php");

// Order kpi by name
foreach ($elemArr as &$baKpi) {
    ksort($baKpi['kpi'], SORT_STRING | SORT_FLAG_CASE);
}

$tpl->assign("elemArr", $elemArr);
$tpl->assign("no_KPI_defined", _("No KPI for this Business Activity, please add a new one."));
$tpl->assign("actions", _("Actions"));
$tpl->assign("kpi_type", _("Type"));
$tpl->assign("kpi_label", _("Key Performance Indicator"));
$tpl->assign("warning_label", _("Warning Impact"));
$tpl->assign("critical_label", _("Critical Impact"));
$tpl->assign("unknown_label", _("Unknown Impact"));
$tpl->assign("ba_label", _("Business Activity"));
$tpl->assign("search_label", _("Search"));

$tpl->assign('msg', array ("addL"=>"?p=".$p."&o=a",
							"add"=>_("Add a KPI"),
							"addLM"=>"?p=".$p."&o=am",
                                                        "addLB"=>"?p=".$p."&o=ab",
							"addLCSV"=>"?p=".$p."&o=csv",
							"addCSV"=>_("Load .csv file"),
							"addM"=>_("Add multiple KPI"),
							"delConfirm"=>_("Do you confirm the deletion ?"),
							"imgCSV" => "./modules/centreon-bam-server/core/common/images/data_into.gif",
							"imgM" => "./modules/centreon-bam-server/core/common/images/element_new_after.gif",
							"img" => "./modules/centreon-bam-server/core/common/images/add2.png"));
?>
<script type="text/javascript">
	function setO(_i) {
		document.forms['form'].elements['o1'].value = _i;
		document.forms['form'].elements['o2'].value = _i;
	}
</script>
<?php
$attrs1 = array(
		'onchange'=>"javascript: " .
            " var bChecked = isChecked(); ".
            " if (this.form.elements['o1'].selectedIndex != 0 && !bChecked) {".
            " alert('"._("Please select one or more items")."'); return false;} " .
			"if (this.form.elements['o1'].selectedIndex == 1 && confirm('"._("Do you confirm the deletion ?")."')) {" .
			" 	setO(this.form.elements['o1'].value); submit();} " .
			"else if (this.form.elements['o1'].selectedIndex == 2) {" .
			" 	setO(this.form.elements['o1'].value); submit();} " .
			"else if (this.form.elements['o1'].selectedIndex == 3) {" .
			" 	setO(this.form.elements['o1'].value); submit();} " .
			"this.form.elements['o1'].selectedIndex = 0");
$form->addElement('select', 'o1', NULL, array(NULL=>_("More actions..."), "md"=>_("Delete"), "ma"=>_("Enable"), "mu"=>_("Disable")), $attrs1);

$attrs2 = array(
		'onchange'=>"javascript: " .
            " var bChecked = isChecked(); ".
            " if (this.form.elements['o2'].selectedIndex != 0 && !bChecked) {".
            " alert('"._("Please select one or more items")."'); return false;} " .
			"if (this.form.elements['o2'].selectedIndex == 1 && confirm('"._("Do you confirm the deletion ?")."')) {" .
			" 	setO(this.form.elements['o2'].value); submit();} " .
			"else if (this.form.elements['o2'].selectedIndex == 2) {" .
			" 	setO(this.form.elements['o2'].value); submit();} " .
			"else if (this.form.elements['o2'].selectedIndex == 3) {" .
			" 	setO(this.form.elements['o2'].value); submit();} " .
			"this.form.elements['o2'].selectedIndex = 0");
$form->addElement('select', 'o2', NULL, array(NULL=>_("More actions..."), "md"=>_("Delete"), "ma"=>_("Enable"), "mu"=>_("Disable")), $attrs2);

$o1 = $form->getElement('o1');
$o1->setValue(NULL);
$o2 = $form->getElement('o2');
$o2->setValue(NULL);

$tpl->assign('limit', $limit);
$tpl->assign("img_edit", "./modules/centreon-bam-server/core/common/images/document_edit.gif");
$tpl->assign("img_delete", "./modules/centreon-bam-server/core/common/images/garbage_empty.gif");
$tpl->assign("img_set_enable", "./modules/centreon-bam-server/core/common/images/element_next.gif");
$tpl->assign("img_set_disable", "./modules/centreon-bam-server/core/common/images/element_previous.gif");

$tpl->assign("warning_level", _("Warning"));
$tpl->assign("critical_level", _("Critical"));
$tpl->assign("searchBA", $searchBA);
$tpl->assign("searchKPI", $searchKPI);

$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
$form->accept($renderer);
$tpl->assign('form', $renderer->toArray());
$tpl->display("configuration_kpi_list.ihtml");
