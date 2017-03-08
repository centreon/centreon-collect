<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:output method="html"/>
<xsl:template match="/">	
	<xsl:for-each select="//root">
	<table class="ListTable">
		<tr class="list_separator">
            <td colspan="6" align="center"><b><xsl:value-of select="ba_label"/> : <xsl:value-of select="ba_name"/></b>
            </td>
        </tr>
		<tr class='list_lvl_1'>			
			<td colspan="2" class="ListColLeft" style="white-space:nowrap;padding:3px">				
				<xsl:value-of select="indicator_label"/>
			</td>
			<td class="ListColCenter" style="white-space:nowrap;padding:3px">
				<xsl:value-of select="indicator_type_label"/>
			</td>
			<td class="ListColCenter" style="white-space:nowrap;padding:3px">
				<xsl:value-of select="status_label"/>
			</td>
			<td class="ListColCenter" style="white-space:nowrap;padding:3px">
				<xsl:value-of select="output_label"/>
			</td>
			<td class="ListColCenter" style="white-space:nowrap;padding:3px">
				<xsl:value-of select="impact_label"/>
			</td>
		</tr>		
		<xsl:for-each select="//kpi">
				<xsl:element name="tr">
					<xsl:attribute name="class">
						<xsl:value-of select="tr_style" />
					</xsl:attribute>				
					<td class="ListColLeft" style="white-space:nowrap;padding:3px">
						<xsl:if test="icon != ''">
							<xsl:element name="img">
									<xsl:attribute name="src">
										<xsl:value-of select="icon" />
									</xsl:attribute>
									<xsl:attribute name="class">ico-16 margin_right</xsl:attribute>
							</xsl:element>
						</xsl:if>
						<xsl:if test="type = '0'">
							<xsl:value-of select="hname" /> / 
						</xsl:if>						
							<xsl:value-of select="sname" />						
					</td>
					<td class="ListColCenter" style="white-space:nowrap;padding:3px">
						<center>
							<xsl:if test="ack > 1">
									<xsl:element name="img">
										<xsl:attribute name="src">./img/icons/technician.png</xsl:attribute>
										<xsl:attribute name="class">ico-20 margin_right</xsl:attribute>
									</xsl:element>
							</xsl:if>
							<xsl:if test="downtime > 1">
									<xsl:element name="img">
										<xsl:attribute name="src">./img/icons/warning.png</xsl:attribute>
										<xsl:attribute name="class">ico-18 margin_right</xsl:attribute>
									</xsl:element>
							</xsl:if>
							<xsl:if test="notif = '0'">
									<xsl:element name="img">						
										<xsl:attribute name="src">./img/icons/notifications_off.png</xsl:attribute>
										<xsl:attribute name="src">ico-18 margin_right</xsl:attribute>
									</xsl:element>
							</xsl:if>
						</center>
					</td>
					<td class="ListColCenter" style="white-space:nowrap;padding:3px">												
						<xsl:value-of select="type_string" />			
					</td>
					<xsl:element name="td">

						<center>
							<span class="badge" style="">
								<xsl:attribute name="style">
									background:<xsl:value-of select="status_color" />
								</xsl:attribute>
								<xsl:value-of select="status" />
							</span></center>
					</xsl:element>
					<td class="ListColLeft" style="white-space:nowrap;padding:3px">
						<xsl:value-of select="output" />				
					</td>
					<td class="ListColCenter" style="white-space:nowrap;">
                                            <xsl:if test="impact != ''">
                                                <xsl:value-of select="impact" />%
                                            </xsl:if>				
					</td>
				</xsl:element>			
		</xsl:for-each>
	</table>
	</xsl:for-each>
</xsl:template>
</xsl:stylesheet>
