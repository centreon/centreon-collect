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

$path = "./modules/centreon-bam-server/core/configuration/boolean/template";

$search = "";
$rows = 0;
$tmp = NULL;
include("./include/common/autoNumLimit.php");

# Smarty template Init
$tpl = new Smarty();
$tpl = initSmartyTpl($path, $tpl);

$pearDB = new CentreonBAM_DB();

$versionDesign = version_compare(getCentreonVersion($pearDB), '2.6.999', '<');

if (isset($_POST["o1"]) && isset($_POST["o2"])){
	if ($_POST["o1"] != "")
		$o = $_POST["o1"];
	if ($_POST["o2"] != "")
		$o = $_POST["o2"];
}

$search = "";
if (isset($_POST['searchBool'])) {
	$search = $_POST['searchBool'];
} elseif (isset($_SESSION['searchBool']) && $_SESSION['searchBool']) {
    $search = $_SESSION['searchBool'];
} elseif (isset($_GET['search']) && $_GET['search']) {
	$search = $_GET['search'];
}
$_SESSION['searchBool'] = $search;
$search_str = "";
if ($search != "") {
	$search_str = " WHERE name LIKE '%".$pearDB->escape($search)."%' ";
}

$rq = "SELECT COUNT(*) as rows FROM `mod_bam_boolean`" . $search_str;
$DBRES = $pearDB->query($rq);
$row = $DBRES->fetchRow();
$rows = $row['rows'];

include("./include/common/checkPagination.php");

$form = new HTML_QuickForm('select_form', 'POST', "?p=".$p);

/* Retrieve list of BA Groups */
$rq = "SELECT * FROM `mod_bam_boolean` ". $search_str ." ORDER BY `name` LIMIT ".$num * $limit.", ".$limit;
$DBRES = $pearDB->query($rq);
$elemArr = array();
$tdStyle = "list_one";
while ($row = $DBRES->fetchRow()) {
	$selectedElements = $form->addElement('checkbox', "select[".$row['boolean_id']."]");
	$elemArr[$row['boolean_id']]['select'] = $selectedElements->toHtml();
	$elemArr[$row['boolean_id']]['url_edit'] = "./main.php?p=".$p."&o=c&boolean_id=".$row['boolean_id'];
	$elemArr[$row['boolean_id']]['url_delete'] = "./main.php?p=".$p."&o=d&boolean_id=".$row['boolean_id'];
	$elemArr[$row['boolean_id']]['activate'] = $row['activate'] ? _('Enabled') : _('Disabled');
	$elemArr[$row['boolean_id']]['name'] = htmlspecialchars($row['name']);
	$elemArr[$row['boolean_id']]['desc'] = htmlspecialchars($row['comments']);
	$elemArr[$row['boolean_id']]['style'] = $tdStyle;
	$dupElements = $form->addElement('text', 'dup_'.$row['boolean_id'], NULL, array("id" => "dup_".$row['boolean_id'], "size" => "3", "value" => "1"));
	$elemArr[$row['boolean_id']]['dup'] = $dupElements->toHtml();
	if ($tdStyle == "list_one") {
		$tdStyle = "list_two";
	} else {
		$tdStyle = "list_one";
	}
}
$nbGroups = $DBRES->numRows();
$tpl->assign("elemArr", $elemArr);
$tpl->assign("nbGroups", $nbGroups);
$tpl->assign("no_group_defined", _("No boolean rule found"));
$tpl->assign("group_name", _("Name"));
$tpl->assign("description", _("Description"));
$tpl->assign("status_label", _("Status"));
$tpl->assign("actions", _("Actions"));
$tpl->assign("boolean_label", _("Boolean rules"));
$tpl->assign("search_label", _("Search"));
$tpl->assign("search", $search);

$tpl->assign('msg', array (
        "addL"=>"?p=".$p."&o=a", 
        "add"=>_("Add"), 
        "delConfirm"=>_("Do you confirm the deletion ?"), 
        "img" => "./modules/centreon-bam-server/core/common/images/add2.png")
        );
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
$form->addElement('select', 'o1', null, array(null => _("More actions..."), "md"=>_("Delete"), "ma"=>_("Enable"), "mu"=>_("Disable"), "dp"=>_("Duplicate")), $attrs1);

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
			"this.form.elements['o2'].selectedIndex = 0");
$form->addElement('select', 'o2', null, array(null => _("More actions..."), "md"=>_("Delete"), "ma"=>_("Enable"), "mu"=>_("Disable"), "dp"=>_("Duplicate")), $attrs2);

$o1 = $form->getElement('o1');
$o1->setValue(null);
$o2 = $form->getElement('o2');
$o2->setValue(null);

$tpl->assign('limit', $limit);
$tpl->assign("img_edit", "./modules/centreon-bam-server/core/common/images/document_edit.gif");
$tpl->assign("img_delete", "./modules/centreon-bam-server/core/common/images/garbage_empty.gif");
$tpl->assign("img_set_visible", "./modules/centreon-bam-server/core/common/images/star_yellow.gif");
$tpl->assign("img_set_invisible", "./modules/centreon-bam-server/core/common/images/star_yellow_delete.png");


$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
$form->accept($renderer);
$tpl->assign('form', $renderer->toArray());
$tpl->display("configuration_boolean_list.ihtml");
