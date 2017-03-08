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

$form = new HTML_QuickForm('Form', 'get', "?p=".$p);

// BV filter
$bv = array(
    'datasourceOrigin' => 'ajax',
    'allowClear' => false,
    'availableDatasetRoute' => './include/common/webServices/rest/internal.php?object=centreon_bam_ba_group&action=list',
    'multiple' => false
);
$bv1 = array_merge(
    $bv,
    array('defaultDatasetRoute' => './include/common/webServices/rest/internal.php?object=centreon_bam_ba_group&action=defaultValues&id=')
);
$form->addElement('select2', 'bv_filter', _("Business View"), array('id' => 'bv_filter'), $bv1);

// BA filter
$ba = array(
    'datasourceOrigin' => 'ajax',
    'allowClear' => false,
    'availableDatasetRoute' => './include/common/webServices/rest/internal.php?object=centreon_bam_ba&action=list',
    'multiple' => false
);
$ba1 = array_merge(
    $ba,
    array('defaultDatasetRoute' => './include/common/webServices/rest/internal.php?object=centreon_bam_ba&action=defaultValues&id=')
);
$form->addElement('select2', 'ba_filter', _("Business Activity"), array('id' => 'ba_filter'), $ba1);

$smartyQuickFormRenderer = new HTML_QuickForm_Renderer_ArraySmarty($smartyTemplateObj);
$form->accept($smartyQuickFormRenderer);
$smartyTemplateObj->assign('form', $smartyQuickFormRenderer->toArray());

/* Check if BAM is in poller display */
$DBRES3 = $centreonDb->query("SELECT id FROM modules_informations WHERE name = 'centreon-poller-display'");
if ($DBRES3->numRows()) {
    $smartyTemplateObj->assign("poller_display", 1);
}

$query = "SELECT * FROM `mod_bam_ba_groups` WHERE visible = '1' ".$bamAclObj->queryBuilder("AND", "id_ba_group", $bamAclObj->getBaGroupStr())."ORDER BY ba_group_name";
$DBRES2 = $centreonDb->query($query);
while ($rowb = $DBRES2->fetchRow()) {
    $ba_groups[] = array('ba_group_name' => $rowb['ba_group_name'], 'id_ba_group' => $rowb['id_ba_group']);
}
$smartyTemplateObj->assign("ba_groups", $ba_groups);

$centreon->optGen["AjaxFirstTimeReloadStatistic"] == 0 ? $tFS = 10 : $tFS = $centreon->optGen["AjaxFirstTimeReloadStatistic"] * 1000;
$centreon->optGen["AjaxFirstTimeReloadMonitoring"] == 0 ? $tFM = 10 : $tFM = $centreon->optGen["AjaxFirstTimeReloadMonitoring"] * 1000;
?>
<script type="text/javascript" src="./modules/centreon-bam-server/core/common/javascript/xslt.js"></script>
<script type="text/javascript">
_debug = false;

<?php $time = time(); include_once "./include/monitoring/status/Common/commonJS.php"; ?>
function goM(_time_reload, _sid, _o) {
    _lock = 1;
    var proc = new Transformation();

    // INIT search informations
    var addrXML = "./modules/centreon-bam-server/core/dashboard/xml/ba_list_xml.php?sid=" + _sid + "&p=" + _p + "&bv_id=" + _current_bv+ "&num=" + _num +'&limit='+_limit;
    var addrXSL = "./modules/centreon-bam-server/core/dashboard/xsl/ba_list.xsl";
    proc.setCallback(monitoringCallBack);
    
    proc.setXml(addrXML);
    proc.setXslt(addrXSL);
    proc.transform("centreon_bam_table");

    _lock = 0;
    _timeoutID = setTimeout('goM("'+ _time_reload +'","'+ _sid +'","'+_o+'")', _time_reload);
    _time_live = _time_reload;
    _on = 1;

    set_header_title();
}
var _sid = '<?php echo session_id(); ?>';
var _time_reload = <?php echo $tM; ?>;
var _time_live = <?php echo $tFM; ?>;
var _lock = 0;
var xhr_bam;
var _current_bv = 0;
var _sort = "";
var _sort_type = "";
var _p = '<?php echo $p; ?>';        
var savedChecked = new Array();

function build_chart() {
    $$('.ListColPicker,.ListColHeaderPicker input').each(function(el) {
        savedChecked[el.id] = 0;
        if (el.checked == 1) {
            savedChecked[el.id] = 1;
        }
    });
    _lock = 1;
    clearTimeout(chart_timeout);
    var proc = new Transformation();
    var addrXML = "./modules/centreon-bam-server/core/dashboard/xml/ba_list_xml.php?sid=" + _sid + "&p=" + _p + "&bv_id=" + _current_bv+ "&num=" + _num +'&limit='+_limit;
    var addrXSL = "./modules/centreon-bam-server/core/dashboard/xsl/ba_list.xsl";
    proc.setCallback(monitoringCallBack);
    
    proc.setXml(addrXML);
    proc.setXslt(addrXSL);
    proc.transform("centreon_bam_table");
    
    _lock = 0;
    chart_timeout = setTimeout('build_chart()', _time_reload);
}

function refreshCallback() {
    $$('.ListColPicker,.ListColHeaderPicker input').each(function(el) {
        if (typeof(savedChecked[el.id]) != 'undefined') {
            if (savedChecked[el.id] == 1) {
                el.checked = 1;
            }
        }
    });
}

