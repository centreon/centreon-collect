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

/*
 * Class that contains all methods for managing Reporting
 */
 class CentreonBam_Log
 {
 	protected $_db;
 	protected $_dbO;
 	protected $_baId;

 	/*
 	 * Constructor
 	 */
 	function __construct($db, $dbO, $baId)
 	{
 		$this->_db = $db;
 		$this->_dbO = $dbO;
 		$this->_baId = $baId;
 	}

 	/*
 	 * Get predefined period
 	 */
 	 private function getDateSelect_predefined($period)
 	 {
		$time = time();
		$day = date("d", $time);
		$year = date("Y", $time);
		$month = date("m", $time);
		if (!is_null($period)){
			if ($period == "today") {
				$start_date = mktime(0, 0, 0, $month, $day, $year);
				$end_date = mktime(24, 0, 0, $month, $day, $year);
			} elseif ($period == "yesterday") {
				$start_date = mktime(0, 0, 0, $month, $day - 1, $year);
				$end_date = mktime(24, 0, 0, $month, $day - 1, $year);
			} elseif ($period == "thisweek") {
				$dd = (date("D",mktime(24, 0, 0, $month, $day - 1, $year)));
				for($ct = 1; $dd != "Mon" ;$ct++) {
					$dd = (date("D",mktime(0, 0, 0, $month, ($day - $ct), $year)));
				}
				$start_date = mktime(0, 0, 0, $month, $day - $ct, $year);
				$end_date = mktime(24, 0, 0, $month, ($day), $year);
			} elseif ($period == "last7days") {
				$start_date = mktime(0, 0, 0, $month, $day - 7, $year);
				$end_date = mktime(24, 0, 0, $month, $day, $year);
			} elseif ($period == "last14days") {
			    $start_date = mktime(0, 0, 0, $month, $day - 14, $year);
				$end_date = mktime(24, 0, 0, $month, $day, $year);
			} elseif ($period == "last30days") {
			    $start_date = mktime(0, 0, 0, $month, $day - 30, $year);
				$end_date = mktime(24, 0, 0, $month, $day, $year);
			} else {
				$start_date = mktime(0, 0, 0, $month - 1, 1, $year);
				$end_date = mktime(0, 0, 0, $month, 1, $year);
			}
		} else {
			$start_date = mktime(0, 0, 0, $month, $day - 1, $year);
			$end_date = mktime(24, 0, 0, $month, $day - 1, $year);
		}
		if ($start_date > $end_date) {
			$start_date = $end_date;
		}
		return (array($start_date, $end_date));
	}

 	 /* Returns period to report */
 	public function getPeriodToReport($period)
 	{
	    $interval = $this->getDateSelect_predefined($period);
	    $start_date = $interval[0];
	    $end_date = $interval[1];
	    return(array($start_date, $end_date));
 	}

 	/* get list of periods*/
 	public function getPeriodList()
 	{
 		$periodList = array();
		$periodList["today"] = _("Today");
		$periodList["yesterday"] = _("Yesterday");
		$periodList["thisweek"] = _("This Week");
		$periodList["last7days"] = _("Last 7 Days");
		$periodList["last14days"] = _("Last 14 Days");
		$periodList["last30days"] = _("Last 30 Days");
		return $periodList;
 	}

 	/*
 	 *  Method that returns the log
 	 */
 	public function retrieveLogs($date_start, $date_end)
 	{
		$query = "SELECT * FROM `mod_bam_reporting_ba_events`
			WHERE ba_id = " . $this->_dbO->escape($this->_baId) . "
			AND start_time BETWEEN " . $this->_dbO->escape($date_start) . " 
			AND " . $this->_dbO->escape($date_end) . " 
			ORDER BY start_time ASC";
 		$res = $this->_dbO->query($query);
 		$tab = array();

	 	while ($row = $res->fetchRow()) {
			$tab[$row['ba_event_id']] = $row;
	 	}
 		return $tab;
 	}
 }
?>
