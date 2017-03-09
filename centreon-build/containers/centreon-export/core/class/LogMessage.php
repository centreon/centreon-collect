<?php
/**
 * CENTREON
 *
 * Source Copyright 2005-2015 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */

namespace CentreonExport;

class logMessage {

    private $errorType;
    private $path;

    /*
     * Constructor
     */
    public function __construct()
    {
            $this->errorType = array();

            /*
             * Init log Directory
             */
            $this->path = "/var/log/centreon/";

            $this->errorType[1] = $this->path."/login.log";
            $this->errorType[2] = $this->path."/sql-error.log";
            $this->errorType[3] = $this->path."/ldap.log";
            $this->errorType[4] = $this->path."/pluginspacks.log";
    }

    /*
     * Function for writing logs
     */
    public function insertLog($id, $str, $print = 0)
    {
        /*
         * Construct alerte message
         */
        $string = date("Y-m-d H:i")."|$str";

        /*
         * Display error on Standard exit
         */
        if ($print) {
                print $str."\n";
        }

        /*
         * Replace special char
         */
        $string = str_replace("`", "", $string);
        $string = str_replace("*", "\*", $string);
        $string = str_replace('$', '\$', $string);

        /*
         * Check log file
         */
        if (!file_exists($this->errorType[$id])) {
            touch($this->errorType[$id]);
            chown($this->errorType[$id],'centreon');
            chgrp($this->errorType[$id],'centreon');
            chmod($this->errorType[$id],0664);

        }
        /*
         * write Error in log file.
         */
        file_put_contents($this->errorType[$id], $string . "\n", FILE_APPEND);
    }
}
