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
 class CentreonBam_Reporting
 {
    /**
     *
     * @var type 
     */
    protected $_db;
    
    /**
     *
     * @var type 
     */
    protected $_dbO;

    /**
     * Constructor
     * 
     * @param type $db
     * @param type $dbO
     */
    function __construct($db, $dbO)
    {
        $this->_db = $db;
        $this->_dbO = $dbO;
    }

    /**
     * Returns period list
     * 
     * @return type
     */
    public function getPeriodList()
    {
        $periodList = array(null => null);
        $periodList["yesterday"] = _("Yesterday");
        $periodList["thisweek"] = _("This Week");
        $periodList["last7days"] = _("Last 7 Days");
        $periodList["thismonth"] = _("This Month");
        $periodList["last30days"] = _("Last 30 Days");
        $periodList["lastmonth"] = _("Last Month");
        $periodList["thisyear"] = _("This Year");
        $periodList["lastyear"] = _("Last Year");
        return $periodList;
    }

    /**
     * Returns time period
     * 
     * @return type
     */
    public function getreportingTimePeriod()
    {
        $reportingTimePeriod = array();
        $query = "SELECT * FROM `contact_param` WHERE cp_contact_id IS NULL";
        $res = $this->_db->query($query);
        $tab = array();
        while ($row = $res->fetchRow()) {
            $reportingTimePeriod[$row["cp_key"]] = $row["cp_value"];
        }
        return $reportingTimePeriod;
    }

    /**
     * get predefined date
     * 
     * @param type $period
     * @return type
     */
    private function getDateSelect_predefined($period)
    {
        $time = time();
        $day = date("d", $time);
        $year = date("Y", $time);
        $month = date("m", $time);
        if (!is_null($period)) {
            if($period == "yesterday") {
                $start_date = mktime(0, 0, 0, $month, $day - 1, $year);
                $end_date = mktime(24, 0, 0, $month, $day - 1, $year);
            } elseif($period == "thisweek") {
                $dd = (date("D",mktime(24, 0, 0, $month, $day - 1, $year)));
                for($ct = 1; $dd != "Mon" ;$ct++) {
                    $dd = (date("D",mktime(0, 0, 0, $month, ($day - $ct), $year)));
                }
                $start_date = mktime(0, 0, 0, $month, $day - $ct, $year);
                $end_date = mktime(24, 0, 0, $month, ($day - 1), $year);
            } elseif($period == "last7days") {
                $start_date = mktime(0, 0, 0, $month, $day - 7, $year);
                $end_date = mktime(24, 0, 0, $month, $day - 1, $year);
            } elseif($period == "last30days") {
                $start_date = mktime(0, 0, 0, $month, $day - 30, $year);
                $end_date = mktime(24, 0, 0, $month, $day - 1, $year);
            } else if($period == "lastyear") {
                $start_date = mktime(0, 0, 0, 1, 1, $year-2);
                $end_date = mktime(0, 0, 0, 1, 1, $year-1);
            } elseif($period == "thismonth") {
                $start_date = mktime(0, 0, 0, $month, 1, $year);
                $end_date = mktime(24, 0, 0, $month, $day - 1, $year);
            } elseif($period == "thisyear"){
                $start_date = mktime(0, 0, 0, 1, 1, $year);
                $end_date = mktime(24, 0, 0, $month, $day - 1, $year);
            } else { /* Because BAM reporting calculate data at 1AM for previous day we need to remove 1 hour */
                $start_date = mktime(0, 0, 0, $month - 1, 1, $year);
                $end_date = mktime(23, 0, 0, $month, 0, $year);
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

    /**
     * 
     * @param type $start
     * @param type $end
     * @return type
     */
    private function getDateSelect_customized($start, $end)
    {
        $time = time();
        $day = date("d",$time);
        $year = date("Y",$time);
        $month = date("m",$time);
        $end_time = mktime(0, 0, 0, $month, $day, $year);
        if ($end != "") {
            list($m,$d,$y) = explode('/',$end);
            $end = mktime(0, 0, 0, $m, $d, $y);
            if ($end < $end_time) {
                $end_time = $end;
            }
        }
        list($m,$d,$y) = explode('/',$start);
        $start_time = mktime(0, 0, 0, $m, $d, $y);

        return (array($start_time, $end_time));
    }

    /**
     * 
     * @param type $start
     * @param type $end
     * @param type $reportTimePeriod
     * @return type
     */
    private function getTotalTimeFromInterval($start, $end, $reportTimePeriod)
    {
        $period_choice = (isset($_POST["period_choice"])) ? $_POST["period_choice"] : "";
        $period = (isset($_POST["period"])) ? $_POST["period"] : "";
        $period = (isset($_GET["period"])) ? $_GET["period"] : $period;
        
        
        $one_day_real_duration = 60 * 60 * 24;
        $totalTime = 0;
        $reportTime = 0;
        $day_duration =  mktime($reportTimePeriod["report_hour_end"], $reportTimePeriod["report_minute_end"], 0, 0, 0, 0)
                                        - mktime($reportTimePeriod["report_hour_start"], $reportTimePeriod["report_minute_start"], 0, 0, 0, 0);
        if ($period_choice == "custom" || ($period_choice != "custom" && $period == "")) {
             $end += 86400;
        }
        while ($start < $end) {
            if (isset($reportTimePeriod["report_".date("l", $start)]) && $reportTimePeriod["report_".date("l", $start)]) {
                $reportTime += $day_duration;
            }
            $totalTime += $day_duration;
            $start += $one_day_real_duration;
        }
        $tab = array("totalTime" => $totalTime, "reportTime" => $reportTime);
        return $tab;
    }


    /**
     * Returns period to report
     * 
     * @return type
     */
    public function getPeriodToReport()
    {
        $period = (isset($_POST["period"])) ? $_POST["period"] : "";
        $period = (isset($_GET["period"])) ? $_GET["period"] : $period;
        $period_choice = (isset($_POST["period_choice"])) ? $_POST["period_choice"] : "";
        
        $end_date = 0;
        $start_date = 0;
        $start_date = (isset($_POST["StartDate"])) ? $_POST["StartDate"] : "";
        $start_date = (isset($_GET["start"])) ? $_GET["start"] : $start_date;
        $end_date = (isset($_POST["EndDate"])) ? $_POST["EndDate"] : "";
        $end_date = (isset($_GET["end"])) ? $_GET["end"] : $end_date;
        $interval = array(0, 0);
        if (($period_choice == "custom" && $start_date != "") || ($period == "" && $start_date != "")) {
            $interval = $this->getDateSelect_customized($start_date, $end_date);
        } else {
            if($period == "") {
                $period = "yesterday";
            }
            $interval = $this->getDateSelect_predefined($period);
        }
        $start_date = $interval[0];
        $end_date = $interval[1];

        return(array($start_date,$end_date));
    }

    /**
     * 
     * @param type $time
     * @param type $reportTimePeriod
     * @return string
     */
    private function getTimeString($time, $reportTimePeriod)
    {
        $min = 60;
        $hour = $min * 60;
        $day = mktime($reportTimePeriod["report_hour_end"], $reportTimePeriod["report_minute_end"], 0, 0, 0, 0)
                        - mktime($reportTimePeriod["report_hour_start"], $reportTimePeriod["report_minute_start"], 0, 0, 0, 0);
        $week = 0;
        if (isset($reportTimePeriod["report_Monday"])  && $reportTimePeriod["report_Monday"]) {
            $week ++;
        }
        if (isset($reportTimePeriod["report_Tuesday"])  && $reportTimePeriod["report_Tuesday"]) {
            $week ++;
        }
        if (isset($reportTimePeriod["report_Wednesday"])  && $reportTimePeriod["report_Wednesday"]) {
            $week ++;
        }
        if (isset($reportTimePeriod["report_Thursday"])  && $reportTimePeriod["report_Thursday"]) {
            $week ++;
        }
        if (isset($reportTimePeriod["report_Friday"])  && $reportTimePeriod["report_Friday"]) {
            $week ++;
        }
        if (isset($reportTimePeriod["report_Saturday"])  && $reportTimePeriod["report_Saturday"]) {
            $week ++;
        }
        if (isset($reportTimePeriod["report_Sunday"])  && $reportTimePeriod["report_Sunday"]) {
            $week ++;
        }
        $week *= $day;
        $year = $week * 52;
        $month = $year / 12;
        $str = "";
        if ($hour && $time / $hour >= 1) {
            $str .= floor($time / $hour)."h ";
            $time = $time % $hour;
        }
        if ($min && $time / $min >= 1) {
            $str .= floor($time / $min)."m ";
            $time = $time % $min;
        }
        if ($time) {
            $str .=  $time."s";
        }
        return $str;
    }

    /**
     * 
     * @return type
     */
    private function getServicesStatsValueName()
    {
        return (array("OK_T", "WARNING_T", "WARNING_A", "CRITICAL_T", "CRITICAL_A", "UNKNOWN_T", "UNKNOWN_A", "UNDETERMINED_T",
                                    "OK_TP", "WARNING_TP", "CRITICAL_TP", "UNKNOWN_TP", "UNDETERMINED_TP"));
    }

    /**
     * 
     * @param type $reportTimePeriod
     * @return string
     */
    private function getReportDaysStr($reportTimePeriod)
    {
        $tab = array("Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday");
        $str = "";
        foreach ($tab as $key => $value) {
            if (isset($reportTimePeriod["report_".$value]) && $reportTimePeriod["report_".$value]) {
                if ($str != "") {
                    $str .= ", ";
                }
                $str .= "'".$value."'";
            }
        }
        if ($str == "") {
            $str = "NULL";
        }
        return $str;
    }

    /**
     * Retrieves BA Logs
     * 
     * @param type $bam_id
     * @param type $start_date
     * @param type $end_date
     * @param type $reportTimePeriod
     * @return string
     */
    public function getbaLogInDb($bam_id, $start_date, $end_date, $reportTimePeriod)
    {
        $status = array("OK", "WARNING", "CRITICAL", "UNKNOWN", "UNDETERMINED");

        foreach($this->getServicesStatsValueName() as $name) {
            $serviceStats[$name] = 0;
        }
        $days_of_week = $this->getReportDaysStr($reportTimePeriod);
        /*
         * Getting stats for each status
         */
        $rq = "SELECT sum(`available`) as OK_T,
                sum(`degraded`) as WARNING_T, sum(`alert_degraded_opened`) as WARNING_A,
                sum(`unavailable`) as CRITICAL_T, sum(`alert_unavailable_opened`) as CRITICAL_A
                FROM `mod_bam_reporting_ba_availabilities`
                WHERE ba_id ='".$bam_id."' AND `time_id` BETWEEN ".$start_date." AND ".$end_date."
                              AND DATE_FORMAT( FROM_UNIXTIME( `time_id`), '%W') IN (".$days_of_week.")
                AND timeperiod_is_default = 1
                GROUP BY `ba_id`";

        $res = & $this->_dbO->query($rq);

        if ($row = $res->fetchRow()) {
            $serviceStats = $row;
        }
        $timeTab = $this->getTotalTimeFromInterval($start_date, $end_date, $reportTimePeriod);
        if (!$timeTab["reportTime"]) {
            foreach ($status as $key => $value) {
                $serviceStats[$value."_T"] = 0;
            }
        }
            $serviceStats["UNDETERMINED_T"] = $timeTab["reportTime"] - $serviceStats["OK_T"] - $serviceStats["WARNING_T"] - $serviceStats["CRITICAL_T"];

        /*
         * Calculate percentage of time (_TP => Total time percentage) for each status
         */
        $serviceStats["TOTAL_TIME"] = $serviceStats["OK_T"] + $serviceStats["WARNING_T"]
									+ $serviceStats["CRITICAL_T"] + $serviceStats["UNDETERMINED_T"];
        $time = $serviceStats["TOTAL_TIME"];
        foreach ($status as $key => $value) {
            if ($time) {
                $serviceStats[$value."_TP"] = round($serviceStats[$value."_T"] / $time * 100, 2);
            } else {
                $serviceStats[$value."_TP"] = 0;
            }
        }

        /*
         * The same percentage (_MP => Mean Time percentage) is calculated ignoring undetermined time
         */
        $serviceStats["MEAN_TIME"] = $serviceStats["OK_T"] + $serviceStats["WARNING_T"] + $serviceStats["CRITICAL_T"];
        $time = $serviceStats["MEAN_TIME"];
        if ($serviceStats["MEAN_TIME"] <= 0) {
            foreach ($status as $key => $value) {
                if ($value != "UNDETERMINED" && $value != "UNKNOWN") {
                    $serviceStats[$value."_MP"] = 0;
                }
            }
        } else {
            foreach ($status as $key => $value) {
                if ($value != "UNDETERMINED" && $value != "UNKNOWN") {
                    $serviceStats[$value."_MP"] = round($serviceStats[$value."_T"] / $time * 100, 2);
                }
            }
        }
        /*
         * Format time for each status (_TF => Time Formated), mean time and total time
         */
        $serviceStats["MEAN_TIME_F"] = $this->getTimeString($serviceStats["MEAN_TIME"], $reportTimePeriod);
        foreach ($status as $key => $value) {
            $serviceStats[$value."_TF"] = $this->getTimeString($serviceStats[$value."_T"], $reportTimePeriod);
        }

        $serviceStats["TOTAL_ALERTS"] = $serviceStats["WARNING_A"] + $serviceStats["CRITICAL_A"];
        /*
         * Getting number of day reported
         */
        $serviceStats["TOTAL_TIME_F"] = ($serviceStats["TOTAL_TIME"] / 86400)."d";
        return $serviceStats;
    }
}
