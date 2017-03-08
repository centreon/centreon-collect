
/**
 * 
 * @param {string} kpiListContainerId
 * @param {Array} kpiList
 * @returns {undefined}
 */
function populateKpiListContainer(kpiListContainerId, data)
{
    // Initialize Kpi List Container
    var $kpiListContainer = jQuery("#" + kpiListContainerId);
    $kpiListContainer.empty();
    
    var header = '<table style="width:100%;">';
    header += '<tr>';
    header += '<td style="vertical-align: top;">';
    header += '<table class="ListTable">';
    header += '<tr class="ListHeader">';
    header += '<td colspan="6" align="left"> ' +
        data.kpi_header_label +
        '<b style="color:#40a52b;margin-left: 4px;">(' +
        data.reporting_period +
        ')</b></td>';
    header += '</tr>';
    header += '<tr class="list_lvl_1">';
    header += '<td style="white-space:nowrap;" colspan="2">' + data.indicator_label + '</td>';
    header += '<td align="center">' + data.indicator_type_label + '</td>';
    header += '<td align="center">' + data.status_label + '</td>';
    header += '<td align="center">' + data.output_label + '</td>';
    header += '<td align="center">' + data.impact_label + '</td>';
    header += '</tr>';
    
    ba_name = data.ba_name;
    svc_index = data.svc_index;
    
    allKpi = renderKpiList(data.kpi);
    
    var footer = '</table>';
    footer += '</td>';
    footer += '</tr>';
    footer += '</table>';
    
    full = header + allKpi + footer;
    $kpiListContainer.append(full);
    populateMostImpactedKpiListContainer('centreon_bam_table', data);
}

/**
 * 
 * @param {string} kpiListContainerId
 * @param {Array} kpiList
 * @returns {undefined}
 */
function populateMostImpactedKpiListContainer(kpiListContainerId, data)
{
    // Initialize Kpi List Container
    var $mostImpactedKpiListContainer = jQuery("#" + kpiListContainerId);
    $mostImpactedKpiListContainer.empty();
    
    var header = '<table style="width:100%;">';
    header += '<tr>';
    header += '<td style="vertical-align: top;">';
    header += '<table class="ListTable">';
    header += '<tr class="ListHeader">';
    header += '<td colspan="6" align="left"> ' +
        data.kpi_header_label +
        '<b style="color:#40a52b;margin-left: 4px;">(' +
        data.reporting_period +
        ')</b></td>';
    header += '</tr>';
    header += '<tr class="list_lvl_1">';
    header += '<td style="white-space:nowrap;" colspan="2">' + data.indicator_label + '</td>';
    header += '<td align="center">' + data.indicator_type_label + '</td>';
    header += '<td align="center">' + data.status_label + '</td>';
    header += '<td align="center">' + data.output_label + '</td>';
    header += '<td align="center">' + data.impact_label + '</td>';
    header += '</tr>';
    
    if (data.kpi_critical.length > 0) {
        allKpi = renderKpiList(data.kpi_critical);
    } else {
        allKpi = '<tr><td align="center" colspan="6"><b>';
        allKpi += data.ba_name + ' is working fine - no KPI impact has been detected';
        allKpi += '</b></td></tr>';
    }
    
    var footer = '</table>';
    footer += '</td>';
    
    footer += '<td style="width:10px;">';
    footer += '</td>';
    footer += '<td style="vertical-align: top;">';
    
    footer += '<table class="ListTable" style="text-align:left;" >';
    
    footer += '<tr class="ListHeader">';
    footer += '<td colspan="2" align="center">' + data.information_label + '</td>';
    footer += '</tr>';
    
    footer += '<tr>';
    footer += '<td class="FormRowField"><b>' + data.ba_health_label + '</b></td>';
    footer += '<td class="FormRowValue">';
    footer += '<span class="state_badge" style="background : ' + data.health_icon + '"></span>';
    if (data.pending !== 1) {
        footer += '(<b>' + data.health + '%</b>)';
    }
    footer += '</td>';
    footer += '</tr>';
    
    footer += '<tr>';
    footer += '<td class="FormRowField"><b>' + data.warning_threshold_label + '</b></td>';
    footer += '<td class="FormRowValue">';
    footer += '<span class="state_badge service_warning"></span>';
    footer += '(<b>' + data.warning_threshold + '%</b>)';
    footer += '</td>';
    footer += '</tr>';
    
    footer += '<tr>';
    footer += '<td class="FormRowField"><b>' + data.critical_threshold_label + '</b></td>';
    footer += '<td class="FormRowValue">';
    footer += '<span class="state_badge service_critical"></span>';
    footer += '(<b>' + data.critical_threshold + '%</b>)';
    footer += '</td>';
    footer += '</tr>';
    footer += '</table>';
    
    footer += '</td>';
    footer += '</tr>';
    footer += '</table>';

    full = header + allKpi + footer;
    $mostImpactedKpiListContainer.append(full);
}

function renderKpiList(data)
{
    // Walk through kpi list to display elements
    var allKpi = '';
    jQuery.each(data, function(index, value) {
        currentLine = '<tr class="' + value.tr_style + '">';

        currentLine += '<td class="ListColLeft" style="white-space:nowrap;">';
        if (value.icon !== '') {
            currentLine += '<img height="16" src="' + value.icon + '" style="padding-right:5px;" width="16" />';
        }
        currentLine += '<a onclick="' + value.ba_action + '" style="cursor: pointer;">';
        if (value.type === '0') {
            currentLine += value.hname + ' ';
        }
        currentLine += value.sname;
        currentLine += '</a>';
        currentLine += '</td>';


        currentLine += '<td class="ListColLeft" style="white-space:nowrap;"><center>';
        if (value.ack > 0) {
            currentLine += '<img src="./img/icons/technician.png" class="ico-20" />';
        }
        if (value.downtime > 0) {
            currentLine += '<img src="./modules/centreon-bam-server/core/common/images/warning.png" />';
        }
        if (value.notif === '0') {
            currentLine += '<img src="./img/icons/notifications_off.png" class="ico-20" />';
        }
        currentLine += '</center></td>';


        currentLine += '<td class="ListColLeft" style="white-space:nowrap;">';
        currentLine += value.type_string;
        currentLine += '</td>';


        currentLine += '<td>';
        currentLine += '<center><span class="badge" style="background: ' + value.status_color + '">';
        currentLine += value.status
        currentLine += '</span></center>';
        currentLine += '</td>';

        currentLine += '<td class="ListColLeft">';
        currentLine += value.output;
        currentLine += '</td>';

        currentLine += '<td class="ListColCenter" style="white-space:nowrap;">';
        if (value.impact !== '') {
            currentLine += value.impact + '%';
        }
        currentLine += '</td>';

        currentLine += '</tr>';
        allKpi += currentLine;
    });
    
    return allKpi;
}

