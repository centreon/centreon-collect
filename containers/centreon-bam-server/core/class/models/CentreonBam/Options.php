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

class CentreonBam_Options
{
	protected $_db;
	protected $_userId;
	protected $_form;

	/*
	 *  Constructor
	 */
	 function __construct($db, $userId, $form = null)
	 {
	 	$this->_db = $db;
	 	$this->_userId = $userId;
	 	$this->_form = $form;
	 }

	 /*
	  *  Sets options
	  */
	 public function setUserOptions()
	 {
	 	$query = "DELETE FROM `mod_bam_user_preferences` WHERE user_id = '".$this->_userId."'";
	 	$this->_db->query($query);

	 	$query = "DELETE FROM `mod_bam_user_overview_relation` WHERE user_id = '".$this->_userId."'";
	 	$this->_db->query($query);

	 	$query = "INSERT INTO `mod_bam_user_preferences` (user_id, pref_key, pref_value) VALUES ";
		$query .= "('".$this->_userId."', 'graph_style', '".$_POST['graph_style']."'), ";
		$query .= "('".$this->_userId."', 'rrd_graph_style', '".$_POST['rrd_graph_style']."'), ";
	 	$query .= "('".$this->_userId."', 'value_color', '".trim($_POST['value_color'], "#")."'), ";
	 	$query .= "('".$this->_userId."', 'warning_color', '".trim($_POST['warning_color'], "#")."'), ";
	 	$query .= "('".$this->_userId."', 'critical_color', '".trim($_POST['critical_color'], "#")."'), ";
	 	$query .= "('".$this->_userId."', 'value_alpha', '".trim($_POST['value_alpha'], "#")."'), ";
	 	$query .= "('".$this->_userId."', 'warning_alpha', '".trim($_POST['warning_alpha'], "#")."'), ";
	 	$query .= "('".$this->_userId."', 'critical_alpha', '".trim($_POST['critical_alpha'], "#")."'), ";
	 	$query .= "('".$this->_userId."', 'display_legend', '".$_POST['display_legend']['display_legend']."'), ";
	 	$query .= "('".$this->_userId."', 'display_guide', '".$_POST['display_guide']['display_guide']."') ";
	 	$this->_db->query($query);

	 	$tmpstr = "";
	 	if (isset($_POST['overview'])) {
	 		foreach ($_POST['overview'] as $id) {
	 			if ($tmpstr != "") {
	 				$tmpstr .= ", ";
	 			}
	 			$tmpstr .= "('".$id."', '".$this->_userId."')";
	 		}
	 	}
	 	if ($tmpstr != "") {
	 		$query = "INSERT INTO `mod_bam_user_overview_relation` (ba_id, user_id) VALUES " . $tmpstr;
	 		$this->_db->query($query);
	 	}
	 }

	 /*
	  * Get user options
	  * Returns an array
	  */
	 public function getUserOptions()
	 {
	 	$query = "SELECT * FROM `mod_bam_user_preferences` WHERE user_id = '".$this->_userId."'";
	 	$db = $this->_db->query($query);
	 	$tab = array();
	 	$options = array();
	 	while ($row = $db->fetchRow()) {
	 		$tab[$row['pref_key']] = $row['pref_value'];
	 	}
	 	isset($tab['graph_style']) ? $options['graph_style'] = $tab['graph_style'] : $options['graph_style'] = 'polar';
	 	isset($tab['rrd_graph_style']) ? $options['rrd_graph_style'] = $tab['rrd_graph_style'] : $options['rrd_graph_style'] = '1';
	 	isset($tab['value_color']) ? $options['value_color'] = $tab['value_color'] : $options['value_color'] = '88B917';
	 	isset($tab['warning_color']) ? $options['warning_color'] = $tab['warning_color'] : $options['warning_color'] = 'FF9A13';
	 	isset($tab['critical_color']) ? $options['critical_color'] = $tab['critical_color'] : $options['critical_color'] = 'E00B3D';
	 	isset($tab['value_alpha']) ? $options['value_alpha'] = $tab['value_alpha'] : $options['value_alpha'] = '100';
	 	isset($tab['warning_alpha']) ? $options['warning_alpha'] = $tab['warning_alpha'] : $options['warning_alpha'] = '75';
	 	isset($tab['critical_alpha']) ? $options['critical_alpha'] = $tab['critical_alpha'] : $options['critical_alpha'] = '75';
	 	isset($tab['display_legend']) ? $options['display_legend'] = $tab['display_legend'] : $options['display_legend'] = '1';
		isset($tab['display_guide']) ? $options['display_guide'] = $tab['display_guide'] : $options['display_guide'] = '1';

		$query = "SELECT * FROM `mod_bam_user_overview_relation` WHERE user_id = '".$this->_userId."'";
		$db = $this->_db->query($query);
		$tab = array();
		$options['overview'] = array();
		while ($row = $db->fetchRow()) {
			$options['overview'][$row['ba_id']] = $row['ba_id'];
		}
	 	return $options;
	 }

