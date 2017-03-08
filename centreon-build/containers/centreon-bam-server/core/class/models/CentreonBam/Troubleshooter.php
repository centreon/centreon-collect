<?php
/*
 * Centreon
 *
 * Source Copyright 2005-2016 Centreon
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@centreon.com
 *
 */

class CentreonBam_Troubleshooter
{
	protected $_centreon;
	protected $_db;

	/*
	 *  Constructor
	 */
	function __construct($centreon, $db)
	{
		$this->_centreon = $centreon;
		$this->_db = $db;
	}

	/*
	 *  Checks if module is loaded
	 */
	public function checkModule()
	{
		if (isset($this->_centreon->modules['centreon-bam-server'])) {
			return _("OK");
		}
		return _("NOK");
	}

	/*
	 *  Returns solution when module is not loaded
	 */
	public function getModuleSolution($status)
	{
		$solution = "-";
		if ($status == _("NOK")) {
			$solution = _("Disconnect from Centreon and reconnect.");
		}
		return $solution;
	}

	/*
	 *  Checks if plugin is installed
	 */
	public function checkPlugin()
	{
		$res = $this->_db->query("SHOW TABLES LIKE 'general_opt'");
		$str_key = "";
		$query = "";
		if ($res->numRows()) {
			$query = "SELECT nagios_path_plugins FROM general_opt LIMIT 1";
			$str_key = "rrdtool_path_bin";
		}
		$res2 = $this->_db->query("SHOW TABLES LIKE 'options'");
		if ($res2->numRows()) {
			$query = "SELECT `value` FROM `options` WHERE `key` = 'nagios_path_plugins' LIMIT 1";
			$str_key = "value";
		}

		if ($query != "") {
			$res = $this->_db->query($query);
			$row = $res->fetchRow();
			$path = $row[$str_key];
			if (file_exists($path . "/check_centreon_bam")) {
				return _("OK");
			}
		}
		return _("NOK");
	}

	/*
	 *  Returns solution when plugin is not present
	 */
	public function getPluginSolution($status)
	{
		$solution = "-";
		if ($status == _("NOK")) {
			$solution = _("Re install module.");
		}
		return $solution;
	}

	/*
	 *  Checks if PHP XML Library is installed
	 */
	public function checkXmlLib()
	{
		if (class_exists("XMLWriter")) {
			return _("OK");
		}
		return _("NOK");
	}

	/*
	 *  Returns solution if xml lib is not present
	 */
	public function getXmlLibSolution($status)
	{
		$solution = "-";
		if ($status == _("NOK")) {
			$solution = _("Install the PHP XML Writer library");
		}
		return $solution;
	}

	/*
	 *  checks if there is a link to the php cli config file
	 */
	public function check_php_cli()
	{
		$php_file = "/etc/php5/cli/php.ini";
		if (file_exists($php_file)) {
			if (is_link($php_file)) {
				return _("OK");
			} else {
				return _("NOK");
			}
		} else {
			return _("OK");
		}
	}

	/*
	 *  returns solution of php cli problem (not linked)
	 */
	public function getPhpCliSolution($status)
	{
		$solution = "-";
		if ($status == _("NOK")) {
			$solution = _("Create a symbolic link /etc/php5/cli/php.ini that targets the Zend php.ini file.");
			$solution .= "<br/>";
			$solution .= _("Please contact your support for more information");
		}
		return $solution;
	}

}
?>
