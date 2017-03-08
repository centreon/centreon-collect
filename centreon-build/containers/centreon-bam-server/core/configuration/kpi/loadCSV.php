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

	if (!isset($oreon))
		exit();

	global $pearDB;

    $versionDesign = version_compare(getCentreonVersion($pearDB), '2.6.999', '<');

	/*
	 * Get Poller List
	 */
	$tab_nagios_server = array(NULL => NULL);
	$DBRESULT = $pearDB->query("SELECT * FROM `nagios_server` ORDER BY `localhost` DESC");
	if (PEAR::isError($DBRESULT))
		print "DB Error : ".$DBRESULT->getDebugInfo()."<br />";
	while ($nagios = $DBRESULT->fetchRow())
		$tab_nagios_server[$nagios['id']] = $nagios['name'];

	/*
	 * Form begin
	 */
	$attrSelect 	= array("style" => "width: 100px;");
	$attrsTextarea 	= array("rows"=>"20", "cols"=>"200");
    $attrBtnSuccess = array();
    $attrBtnDefault = array();
    if (!$versionDesign) {
        $attrBtnSuccess = array("class" => "btc bt_success");
        $attrBtnDefault = array("class" => "btc bt_default");
    }

	$form = new HTML_QuickForm('Form', 'post', "?p=".$p);

	$form->addElement('header', 'title', _("KPI CSV Loader"));

	$form->addElement('header', 'fileType', _("File Type"));

	$file = $form->addElement('file', 'filename', _(".csv File"));

	$kpi_type[] = &HTML_QuickForm::createElement('radio', 'kpi_type', null, _("Regular KPI"), '0');
	$kpi_type[] = &HTML_QuickForm::createElement('radio', 'kpi_type', null, _("Meta Service"), '1');
	$kpi_type[] = &HTML_QuickForm::createElement('radio', 'kpi_type', null, _("Business Activity"), '2');
	$kpitype = $form->addGroup($kpi_type, 'kpitype', _("KPI Type"), '&nbsp;');
	$form->setDefaults(array('kpitype' => '0'));

	$form->addElement('textarea', 'manualDef', _("Manual Filling"), $attrsTextarea);

	$redirect = $form->addElement('hidden', 'o');
	$redirect->setValue($o);

	$form->applyFilter('__ALL__', 'myTrim');

	$nagiosCFGPath = $centreon_path . "/filesUpload/nagiosCFG/";

	/*
	 * Smarty template Init
	 */
	$path = "./modules/centreon-bam-server/core/configuration/kpi/";
	$tpl = new Smarty();
	$tpl = initSmartyTpl($path . "template", $tpl);

	$sub = $form->addElement('submit', 'submit', _("Load"), $attrBtnSuccess);
	$msg = NULL;
	if ($form->validate()) {
		$ret = $form->getSubmitValues();

		$fDataz = array();
		$buf = NULL;

		$fDataz = $file->getValue();
		switch ($fDataz["type"]) {
			case "application/csv-tab-delimited-table" :
				$file->moveUploadedFile($nagiosCFGPath);
				break;
			case "text/plain" :
				$file->moveUploadedFile($nagiosCFGPath);
				break;
			case "text/csv" :
				$file->moveUploadedFile($nagiosCFGPath);
                break;
            case "application/force-download" :
                $file->moveUploadedFile($nagiosCFGPath);
                break;
			default :
				$file->moveUploadedFile($nagiosCFGPath);
				break;
		}

		/*
		 * Buffering Data
		 */
		if (is_file($nagiosCFGPath.$fDataz["name"]))	{
			$buf = file($nagiosCFGPath.$fDataz["name"]);
		} else if ($ret["manualDef"])	{
			$buf = $ret["manualDef"];
		}

		/*
		 * Enum Object Types
		 */
		if ($buf)	{
		  	$ktype = $kpitype->getValue();
		  	$msg = $kpi->csvLoad($buf, $ktype['kpi_type']);
			if (is_file($nagiosCFGPath.$fDataz["name"]))
				unlink($nagiosCFGPath.$fDataz["name"]);
		}
	}

	$form->addElement('header', 'status', _("Status"));
	if (isset($msg) && $msg)
		$tpl->assign('msg', $msg);

	/*
	 * Apply a template definition
	 */
	$renderer = new HTML_QuickForm_Renderer_ArraySmarty($tpl);
	$renderer->setRequiredTemplate('{$label}&nbsp;<font color="red" size="1">*</font>');
	$renderer->setErrorTemplate('<font color="red">{$error}</font><br />{$html}');
	$form->accept($renderer);
	$tpl->assign('form', $renderer->toArray());
	$tpl->assign('o', $o);
        
        $helptext = "";
        include_once("help.php");
        foreach ($help as $key => $text) {
            $helptext .= '<span style="display:none" id="help:'.$key.'">'.$text.'</span>'."\n";
        }
        $tpl->assign("helptext", $helptext);
        
	$tpl->assign('formatLabel', _("Format"));
	$tpl->assign('formatLabelRegular', sprintf("<b>%s</b> : %s", _("Regular KPI"), _("BA name ; Host name; Service name ; Warning impact ; Critical impact ; Unknown impact")));
	$tpl->assign('formatLabelMeta', sprintf("<b>%s</b> : %s", _("Meta Service"), _("BA name ; Meta Service name ; Warning impact ; Critical impact ; Unknown impact")));
	$tpl->assign('formatLabelBA', sprintf("<b>%s</b> : %s", _("Business Activity"), _("BA name ; KPI BA name ; Warning impact ; Critical impact ; Unknown impact")));

    $tpl->display("loadCSV.ihtml");
?>
<script type='text/javascript' src='./modules/centreon-bam-server/core/common/javascript/initHelpTooltips.js'></script>
