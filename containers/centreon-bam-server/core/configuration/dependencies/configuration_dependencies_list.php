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

$path = "./modules/centreon-bam-server/core/configuration/dependencies/template";

$search = "";
$rows = 0;
$tmp = NULL;
include("./include/common/autoNumLimit.php");

# Smarty template Init
$tpl = new Smarty();
$tpl = initSmartyTpl($path, $tpl);

$iconObj = new CentreonBam_Icon($centreonDb);

$versionDesign = version_compare(getCentreonVersion($centreonDb), '2.6.999', '<');

if (isset($_POST["o1"]) && isset($_POST["o2"])){
	if ($_POST["o1"] != "")
		$o = $_POST["o1"];
	if ($_POST["o2"] != "")
		$o = $_POST["o2"];
}


$tpl->assign("img_edit", "./modules/centreon-bam-server/core/common/images/document_edit.gif");
$tpl->assign("img_delete", "./modules/centreon-bam-server/core/common/images/garbage_empty.gif");
$tpl->assign("img_set_enable", "./modules/centreon-bam-server/core/common/images/element_next.gif");
$tpl->assign("img_set_disable", "./modules/centreon-bam-server/core/common/images/element_previous.gif");

$tpl->assign("warning_level", _("Warning"));
$tpl->assign("critical_level", _("Critical"));
$tpl->assign("search", $search);


$tpl->assign("headerMenu_name", _("Name"));
$tpl->assign("headerMenu_description", _("Description"));
$tpl->assign("headerMenu_options", _("Options"));

$tpl->assign(
    'msg',
    array (
        "addL" => "?p=".$p."&o=a",
        "add" => _("Add"), "delConfirm"=>_("Do you confirm the deletion ?"),
        "img" => "./modules/centreon-bam-server/core/common/images/add2.png"
    )
);

$dependencyList = CentreonBam_Dependency::getList($centreonDb);

$form = new HTML_QuickForm('select_form', 'POST', "?p=".$p);

# Different style between each lines
$style = "one";

# Fill a tab with a mutlidimensionnal Array we put in $tpl
$elemArr = array();
$i = 0;

foreach ($dependencyList as $dep) {
    $moptions = "";
    $selectedElements = $form->addElement('checkbox', "select[" . $dep['dep_id'] . "]");
    $moptions .= "&nbsp;<input onKeypress=\"if(event.keyCode > 31 && (event.keyCode < 45 || event.keyCode > 57)) "
        . "event.returnValue = false; if(event.which > 31 && (event.which < 45 || event.which > 57)) return false;\" "
        . "maxlength=\"3\" size=\"3\" value='1' style=\"margin-bottom:0px;\" name='dupNbr["
        . $dep['dep_id']."]'></input>";
    $elemArr[$i] = array(
        "MenuClass"=>"list_".$style,
        "RowMenu_select" => $selectedElements->toHtml(),
        "RowMenu_name" => CentreonUtils::escapeSecure(myDecode($dep["dep_name"])),
        "RowMenu_description" => CentreonUtils::escapeSecure(myDecode($dep["dep_description"])),
        "RowMenu_link" => "?p=".$p."&o=c&dep_id=".$dep['dep_id'],
        "RowMenu_options" => $moptions
    );
    $style != "two" ? $style = "two" : $style = "one";
    $i++;
}
$tpl->assign("elemArr", $elemArr);

$attrs1 = array(
    'onchange' => "javascript: " .
        " var bChecked = isChecked(); ".
        " if (this.form.elements['o1'].selectedIndex != 0 && !bChecked) {".
        " alert('"._("Please select one or more items")."'); return false;} " .
        "if (this.form.elements['o1'].selectedIndex == 1 && confirm('"._("Do you confirm the duplication ?")."')) {" .
        " 	setO(this.form.elements['o1'].value); submit();} " .
        "else if (this.form.elements['o1'].selectedIndex == 2 && confirm('"._("Do you confirm the deletion ?")."')) {" .
        " 	setO(this.form.elements['o1'].value); submit();} " .
        "else if (this.form.elements['o1'].selectedIndex == 3) {" .
        " 	setO(this.form.elements['o1'].value); submit();} " .
        ""
);
$form->addElement('select', 'o1', null, array(null=>_("More actions..."), "d"=>_("Delete")), $attrs1);
$form->setDefaults(array('o1' => null));

$attrs2 = array(
    'onchange' => "javascript: " .
        " var bChecked = isChecked(); ".
        " if (this.form.elements['o2'].selectedIndex != 0 && !bChecked) {".
        " alert('"._("Please select one or more items")."'); return false;} " .
        "if (this.form.elements['o2'].selectedIndex == 1 && confirm('"._("Do you confirm the duplication ?")."')) {" .
        " 	setO(this.form.elements['o2'].value); submit();} " .
        "else if (this.form.elements['o2'].selectedIndex == 2 && confirm('"._("Do you confirm the deletion ?")."')) {" .
        " 	setO(this.form.elements['o2'].value); submit();} " .
        "else if (this.form.elements['o2'].selectedIndex == 3) {" .
        " 	setO(this.form.elements['o2'].value); submit();} " .
        ""
);
$form->addElement('select', 'o2', null, array(null=>_("More actions..."), "d"=>_("Delete")), $attrs2);
$form->setDefaults(array('o2' => null));

$o1 = $form->getElement('o1');
$o1->setValue(null);
$o1->setSelected(null);

$o2 = $form->getElement('o2');
$o2->setValue(null);
$o2->setSelected(null);

$tpl->assign('limit', $limit);
$tpl->assign('searchHD', $search);

/*
 * Apply a template definition
 */

$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
$form->accept($renderer);
$tpl->assign('form', $renderer->toArray());

$tpl->display("configuration_dependencies_list.ihtml");

?>

<script type="text/javascript">
function setO(_i) {
    document.forms['form'].elements['o'].value = _i;
}
</script>
