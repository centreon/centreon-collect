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

<script language="javascript">AC_FL_RunContent = 0;</script>
<script language="javascript"> DetectFlashVer = 0; </script>
<script src="./modules/centreon-bam-server/core/lib/xmlswf/AC_RunActiveContent.js" language="javascript"></script>
<script language="JavaScript" type="text/javascript">
var requiredMajorVersion = 9;
var requiredMinorVersion = 0;
var requiredRevision = 45;
</script>

<script type="text/javascript" src="./modules/centreon-bam-server/core/common/javascript/xslt.js"></script>
<script type="text/javascript">			
		var _sid = '<?php echo session_id(); ?>';	
		var _time_reload = <?php echo $tM; ?>;
		var _time_live = <?php echo $tFM; ?>;	
		
		var global_ba_id = 0;
		var glb_timeperiod = '<?php echo $period; ?>';
		var scroll_start;
		var scroll_span;
				
		function draw_graph(xmlFile, chart_id, ba_id) {				
			if (AC_FL_RunContent == 0 || DetectFlashVer == 0) {
				alert("This page requires AC_RunActiveContent.js.");
			} 
			else {
				var hasRightVersion = DetectFlashVer(requiredMajorVersion, requiredMinorVersion, requiredRevision);
				if(hasRightVersion) { 
					AC_FL_RunContent(
						'codebase', 'http://download.macromedia.com/pub/shockwave/cabs/flash/swflash.cab#version=9,0,45,0',
						'width', '1000',
						'height', '620',
						'scale', 'noscale',
						'salign', 'TL',
						'bgcolor', '#FFFFFF',
						'wmode', 'opaque',
						'movie', './modules/centreon-bam-server/core/lib/xmlswf/charts',
						'src', './modules/centreon-bam-server/core/lib/xmlswf/charts',
						'FlashVars', 'library_path=./modules/centreon-bam-server/core/lib/xmlswf/charts_library&xml_source=./modules/centreon-bam-server/core/logs/graphs/' + xmlFile+'?var='+ ba_id + ';'+ glb_timeperiod, 
						'id', chart_id,
						'name', chart_id,
						'menu', 'true',
						'allowFullScreen', 'true',
						'allowScriptAccess','sameDomain',
						'quality', 'high',
						'align', 'middle',
						'pluginspage', 'http://www.macromedia.com/go/getflashplayer',
						'play', 'true',
						'devicefont', 'false'
					); 
					global_ba_id = ba_id;					
				} 
				else { 
					var alternateContent = 'This content requires the Adobe Flash Player. '
					+ '<u><a href=http://www.macromedia.com/go/getflash/>Get Flash</a></u>.';
					document.write(alternateContent); 
				}			
			}
		}
		
		function updateGraph(timeperiod) {
			if (global_ba_id) {
				glb_timeperiod = timeperiod;
				mode = "normal";
				timeout = 10; 
				retry = 2;
				spinning_wheel = false;
				url = "./modules/centreon-bam-server/core/logs/graphs/graph.php";
				url += "?ba_id=" + global_ba_id + "&tp="+timeperiod;
				document.logs.Update_URL( url, spinning_wheel, timeout, retry, mode );
			}
		}
		
		function showHideDetails(value) {			
			if (global_ba_id) {
				mode = "normal";
				timeout = 10; 
				retry = 2;
				spinning_wheel = false;
				url = "./modules/centreon-bam-server/core/logs/graphs/graph.php";
				url += "?ba_id=" + global_ba_id + "&start=" + scroll_start + "&span=" + scroll_span;
				if (glb_timeperiod) {
					url += "&tp=" + glb_timeperiod;
				}
				url += "&details=" + value;
				document.logs.Update_URL( url, spinning_wheel, timeout, retry, mode );
			}
		}
		
		function displayKpiLogs(datetime) {
			_lock = 1;
			var proc = new Transformation();
			var periodList = document.getElementById('periodlist').value;
			var addrXML = "./modules/centreon-bam-server/core/logs/xml/load_kpi_xml.php?sid=" + _sid + "&datetime="+ datetime + "&ba_id=" + global_ba_id + "&periodz=" + periodList;
			var addrXSL = "./modules/centreon-bam-server/core/logs/xsl/load_kpi_xsl.xsl";
			proc.setXml(addrXML);
			proc.setXslt(addrXSL);		
			proc.transform("kpi_logs");
			_lock = 0;			
		}
		
		function Scrolled_Chart(id, start, span){
 			scroll_start = start;
 			scroll_span = span; 			
		}
		
		
</script>
