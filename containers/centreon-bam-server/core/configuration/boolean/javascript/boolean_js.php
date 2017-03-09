<?php
/*
 * MERETHIS
 *
 * Source Copyright 2005-2009 MERETHIS
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@merethis.com
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

        /**
         * Function is called from Boolean KPI form
         * 
         * @return void
         */
        function insertIntoExp() {
            var host = document.getElementById('resource_host').value;
            var svc = document.getElementById('resource_service').value;
            var op = document.getElementById('exp_operator').value;
            var state = document.getElementById('resource_states').value;
            var boolExp = document.getElementById('boolExp');
            var val;
            
            val  = '';
            val += '{'+host+' '+svc+'} ';
            val += '{'+op+'} ';
            val += '{'+state+'}';
            appendToTxtArea('boolExp', val);
            //boolExp.value += val;
        }
        
        /**
         * Function is called from Boolean KPI form
         * 
         * @param string str
         * @return void
         */
        function insertAndOrIntoExp(str) {
            appendToTxtArea('boolExp', "\n{"+str+"}\n");
        }
 
        /**
         * Function is called when user change host select box
         * 
         * @param string host_name
         * @return void
         */
        function loadServiceListBool(host_name) {
            if (host_name == '') {
                document.getElementById('ajax_service_list').innerHTML= '';
            } else {
                _lock = 1;
                var proc = new Transformation();
                var addrXML = "./modules/centreon-bam-server/core/configuration/kpi/xml/host_service_bool_xml.php?sid=" + _sid + "&host_name=" + encodeURIComponent(host_name);
                var addrXSL = "./modules/centreon-bam-server/core/configuration/kpi/xsl/host_service_bool.xsl";
                proc.setXml(addrXML);
                proc.setXslt(addrXSL);
                proc.transform("ajax_service_list");
                _lock = 0;
            }
	}
        
        /**
         * Function is called when user clicks on the test button
         * 
         * @param int mode | 0 = real time status, 1 simulation
         * @param string extraParam
         * @return void
         */
        function evalExp(mode, extraParam) {
            if (mode == 0) {
                document.getElementById('simDiv').innerHTML = '';
            }
            document.getElementById('expr_result').innerHTML = '<img src="modules/centreon-bam-server/core/common/images/ajax-loader.gif">';
            var expr = document.getElementById('boolExp').value;
            var proc = new Transformation();
            var addrXML = "./modules/centreon-bam-server/core/configuration/kpi/xml/eval_xml.php?sid=" + _sid + "&expr=" + encodeURIComponent(expr);
            addrXML += extraParam;
            var addrXSL = "./modules/centreon-bam-server/core/configuration/kpi/xsl/eval.xsl";
            proc.setXml(addrXML);
            proc.setXslt(addrXSL);
            proc.transform("expr_result");
        }
        
        /**
         * Append to text area
         * 
         * @param string id
         * @param string newTxt
         * @return void
         */
        function appendToTxtArea(id, newTxt) {                   
            var txtArea = document.getElementById(id);
                
            if (document.selection) { // ie
                txtArea.focus();
                var sel = document.selection.createRange();
                sel.text = newTxt;
                return;
            } else if (txtArea.selectionStart || txtArea.selectionStart == '0') { // ff, chrome
                var startPos = txtArea.selectionStart;
                var endPos = txtArea.selectionEnd;
                var scrollTop = txtArea.scrollTop;
                txtArea.value = txtArea.value.substring(0, startPos) + newTxt + txtArea.value.substring(endPos, txtArea.value.length);
                txtArea.focus();
                txtArea.selectionStart = startPos + newTxt.length;
                txtArea.selectionEnd = startPos + newTxt.length;
            } else { // others
                txtArea.value += textArea.value;
                txtArea.focus();
            }
        }
        
        /**
         * Wrap text with brackets
         * 
         * @param string id
         * @return void
         */
        function wrapText(id) {
            var textArea = document.getElementById(id);
            var len = textArea.value.length;
            var start = textArea.selectionStart;
            var end = textArea.selectionEnd;
            var selectedText = textArea.value.substring(start, end);
            var replacement = '( ' + selectedText + ' )';
            textArea.value = textArea.value.substring(0, start) + replacement + textArea.value.substring(end, len);
        }
        
        /**
         * Display simulation table
         * 
         * @param dom input btn
         * @return void
         */
        function displaySimTable(btn) {
            btn.style.display = 'none';
            var simDiv = document.getElementById('simDiv');
            simDiv.innerHTML = document.getElementById('hiddenSimDiv').innerHTML;
            simDiv.style.display = 'block';
        }
        
        /**
         * Simulate status
         * Function called whenever user changes selectbox value
         * 
         * @return void
         */
        function simulateStatus() {
            var extraParam = '';
            
            $$('#simDiv select.simstat').each(function(el) {
                extraParam += '&sim['+encodeURIComponent(el.getAttribute('name'))+']='+el.value;
            });
            evalExp(1, extraParam);
        }
        
        /**
         * Hide simulation div
         * 
         * @return void
         */
        function hideSimul() {
            document.getElementById('simDiv').style.display = 'none';
        }
</script>
