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
	
$oreon->optGen["AjaxFirstTimeReloadStatistic"] == 0 ? $tFS = 10 : $tFS = $oreon->optGen["AjaxFirstTimeReloadStatistic"] * 1000;
$oreon->optGen["AjaxFirstTimeReloadMonitoring"] == 0 ? $tFM = 10 : $tFM = $oreon->optGen["AjaxFirstTimeReloadMonitoring"] * 1000;
?>

<script type="text/javascript" src="./modules/centreon-bam-server/core/common/javascript/xslt.js"></script>
<script type="text/javascript">
	
	var _sid = '<?php echo session_id(); ?>';	
	var _time_reload = <?php echo $tM; ?>;
	var _time_live = <?php echo $tFM; ?>;	
	var _lock = 0;		
	var errorMessage1 = '<?php echo _("Select at least one Business Activity\\n"); ?>';
	var errorMessage2 = '<?php echo _("Select at least one KPI"); ?>';
	
	function swap_object(obj_type)
	{
		initialize_select_list();
		if (obj_type == 0) {			
			Effect.Appear('host_list_id', { duration : 0 });
		}
		else if (obj_type == 1) {			
			Effect.Appear('hg_list_id', { duration : 0 });
		}
		else if (obj_type == 2) {
			Effect.Appear('sg_list_id', { duration : 0 });			
		}
	}
	
	function initialize_select_list()
	{		
		document.getElementById('host_list').selectedIndex = 0;
		document.getElementById('hg_list').selectedIndex = 0;
		document.getElementById('sg_list').selectedIndex = 0;
		Effect.Fade('host_list_id', {duration : 0});
		Effect.Fade('hg_list_id', {duration : 0});
		Effect.Fade('sg_list_id', {duration : 0});
	}
	
	function loadKPIList()
	{
		var selected;
		var configType;
		var addrXSL;
		var proc = new Transformation();
		var obj_type = document.getElementById("obj_type").value;
		var advancedConfig = document.getElementById('config_mode_adv').checked;
		if (obj_type == 0) {
			selected = document.getElementById("host_list").value;
		}
		else if (obj_type == 1) {
			selected = document.getElementById("hg_list").value;
		}
		else if (obj_type == 2) {
			selected = document.getElementById("sg_list").value;
		}
		var params = "?sid=" + _sid + "&obj_type=" + obj_type + "&selected=" + selected;
		var addrXML = "./modules/centreon-bam-server/core/configuration/kpi/xml/dynamicKPI_xml.php" + params;
		if (advancedConfig) {
			addrXSL = "./modules/centreon-bam-server/core/configuration/kpi/xsl/dynamicKPI.xsl";
		}
		else {
			addrXSL = "./modules/centreon-bam-server/core/configuration/kpi/xsl/dynamicKPI_basic.xsl";
		}
		document.getElementById("dynamicKPI").innerHTML = "";		
		proc.setXml(addrXML);
		proc.setXslt(addrXSL);
		proc.transform("dynamicKPI");
		//_lock = 0;
	}
			
	function checkSelectedItem(theElement)
	{
    	var theForm = theElement;
    	var z = 0;
	 	for (z = 0; z < theForm.length;z++){
      		if (theForm[z].type == 'checkbox'){
		  		if (theForm[z].checked) {
					return true;
		   		}			
			}
		}
		return false;
	}
	
	function checkIfBaSelected() {
		var msg;
		var testSelected;
		
		msg = "";
		testSelected = checkSelectedItem(document.forms['form']);
		if(document.getElementById("ba_list_id").value && testSelected) {
			document.forms['form'].submit();
		}		
		else {
			if (!document.getElementById("ba_list_id").value) {
				msg += errorMessage1;
			}
			if (!testSelected) {
				msg += errorMessage2;
			}
			if (msg != "") {
				alert(msg);
			}
		}				
	}
</script>