<?php
/**
 * CENTREON
 *
 * Source Copyright 2005-2015 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */

ini_set("display_errors", "On");

#Pear library
require_once "HTML/QuickForm.php";
require_once 'HTML/QuickForm/advmultiselect.php';
require_once 'HTML/QuickForm/Renderer/ArraySmarty.php';

require_once realpath(dirname(__FILE__) . '/../centreon-export.conf.php');

use \CentreonExport\Factory;
use \CentreonExport\DBManager;
use \CentreonExport\PluginPack;
use \CentreonExport\Services\ListPluginPack;

require_once realpath(dirname(__FILE__). '/DB-Func.php');

include("./include/common/autoNumLimit.php");

$exportFactory = new Factory();

/* Delete pluginpacks */
if (isset($_POST['o']) && $_POST['o'] == 'd') {
    $pluginPack = $exportFactory->newPluginPack();
    $listIds = array();
    foreach ($_POST['select'] as $id => $value) {
        if ($value == 1 && is_int($id)) {
            $listIds[] = $id;
        }
    }
    $pluginPack->deleteByIds($listIds);
}

if (isset($_POST)) {
    $search  = checkFilter($_POST["searchP"]);
    $version = checkFilter($_POST["version"]);
    $modified  = checkFilter($_POST["modified"]);
    $dateLimit  = checkFilter($_POST["dateLimit"], 'date');
    $operator  = checkFilter($_POST["operator"]);
} else {
    $search = null;
    $version = null;
    $modified = null;
    $dateLimit = null;
    $operator = null;
}

$oPlugin = new ListPluginPack();

$form = new \HTML_QuickForm('PluginPack', 'post', "?p=".$p);

$form->addElement('select', 'o1', NULL, array(NULL=>_("More actions..."), "d"=>_("Delete")), array(
    'onchange' => 'javascript: ' .
        ' var bChecked = isChecked(); ' .
        ' if (this.form.elements["o1"].selectedIndex != 0 && !bChecked) {' .
        ' alert("' . _('Please select one or more items') . '"); return false;} ' .
        ' if (this.form.elements["o1"].selectedIndex == 1 && confirm("' . _('Do you confirm the deletion ?') . '")) {' .
        ' setO(this.form.elements["o1"].value); submit(); }'
));

$form->addElement('select', 'o2', NULL, array(NULL=>_("More actions..."), "d"=>_("Delete")), array(
    'onchange' => 'javascript: ' .
        ' var bChecked = isChecked(); ' .
        ' if (this.form.elements["o2"].selectedIndex != 0 && !bChecked) {' .
        ' alert("' . _('Please select one or more items') . '"); return false;} ' .
        ' if (this.form.elements["o2"].selectedIndex == 1 && confirm("' . _('Do you confirm the deletion ?') . '")) {' .
        ' setO(this.form.elements["o2"].value); submit(); }'
));

$o1 = $form->getElement('o1');
$o1->setValue(NULL);
$o1->setSelected(NULL);
$o2 = $form->getElement('o2');
$o2->setValue(NULL);
$o2->setSelected(NULL);


$aParams['num'] = $num;
$aParams['limit'] = $limit;
$aParams['search'] = $search;
$aParams['version'] = $version;
$aParams['modified'] = $modified;
$aParams['dateLimit'] = $dateLimit;
$aParams['operator'] = $operator;

$aPlugin = $oPlugin->getList($aParams, 0);
$nbCount = $oPlugin->getList($aParams, 1);
$rows = $nbCount[0];

include_once "./include/common/checkPagination.php";
/*
 * Smarty template Init
 */
$sDirExport = realpath(dirname(__FILE__)) . '/templates/';

$aModified = array(
    '-1' => _('All'),
    '0'  => _('No'),
    '1'  => _('Yes')
    );

$aOperators = array(
    ''   => '',
    '>'  => '>',
    '>=' => '>=',
    '<'  => '<',
    '<=' => '<=',
    '='  => '='
);

$tpl = new \Smarty();
$tpl = initSmartyTpl($path, $tpl);

$tpl->assign('limit', $limit);
$tpl->assign("myArray", $aPlugin);
$tpl->assign('dateLimit', _("Date limit"));
$tpl->assign('msg', array("addL" => "?p=61101&o=a", "addT"=>_("Add")));

$form->addElement('text', "searchP", _('Plugin pack'));
$form->addElement('text', "version", _('Version'), array("size" => 5));
$form->addElement('select', 'modified', _("Modified"), $aModified);
$form->addElement('text', 'dateLimit',  _("Date limit"), array("id" => "dateLimit", "class" => "datepicker", "size" => 8));
$form->addElement('select', 'operator', _("Operator"), $aOperators);

$form->addElement('button', 'exportAll', _("Export all"), array('id' => 'exportAll', 'class' => 'btc bt_info'));

$renderer = new \HTML_QuickForm_Renderer_ArraySmarty($tpl);
$form->accept($renderer);
$tpl->assign('form', $renderer->toArray());

$tpl->display($sDirExport."/listPluginPack.tpl");
