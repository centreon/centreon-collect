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

use \CentreonExport\DBManager;

class Command
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
     * @var int identifiant of command
     */
    protected $Command_id;
    
    /**
     *
     * @var type 
     */
    protected $Name;
    
    /**
     *
     * @var type 
     */
    protected $Line;

    /**
     * 
     * @param int $val
     * @return \Command
     */
    public function setId($val)
    {
        $this->Id = $val;
        return $this;
    }

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
     * 
     * @param int $val
     * @return \Command
     */
    public function setCommand_id($val)
    {
        $this->Command_id = $val;
        return $this;
    }

    /**
     * 
     * @return int
     */
    public function getCommand_id()
    {
        return $this->Command_id;
    }
        
    /**
     * 
     * @param int $val
     * @return \Command
     */
    public function setName($val)
    {
        $this->Name = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getName()
    {
        return $this->Name;
    }
    
    /**
     * 
     * @param int $val
     * @return \Command
     */
    public function setLine($val)
    {
        $this->Line = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getLine()
    {
        return $this->Line;
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
            throw new \Exception('set : propriete inconnue ' . $methodName);
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
            throw new \Exception('get : propriete inconnue ' . $methodName);
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
    public function getAllCommandByPlugin($iId)
    {
        $aDatas = array();
        if (empty($iId)) {
            return $aDatas;
        }
        $sQuery = "(SELECT DISTINCT command_name AS name, command_line AS line, command_type AS type 
            FROM command 
            WHERE command_id IN ( 
                SELECT command_command_id 
                FROM host 
                WHERE command_command_id IS NOT NULL 
                AND host_id IN ( 
                    SELECT host_id 
                    FROM mod_export_hostTpl 
                    WHERE plugin_id = :plugin_id 
                    AND status = '1' 
                    ) 
                ) 
            ) 
            UNION 
            (SELECT DISTINCT command_name AS name, command_line AS line, command_type AS type 
            FROM command 
            WHERE command_id IN ( 
                SELECT command_command_id 
                FROM service 
                WHERE command_command_id IS NOT NULL 
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
            foreach ($aData as $element => $valeur) {
                $name = "";
                $line = "";
                $type = "";
                foreach ($valeur as $cle => $ele) {
                    if ($cle == "name") {$name = $ele;}
                    if ($cle == "line") {$line = $ele;}
                    if ($cle == "type") {$type = $ele;}
                }
                $aDatas[$name] = array($line => intval($type));
            }
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }

        return $aDatas;
    }

    /**
     * This method delete list of command linked with the plugin
     * 
     * @param int $iIdPlugin Identifiant of plugin
     */
    private function deleteCommandByPlugin($iIdPlugin)
    {
        if (empty($iIdPlugin)) {
            return false;
        }
        $sQuery = 'DELETE FROM `mod_export_command` WHERE plugin_id = :plugin_id ';
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
        try {
            $sth->execute();
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }
    }

    /**
     * This method add link the list command with the plugin
     * 
     * @param int $iIdPlugin Identifiant of plugin
     * @param array $aCommands List of command
     * @return boolean
     */
    public function addCommandInPlugin($iIdPlugin, $aCommands)
    {
        if (empty($iIdPlugin)) {
            return false;
        }

        $this->deleteCommandByPlugin($iIdPlugin);

        if (count($aCommands) == 0) {
            return false;
        }

        $sQuery = 'INSERT INTO `mod_export_command` (plugin_id, command_id) VALUES (:plugin_id , :command_id)';
        $sth = $this->db->db->prepare($sQuery);

        foreach ($aCommands as $value) {
            $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
            $sth->bindParam(':command_id', $value, \PDO::PARAM_INT);
            try {
                $sth->execute();
            } catch (\PDOException $e) {
                echo "Error " . $e->getMessage();
            }
        }
    }

    /**
     * This method return the number of included command linked with plugin
     * 
     * @param int $iId Identifiant of plugin
     * @return array
     */
    public function getNbCommandByPlugin($iId)
    {
        if (empty($iId)) {
            return 0;
        }

        # Get PP discovery commands
        $sQuery = "SELECT count(*) as nb FROM `mod_export_command` WHERE plugin_id = :plugin_id";
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        $sth->execute();
        $aData = $sth->fetch(\PDO::FETCH_ASSOC);

        # Get commands from services templates
        $sQuery = "SELECT count(*) AS nb FROM service WHERE command_command_id IS NOT NULL AND service_id IN (SELECT service_id FROM mod_export_serviceTpl WHERE plugin_id = :plugin_id AND status = '1')";
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        $sth->execute();
        $temp = $sth->fetch(\PDO::FETCH_ASSOC);
        $aData['nb'] += $temp['nb'];

        # Get commands from hosts templates
        $sQuery = "SELECT count(*) AS nb FROM host WHERE command_command_id IS NOT NULL and host_id IN (SELECT host_id FROM mod_export_hostTpl  WHERE plugin_id = :plugin_id AND status = '1')";
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        $sth->execute();
        $temp = $sth->fetch(\PDO::FETCH_ASSOC);
        $aData['nb'] += $temp['nb'];

        return $aData['nb'];
    }
}
