<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:output method="html" encoding="UTF-8"/>
    <xsl:template match="/">
        <xsl:for-each select="//root">
            <table class="ListTable">
                <xsl:for-each select="//current_bv">
                    <tr class="list_lvl_1">
                        <td colspan="11" class="ListColLvl1_name">
                            <h4>
                               <xsl:value-of select="//root/business_view_label" /> :
                               <xsl:value-of select="name" />
                            </h4>
                        </td>
                    </tr>
                </xsl:for-each>
                <tr class='ListHeader'>
                    <td class="ListColHeaderPicker">				
                        <xsl:element name="input">
                            <xsl:attribute name="type">checkbox</xsl:attribute>
                            <xsl:attribute name="name">checkall</xsl:attribute>
                            <xsl:attribute name='id'>0</xsl:attribute>
                            <xsl:attribute name="onclick">checkUncheckAll(this);</xsl:attribute>
                        </xsl:element>
                    </td>
                    <td align="left">
                        <xsl:value-of select="current_label"/>
                    </td>			
                    <td class="ListColLeft" style="white-space:nowrap;" colspan='2'>				
                        <xsl:value-of select="ba_label"/>				
                    </td>
                    <td class="ListColLeft" colspan="2">
                        <xsl:value-of select="ba_description"/>
                    </td>
                    <td align="center">
                        <xsl:value-of select="duration_label"/>
                    </td>
                    <td align="center">
                        <xsl:value-of select="reporting_period_label"/>
                    </td>
                </tr>
                <xsl:for-each select="//ba">
                    <xsl:element name="tr">
                        <xsl:attribute name="class">
                            <xsl:value-of select="tr_style" />
                        </xsl:attribute>
                        <xsl:element name="td">
                            <xsl:element name="input">
                                <xsl:attribute name="type">checkbox</xsl:attribute>
                                <xsl:attribute name="class">ListColPicker</xsl:attribute>						
                                <xsl:attribute name="id">
                                    <xsl:value-of select="ba_id" />
                                </xsl:attribute>
                            </xsl:element>
                        </xsl:element>
                        <td class="ListColLeft" width="100px;">							
                            <xsl:element name="span">
                                <xsl:attribute name="class">state_badge <xsl:value-of select="status_badge"/> margin_right</xsl:attribute>

                                <xsl:attribute name="title">Warning : <xsl:value-of select="warning"/>%, Critical : <xsl:value-of select="critical"/>%
                                </xsl:attribute>
                            </xsl:element>
                            <xsl:if test="status != ''">
                                <b><xsl:value-of select="current" />%</b>
                            </xsl:if>	
                        </td>
                        <td class="ListColLeft" style="white-space:nowrap;">
                            <xsl:if test="icon != ''">
                                <xsl:element name="img">
                                    <xsl:attribute name="src">
                                        <xsl:value-of select="icon" />
                                    </xsl:attribute>
                                    <xsl:attribute name="class">ico-18 margin_right</xsl:attribute>
                                </xsl:element>
                            </xsl:if>
                            <xsl:element name="a">
                                <xsl:attribute name="href">
                                    <xsl:value-of select="ba_url" />
                                </xsl:attribute>
                                <xsl:attribute name="class">baminfobulle</xsl:attribute>
                                <xsl:attribute name="style">cursor: pointer;</xsl:attribute>
                                <xsl:attribute name="onmouseover">
                                    _myTimer = setTimeout('showBADetails(\'span_<xsl:value-of select="ba_id"/>\', <xsl:value-of select="ba_id"/>)', 800);
                                </xsl:attribute>
                                
                                <xsl:attribute name="onmouseout">
                                    clearTimeout(_myTimer);
                                    hideBADetails('span_<xsl:value-of select="ba_id"/>');
                                </xsl:attribute>
                                <xsl:value-of select="name"/>
                                <xsl:element name="span">
                                    <xsl:attribute name="id">span_<xsl:value-of select="ba_id"/></xsl:attribute>								
                                </xsl:element>			
                            </xsl:element>
                        </td>
                        <td>
                          <xsl:if test="acknowledged > 0">
                            <xsl:element name="img">
                              <xsl:attribute name="src">./img/icons/technician.png</xsl:attribute>
                              <xsl:attribute name="class">ico-20</xsl:attribute>
                            </xsl:element>
                          </xsl:if>
                          <xsl:if test="downtime > '0'">
                            <xsl:element name="img">								
                              <xsl:attribute name="src">./img/icons/warning.png</xsl:attribute>
                              <xsl:attribute name="class">ico-16</xsl:attribute>
                            </xsl:element>
                          </xsl:if>
                          <xsl:if test="notif = '0'">
                            <xsl:element name="img">								
                              <xsl:attribute name="src">./img/icons/notifications_off.png</xsl:attribute>
                                <xsl:attribute name="class">ico-16</xsl:attribute>
                            </xsl:element>
                          </xsl:if>
                        </td>
                        <td class="ListColLeft" style="white-space:nowrap;">
                            <xsl:element name="a">
                                <xsl:attribute name="href">
                                    <xsl:value-of select="ba_url" />
                                </xsl:attribute>
                                <xsl:attribute name="class">baminfobulle</xsl:attribute>
                                <xsl:attribute name="style">cursor: pointer;</xsl:attribute>
                                <xsl:attribute name="onmouseover">
                                    _myTimer = setTimeout('showBADetails(\'span2_<xsl:value-of select="ba_id"/>\', <xsl:value-of select="ba_id"/>)', 800);
                                </xsl:attribute>
                                <xsl:attribute name="onmouseout">
                                    clearTimeout(_myTimer);
                                    hideBADetails('span2_<xsl:value-of select="ba_id"/>');
                                </xsl:attribute>							
                                <xsl:value-of select="description" />
                                <xsl:element name="span">								
                                    <xsl:attribute name="id">span2_<xsl:value-of select="ba_id"/></xsl:attribute>								
                                </xsl:element>			
                            </xsl:element>
                        </td>					
                        <td class="ListColLeft" style="white-space:nowrap;">
                            <xsl:if test="svc_index > 0">
                                <center>						
                                    <xsl:element name="a">
                                        <xsl:attribute name="href">main.php?p=20401&amp;mode=0&amp;svc_id=<xsl:value-of select="host_id"/>;<xsl:value-of select="service_id"/>
                                        </xsl:attribute>
                                        <xsl:element name="img">
                                            <xsl:attribute name="src">./img/icons/chart.png</xsl:attribute>
                                            <xsl:attribute name="class">ico-18</xsl:attribute>
                                            <xsl:attribute name="onmouseover">displayIMG(<xsl:value-of select="host_id"/>,<xsl:value-of select="service_id"/>);
                                            </xsl:attribute>
                                            <xsl:attribute name="onmouseout">hiddenIMG();</xsl:attribute>					
                                        </xsl:element>
                                    </xsl:element>
                                </center>
                            </xsl:if>			
                        </td>
                        <td align="center">
                            <xsl:value-of select="duration" />
                        </td>
                        <td class="ListColCenter" style="white-space:nowrap;">
                            <xsl:value-of select="reportingperiod" />
                        </td>
                    </xsl:element>			
                </xsl:for-each>
            </table>
            <!--<div id="div_img" class="img_volante"></div>-->
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>
