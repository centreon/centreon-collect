<?php
/**
 * CENTREON
 *
 * Source Copyright 2005-2016 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */

namespace CentreonExport;

use \CentreonExport\DBManager;

class Timeperiod
{
    /**
     *
     * @var type 
     */
    public $db;

    /**
     *
     * @var int identifiant of relation
     */
    protected $Id;

    /**
     *
     * @var int identifiant of plugin 
     */
    protected $Plugin_id;

    /**
     *
     * @return int
     */
    public function getId()
    {
        return $this->Id;
    }

    /**
     *
     * @param int $val
     * @return \Command
     */
    public function setPlugin_id($val)
    {
        $this->Plugin_id = $val;
        return $this;
    }

    /**
     *
     * @return int
     */
    public function getPlugin_id()
    {
        return $this->Plugin_id;
    }

    /**
     * Setter Proxy (slow)
     *
     * @param string $name
     * @param mixed  $value
     * @throws Exception
     * @return mixed
     */
    public function __set($name, $value)
    {
        $methodName = 'set' . ucfirst($name);
        if (method_exists($this, $methodName)) {
            return call_user_func(array($this, $methodName), $value);
        } else {
            throw new Exception('set : propriete inconnue ' . $methodName);
        }
    }


    /**
     * Getter Proxy (slow)
     *
     * @param string $name
     * @throws Exception
     * @internal param $value
     * @return mixed
     */
    public function __get($name)
    {
        $methodName = 'get' . ucfirst($name);
        if (method_exists($this, $methodName)) {
            return call_user_func(array($this, $methodName));
        } else {
            throw new Exception('get : propriete inconnue ' . $methodName);
        }
    }

    /**
     * 
     */
    public function __construct()
    {
        $this->db = new DBManager();
    }

    /**
     * This method return the list of all command linked with plugin
     *
     * @param int $iId Identifiant of plugin
     * @return array
     */
    public function getTimeperiodByPlugin($iId)
    {
        $aDatas = array();
        if (empty($iId)) {
            return $aDatas;
        }

        $sQuery = "(SELECT DISTINCT tp_name AS name, tp_alias AS alias, 
                tp_sunday AS sunday, 
                tp_monday AS monday, 
                tp_tuesday AS tuesday, 
                tp_wednesday AS wednesday, 
                tp_thursday AS thursday, 
                tp_friday AS friday, 
                tp_saturday AS saturday 
                FROM timeperiod 
                WHERE tp_id IN ( 
                    SELECT timeperiod_tp_id 
                    FROM host 
                    WHERE timeperiod_tp_id IS NOT NULL 
                    AND host_id IN ( 
                        SELECT host_id 
                        FROM mod_export_hostTpl 
                        WHERE plugin_id = :plugin_id  
                        AND status = '1' 
                        ) 
                    ) 
                ) 
                UNION 
                (SELECT DISTINCT tp_name AS name, tp_alias AS alias, 
                tp_sunday AS sunday, 
                tp_monday AS monday, 
                tp_tuesday AS tuesday, 
                tp_wednesday AS wednesday, 
                tp_thursday AS thursday, 
                tp_friday AS friday, 
                tp_saturday AS saturday 
                FROM timeperiod 
                WHERE tp_id IN ( 
                    SELECT timeperiod_tp_id 
                    FROM service 
                    WHERE timeperiod_tp_id IS NOT NULL 
                    AND service_id IN ( 
                        SELECT service_id 
                        FROM mod_export_serviceTpl 
                        WHERE plugin_id = :plugin_id 
                        AND status = '1' 
                        ) 
                    ) 
                );";
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        try {
            $sth->execute();
            $aData = $sth->fetchAll(\PDO::FETCH_ASSOC);
            foreach ($aData as $element => $value) {
		array_push($aDatas, array(
                    'name' => $value['name'],
                    'alias' => $value['alias'],
                    'sunday' => $value['sunday'],
                    'monday' => $value['monday'],
                    'tuesday' => $value['tuesday'],
                    'wednesday' => $value['wednesday'],
                    'thursday' => $value['thursday'],
                    'friday' => $value['friday'],
                    'saturday' => $value['saturday']
                ));
            }
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }


        return $aDatas;
    }
}
