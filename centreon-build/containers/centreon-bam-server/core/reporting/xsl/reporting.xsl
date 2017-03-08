<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:template match="/">	
	<xsl:for-each select="//root">
	<table class="ListTable">
		<tr class='ListHeader'>
			<td colspan="2">				
				<xsl:value-of select="ba_label"/>				
			</td>
			<td>
				<xsl:value-of select="ba_status"/>
			</td>
			<td>
				<xsl:value-of select="last_check_label"/>
			</td>
			<td>
				<xsl:value-of select="duration_label"/>
			</td>
			<td>
				<xsl:value-of select="current_label"/>
			</td>
			<td>
				<xsl:value-of select="warning_label"/>
			</td>
			<td>
				<xsl:value-of select="critical_label"/>
			</td>
		</tr>
		<xsl:for-each select="//ba">
			<xsl:if test="is_group = 'yes'">
				<tr class="list_lvl_1">
					<td colspan="8">
						<xsl:value-of select="name" />
					</td>
				</tr>
			</xsl:if>
			<xsl:if test="is_group = 'no'">		
				<xsl:element name="tr">
					<xsl:attribute name="class">
						<xsl:value-of select="tr_style" />
					</xsl:attribute>
					<td>	
						<a href="javascript:">
							<xsl:attribute name="onClick">
								javascript:updateGraph('<xsl:value-of select="name" />')
							</xsl:attribute>
							<xsl:value-of select="name" />
						</a>
					</td>
					<td>
						<center>
						<xsl:element name="img">
							<xsl:attribute name="src">./modules/centreon-bam-server/core/common/images/column-chart.gif</xsl:attribute>
							<xsl:attribute name="onmouseover">displayIMG('<xsl:value-of select="svc_index"/>','<xsl:value-of select="//sid"/>','<xsl:value-of select="name"/>');</xsl:attribute>
							<xsl:attribute name="onmouseout">hiddenIMG();</xsl:attribute>					
						</xsl:element>
						</center>				
					</td>
					<xsl:element name="td">				
						<xsl:attribute name="bgcolor">
							<xsl:value-of select="status_color" />
						</xsl:attribute>				
						<center><b><xsl:value-of select="status" /></b></center>				
					</xsl:element>
					<td align="center">
						<xsl:value-of select="last_check" />		
					</td>
					<td align="center">
						<xsl:value-of select="duration" />
					</td>
					<td align="center">
						<xsl:value-of select="current" />%		
					</td>				
					<td align="center">
						<xsl:value-of select="warning" />%				
					</td>
					<td align="center">
						<xsl:value-of select="critical" />%	
					</td>
				</xsl:element>
			</xsl:if>
		</xsl:for-each>
	</table>
		<div id="div_img" class="img_volante"></div>
	</xsl:for-each>
</xsl:template>
</xsl:stylesheet>