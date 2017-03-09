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

class Dependency
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
    protected $Dependencies_id;

    /**
     *
     * @var int identifiant of plugin 
     */
    protected $Dependencies_plugin_id;

    /**
     *
     * @var string dependencies name
     */
    protected $Dependencies_name;

    /**
     *
     * @var string dependencies version
     */
    protected $Dependencies_version;

    /**
     * 
     * @param int $val
     * @return \Dependencies
     */
    public function setDependencies_id($val)
    {
        $this->Dependencies_id = $val;
        return $this;
    }

    /**
     * 
     * @return int
     */
    public function getDependencies_id()
    {
        return $this->Dependencies_id;
    }

    /**
     * 
     * @param int $val
     * @return \Dependencies
     */
    public function setDependencies_plugin_id($val)
    {
        $this->Dependencies_plugin_id = $val;
        return $this;
    }

    /**
     * 
     * @return int
     */
    public function getDependencies_plugin_id()
    {
        return $this->Dependencies_plugin_id;
    }

    /**
     * 
     * @return string
     */
    public function getDependencies_name()
    {
        return $this->Dependencies_name;
    }

    /**
     * 
     * @param string $val
     * @return \Dependencies
     */
    public function setDependencies_name($val)
    {
        $this->Dependencies_name = $val;
        return $this;
    }

    /**
     * 
     * @param string $val
     * @return \Dependencies
     */
    public function setDependencies_version($val)
    {
        $this->Dependencies_version = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getDependencies_version()
    {
        return $this->Dependencies_version;
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
     * This method return the list of Dependencies linked with plugin
     * 
     * @param int $iId Identifiant of plugin
     * @return array
     */
    public function getDependenciesByPlugin($iId)
    {
        $aDatas = array();
        if (empty($iId)) {
            return $aDatas;
        }

        $sQuery = "SELECT * FROM `mod_export_dependencies` WHERE dependencies_plugin_id = :plugin_id";
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        $sth->execute();
        $aData = $sth->fetchAll(\PDO::FETCH_ASSOC);

        foreach ($aData as $element => $valeur) {
            $aDatas[] = array(
                'name' => $valeur['dependencies_name'],
                'version' => $valeur['dependencies_version']
                );
        }

        return $aDatas;
    }

    /**
     * This method delete list of Dependencies linked with the plugin
     * 
     * @param int $iIdPlugin Identifiant of plugin
     */
    private function deleteDependenciesByPlugin($iIdPlugin)
    {
        if (empty($iIdPlugin)) {
            return false;
        }
        $sQuery = 'DELETE FROM `mod_export_dependencies` WHERE dependencies_plugin_id = :plugin_id ';
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
        try {
            $sth->execute();
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }
    }

    /**
     * This method add link the list Dependencies with the plugin
     * 
     * @param int $iIdPlugin Identifiant of plugin
     * @param array $aDependencies List of dependencies
     * @return boolean
     */
    public function addDependenciesInPlugin($iIdPlugin, $aDependencies)
    {
        if (empty($iIdPlugin)) {
            return false;
        }

        $this->deleteDependenciesByPlugin($iIdPlugin);
        $sQuery = 'INSERT INTO `mod_export_dependencies` (dependencies_plugin_id, dependencies_name, dependencies_version) VALUES (:plugin_id, :name, :version)';
        $sth = $this->db->db->prepare($sQuery);

        foreach ($aDependencies as $key => $value) {
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
     * This method return the list of Dependencies linked with plugin formatted for forms
     * 
     * @param int $iIdPlugin Identifiant of plugin
     * @return array
     */
    public function getListByPlugin($iIdPlugin)
    {
        $aReturn = array();
        $i = 0;

        if (!isset($_REQUEST['nameDependency']) && $iIdPlugin) {
            $sQuery = "SELECT * FROM `mod_export_dependencies` WHERE dependencies_plugin_id = :plugin_id";
            $sth = $this->db->db->prepare($sQuery);
            $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
            $sth->execute();
            $aData = $sth->fetchAll(\PDO::FETCH_ASSOC);
            foreach ($aData as $element => $valeur) {
                $aReturn[$i]['nameDependency_#index#'] = $valeur['dependencies_name'];
                $aReturn[$i]['versionDependency_#index#'] = $valeur['dependencies_version'];
                $i++;
            }
        } elseif (isset($_REQUEST['nameDependency'])) {
            foreach ($_REQUEST['nameDependency'] as $key => $val) {
                $index = $i;
                $aReturn[$index]['nameDependency_#index#'] = $val;
                $aReturn[$index]['versionDependency_#index#'] = isset($_REQUEST['versionDependency'][$key]) ? $_REQUEST['versionDependency'][$key] : null;
                $i++;
            }
        }
        return $aReturn;
    }
}
