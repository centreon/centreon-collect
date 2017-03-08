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
	require_once ("./modules/centreon-bam-server/core/common/functions.php");
	
	#Pear library
	require_once "HTML/QuickForm.php";
	require_once 'HTML/QuickForm/advmultiselect.php';
	require_once 'HTML/QuickForm/Renderer/ArraySmarty.php';
	
	$path = "./modules/centreon-bam-server/core/logs/template";
		
	isset($_GET["item"]) ? $bam_id = $_GET["item"] : $bam_id = 0;
	isset($_POST["item"]) ? $bam_id = $_POST["item"] : $bam_id;
	
	$period = 0;
	isset($_POST["periodz"]) ? $period = $_POST["periodz"] : 0; 
	isset($_GET["periodz"]) ? $period = $_GET["periodz"] : $period;
		
	$pearDB = new CentreonBam_Db();	
	$pearDBO = new CentreonBam_Db("centstorage");
	$ba_meth = new CentreonBam_Ba($pearDB, null, $pearDBO);
	$ba_acl_meth = new CentreonBam_Acl($pearDB, $oreon->user->user_id);
	$logs_meth = new CentreonBam_Log($pearDB, $pearDBO, $bam_id);
			
	
	/*
	 * QuickForm templates
	 */
	$attrsText 		= array("size"=>"30");
	$attrsText2 	= array("size"=>"60");
	$attrsAdvSelect = array("style" => "width: 200px; height: 100px;");
	$attrsTextarea 	= array("rows"=>"5", "cols"=>"40");	

	/* Smarty template initialization */
	$tpl = new Smarty();
	$tpl = initSmartyTpl($path, $tpl, "");
	$tpl->assign('o', $o);
	/*
	 *  Assign centreon path
	 */	
	$tpl->assign("centreon_path", $centreon_path);
	
	$tpl->assign('periodTitle', _("Period Selection"));	

	 /*
	  * CSS Definition for status colors
	  */

	/*
	 * Init Timeperiod List
	 */
	
	# Getting period table list to make the form period selection (today, this week etc.)
	$periodList = $logs_meth->getPeriodList();
	

	/* 
	 * setting variables for link with services
	 */
	if ($period == "") {
		$period = "today";
	}
	$tpl->assign("get_period", $period);

	/*
	 * Form
	 */
	$form = new HTML_QuickForm('formItem', 'post', "?p=".$p);

	/* service Selection */	
	$items = array(NULL => NULL);
	if (!$oreon->user->admin) {
		$items2  = $ba_acl_meth->getBa();
		foreach ($items2 as $key => $value) {
			$items[$key] = $ba_meth->getBA_Name($key);
		}				
	}
	else {
		$items2 = $ba_meth->getBA_list();
		foreach ($items2 as $key => $value)
			$items[$key] = $value;	
	}	
	asort($items);
	$form->addElement('hidden', 'p', $p);
	$redirect =& $form->addElement('hidden', 'o');
	$redirect->setValue($o);
	$select =& $form->addElement('select', 'item', _("Business Activity"), $items, array("onChange" =>"this.form.submit();"));
	$form->addElement('select', 'periodz', _("Period"), $periodList, array("id"=>"periodlist", "onChange"=>"updateGraph(this.value);"));
	$form->addElement('checkbox', 'details', "&nbsp;&nbsp;", _("  Display details"), array("id"=>"checkDetails", "onClick" => "showHideDetails(this.checked);"));	
	
	if ($bam_id != "NULL") {	
		$form->setDefaults(array('item' => $bam_id));
		$form->setDefaults(array('periodz' => $period));		
	}	
	
	/* page id */
	$tpl->assign('p', $p);
	
	/*
	 * END OF FORMS
	 */
			
	/* Exporting variables for ihtml */			
	$tpl->assign('ba_id', $bam_id);
	$tpl->assign('logs_label', _("Logs"));					
	$form->setDefaults(array('period' => $period));
	$tpl->assign('id', $bam_id);
	
	require_once ("./modules/centreon-bam-server/core/logs/javascript/logs_js.php");
	/*
	 * Rendering forms
	 */	
	$tpl->assign('please_select', _("Please select a Business Activity"));
	$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
	$form->accept($renderer);
	$tpl->assign('formItem', $renderer->toArray());
	$tpl->display("logs.ihtml");	
?>
