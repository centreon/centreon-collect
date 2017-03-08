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

<script type="text/javascript">

function disableFields(value) {
    if (value != 3) {
        document.getElementById('boolean_impact_regular').disabled = true;
        document.getElementById('warning_impact_regular').disabled = false;
        document.getElementById('critical_impact_regular').disabled = false;
        document.getElementById('unknown_impact_regular').disabled = false;

        if (document.getElementById('config_mode_adv').checked) {
            Effect.Fade('bool_adv', { duration : 0 });
            Effect.Appear('warn_adv', { duration : 0 });
            Effect.Appear('crit_adv', { duration : 0 });
            Effect.Appear('unkn_adv', { duration : 0 });
        } else {
            Effect.Fade('bool_reg', { duration : 0 });
            Effect.Appear('warn_reg', { duration : 0 });
            Effect.Appear('crit_reg', { duration : 0 });
            Effect.Appear('unkn_reg', { duration : 0 });
        }
    } else {
        document.getElementById('boolean_impact_regular').disabled = false;
        document.getElementById('warning_impact_regular').disabled = true;
        document.getElementById('critical_impact_regular').disabled = true;
        document.getElementById('unknown_impact_regular').disabled = true;

        if (document.getElementById('config_mode_adv').checked) {
            Effect.Appear('bool_adv', { duration : 0 });
            Effect.Fade('warn_adv', { duration : 0 });
            Effect.Fade('crit_adv', { duration : 0 });
            Effect.Fade('unkn_adv', { duration : 0 });
        } else {
            Effect.Appear('bool_reg', { duration : 0 });
            Effect.Fade('warn_reg', { duration : 0 });
            Effect.Fade('crit_reg', { duration : 0 });
            Effect.Fade('unkn_reg', { duration : 0 });
        }
    }
} 

var _sid = '<?php echo session_id(); ?>';
var _time_reload = <?php echo $tM; ?>;
var _time_live = <?php echo $tFM; ?>;
var _lock = 0;

function loadIndicatorList(kpi_type, selected) {
    // Get KPI list by Ajax
    var request = jQuery.ajax({
        'url': './include/common/webServices/rest/internal.php?object=centreon_bam_kpi&action=elementListByType&type=' + kpi_type,
        'dataType': 'json',
    });

    request.done(function(data) {
        // Render the Kpi list
        displayIndicatorList(kpi_type, data, selected);
    });
    
    // 
    request.fail(function() {
        console.log('failed request');
    });
}

function displayIndicatorList(kpi_type, indicators, selected) {
    // Initialize Kpi List Container
    var $kpiListContainer = jQuery("#ajax_kpi_list");

    var html = '<select id="kpi_select" name="kpi_select" ';
    if (kpi_type == 0) {
        html += 'onChange="loadServiceList(this.value)"';
    } else {
        var $serviceListContainer = jQuery("#ajax_service_list");
        $serviceListContainer.html("");
    }
    html += '>';
    html += '<option value="0"></option>';

    jQuery.each(indicators, function(id, value) {
        html += '<option value="' + id + '"';
        if (id == selected) {
            html += ' selected';
        }
        html += '>';
        html += value;
        html += '</option>';
    });

    html += '</select>';

    $kpiListContainer.html(html);
}

function loadServiceList(host_id, selected) {
    _lock = 1;

    if (host_id == 0) {
        var $serviceListContainer = jQuery("#ajax_service_list");
        $serviceListContainer.html("");
        return;
    }

    // Get service list by Ajax
    var request = jQuery.ajax({
        'url': './include/common/webServices/rest/internal.php?object=centreon_configuration_host&action=services&id=' + host_id + '&all',
        'dataType': 'json',
    });

    request.done(function(data) {
        // Render the Kpi list
        displayServiceList(data, selected);
    });

    //
    request.fail(function() {
        console.log('failed request');
    });

    _lock = 0;
}

function displayServiceList(services, selected) {
    // Initialize service List Container
    var $serviceListContainer = jQuery("#ajax_service_list");

    var html = '<select id="svc_select" name="svc_select">';
    html += '<option value="0"></option>';

    jQuery.each(services, function(id, value) {
        html += '<option value="' + id + '"';
        if (id == selected) {
            html += ' selected';
        }
        html += '>';
        html += value;
        html += '</option>';
    });

    html += '</select>';

    $serviceListContainer.html(html);
}

/**
 * Function is called from KPI form, when user changes config mode
 * 
 * @return void
 */
function impactTypeProcess() {
    // Get Value of KPI Type
    select = document.getElementById('kpi_type');
    choice = document.getElementById('kpi_type').selectedIndex;

    if (select.options[choice].value == '') {
        Effect.Fade('warn_reg', { duration : 0 });
        Effect.Fade('crit_reg', { duration : 0 });
        Effect.Fade('unkn_reg', { duration : 0 });
        Effect.Fade('warn_adv', { duration : 0 });
        Effect.Fade('crit_adv', { duration : 0 });
        Effect.Fade('unkn_adv', { duration : 0 });
        Effect.Fade('bool_adv', { duration : 0 });
        Effect.Fade('bool_reg', { duration : 0 });
    } else {
        if (select.options[choice].value == 3) {
            if (document.getElementById('config_mode_adv').checked) {

                Effect.Fade('warn_reg', { duration : 0 });
                Effect.Fade('crit_reg', { duration : 0 });
                Effect.Fade('unkn_reg', { duration : 0 });

                Effect.Fade('warn_adv', { duration : 0 });
                Effect.Fade('crit_adv', { duration : 0 });
                Effect.Fade('unkn_adv', { duration : 0 });

                Effect.Fade('bool_reg', { duration : 0 });
                Effect.Appear('bool_adv', { duration : 0 });
            } else {

                Effect.Fade('warn_reg', { duration : 0 });
                Effect.Fade('crit_reg', { duration : 0 });
                Effect.Fade('unkn_reg', { duration : 0 });

                Effect.Fade('warn_adv', { duration : 0 });
                Effect.Fade('crit_adv', { duration : 0 });
                Effect.Fade('unkn_adv', { duration : 0 });

                Effect.Fade('bool_adv', { duration : 0 });
                Effect.Appear('bool_reg', { duration : 0 });
            }
        } else {
            if (document.getElementById('config_mode_adv').checked) {

                Effect.Fade('bool_adv', { duration : 0 });
                Effect.Fade('bool_reg', { duration : 0 });

                Effect.Fade('warn_reg', { duration : 0 });
                Effect.Fade('crit_reg', { duration : 0 });
                Effect.Fade('unkn_reg', { duration : 0 });
                Effect.Appear('warn_adv', { duration : 0 });
                Effect.Appear('crit_adv', { duration : 0 });
                Effect.Appear('unkn_adv', { duration : 0 });
            } else {

                Effect.Appear('warn_reg', { duration : 0 });
                Effect.Appear('crit_reg', { duration : 0 });
                Effect.Appear('unkn_reg', { duration : 0 });
                Effect.Fade('warn_adv', { duration : 0 });
                Effect.Fade('crit_adv', { duration : 0 });
                Effect.Fade('unkn_adv', { duration : 0 });

                Effect.Fade('bool_adv', { duration : 0 });
                Effect.Fade('bool_reg', { duration : 0 });
            }
        }
    }
}
</script>
