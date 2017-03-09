<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:template match="/">
	<xsl:for-each select="//root">
            <xsl:element name='label'>
                <xsl:if test="valid='true'">
                    <xsl:attribute name='style'>color:#99CC33; font-weight:bold;</xsl:attribute>
                </xsl:if>
                <xsl:if test="valid='false'">
                    <xsl:attribute name='style'>color:#EE4110; font-weight:bold;</xsl:attribute>
                </xsl:if>
                <xsl:value-of select='result'/>
            </xsl:element>
            &#160;
            <xsl:if test="mode = 0">
                <xsl:element name='input'>
                    <xsl:attribute name='id'>simButton</xsl:attribute>
                    <xsl:attribute name='type'>button</xsl:attribute>
                    <xsl:attribute name='onClick'>displaySimTable(this);</xsl:attribute>
                    <xsl:attribute name='value'><xsl:value-of select='simModeLabeL'/></xsl:attribute>
                </xsl:element>
            </xsl:if>
            <xsl:element name='div'>
                <xsl:attribute name='id'>hiddenSimDiv</xsl:attribute>
                <xsl:attribute name='style'>display: none;</xsl:attribute>
                <table class="ListTable">
                    <tr class='ListHeader'>
                        <td class='FormHeader' colspan='2'><xsl:value-of select='simtitle'/></td>
                    </tr>
                    <xsl:for-each select="//root/resource">
                        <xsl:element name='tr'>
                            <xsl:attribute name='class'><xsl:value-of select='@trStyle'/></xsl:attribute>
                            <td class='FormRowField'><xsl:value-of select='@name'/></td>
                            <td class='FormRowValue'>
                            <xsl:element name='select'>
                                <xsl:attribute name='onChange'>simulateStatus();</xsl:attribute>
                                <xsl:attribute name='class'>simstat</xsl:attribute>
                                <xsl:attribute name='name'><xsl:value-of select='@name'/></xsl:attribute>
                                <xsl:element name='option'>
                                    <xsl:attribute name='value'>0</xsl:attribute>
                                    <xsl:if test = '@status = 0'>
                                        <xsl:attribute name='selected'>selected</xsl:attribute>
                                    </xsl:if>
                                    Ok
                                </xsl:element>
                                <xsl:element name='option'>
                                    <xsl:attribute name='value'>1</xsl:attribute>
                                    <xsl:if test = '@status = 1'>
                                        <xsl:attribute name='selected'>selected</xsl:attribute>
                                    </xsl:if>
                                    Warning
                                </xsl:element>
                                <xsl:element name='option'>
                                    <xsl:attribute name='value'>2</xsl:attribute>
                                    <xsl:if test = '@status = 2'>
                                        <xsl:attribute name='selected'>selected</xsl:attribute>
                                    </xsl:if>
                                    Critical
                                </xsl:element>
                                <xsl:element name='option'>
                                    <xsl:attribute name='value'>3</xsl:attribute>
                                    <xsl:if test = '@status = 3'>
                                        <xsl:attribute name='selected'>selected</xsl:attribute>
                                    </xsl:if>
                                    Unknown
                                </xsl:element>
                            </xsl:element>
                            </td>
                        </xsl:element>
                    </xsl:for-each>
                </table>
            </xsl:element>
	</xsl:for-each>
</xsl:template>
</xsl:stylesheet>