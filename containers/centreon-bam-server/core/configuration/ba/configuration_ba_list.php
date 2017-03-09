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

$path = "./modules/centreon-bam-server/core/configuration/ba/template";

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
	if ($_POST["o1"] != "")
		$o = $_POST["o1"];
	if ($_POST["o2"] != "")
		$o = $_POST["o2"];
}

$search = "";
if (isset($_POST['searchBA'])) {
	$search = $_POST['searchBA'];
} elseif (isset($_SESSION['searchBA']) && $_SESSION['searchBA']) {
    $search = $_SESSION['searchBA'];
} elseif (isset($_GET['search']) && $_GET['search']) {
	$search = $_GET['search'];
}
$_SESSION['searchBA'] = $search;
$search_str = "";
if ($search != "") {
	$search_str = " WHERE name LIKE '%".$pearDB->escape($search)."%' ";
}

$rq = "SELECT COUNT(*) as rows FROM `mod_bam` ".$search_str;
$DBRES = 	$pearDB->query($rq);
$row = $DBRES->fetchRow();
$rows = $row['rows'];

include("./include/common/checkPagination.php");


$form = new HTML_QuickForm('select_form', 'POST', "?p=".$p);

/* Retrieve list of BA Groups */
$rq = "SELECT * FROM `mod_bam` ".$search_str." ORDER BY `name` LIMIT ".($num * $limit).", ".$limit;
$DBRES = $pearDB->query($rq);
$elemArr = array();
$tdStyle = "list_one";
while ($row = $DBRES->fetchRow()) {
	$selectedElements = $form->addElement('checkbox', "select[".$row['ba_id']."]");
	$elemArr[$row['ba_id']]['select'] = $selectedElements->toHtml();
	$elemArr[$row['ba_id']]['url_edit'] = "./main.php?p=".$p."&o=c&ba_id=".$row['ba_id'];
	$elemArr[$row['ba_id']]['url_delete'] = "./main.php?p=".$p."&o=d&ba_id=".$row['ba_id'];
	$elemArr[$row['ba_id']]['url_set_enable'] = "./main.php?p=".$p."&o=sa&ba_id=".$row['ba_id'];
	$elemArr[$row['ba_id']]['url_set_disable'] = "./main.php?p=".$p."&o=su&ba_id=".$row['ba_id'];
	$elemArr[$row['ba_id']]['icon'] = $iconObj->getFullIconPath($row['icon_id']);
	$elemArr[$row['ba_id']]['name'] = htmlspecialchars($row['name']);
	$elemArr[$row['ba_id']]['desc'] = htmlspecialchars($row['description']);
	$elemArr[$row['ba_id']]['warning'] = $row['level_w'];
	$elemArr[$row['ba_id']]['critical'] = $row['level_c'];
	$elemArr[$row['ba_id']]['activate'] = $row['activate'];
	$elemArr[$row['ba_id']]['style'] = $tdStyle;
	
	$dupElements = $form->addElement('text', 'dup_'.$row['ba_id'], NULL, array("id" => "dup_".$row['ba_id'], "size" => "3", "value" => "1"));
	$elemArr[$row['ba_id']]['dup'] = $dupElements->toHtml();
	
	($tdStyle == "list_one") ? $tdStyle = "list_two" : $tdStyle = "list_one";
}

$tpl->assign("elemArr", $elemArr);
$tpl->assign("nbBA", $DBRES->numRows());
$tpl->assign("no_ba_defined", _("No Business Activity found."));
$tpl->assign("ba_name", _("Name"));
$tpl->assign("description", _("Description"));
$tpl->assign("displayed", _("Displayed"));
$tpl->assign("actions", _("Actions"));
$tpl->assign("ba_label", _("Business Activity"));
$tpl->assign("search_label", _("Search"));

$tpl->assign('msg', array ("addL"=>"?p=".$p."&o=a", "add"=>_("Add"), "delConfirm"=>_("Do you confirm the deletion ?"), "img" => "./modules/centreon-bam-server/core/common/images/add2.png"));
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
			"else if (this.form.elements['o1'].selectedIndex == 4) {" .
			" 	setO(this.form.elements['o1'].value); submit();} " .
			"this.form.elements['o1'].selectedIndex = 0");
$form->addElement('select', 'o1', NULL, array(NULL=>_("More actions..."), "md"=>_("Delete"), "ma"=>_("Enable"), "mu"=>_("Disable"), "dp"=>_("Duplicate")), $attrs1);

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
			"else if (this.form.elements['o2'].selectedIndex == 4) {" .
			" 	setO(this.form.elements['o2'].value); submit();} " .
			"this.form.elements['o1'].selectedIndex = 0");
$form->addElement('select', 'o2', NULL, array(NULL=>_("More actions..."), "md"=>_("Delete"), "ma"=>_("Enable"), "mu"=>_("Disable"), "dp"=>_("Duplicate")), $attrs2);

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
$tpl->assign("search", $search);

$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
$form->accept($renderer);
$tpl->assign('form', $renderer->toArray());
$tpl->display("configuration_ba_list.ihtml");
