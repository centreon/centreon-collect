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

class ServiceTemplate
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
     * @var int identifiant of service template 
     */
    protected $Service_id;

    /**
     *
     * @var int Type of link between service template and plugin
     *  0 => exclude
     *  1 => include
     */
    protected $Status;
    
    /**
     *
     * @var string Discovery command
     */
    protected $Discovery_command;
    
    /**
     * 
     * @param int $val
     * @return \ServiceTemplate
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
     * @return \ServiceTemplate
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
     * @return string
     */
    public function getDiscovery_command()
    {
        return $this->Discovery_command;
    }

    /**
     * 
     * @param string $val
     * @return \ServiceTemplate
     */
    public function setDiscovery_command($val)
    {
        $this->Discovery_command = $val;
        return $this;
    }

    /**
     * 
     * @param int $val
     * @return \ServiceTemplate
     */
    public function setService_id($val)
    {
        $this->Service_id = $val;
        return $this;
    }

    /**
     * 
     * @return int
     */
    public function getService_id()
    {
        return $this->Service_id;
    }
    
    /**
     * 
     * @return string
     */
    public function getStatus()
    {
        return $this->Status;
    }

    /**
     * 
     * @param string $val
     * @return \ServiceTemplate
     */
    public function setStatus($val)
    {
        $this->Status = $val;
        return $this;
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
     * This method return list of service linked with the plugin
     * 
     * @param int $iId Identifiant of plugin
     * @return array
     */
    public function getServiceTemplateByPlugin($iId)
    {
        $aDatas = array();
        if (empty($iId)) {
            return $aDatas;
        }

        $sQuery = "SELECT * FROM `mod_export_serviceTpl` WHERE plugin_id = :plugin_id";
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        $sth->execute();
        $aData = $sth->fetchAll(\PDO::FETCH_ASSOC);

        foreach ($aData as $element => $valeur) {
            $obj = new self();
            foreach ($valeur as $cle => $ele) {
                $obj->__set($cle, $ele);
            }
            $aDatas[$obj->Id] = $obj;
        }

        return $aDatas;
    }

    /**
     * This method delete list of service linked with the plugin
     * 
     * @param int $iIdPlugin Identifiant of plugin
     */
    private function deleteServiceTemplateByPlugin($iIdPlugin)
    {
        if (empty($iIdPlugin)) {
            return false;
        }
        $sQuery = 'DELETE FROM `mod_export_serviceTpl` WHERE plugin_id = :plugin_id ';
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
        try {
            $sth->execute();
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }
    }

    /**
     * This method add link the list service with the plugin
     * 
     * @param int $iIdPlugin Identifiant of plugin
     * @param array $aServiceTemplate List of service template
     * If the status = 1 The service is included
     * If the status = 0 The service is excluded
     * @return boolean
     */
    public function addServiceTemplateInPlugin($iIdPlugin, $aServiceTemplate)
    {
        if (empty($iIdPlugin)) {
            return false;
        }
  
        $this->deleteServiceTemplateByPlugin($iIdPlugin);
        
        if (count($aServiceTemplate) == 0) {
            return false;
        }
        $sQuery = 'INSERT INTO `mod_export_serviceTpl` (plugin_id, service_id, status, discovery_command) VALUES (:plugin_id, :service_id, :status, :discovery_command)';
        $sth = $this->db->db->prepare($sQuery);

        foreach ($aServiceTemplate as $key => $value) {
            $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
            $sth->bindParam(':service_id', $value['service_id'], \PDO::PARAM_INT);
            $sth->bindParam(':status', $value['status'], \PDO::PARAM_STR);
            $sth->bindParam(':discovery_command', $value['discovery_command'], \PDO::PARAM_STR);
            try {
                $sth->execute();
            } catch (\PDOException $e) {
                echo "Error " . $e->getMessage();
            }
        }
    }

    /**
     * This method return the number of included service linked with plugin
     * 
     * @param int $iId Identifiant of plugin
     * @return array
     */
    public function getNbServiceTemplateByPlugin($iId)
    {
        if (empty($iId)) {
            return 0;
        }
        $sQuery = 'SELECT count(*) as nb FROM `mod_export_serviceTpl` WHERE status = "1" and plugin_id = :plugin_id';

        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        $sth->execute();
        $aData = $sth->fetch(\PDO::FETCH_ASSOC);

        return $aData['nb'];
    }
    
    /**
     * 
     * @param type $iServiceId
     * @return string
     */
    public function getIcon($iServiceId)
    {
        $sIcon = "";

        if (empty($iServiceId)) {
            return $sIcon;
        }

        $oIcon = new Icon();
        $sDefaultDirectory = $oIcon->getDefaultDirectory();
        
        $queryParent = "SELECT dir_name, img_path
                        FROM view_img vi JOIN extended_service_information ON esi_icon_image = img_id
                        LEFT JOIN view_img_dir_relation vidr ON vi.img_id = vidr.img_img_id 
                        LEFT JOIN view_img_dir vid ON vid.dir_id = vidr.dir_dir_parent_id 
                        WHERE  service_service_id = :service_service_id";

        $sth = $this->db->db->prepare($queryParent);
        $sth->bindParam(':service_service_id', $iServiceId, \PDO::PARAM_INT);
        $sth->execute();
        $row = $sth->fetch();
        if ($row['dir_name'] && $row['img_path']) {
            $sEle = trim($row['dir_name']). DIRECTORY_SEPARATOR .trim($row['img_path']);
            $sIcon =  $sDefaultDirectory . $sEle;
        }
        return $sIcon;
    }

    /**
     * This method get timeperiod name of a service
     *
     * @param type $iServiceId
     * @return string
     */

    public function getTimeperiod($iServiceId)
    {
        $sTimeperiod = "";

        if (empty($iServiceId)) {
            return $sIcon;
        }

        $query = "SELECT tp_name AS name FROM timeperiod, service WHERE tp_id = timeperiod_tp_id AND service_id = :service_service_id";
        $sth = $this->db->db->prepare($query);
        $sth->bindParam(':service_service_id', $iServiceId, \PDO::PARAM_INT);
        $sth->execute();
        $row = $sth->fetch();
        if ($row['name']) {
            $sTimeperiod = $row['name'];
        }
        return $sTimeperiod;
    }   
}