	 /*
	  * Sets options
	  */
	 public function setGeneralOptions()
	 {
	 	$query = "DELETE FROM `mod_bam_user_preferences` WHERE user_id IS NULL";
	 	$this->_db->query($query);

	 	$query = "INSERT INTO `mod_bam_user_preferences` (pref_key, pref_value) VALUES ";
		$query .= "('kpi_warning_drop', '".$_POST['kpi_warning_impact']."'), ";
	 	$query .= "('kpi_critical_drop', '".$_POST['kpi_critical_impact']."'), ";
	 	$query .= "('kpi_unknown_drop', '".$_POST['kpi_unknown_impact']."'), ";
	 	$query .= "('kpi_boolean_drop', '".$_POST['kpi_boolean_impact']."'), ";
	 	$query .= "('ba_warning_threshold', '".$_POST['ba_warning_threshold']."'), ";
	 	$query .= "('ba_critical_threshold', '".$_POST['ba_critical_threshold']."'), ";
	 	$query .= "('id_reporting_period', '".$_POST['id_reporting_period']."'), ";
	 	$query .= "('id_notif_period', '".$_POST['id_notif_period']."'), ";
        $query .= "('command_id', '".$_POST['command_id']."'), ";
	 	if (isset($_POST['bam_contact']) && is_array($_POST['bam_contact'])) {
	 	    $query .= "('bam_contact', '".implode(",", $_POST['bam_contact'])."'), ";
	 	} else {
	 	    $query .= "('bam_contact', 'NULL'), ";
	 	}
	 	$query .= "('notif_interval', '".$_POST['notif_interval']."') ";
	 	$this->_db->query($query);

	 	$this->_db->query("UPDATE mod_bam_impacts SET impact = '".$_POST['weak_impact']."' WHERE code = '".CRITICITY_WEAK."'");
	 	$this->_db->query("UPDATE mod_bam_impacts SET impact = '".$_POST['minor_impact']."' WHERE code = '".CRITICITY_MINOR."'");
	 	$this->_db->query("UPDATE mod_bam_impacts SET impact = '".$_POST['major_impact']."' WHERE code = '".CRITICITY_MAJOR."'");
	 	$this->_db->query("UPDATE mod_bam_impacts SET impact = '".$_POST['critical_impact']."' WHERE code = '".CRITICITY_CRITICAL."'");
	 	$this->_db->query("UPDATE mod_bam_impacts SET impact = '".$_POST['blocking_impact']."' WHERE code = '".CRITICITY_BLOCKING."'");
	 }

	 /*
	  * Get user options
	  * Returns an array
	  */
	 public function getGeneralOptions()
	 {
	 	$query = "SELECT * FROM `mod_bam_user_preferences` WHERE user_id IS NULL";
		$db = $this->_db->query($query);
		$data_tab = array();
		$tab = array();
		while ($row = $db->fetchRow()) {
			$tab[$row['pref_key']] = $row['pref_value'];
		}
		isset($tab['kpi_warning_drop']) ? $data_tab['kpi_warning_impact'] = $tab['kpi_warning_drop'] : $data_tab['kpi_warning_impact'] = NULL;
		isset($tab['kpi_critical_drop']) ? $data_tab['kpi_critical_impact'] = $tab['kpi_critical_drop'] : $data_tab['kpi_critical_impact'] = NULL;
		isset($tab['kpi_unknown_drop']) ? $data_tab['kpi_unknown_impact'] = $tab['kpi_unknown_drop'] : $data_tab['kpi_unknown_impact'] = NULL;
		isset($tab['kpi_boolean_drop']) ? $data_tab['kpi_boolean_impact'] = $tab['kpi_boolean_drop'] : $data_tab['kpi_boolean_impact'] = NULL;
		isset($tab['ba_warning_threshold']) ? $data_tab['ba_warning_threshold'] = $tab['ba_warning_threshold'] : $data_tab['ba_warning_threshold'] = NULL;
		isset($tab['ba_critical_threshold']) ? $data_tab['ba_critical_threshold'] = $tab['ba_critical_threshold'] : $data_tab['ba_critical_threshold'] = NULL;
        isset($tab['command_id']) ? $data_tab['command_id'] = $tab['command_id'] : $data_tab['command_id'] = NULL;
		isset($tab['id_reporting_period']) ? $data_tab['id_reporting_period'] = $tab['id_reporting_period'] : $data_tab['id_reporting_period'] = NULL;
		isset($tab['id_notif_period']) ? $data_tab['id_notif_period'] = $tab['id_notif_period'] : $data_tab['id_notif_period'] = NULL;
		isset($tab['bam_contact']) ? $data_tab['bam_contact'] = $tab['bam_contact'] : $data_tab['bam_contact'] = NULL;
		isset($tab['notif_interval']) ? $data_tab['notif_interval'] = $tab['notif_interval'] : $data_tab['notif_interval'] = NULL;

		$query = "SELECT * FROM `mod_bam_impacts` ORDER BY code";
		$res = $this->_db->query($query);
		while ($row = $res->fetchRow()) {
		    if ($row['code'] == CRITICITY_WEAK) {
		        $data_tab['weak_impact'] = $row['impact'];
		    } elseif ($row['code'] == CRITICITY_MINOR) {
		        $data_tab['minor_impact'] = $row['impact'];
		    } elseif ($row['code'] == CRITICITY_MAJOR) {
		        $data_tab['major_impact'] = $row['impact'];
		    } elseif ($row['code'] == CRITICITY_CRITICAL) {
		        $data_tab['critical_impact'] = $row['impact'];
		    } elseif ($row['code'] == CRITICITY_BLOCKING) {
                $data_tab['blocking_impact'] = $row['impact'];
            }
		}
	 	return $data_tab;
	 }
}
