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

require_once ("DB.php");

class CentreonBam_Db
{
	protected $_dbType = "mysql";
	protected $_retry;
	protected $_privateDb;
	protected $_dsn;
	protected $_options;
	protected $_db;
	protected $_dbPort;

	/*
	 *  Constructor only accepts 1 parameter which can be :
	 *  - centreon or NULL
	 *  - centstorage
	 *  - ndo
	 */
    function __construct($db = "centreon", $retry = 3)
    {
		global $conf_centreon;
		$this->_db = $db;
		$this->_retry = $retry;
		$this->_options = array('debug' => 2,'portability' => DB_PORTABILITY_ALL ^ DB_PORTABILITY_LOWERCASE);
        $this->_dbPort = 3306;
		if (isset($conf_centreon["port"]) && $conf_centreon["port"] != "") {
            $this->_dbPort = $conf_centreon["port"];
        }
		switch (strtolower($db)) {
			case "centreon" :
				$this->connectToCentreon($conf_centreon);
				$this->connect();
				break;
			case "centstorage" :
				$this->connectToCentstorage($conf_centreon);
				$this->connect();
				break;
			case "ndo" :
				$this->connectToCentreon($conf_centreon);
				$this->connect();
				$this->connectToNDO($conf_centreon);
				$this->connect();
				break;
			case "default" :
				$this->connectToCentreon($conf_centreon);
				$this->connect();
				break;
		}
    }

    /*
     *  Get info to connect to Centreon DB
     */
    private function connectToCentreon($conf_centreon)
    {
		$this->_dsn = array(
	    	'phptype'  => $this->_dbType,
	    	'username' => $conf_centreon["user"],
	    	'password' => $conf_centreon["password"],
	    	'hostspec' => $conf_centreon["hostCentreon"].":".$this->_dbPort,
	    	'database' => $conf_centreon["db"],
		);
    }

    /*
     *  Get info to connect to Centstorage DB
     */
    private function connectToCentstorage($conf_centreon)
    {
    	$this->_dsn = array(
	    	'phptype'  => $this->_dbType,
	    	'username' => $conf_centreon["user"],
	    	'password' => $conf_centreon["password"],
	    	'hostspec' => $conf_centreon["hostCentstorage"].":".$this->_dbPort,
	    	'database' => $conf_centreon["dbcstg"],
		);
    }

    /*
     *  Get info to connect to NDO DB
     */
    private function connectToNDO($conf_centreon)
    {
		$result = $this->_privateDb->query("SELECT db_name, db_prefix, db_user, db_pass, db_host, db_port FROM cfg_ndo2db WHERE activate = '1' LIMIT 1");
		if (PEAR::isError($result)) {
			print "DB Error : ".$result->getDebugInfo()."<br />";
		}
		$confNDO = $result->fetchRow();
		unset($result);

		$this->_dsn = array(
	    	'phptype'  => $this->_dbType,
	    	'username' => $confNDO['db_user'],
	    	'password' => $confNDO['db_pass'],
	    	'hostspec' => $confNDO['db_host'].":".$confNDO['db_port'],
	    	'database' => $confNDO['db_name'],
		);
    }

    /*
     *  The connection is established here
     */
    public function connect()
    {
    	$this->_privateDb = DB::connect($this->_dsn, $this->_options);
		$i = 0;
		while (PEAR::isError($this->_privateDb) && ($i < $this->_retry)) {
			$this->_privateDb = DB::connect($this->_dsn, $this->_options);
			$i++;
		}
		$this->_privateDb->setFetchMode(DB_FETCHMODE_ASSOC);
        $this->_privateDb->query("SET NAMES 'utf8'");
    }

    /*
     *  Disconnection
     */
    public function disconnect()
    {
    	$this->_privateDb->disconnect();
    }

    public function toString()
    {
    	return $this->_privateDb->toString();
    }

    /*
     *  Query
     */
    public function query($query_string = null)
    {
    	global $oreon;

    	$res = $this->_privateDb->query($query_string);
    	if (PEAR::isError($res)) {
    	    throw new Exception($res->getMessage());
    	}
    	return $res;
    }

    /**
     * return number of rows
     *
     */
    public function numberRows()
    {
        $number = 0;
        $DBRESULT = $this->query("SELECT FOUND_ROWS() AS number");
        $data = $DBRESULT->fetchRow();
        if (isset($data["number"])) {
            $number = $data["number"];
        }
        return $number;
    }

    /**
     * Escape method
     *
     * @param string $str
     * @return string
     */
    public function escape($str)
    {
         return mysql_real_escape_string($str);
    }
}
