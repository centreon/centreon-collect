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

use \CentreonExport\Icon;
use \CentreonExport\DBManager;

class HostTemplate
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
     * @var int identifiant of host template 
     */
    protected $Host_id;

    /**
     *
     * @var int Type of link between host template and plugin
     *  0 => exclude
     *  1 => include
     */
    protected $Status;
    
    /**
     * 
     * @param int $val
     * @return \HostTemplate
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
     * @return \HostTemplate
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
    public function getStatus()
    {
        return $this->Status;
    }

    /**
     * 
     * @param string $val
     * @return \HostTemplate
     */
    public function setStatus($val)
    {
        $this->Status = $val;
        return $this;
    }

    /**
     * 
     * @param int $val
     * @return \HostTemplate
     */
    public function setHost_id($val)
    {
        $this->Host_id = $val;
        return $this;
    }

    /**
     * 
     * @return int
     */
    public function getHost_id()
    {
        return $this->Host_id;
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
     * This method return list of host linked with the plugin
     * 
     * @param int $iId identifiant of plugin
     * @return array
     */
    public function getHostTemplateByPlugin($iId)
    {
        $aDatas = array();
        if (empty($iId)) {
            return $aDatas;
        }
        $sQuery = "SELECT * FROM `mod_export_hostTpl` WHERE plugin_id = :plugin_id";
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
     * 
     * @param int $iId
     * @return array
     */
    public function getHostTemplateParametersByPlugin($iId)
    {
        $hostIncludedParametersQuery = "SELECT "
            . "`pluginpack_id`, `host_id`, `discovery_protocol`, `discovery_command`, `discovery_validator`, "
            . "`discovery_documentation` "
            . "FROM `mod_export_pluginpack_host` "
            . "WHERE `pluginpack_id` = :plugin_id";

        $sth = $this->db->db->prepare($hostIncludedParametersQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        $sth->execute();
        $aData = $sth->fetchAll(\PDO::FETCH_ASSOC);

        $aHostTemplateFinal = array();
        foreach ($aData as $myData) {
            $hostId = $myData['host_id'];
            unset($myData['host_id']);
            unset($myData['pluginpack_id']);
            $aHostTemplateFinal[$hostId] = $myData;
        }

        return $aHostTemplateFinal;
    }

    /**
     * This method delete list of host linked with the plugin
     * 
     * @param int $iIdPlugin identifiant of plugin
     */
    private function deleteHostTemplateByPlugin($iIdPlugin)
    {
        if (empty($iIdPlugin)) {
            return false;
        }
        $sQuery = 'DELETE FROM `mod_export_hostTpl` WHERE plugin_id = :plugin_id ';
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
        try {
            $sth->execute();
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }
    }

    /**
     * 
     * @param type $iIdPlugin
     * @return boolean
     */
    private function deleteHostTemplateParametersByPlugin($iIdPlugin)
    {
        if (empty($iIdPlugin)) {
            return false;
        }
        $sQuery = 'DELETE FROM `mod_export_pluginpack_host` WHERE pluginpack_id = :plugin_id ';
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
        try {
            $sth->execute();
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }
    }

    /**
     * This method add link the list host with the plugin
     * 
     * @param int $iIdPlugin Identifiant of plugin
     * @param array $aHostTemplate List of host template
     * If the status = 1 The host is included
     * If the status = 0 The host is excluded
     * @return boolean
     */
    public function addHostTemplateInPlugin($iIdPlugin, $aHostTemplate, $aHostTemplateParameters)
    {
        if (empty($iIdPlugin)) {
            return false;
        }

        $this->deleteHostTemplateByPlugin($iIdPlugin);
        $this->deleteHostTemplateParametersByPlugin($iIdPlugin);

        if (count($aHostTemplate) == 0) {
            return false;
        }

        $sQuery = 'INSERT INTO `mod_export_hostTpl` (plugin_id, host_id, status) VALUES (:plugin_id, :host_id, :status)';
        $sth = $this->db->db->prepare($sQuery);

        foreach ($aHostTemplate as $key => $value) {
            $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
            $sth->bindParam(':host_id', $value['host_id'], \PDO::PARAM_INT);
            $sth->bindParam(':status', $value['status'], \PDO::PARAM_STR);
            try {
                $sth->execute();
            } catch (\PDOException $e) {
                echo "Error " . $e->getMessage();
            }
            if ($value['status'] == 1) {
                $this->addHostTemplateParametersInPlugin($iIdPlugin, $value['host_id'], $aHostTemplateParameters[$value['host_id']]);
            }
        }
    }

    /**
     * 
     * @param int $iIdPlugin
     * @param int $iIdHostTemplate
     * @param array $aParams
     */
    private function addHostTemplateParametersInPlugin($iIdPlugin, $iIdHostTemplate, $aParams)
    {
        $sHostTemplateParametersInsertQuery = "INSERT INTO `mod_export_pluginpack_host` ";
        $sHostTemplateParametersInsertQuery .= "(`pluginpack_id`, `host_id`, `discovery_protocol`, `discovery_command`, `discovery_validator`, `discovery_documentation`) ";
        $sHostTemplateParametersInsertQuery .= "VALUES (:pluginpack_id, :host_id, :discovery_protocol, :discovery_command, :discovery_validator, :discovery_documentation)";

        $sth = $this->db->db->prepare($sHostTemplateParametersInsertQuery);
        $sth->bindParam(':pluginpack_id', $iIdPlugin, \PDO::PARAM_INT);
        $sth->bindParam(':host_id', $iIdHostTemplate, \PDO::PARAM_INT);
        $sth->bindParam(':discovery_protocol', $aParams['discovery_protocol'], \PDO::PARAM_STR);
        $sth->bindParam(':discovery_command', $aParams['discovery_command'], \PDO::PARAM_STR);
        $sth->bindParam(':discovery_validator', $aParams['discovery_category'], \PDO::PARAM_STR);
        $sth->bindParam(':discovery_documentation', $aParams['documentation'], \PDO::PARAM_STR);

        try {
            $sth->execute();
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }
    }

    /**
     * This method return the number of included host linked with plugin
     * 
     * @param int $iId Identifiant of plugin
     * @return array
     */
    public function getNbHostTemplateByPlugin($iId)
    {
        if (empty($iId)) {
            return 0;
        }
        $sQuery = 'SELECT count(*) as nb FROM `mod_export_hostTpl` WHERE status = "1" and plugin_id = :plugin_id';
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iId, \PDO::PARAM_INT);
        $sth->execute();
        $aData = $sth->fetch(\PDO::FETCH_ASSOC);
        return $aData['nb'];
    }

    public function getDetail($iHostId)
    {
        $aDetail = array();
        if (empty($iHostId)) {
            return $aDetail;
        }

        $sSql = "SELECT host_name, host_alias, host_comment, host_max_check_attempts, host_check_interval, command_command_id,
                    host_retry_check_interval, host_active_checks_enabled, host_passive_checks_enabled, host_checks_enabled
                    FROM host
                    WHERE host_id = :host_id";

        $sth = $this->db->db->prepare($sSql);
        $sth->bindParam(':host_id', $iHostId, \PDO::PARAM_INT);
        $sth->execute();
        $aData = $sth->fetchAll(\PDO::FETCH_ASSOC);
        $sParent = $this->getParent($iHostId);      

        foreach ($aData as $myData) {
            $aDetail = array(
                'host_name' => $myData['host_name'],
                'host_parent' => $sParent,
                'host_alias' => $myData['host_alias'],
                'host_comment' => cleanString($myData['host_comment']),
                'host_macro' => $this->getMacros($iHostId),
                'command_command_id' => $myData['command_command_id'],
                'host_service' => $this->getServiceTemplate($iHostId),
                'host_max_check_attempts' => $myData['host_max_check_attempts'],
                'host_check_interval' => $myData['host_check_interval'],
                'host_retry_check_interval' => $myData['host_retry_check_interval'],
                'host_active_checks_enabled' => $myData['host_active_checks_enabled'],
                'host_passive_checks_enabled' => $myData['host_passive_checks_enabled'],
                'host_checks_enabled' => $myData['host_checks_enabled'],
                'icon' => $this->getIcon($iHostId)
            );
        }

        return $aDetail;
    }
    
    /**
     * 
     * @param type $iHostId
     * @return array
     */
    private function getMacros($iHostId)
    {
        $aMacro = array();
        if (empty($iHostId)) {
            return $aMacro;
        }

        $sSql = "SELECT host_macro_name, host_macro_value
                    FROM on_demand_macro_host
                    WHERE host_host_id = :host_id ORDER BY macro_order ASC";

        $sth = $this->db->db->prepare($sSql);
        $sth->bindParam(':host_id', $iHostId, \PDO::PARAM_INT);
        $sth->execute();
        $aData = $sth->fetchAll(\PDO::FETCH_ASSOC);

        foreach ($aData as $myData) {
            $aMacro[] = array(
                'key' => $myData['host_macro_name'],
                'value' => $myData['host_macro_value']
            );
        }

        return $aMacro;
    }

    /**
     * 
     * @param type $iHostId
     * @return array
     */
    private function getServiceTemplate($iHostId)
    {     
        $aServiceTpl = array();
        if (empty($iHostId)) {
            return $aServiceTpl;
        }

        $queryService = "SELECT DISTINCT s.service_id, s.service_description "
            . "FROM service s JOIN host_service_relation hsr ON hsr.service_service_id = s.service_id"
            . " WHERE s.service_register = '0' "
            . " AND hsr.host_host_id = :host_host_id "
            . " ORDER BY s.service_description desc";

        $sth = $this->db->db->prepare($queryService);
        $sth->bindParam(':host_host_id', $iHostId, \PDO::PARAM_INT);
        $sth->execute();
        $aServiceTpl = $sth->fetchAll(\PDO::FETCH_COLUMN|\PDO::FETCH_UNIQUE);

        return $aServiceTpl;
    }

    /**
     * 
     * @param type $iHostId
     * @return string
     */
    private function getParent($iHostId)
    {
        $sName = "";
        if (empty($iHostId)) {
            return $sName;
        }

        $queryParent = "SELECT host_name "
            . "FROM host_template_relation JOIN host ON host_id = host_tpl_id "
            . " WHERE host_host_id = :host_host_id "
            . " ORDER BY `order` asc";

        $sth = $this->db->db->prepare($queryParent);
        $sth->bindParam(':host_host_id', $iHostId, \PDO::PARAM_INT);
        $sth->execute();
        while ($row = $sth->fetch()) {
            $sName[] = trim($row['host_name']);
        }
        return $sName;
    }

    /**
     * 
     * @param type $iHostId
     * @return string
     */
    private function getIcon($iHostId)
    {
        $sIcon = "";

        if (empty($iHostId)) {
            return $sIcon;
        }

        $oIcon = new Icon();
        $sDefaultDirectory = $oIcon->getDefaultDirectory();
        
        $queryParent = "SELECT dir_name, img_path
                        FROM view_img vi JOIN extended_host_information ON ehi_icon_image = img_id
                        LEFT JOIN view_img_dir_relation vidr ON vi.img_id = vidr.img_img_id 
                        LEFT JOIN view_img_dir vid ON vid.dir_id = vidr.dir_dir_parent_id 
                        WHERE  host_host_id = :host_host_id";

        $sth = $this->db->db->prepare($queryParent);
        $sth->bindParam(':host_host_id', $iHostId, \PDO::PARAM_INT);
        $sth->execute();
        $row = $sth->fetch();
        if ($row['dir_name'] && $row['img_path']) {
            $sEle = trim($row['dir_name']). DIRECTORY_SEPARATOR .trim($row['img_path']);
            $sIcon =  $sDefaultDirectory . $sEle;
        }

        return $sIcon;
    }
}
