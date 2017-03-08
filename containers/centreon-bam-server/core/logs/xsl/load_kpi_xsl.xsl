<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:template match="/">	
	<xsl:for-each select="//root">
	<xsl:if test="nb = 1">
		<table class="ListTable">
			<tr class='ListHeader'>
				<td align="center">				
					<xsl:value-of select="kpi_label"/>				
				</td>
				<td align="center">
					<xsl:value-of select="type_label"/>					
				</td>
				<td align="center">
					<xsl:value-of select="status_label"/>
				</td>
				<td align="center">
					<xsl:value-of select="impact_label"/>
				</td>
				<td align="center">
					<xsl:value-of select="downtime_label"/>
				</td>
				<td align="center">
					<xsl:value-of select="start_time_label"/>
				</td>
				<td align="center">
					<xsl:value-of select="end_time_label"/>
				</td>
				<td align="center">
					<xsl:value-of select="output_label"/>
				</td>
			</tr>
			<xsl:for-each select="//kpi">			
				<xsl:element name="tr">
					<xsl:attribute name="class">
						<xsl:value-of select="tr_style" />
					</xsl:attribute>
					<td align="left">
						<xsl:value-of select="name" />
					</td>
					<td align="center">
						<xsl:value-of select="type" />		
					</td>
					<td class="ListColCenter">
						<span>
							<xsl:attribute name="class">
								<xsl:value-of select="badge" />
							</xsl:attribute>				
							<xsl:value-of select="status" />
						</span>
					</td>
					<td align="center">
						<xsl:value-of select="impact" />		
					</td>
					<td align="center">
						<xsl:value-of select="in_downtime" />
					</td>
					<td align="center">
						<xsl:value-of select="start_time" />
					</td>
					<td align="center">
						<xsl:value-of select="end_time" />
					</td>
					<td align="left">
						<xsl:value-of select="output" />
					</td>
				</xsl:element>			
			</xsl:for-each>
		</table>
	</xsl:if>			
	</xsl:for-each>
</xsl:template>
</xsl:stylesheet>
