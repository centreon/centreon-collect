<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:template match="/">
	<xsl:for-each select="//root">
		<xsl:element name="select">
			<xsl:attribute name="id">svc_select</xsl:attribute>
			<xsl:attribute name="name">svc_select</xsl:attribute>
			<xsl:for-each select="//svc">				
				<xsl:element name="option">
					<xsl:attribute name="value">
						<xsl:value-of select="id" />
					</xsl:attribute>
					<xsl:if test = "//root/selected = id">
						<xsl:attribute name="selected">true</xsl:attribute>
					</xsl:if>
					<xsl:value-of select="name" />
				</xsl:element>
			</xsl:for-each>
		</xsl:element>
	</xsl:for-each>
</xsl:template>
</xsl:stylesheet>