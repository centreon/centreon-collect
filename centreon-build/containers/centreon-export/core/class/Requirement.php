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

class Requirement
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
    protected $Requirement_id;

    /**
     *
     * @var int identifiant of plugin 
     */
    protected $Requirement_plugin_id;

    /**
     *
     * @var string Requirement name
     */
    protected $Requirement_name;

    /**
     *
     * @var string Requirement version
     */
    protected $Requirement_version;

    /**
     * 
     * @param int $val
     * @return \Requirement
     */
    public function setRequirement_id($val)
    {
        $this->Requirement_id = $val;
        return $this;
    }

    /**
     * 
     * @return int
     */
    public function getRequirement_id()
    {
        return $this->Requirement_id;
    }

    /**
     * 
     * @param int $val
     * @return \Requirement
     */
    public function setRequirement_plugin_id($val)
    {
        $this->Requirement_plugin_id = $val;
        return $this;
    }

    /**
     * 
     * @return int
     */
    public function getRequirement_plugin_id()
    {
        return $this->Requirement_plugin_id;
    }

    /**
     * 
     * @return string
     */
    public function getRequirement_name()
    {
        return $this->Requirement_name;
    }

    /**
     * 
     * @param string $val
     * @return \Requirement
     */
    public function setRequirement_name($val)
    {
        $this->Requirement_name = $val;
        return $this;
    }

    /**
     * 
     * @param string $val
     * @return \Requirement
     */
    public function setRequirement_version($val)
    {
        $this->Requirement_version = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getRequirement_version()
    {
        return $this->Requirement_version;
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
     * This method return the list of requirement linked with plugin
     * 
     * @param int $iId Identifiant of plugin
     * @return array
     */
    public function getRequirementByPlugin($iId)
    {
        $aDatas = array();
        if (empty($iId)) {
            return $aDatas;
        }

        $sQuery = "SELECT * FROM `mod_export_requirements` WHERE requirement_plugin_id = :plugin_id";
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        $sth->execute();
        $aData = $sth->fetchAll(\PDO::FETCH_ASSOC);

        foreach ($aData as $element => $valeur) {
            $aDatas[] = array(
                'name' => $valeur['requirement_name'],
                'version' => $valeur['requirement_version']
            );            
        }

        return $aDatas;
    }

    /**
     * This method delete list of requirement linked with the plugin
     * 
     * @param int $iIdPlugin Identifiant of plugin
     */
    private function deleteRequirementByPlugin($iIdPlugin)
    {
        if (empty($iIdPlugin)) {
            return false;
        }
        $sQuery = 'DELETE FROM `mod_export_requirements` WHERE requirement_plugin_id = :plugin_id ';
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
        try {
            $sth->execute();
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }
    }

    /**
     * This method add link the list requirement with the plugin
     * 
     * @param int $iIdPlugin Identifiant of plugin
     * @param array $aRequirement List of requirement
     * @return boolean
     */
    public function addRequirementInPlugin($iIdPlugin, $aRequirement)
    {
        if (empty($iIdPlugin)) {
            return false;
        }

        $this->deleteRequirementByPlugin($iIdPlugin);
        $sQuery = 'INSERT INTO `mod_export_requirements` (requirement_plugin_id, requirement_name, requirement_version) VALUES (:plugin_id, :name, :version)';
        $sth = $this->db->db->prepare($sQuery);

        foreach ($aRequirement as $key => $value) {
            $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
            $sth->bindParam(':name', $value['name'], \PDO::PARAM_STR);
            $sth->bindParam(':version', $value['version'], \PDO::PARAM_STR);
            try {
                $sth->execute();
            } catch (\PDOException $e) {
                echo "Error " . $e->getMessage();
            }
        }
    }

    /**
     * This method return the list of requirement linked with plugin formatted for forms
     * 
     * @param int $iIdPlugin Identifiant of plugin
     * @return array
     */
    public function getListByPlugin($iIdPlugin)
    {
        $aReturn = array();
        $i = 0;

        if (!isset($_REQUEST['nameRequirement']) && $iIdPlugin) {
            $sQuery = "SELECT * FROM `mod_export_requirements` WHERE requirement_plugin_id = :plugin_id";
            $sth = $this->db->db->prepare($sQuery);
            $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
            $sth->execute();
            $aData = $sth->fetchAll(\PDO::FETCH_ASSOC);
            foreach ($aData as $element => $valeur) {
                $aReturn[$i]['nameRequirement_#index#'] = $valeur['requirement_name'];
                $aReturn[$i]['versionRequirement_#index#'] = $valeur['requirement_version'];
                $i++;
            }
        } elseif (isset($_REQUEST['nameRequirement'])) {
            foreach ($_REQUEST['nameRequirement'] as $key => $val) {
                $index = $i;
                $aReturn[$index]['nameRequirement_#index#'] = $val;
                $aReturn[$index]['versionRequirement_#index#'] = isset($_REQUEST['versionRequirement'][$key]) ? $_REQUEST['versionRequirement'][$key] : null;
                $i++;
            }
        }
        return $aReturn;
    }
}