function mk_img(_src, _alt)	{
    var _img = document.createElement("img");
    _img.src = _src;
    _img.alt = _alt;
    _img.title = _alt;
    if (_img.complete){
        _img.alt = _alt;
    } else {
        _img.alt = "Image could not be loaded (" +_alt + ").";
    }
    return _img;
}

var tempX = 0;
var tempY = 0;

function position(e){
    tempX = (navigator.appName.substring(0,3) == "Net") ? e.pageX : event.x+document.body.scrollLeft;
    tempY = (navigator.appName.substring(0,3) == "Net") ? e.pageY : event.y+document.body.scrollTop;
}

if (navigator.appName.substring(0, 3) == "Net") {
    document.captureEvents(Event.MOUSEMOVE);
}
document.onmousemove = position;

function displayIMG(host_id, service_id) {

    var chartDiv = jQuery('#div_img');
    var l = screen.availWidth;
    var h = screen.availHeight;
    var posy = tempY + 10;
    if(h - tempY < 420){
        posy = tempY-230;
    }
    chartDiv.css('display', 'block');
    chartDiv.css('left',  (tempX / 2)+'px');
    chartDiv.css('top', posy +'px');


    var chartContainer = '<div class="chart" data-graph-id="' + host_id + '_' + service_id + '" data-graph-type="service"></div>';

    var times = {
        height: 200
    };
    times.interval = '5d';
    chartDiv.html(chartContainer);
    jQuery('#div_img > .chart').centreonGraph(times);
}

function hiddenIMG(){
    jQuery('#div_img')
        .css('display', 'none')
        .empty();
}

function executeAction(action) {
    var select_action1 = document.getElementById("actions1");
    var select_action2 = document.getElementById("actions2");
    var myForm = action.form;
    var z;
    var ba_list = "";

    $$('.ListColPicker').each(function(el) {
        if (el.checked == 1) {
            if (ba_list != "") {
                ba_list += ",";
            }
            ba_list += el.id;
        }
    });
    if (ba_list != "") {
        var req;
        if (action.value != 'schedule_downtime') {
            if (typeof XMLHttpRequest != "undefined") {
                req = new XMLHttpRequest();
            }
            else if (window.ActiveXObject) {
                req = new ActiveXObject("Microsoft.XMLHTTP");
            }
            var addrXML = "./modules/centreon-bam-server/core/dashboard/xml/action_xml.php?sid=" + _sid +  "&action=" + action.value + "&ba_list=" + ba_list;
            if (req) {
                req.open("GET", addrXML, true);
                req.send(null);
            }
            uncheckSelected();
        } else {
            Modalbox.show($('downtimebox'), {title: '<?php echo addslashes(_("Schedule downtime")) ?>', width: 600, afterHide: uncheckSelected });
        }
    }
    select_action1.selectedIndex = 0;
    select_action2.selectedIndex = 0;        
}

function uncheckSelected() {
    $$('.ListColPicker,.ListColHeaderPicker input').each(function(el) {
        el.checked = 0; 
    });
}

function selectView(bv_id) {
    _current_bv = bv_id;
    build_chart();
}

var _myTimer;
function showBADetails(div_id, ba_id) {
    var span = document.getElementById(div_id);
    var proc = new Transformation();
    var addrXML = "./modules/centreon-bam-server/core/dashboard/xml/ba_details_xml.php?sid=" + _sid + "&ba_id=" + ba_id + "&time=<?php print time(); ?>";
    var addrXSL = "./modules/centreon-bam-server/core/dashboard/xsl/ba_details.xsl";
    proc.setXml(addrXML);
    proc.setXslt(addrXSL);
    proc.transform(div_id);

    var l = screen.availWidth;
    var h = screen.availHeight;
    var posy = 20;
    if(h - tempY < 420){
        posy = -200;
    }

    span.style.display = "block";
    span.style.left = '0' + 'px';
    span.style.top = posy +'px';

    span.className = "popup_volante";
}

function hideBADetails(div_id) {
    var span = document.getElementById(div_id);
    span.innerHTML = '';
}

function toggleFields(cb) {
    if (cb.checked) {
        document.getElementById('duration').disabled = true;
    } else {
        document.getElementById('duration').disabled = false;
    }
}

function scheduleDowntime() {        
    var ba_list = "";
    $$('.ListColPicker').each(function(el) {
        if (el.checked == 1) {
            if (ba_list != "") {
                ba_list += ",";
            }
            ba_list += el.id;
        }
    });
    var start = document.getElementById('start').value;
    var end = document.getElementById('end').value;
    var duration = document.getElementById('duration').value;
    var fixed = document.getElementById('fixed').checked;
    var author = document.getElementById('author').value;
    var comments = document.getElementById('comments').value;
    new Ajax.Request('./modules/centreon-bam-server/core/dashboard/scheduledowntime.php', {
        method: 'post',
        parameters: 'start='+start+'&end='+end+'&fixed='+fixed+'&duration='+duration+'&author='+author+'&ba='+ba_list+'&sid='+_sid+'&comments='+comments,
        onSuccess: function(transport) {
            Modalbox.hide();
            buildChart();
        }
    });
}

var chart_timeout = setTimeout('build_chart()', 200);

</script>

<?php

$smartyTemplateObj->display("dashboard.html");
    
