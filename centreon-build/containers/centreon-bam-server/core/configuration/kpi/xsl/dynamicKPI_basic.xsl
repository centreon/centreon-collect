<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:template match="/">
	<xsl:for-each select="//root">	
		<xsl:element name="table">
			<xsl:attribute name="class">ListTable</xsl:attribute>
			<xsl:element name="tr">
				<xsl:attribute name="class">ListHeader</xsl:attribute>
				<xsl:element name="td">
					<xsl:attribute name="class">ListColHeaderPicker</xsl:attribute>
					<input type="checkbox" name="checkall" onclick="checkUncheckAll(this);"/>
				</xsl:element>
				<xsl:element name="td"><xsl:value-of select="info/host_label" /></xsl:element>
				<xsl:element name="td"><xsl:value-of select="info/svc_label" /></xsl:element>
				<xsl:element name="td">
					<xsl:attribute name="class">ListColHeaderCenter</xsl:attribute>
					<xsl:value-of select="info/warning_label" />
				</xsl:element>
				<xsl:element name="td">
					<xsl:attribute name="class">ListColHeaderCenter</xsl:attribute>
					<xsl:value-of select="info/critical_label" />
				</xsl:element>
				<xsl:element name="td">
					<xsl:attribute name="class">ListColHeaderCenter</xsl:attribute>
					<xsl:value-of select="info/unknown_label" />
				</xsl:element>
			</xsl:element>
			<xsl:if test = "//root/info/number_of_kpi = '0'">
				<xsl:element name="tr">					
					<xsl:element name="td">
						<xsl:attribute name="colspan">6</xsl:attribute>
						<xsl:attribute name="class">ListColCenter</xsl:attribute>
						<b><xsl:value-of select="//root/info/no_entry" /></b>
					</xsl:element>
				</xsl:element>
			</xsl:if>
			<xsl:for-each select="//kpi">
				<xsl:element name="tr">
					<xsl:attribute name="class"><xsl:value-of select="style" /></xsl:attribute>
					<xsl:element name="td">
						<xsl:attribute name="class">ListColHeaderPicker</xsl:attribute>						
						<xsl:element name="input">
							<xsl:attribute name="type">checkbox</xsl:attribute>
							<xsl:attribute name="name">select[<xsl:value-of select="host_id" />;<xsl:value-of select="service_id" />]</xsl:attribute>
						</xsl:element>						
					</xsl:element>
					<xsl:element name="td"><xsl:value-of select="host_name" /></xsl:element>
					<xsl:element name="td"><xsl:value-of select="service_desc" /></xsl:element>
					<xsl:element name="td">
						<xsl:attribute name="class">ListColCenter</xsl:attribute>
						<xsl:element name="select">							
							<xsl:attribute name="name">bw_<xsl:value-of select="host_id" />_<xsl:value-of select="service_id" /></xsl:attribute>
							<xsl:for-each select="//root/impact">
								<xsl:element name="option">
									<xsl:attribute name="value"><xsl:value-of select="code"/></xsl:attribute>
									<xsl:attribute name="style">
										background-color: <xsl:value-of select="color"/>;font-weight: bold;
									</xsl:attribute>
									<xsl:value-of select="label"/>
								</xsl:element>
							</xsl:for-each>
						</xsl:element>
					</xsl:element>
					<xsl:element name="td">
						<xsl:attribute name="class">ListColCenter</xsl:attribute>
						<xsl:element name="select">
							<xsl:attribute name="name">bc_<xsl:value-of select="host_id" />_<xsl:value-of select="service_id" /></xsl:attribute>
							<xsl:for-each select="//root/impact">
								<xsl:element name="option">
									<xsl:attribute name="value"><xsl:value-of select="code"/></xsl:attribute>
									<xsl:attribute name="style">
										background-color: <xsl:value-of select="color"/>;font-weight: bold;
									</xsl:attribute>
									<xsl:value-of select="label"/>
								</xsl:element>
							</xsl:for-each>
						</xsl:element>
					</xsl:element>
					<xsl:element name="td">
						<xsl:attribute name="class">ListColCenter</xsl:attribute>
						<xsl:element name="select">
							<xsl:attribute name="name">bu_<xsl:value-of select="host_id" />_<xsl:value-of select="service_id" /></xsl:attribute>
							<xsl:for-each select="//root/impact">
								<xsl:element name="option">
									<xsl:attribute name="value"><xsl:value-of select="code"/></xsl:attribute>
									<xsl:attribute name="style">
										background-color: <xsl:value-of select="color"/>;font-weight: bold;
									</xsl:attribute>
									<xsl:value-of select="label"/>
								</xsl:element>
							</xsl:for-each>
						</xsl:element>
					</xsl:element>
				</xsl:element>
			</xsl:for-each>
		</xsl:element>
		<xsl:if test = "//root/info/number_of_kpi > 0">
			<br/>
			<center>		
			<xsl:element name="input">
				<xsl:attribute name="class">btc bt_success</xsl:attribute>
				<xsl:attribute name="type">button</xsl:attribute>
				<xsl:attribute name="value"><xsl:value-of select="//root/info/button_label" /></xsl:attribute>
				<xsl:attribute name="onClick">checkIfBaSelected();</xsl:attribute>			
			</xsl:element>		
			</center>
		</xsl:if>
	</xsl:for-each>
</xsl:template>
</xsl:stylesheet>
