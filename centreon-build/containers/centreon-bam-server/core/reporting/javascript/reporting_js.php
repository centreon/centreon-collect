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


$arg = "id=".$bam_id;
$arg .= "&color[UP]=#88b917";
$arg .= "&color[UNDETERMINED]=#818185";
$arg .=	"&color[DOWN]=#e00b3d";
$arg .= "&color[UNREACHABLE]=#818185";
$arg .= "&color[OK]=#88b917";
$arg .= "&color[WARNING]=#ff9a13";
$arg .= "&color[CRITICAL]=#e00b3d";
$arg .= "&color[UNKNOWN]=#bcbdc0";
$arg = str_replace("#", "%23", $arg);

$url = "./modules/centreon-bam-server/core/reporting/xml/timeline_xml.php?".$arg;

?>
<script type="text/javascript">
	var tl;

	function initTimeline() {
		var eventSource = new Timeline.DefaultEventSource();
		var bandInfos = [
		Timeline.createBandInfo({
				eventSource:    eventSource,
				width:          "70%",
				intervalUnit:   Timeline.DateTime.DAY,
				intervalPixels: 300
		    }),
			Timeline.createBandInfo({
		    	showEventText:  false,
		   		eventSource:    eventSource,
		    	width:          "30%",
		    	intervalUnit:   Timeline.DateTime.MONTH,
			    intervalPixels: 300
			})
		];

		bandInfos[1].syncWith = 0;
		bandInfos[1].highlight = true;
		bandInfos[1].eventPainter.setLayout(bandInfos[0].eventPainter.getLayout());

		tl = Timeline.create(document.getElementById("my-timeline"), bandInfos);
		Timeline.loadXML('<?php echo $url ?>', function(xml, url) { eventSource.loadXML(xml, url); });
	}
</script>
