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

use \CentreonExport\Factory;
use \CentreonExport\DBManager;
use \CentreonExport\Command;
use \CentreonExport\HostTemplate;
use \CentreonExport\ServiceTemplate;
use \CentreonExport\DiscoveryValidator;
use \CentreonExport\Description;
use \CentreonExport\Timeperiod;
use \CentreonExport\Requirement;
use \CentreonExport\Dependency;

class PluginPack
{
    /**
     *
     * @var type 
     */
    public $db;

    protected $exportFactory;
    protected $commandObject;
    protected $hostTemplateObject;
    protected $serviceTemplateObject;

    /**
     *
     * @var int identifiant of plugin 
     */
    protected $Plugin_id;

    /**
     *
     * @var string name of plugin 
     */
    protected $Plugin_name;
    
    /**
     *
     * @var string display name of plugin 
     */
    protected $Plugin_Display_Name;

    /**
     *
     * @var string status of plugin
     */
    protected $Plugin_status;

    /**
     *
     * @var string status_message Message to be displayed in a tooltip like 'deprecated by XXX'
     */
    protected $Plugin_status_message;

    /**
     *
     * @var string compatibility Permet de lister les versions/modèles avec lesquels le pack est compatible
     */
    protected $Plugin_compatibility;

    /**
     *
     * @var int number of of host template attached to the plugin 
     */
    protected $Plugin_nbHostTpl;
    
    /**
     *
     * @var array Containts the included host template 
     */
    protected $aHostIncluded = array();
    
    /**
     *
     * @var type 
     */
    protected $aHostIncludedParameters = array();


    /**
     *
     * @var array Containts the exculeded host template 
     */
    protected $aHostExcluded = array();
    
    /**
     *
     * @var type 
     */
    protected $aHostExcludedParameters = array();

    /**
     *
     * @var int number of of service template attached to the plugin 
     */
    protected $Plugin_nbServiceTpl;
    
    /**
     *
     * @var array Containts the included Service template 
     */
    protected $aServiceIncluded = array();
    
    /**
     *
     * @var type 
     */
    protected $aServiceIncludedParameters = array();
    
    /**
     *
     * @var array Containts the exculeded Service template 
     */
    protected $aServiceExcluded = array();
    
    /**
     *
     * @var type 
     */
    protected $aServiceExcludedParameters = array();


    /**
     *
     * @var array Containts the included command
     */
    protected $aCommand = array();
   
    /**
     *
     * @var array Containts the included timeperiod
     */
    protected $aTimeperiod = array(); 
    /**
     *
     * @var int timestamp of last update 
     */
    protected $Plugin_last_Upade;

    /**
     *
     * @var string path to the plugin
     */
    protected $Plugin_path;

    /**
     *
     * @var string slug to the plugin
     */
    protected $Plugin_slug;

    /**
     *
     * @var string icon to the plugin
     */
    protected $Plugin_icon;
    
    /**
     *
     * @var string icon to the plugin
     */
    protected $Plugin_icon_base64;

    /**
     *
     * @var string version to the plugin
     */
    protected $Plugin_version;

    /**
     *
     * @var string monitoring_procedure to the plugin
     */
    protected $Plugin_monitoring_procedure;

    /**
     * 
     * @var array  linked to the plugin
     */
    protected $Plugin_tags;

    /**
     * 
     * @var string Une catégorie est unique par plugin pack
     */
    protected $Plugin_discovery_category;

    /**
     * 
     * @var string Possible d'avoir des PP fournis par d'autres entreprises
     */
    protected $Plugin_author;

    /**
     * 
     * @var string email email associée à author
     */
    protected $Plugin_email;

    /**
     * 
     * @var string
     */
    protected $Plugin_website;

    /**
     * 
     * @var string
     */
    protected $Plugin_change_log;

    /**
     *
     * @var array 
     */
    protected $Plugin_description;

    /**
     * 
     * @var array Dependance vers d autre plugin pack
     */
    protected $Plugin_dependency;

    /**
     *
     * @var array Informatif 
     */
    protected $Plugin_requirement;
    
    /**
     *
     * @var int flag indicate if plugin is actived
     */
    protected $Plugin_flag;
    
    /**
     *
     * @var int time of generate plugin
     */
    protected $Plugin_time_generate;
    
    /**
     *
     * @var type 
     */
    protected $Plugin_parent;

    /**
     *
     * @var array
     */
    protected $aExportedServiceTemplates = array();


    public function getAExportedServiceTemplates()
    {
        return $this->aExportedServiceTemplates;
    }
    /**
     * 
     * @param int $val
     * @return \PluginPack
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
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_name($val)
    {
        $this->Plugin_name = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_name()
    {
        return $this->Plugin_name;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_Display_Name($val)
    {
        $this->Plugin_Display_Name = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_Display_Name()
    {
        return $this->Plugin_Display_Name;
    }
    
    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_change_log($val)
    {
        $this->Plugin_change_log = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_change_log()
    {
        return $this->Plugin_change_log;
    }
    
    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_slug($val)
    {
        $this->Plugin_slug = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_slug()
    {
        return $this->Plugin_slug;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_author($val)
    {
        $this->Plugin_author = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_author()
    {
        return $this->Plugin_author;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_email($val)
    {
        $this->Plugin_email = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_email()
    {
        return $this->Plugin_email;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_website($val)
    {
        $this->Plugin_website = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_website()
    {
        return $this->Plugin_website;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_status($val)
    {
        $this->Plugin_status = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_status()
    {
        return $this->Plugin_status;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_compatibility($val)
    {
        $this->Plugin_compatibility = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_compatibility()
    {
        return $this->Plugin_compatibility;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_status_message($val)
    {
        $this->Plugin_status_message = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_status_message()
    {
        return $this->Plugin_status_message;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_last_update($val)
    {
        $this->Plugin_last_update = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_last_update()
    {
        return $this->Plugin_last_update;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_path($val)
    {
        $this->Plugin_path = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_path()
    {
        return $this->Plugin_path;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_icon($val)
    {
        $this->Plugin_icon = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_icon()
    {
        return $this->Plugin_icon;
    }
    
    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_icon_base64($val)
    {
        $this->Plugin_icon_base64 = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_icon_base64()
    {
        return $this->Plugin_icon_base64;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_version($val)
    {
        $this->Plugin_version = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_version()
    {
        return $this->Plugin_version;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_monitoring_procedure($val)
    {
        $this->Plugin_monitoring_procedure = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_monitoring_procedure()
    {
        return $this->Plugin_monitoring_procedure;
    }

    /**
     * 
     * @param string $val
     * @return \PluginPack
     */
    public function setPlugin_discovery_category($val)
    {
        $this->Plugin_discovery_category = $val;
        return $this;
    }

    /**
     * 
     * @return string
     */
    public function getPlugin_discovery_category()
    {
        return $this->Plugin_discovery_category;
    }
    
    /**
     * 
     * @param type $val
     * @return \PluginPack
     */
    public function setPlugin_flag($val)
    {
        $this->Plugin_flag = $val;
        return $this;
    }

    /**
     * 
     * @return type
     */
    public function getPlugin_flag()
    {
        return $this->Plugin_flag;
    }
    
    /**
     * 
     * @param type $val
     * @return \PluginPack
     */
    public function setPlugin_time_generate($val)
    {
        $this->Plugin_time_generate = $val;
        return $this;
    }

    /**
     * 
     * @return type
     */
    public function getPlugin_time_generate()
    {
        return $this->Plugin_time_generate;
    }
    
    /**
     * 
     * @param type $aData
     * @return \PluginPack
     */
    public function setPlugin_requirement($aData)
    {
        $this->Plugin_requirement = (array) $aData;
        return $this;
    }

    /**
     * 
     * @return type
     */
    public function getPlugin_requirement()
    {
        if (!isset($this->Plugin_requirement) || !is_array($this->Plugin_requirement)) {
            $this->Plugin_requirement = array();
        }
        
        return $this->Plugin_requirement;
    }
    
    /**
     * 
     * @param type $aData
     * @return \PluginPack
     */
    public function setHostIncluded($aData)
    {
        $this->aHostIncluded = (array) $aData;
        return $this;
    }
    
    /**
     * 
     * @return type
     */
    public function getHostIncluded()
    {
        if (!isset($this->aHostIncluded) || !is_array($this->aHostIncluded)) {
            $this->aHostIncluded = array();
        }
        
        return $this->aHostIncluded;
    }

    /**
     * 
     * @param int $iId
     * @param string $sName
     */
    public function addHostIncluded($iId, $sName)
    {
        $this->aHostIncluded[$iId] = $sName;
        return $this;
    }
    
    /**
     * 
     * @param int $iId
     * @param array $aParam
     */
    public function setHostIncludedParameters($aParams)
    {
        $this->aHostIncludedParameters = $aParams;
        return $this;
    }
    
    /**
     * 
     * @return array
     */
    public function getHostIncludedParameters()
    {
        return $this->aHostIncludedParameters;
    }

    /**
     *
     * @global type $pearDB
     * @return array
     */
    public function getExportedHostTemplates()
    {
        global $pearDB;

        $aHostTemplates = array_keys($this->aHostIncluded);
        foreach ($this->aHostIncluded as $key => $hostIncluded) {
            $hostChain = $this->hostTemplateObject->getTemplateChain($key);

            foreach($hostChain as $hostTpl) {
                if (!in_array($hostTpl['host_id'], $aHostTemplates) && count($hostTpl) > 0) {
                    $aHostTemplates[] = $hostTpl['host_id'];
                }
            }
        }

        $finalHostTemplates = array();
        foreach($aHostTemplates as &$hostTemplate) {
            if (!isset($this->aHostExcluded[$hostTemplate])) {
                $finalHostTemplates[] = $hostTemplate;
            }
        }

        return $finalHostTemplates;
    }

    /**
     * 
     * @global type $pearDB
     * @return array
     */
    public function exportHostTemplates()
    {
        $exportedHostTemplates = $this->getExportedHostTemplates();

        $finalHostTemplates = array();
        foreach ($exportedHostTemplates as $exportedHostTemplate) {
            $finalHostTemplates[] = $this->prepareHostTemplateForExport($exportedHostTemplate);
        }

        return $finalHostTemplates;
    }

    /**
     * @global type $pearDB
     * @return array
     */
    public function exportServiceTemplates()
    {
        global $pearDB;

        $aServiceTemplates = array_unique(
            array_merge(
                array_keys($this->getAExportedServiceTemplates()),
                array_keys($this->aServiceIncluded)
            )
        );   

        $finalServiceTemplates = array();
        foreach($aServiceTemplates as $serviceTemplate) {
            $aList = array_reverse($this->serviceTemplateObject->getTemplatesChain($serviceTemplate));
           
            array_unshift($aList, $serviceTemplate);
            foreach ($aList as $value) {
                
                if (isset($this->aServiceExcluded[$value])) {
                    break;
                }
                $aPrepare = $this->prepareServiceTemplateForExport($value);
                $aa = array_column($finalServiceTemplates, 'name');

                if (!in_array($aPrepare['name'], $aa)) {
                    $finalServiceTemplates[] = $aPrepare;
                }
            }
        }
        
        return $finalServiceTemplates;

    }

    /**
     * 
     * @param int $hostTplId
     */
    private function prepareHostTemplateForExport($hostTplId)
    {
        global $pearDB;
        
        $oHostTpl = new HostTemplate();
        $aDetail = $oHostTpl->getDetail($hostTplId);

        $this->aExportedServiceTemplates = $this->aExportedServiceTemplates + $aDetail['host_service'];
        $discovery_command = "";
        if (!empty($this->aHostIncludedParameters[$hostTplId]['discovery_command'])) {
            $discovery_command = DiscoveryValidator::sliceDiscoveryValidator(
                $this->aHostIncludedParameters[$hostTplId]['discovery_command']
            );
        }
        
        $hostReturn = array(
            "name" => $aDetail['host_name'], 
            "parent_template" => $aDetail['host_parent'], 
            "alias" => $aDetail['host_alias'],
            "max_check_attempts" => intval($aDetail['host_max_check_attempts']),
            "check_interval" => intval($aDetail['host_check_interval']),
            "active_checks_enabled" => intval($aDetail['host_active_checks_enabled']),
            "passive_checks_enabled" => intval($aDetail['host_passive_checks_enabled']),
            "checks_enabled" => intval($aDetail['host_checks_enabled']),
            "macros" => array_values($aDetail['host_macro']),
            "services" => array_values($aDetail['host_service']),
            "command_name" => $aDetail['host_command'],
            "icon" => $aDetail['icon'],
            "discovery_validator" => $discovery_command
        );
        
        # prepare command name for exportcommand_id
        if (!empty($aDetail['command_command_id']) && $aDetail['command_command_id'] !== NULL) {
            $commandObject = new CentreonCommand($pearDB);
            $commandParameters = $commandObject->getParameters($aDetail['command_command_id'], array('command_name'));
            $hostReturn['command_name'] = $commandParameters['command_name'];
        }
        unset($aDetail['command_command_id']);

        if ($aDetail['host_retry_check_interval'] != NULL) {
            $hostReturn['host_retry_check_interval'] = intval($aDetail['host_retry_check_interval']);
        }

        return $hostReturn;
    }
    
     /**
     *
     * @global type $pearDB
     * @return array
     */
    private function  prepareServiceTemplateForExport($serviceId)
    {
        global $pearDB;

        $oService = new ServiceTemplate();
        $detail = $this->serviceTemplateObject->getParameters($serviceId,
            array("service_description as name",
                  "service_alias as alias",
                  "service_template_model_stm_id as parent_template",
                  "command_command_id as command_id", 
                  "service_comment as _comment",
                  "service_is_volatile as is_volatile",
                  "service_max_check_attempts as max_check_attempts",
                  "service_normal_check_interval as normal_check_interval",
                  "service_retry_check_interval as retry_check_interval",
                  "service_active_checks_enabled as active_checks_enabled",
                  "service_passive_checks_enabled as passive_checks_enabled"
                )
            );
        
        # cast interger
        $detail['is_volatile'] = intval($detail['is_volatile']);	
        $detail['max_check_attempts'] = intval($detail['max_check_attempts']);	
        $detail['normal_check_interval'] = intval($detail['normal_check_interval']);	
        $detail['retry_check_interval'] = intval($detail['retry_check_interval']);	
        $detail['active_checks_enabled'] = intval($detail['active_checks_enabled']);	
        $detail['passive_checks_enabled'] = intval($detail['passive_checks_enabled']);
	
        # prepare comment for export
        if (!empty($detail['_comment']) && $detail['_comment'] !== NULL) {
            $detail['_comment'] = cleanString($detail['_comment']);
        }
        
        $aMacros = $this->serviceTemplateObject->getCustomMacroInDb($serviceId);
        $detail['macros'] = array();
        foreach ($aMacros as $macro) {
            $detail['macros'][] = array(
                'key' => '$_SERVICE'.$macro['macroInput_#index#'].'$',
                'value' => $macro['macroValue_#index#'],
                'password' => ($macro['macroPassword_#index#'] == '1') ? true: false,
                'description' => $macro['macroDescription_#index#']
            );
        }

        # prepare command name for export
        if (!empty($detail['command_id']) && $detail['command_id'] !== NULL) {
            $commandParameters = $this->commandObject->getParameters($detail['command_id'], array('command_name'));
            $detail['command_name'] = $commandParameters['command_name'];
        }
        unset($detail['command_id']);

        # prepare parent service template for export
        if (!empty($detail['parent_template']) && $detail['parent_template'] !== NULL) {
            $serviceParameters = $this->serviceTemplateObject->getParameters($detail['parent_template'], array('service_description'));
            $detail['parent_template'] = $serviceParameters['service_description'];
        }

        # prepare discovery command for export
        $detail['discovery_list_mode'] = "";
        if (!empty($this->aServiceIncluded[$serviceId]['discovery_command'])) {
            $detail['discovery_list_mode'] = $this->aServiceIncluded[$serviceId]['discovery_command'];
        }

        # prepare service template icon for export        
        $detail['icon'] = $oService->getIcon($serviceId);

	# prepare service timeperiod for export
        $detail['timeperiod'] = $oService->getTimeperiod($serviceId);

        return $detail;  
    }
   
    /**
     * 
     * @param array $aData
     * @return \Plugins
     */
    public function setServiceIncluded($aData)
    {
        $this->aServiceIncluded = (array) $aData;
        return $this;
    }
    
    /**
     * 
     * @return array
     */
    public function getServiceIncluded()
    {
        if (!isset($this->aServiceIncluded) || !is_array($this->aServiceIncluded)) {
            $this->aServiceIncluded = array();
        }
        
        return $this->aServiceIncluded;
    }
    
    /**
     * 
     * @param int $iId
     * @param array $aParam
     */
    public function setServiceIncludedParameters($aParams)
    {
        $this->aServiceIncludedParameters = $aParams;
        return $this;
    }
    
    /**
     * 
     * @return array
     */
    public function getServiceIncludedParameters()
    {
        return $this->aServiceIncludedParameters;
    }

    /**
     * 
     * @param array $aParam
     */
    public function addServiceIncluded($aParam)
    {
        $this->aServiceIncluded[$aParam['id']] = $aParam;
    }
    
    /**
     * 
     * @param array $aData
     * @return \Plugins
     */
    public function setHostExcluded($aData)
    {
        $this->aHostExcluded = (array) $aData;
        return $this;
    }
    
    /**
     * 
     * @return array
     */
    public function getHostExcluded()
    {
        if (!isset($this->aHostExcluded) || !is_array($this->aHostExcluded)) {
            $this->aHostExcluded = array();
        }
        
        return $this->aHostExcluded;
    }

    /**
     * 
     * @param int $iId
     * @param string $sName
     */
    public function addHostExcluded($iId, $sName)
    {
        $this->aHostExcluded[$iId] = $sName;
    }
    
    /**
     * 
     * @param array $aData
     * @return \Plugins
     */
    public function setServiceExcluded($aData)
    {
        $this->aServiceExcluded = (array) $aData;
        return $this;
    }
    
    /**
     * 
     * @return array
     */
    public function getServiceExcluded()
    {
        if (!isset($this->aServiceExcluded) || !is_array($this->aServiceExcluded)) {
            $this->aServiceExcluded = array();
        }
        
        return $this->aServiceExcluded;
    }

    /**
     * 
     * @param int $iId
     * @param string $sName
     */
    public function addServiceExcluded($iId, $sName)
    {
        $this->aServiceExcluded[$iId] = $sName;
    }
   
    /**
     *
     * @param array $aData
     * @return \Plugins
     */
    public function setTimeperiod($aData)
    {
        $this->aTimeperiod['timeperiod'] = $aData;
        return $this;
    }
 
    /**
     *
     * @return array
     */
    public function getTimeperiod()
    {
        if (!isset($this->aTimeperiod) || !is_array($this->aTimeperiod)) {
            $this->aTimeperiod = array();
        }

        return $this->aTimeperiod;
    }

    /**
     *
     * @return array
     */
    public function getTimeperiodAsArray()
    {
        $aTimeperiodDetail = array();

        if (count($this->aTimeperiod) == 0) {
            return $aTimeperiodDetail;
        }

	return $this->aTimeperiod;
    }

    /**
     * 
     * @param array $aData
     * @return \Plugins
     */
    public function setCommand($aData)
    {
        $this->aCommand = $aData;
        return $this;
    }
    
    /**
     * 
     * @return array
     */
    public function getCommand()
    {
        if (!isset($this->aCommand) || !is_array($this->aCommand)) {
            $this->aCommand = array();
        }
        
        return $this->aCommand;
    }

    /**
     * 
     * @return array
     */
    public function getCommandAsArray()
    {
        $aCommandDetail = array();
        
        if (count($this->aCommand) == 0) {
            return $aCommandDetail;
        }
        
        foreach ($this->aCommand as $name => $item) {
            foreach ($item as $line => $type) {
                $aCommandDetail[] = array(
                    "name" => $name, 
                    "line" => addslashes(json_encode($line)),
                    "type" => $type
                );
            }
        }
        
        return $aCommandDetail;
    }
    
    /**
     * 
     * @param array $aData
     * @return \Plugins
     */
    public function setPlugin_tags($aData)
    {
        $this->Plugin_tags = $aData;
        return $this;
    }
    
    /**
     * 
     * @return array
     */
    public function getPlugin_tags()
    {
        if (!isset($this->Plugin_tags) || !is_array($this->Plugin_tags)) {
            $this->Plugin_tags = array();
        }
        
        return $this->Plugin_tags;
    }
    
    /**
     * 
     * @param array $aData
     * @return \Plugins
     */
    public function setPlugin_dependency($aData)
    {
        $this->Plugin_dependency = $aData;
        return $this;
    }
    
    /**
     * 
     * @return array
     */
    public function getPlugin_dependencies()
    {
        if (!isset($this->Plugin_dependency) || !is_array($this->Plugin_dependency)) {
            $this->Plugin_dependency = array();
        }
        
        return $this->Plugin_dependency;
    }
    
    /**
     * 
     * @param int $val
     */
    public function setPlugin_parent($val)
    {
        $this->Plugin_parent = $val;
    }
    
    /**
     * 
     * @return int
     */
    public function getPlugin_parent()
    {
        return $this->Plugin_parent;
    }

    /**
     *
     * @param array $aData
     * @return \Plugins
     */
    public function setPlugin_description($aData)
    {
        $this->Plugin_description = $aData;
        return $this;
    }

    /**
     *
     * @return array
     */
    public function getPlugin_description()
    {
        if (!isset($this->Plugin_description) || !is_array($this->Plugin_description)) {
            $this->Plugin_description = array();
        }

        return $this->Plugin_description;
    }

    /**
     *
     * @return array
     */
    public function getDescriptionAsArray()
    {
        $descriptions = array();

        foreach ($this->Plugin_description as $locale => $description) {
            $descriptions[] = array(
                "lang" => $locale,
                "value" => addslashes($description)
            );
        }

        return $descriptions;
    }
    
    /**
     *
     * @return array
     */
    public function getTagAsArray()
    {
        $tags = array();
        foreach ($this->Plugin_tags as $key =>  $tag) {
            $tags[] = $tag['text'];
        }

        return $tags;
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
    public function __construct($commandObject, $hostTemplateObject, $serviceTemplateObject)
    {
        $this->db = new DBManager();

        $this->exportFactory = new Factory();
        $this->commandObject = $commandObject;
        $this->hostTemplateObject = $hostTemplateObject;
        $this->serviceTemplateObject = $serviceTemplateObject;
    }

    /**
     * 
     * @param int $iId
     * @param array $aHostTemplate
     * @param array $aServiceTemplate
     * @return \Plugins
     */
    public function getDetail($iId, $hostTemplates, $serviceTemplates)
    {
        $res = $this->db->db->query("SELECT * FROM mod_export_pluginpack WHERE plugin_id = " . $iId);

        $pluginPack = $res->fetch(\PDO::FETCH_ASSOC);
        
        foreach ($pluginPack as $key => $value) {
            $this->__set($key, $value);
        }

        if ($this->getPlugin_id()) {
            //Add host template
            $oHost = new HostTemplate();
            $aHost = $oHost->getHostTemplateByPlugin($this->getPlugin_id());
            $HostTemplateParameters = $oHost->getHostTemplateParametersByPlugin($this->getPlugin_id());
            $this->setHostIncludedParameters($HostTemplateParameters);
            foreach ($aHost as $key => $obj) {
                if ($obj instanceof HostTemplate && isset($hostTemplates[$obj->getHost_id()])) {
                    if ($obj->getStatus() == 0) {
                        $this->addHostExcluded($obj->getHost_id(), $hostTemplates[$obj->getHost_id()]);
                    } else {
                        $this->addHostIncluded($obj->getHost_id(), $hostTemplates[$obj->getHost_id()]);
                    }
                }
            }
            
            //Add service template
            $oService = new ServiceTemplate();
            $aService = $oService->getServiceTemplateByPlugin($this->getPlugin_id());
            foreach ($aService as $key => $obj) {
                if ($obj instanceof ServiceTemplate && isset($serviceTemplates[$obj->getService_id()])) {
                    if ($obj->getStatus() == 0) {
                        $this->addServiceExcluded($obj->getService_id(), $serviceTemplates[$obj->getService_id()]);
                    } else {
                        $aParam = array(
                            'id' => $obj->getService_id(),
                            'name' => $serviceTemplates[$obj->getService_id()],
                            'discovery_command' => $obj->getDiscovery_command()
                        );
                        $this->addServiceIncluded($aParam);
                    }
                }
            }
            
            //Add command
            $oCommand = new Command();
            $this->setCommand($oCommand->getAllCommandByPlugin($this->getPlugin_id()));

            //Add timeperiod
            $oTimeperiod = new Timeperiod();
            $this->setTimeperiod($oTimeperiod->getTimeperiodByPlugin($this->getPlugin_id()));
            
            if ($this->getPlugin_icon()) {
                $oIcon = new Icon();
                $this->setPlugin_icon_base64($oIcon->getIcon($this->getPlugin_icon()));
            }

            // Add dependencies
            $oDependency = new Dependency();
            $this->setPlugin_dependency($oDependency->getDependenciesByPlugin($this->getPlugin_id()));

            // Add requirement
            $oRequirement = new Requirement();
            $this->setPlugin_requirement($oRequirement->getRequirementByPlugin($this->getPlugin_id()));

            //Add description
            $oDescription = new Description();
            $this->setPlugin_description($oDescription->getDescriptionByPlugin($this->getPlugin_id()));
            
            //Add tags
            $oTag = $this->exportFactory->newTag();
            $this->setPlugin_tags($oTag->getTagsByPlugin($this->getPlugin_id()));
        }

        return $this;
    }

    /**
     * 
     * @return int
     */
    public function insert()
    {
        $id = 0;
        
        $sth = $this->db->db->prepare('INSERT INTO `mod_export_pluginpack` ('
            . '`plugin_name`, '
            . '`plugin_display_name`, '
            . '`plugin_slug`, '
            . '`plugin_author`, '
            . '`plugin_email`, '
            . '`plugin_website`, '
            . '`plugin_status`, '
            . '`plugin_compatibility`, '
            . '`plugin_status_message`, '
            . '`plugin_icon`, '
            . '`plugin_version`, '
            . '`plugin_monitoring_procedure`, '
            . '`plugin_discovery_category`, '
            . '`plugin_change_log`, '
            . '`plugin_time_generate`, '
            . '`plugin_last_update` '
            . ') VALUES ('
            . ':name, :display_name, :slug, :author, :email, :website, :status, :compatibility, :status_message, :icon, :version, '
            . ':monitoring_procedure, :discovery_category, :change_log, :time_generate, UNIX_TIMESTAMP(NOW()))');
        
        $sth->bindParam(':name', $this->getPlugin_name(), \PDO::PARAM_STR);
        $sth->bindParam(':display_name', $this->getPlugin_Display_Name(), \PDO::PARAM_STR);
        $sth->bindParam(':slug', $this->getPlugin_slug(), \PDO::PARAM_STR);
        $sth->bindParam(':author', $this->getPlugin_author(), \PDO::PARAM_STR);
        $sth->bindParam(':email', $this->getPlugin_email(), \PDO::PARAM_STR);
        $sth->bindParam(':website', $this->getPlugin_website(), \PDO::PARAM_STR);
        $sth->bindParam(':status', $this->getPlugin_status(), \PDO::PARAM_STR);
        $sth->bindParam(':compatibility', $this->getPlugin_compatibility(), \PDO::PARAM_STR);
        $sth->bindParam(':status_message', $this->getPlugin_status_message(), \PDO::PARAM_STR);
        $sth->bindParam(':icon', $this->getPlugin_icon(), \PDO::PARAM_STR);
        $sth->bindParam(':version', $this->getPlugin_version(), \PDO::PARAM_STR);
        $sth->bindParam(':monitoring_procedure', $this->getPlugin_monitoring_procedure(), \PDO::PARAM_STR);
        $sth->bindParam(':discovery_category', $this->getPlugin_discovery_category(), \PDO::PARAM_STR);
        $sth->bindParam(':change_log', $this->getPlugin_change_log(), \PDO::PARAM_STR);
        $sth->bindParam(':time_generate', $this->getPlugin_time_generate(), \PDO::PARAM_STR);

        try {
            $sth->execute();
            $id = $this->db->db->lastInsertId();
            $this->insertDescription($id);
        } catch (\PDOException $e) {
            echo "Error ".$e->getMessage();
        }

        return $id;
    }

    /**
     *
     * @return boolean
     */
    private function insertDescription($pluginId)
    {
        foreach ($this->getPlugin_description() as $locale_short_name => $description) {
            $sth = $this->db->db->prepare('INSERT INTO `mod_export_description` ('
                . '`description_plugin_id`, '
                . '`description_text`, '
                . '`description_locale` '
                . ') VALUES ('
                . ':plugin_id, '
                . ':description, '
                . '(SELECT locale_id FROM locale WHERE locale_short_name = :locale_short_name)'
                . ')');

            $sth->bindParam(':plugin_id', $pluginId, \PDO::PARAM_INT);
            $sth->bindParam(':description', $description, \PDO::PARAM_STR);
            $sth->bindParam(':locale_short_name', $locale_short_name, \PDO::PARAM_STR);

            try {
                $sth->execute();
            } catch (\PDOException $e) {
                echo "Error ".$e->getMessage();
                return false;
            }
        }

        return true;
    }
    
    /**
     * 
     * @return boolean
     */
    public function update()
    {
        $sQuery = 'UPDATE `mod_export_pluginpack` SET '
            . '`plugin_name` = :name,  '
            . '`plugin_display_name` = :display_name,  '
            . '`plugin_slug` = :slug, '
            . '`plugin_author` = :author,  '
            . '`plugin_email` = :email, '
            . '`plugin_website` = :website, '
            . '`plugin_status` = :status, '
            . '`plugin_compatibility` = :compatibility, '
            . '`plugin_status_message` = :status_message, '
            . '`plugin_icon` = :icon, '
            . '`plugin_version` = :version, '
            . '`plugin_monitoring_procedure` = :monitoring_procedure, '
            . '`plugin_discovery_category` = :discovery_category, '
            . '`plugin_change_log` = :change_log, '
            . '`plugin_time_generate` = :time_generate, '
            . '`plugin_last_update` = UNIX_TIMESTAMP(NOW()) '
            . ' WHERE plugin_id = :plugin_id ';
        $sth = $this->db->db->prepare($sQuery);
        
        $sth->bindParam(':name', $this->getPlugin_name(), \PDO::PARAM_STR);
        $sth->bindParam(':display_name', $this->getPlugin_Display_Name(), \PDO::PARAM_STR);
        $sth->bindParam(':slug', $this->getPlugin_slug(), \PDO::PARAM_STR);
        $sth->bindParam(':author', $this->getPlugin_author(), \PDO::PARAM_STR);
        $sth->bindParam(':email', $this->getPlugin_email(), \PDO::PARAM_STR);
        $sth->bindParam(':website', $this->getPlugin_website(), \PDO::PARAM_STR);
        $sth->bindParam(':status', $this->getPlugin_status(), \PDO::PARAM_STR);
        $sth->bindParam(':compatibility', $this->getPlugin_compatibility(), \PDO::PARAM_STR);
        $sth->bindParam(':status_message', $this->getPlugin_status_message(), \PDO::PARAM_STR);
        $sth->bindParam(':icon', $this->getPlugin_icon(), \PDO::PARAM_STR);
        $sth->bindParam(':version', $this->getPlugin_version(), \PDO::PARAM_STR);
        $sth->bindParam(':monitoring_procedure', $this->getPlugin_monitoring_procedure(), \PDO::PARAM_STR);
        $sth->bindParam(':discovery_category', $this->getPlugin_discovery_category(), \PDO::PARAM_STR);
        $sth->bindParam(':change_log', $this->getPlugin_change_log(), \PDO::PARAM_STR);
        $sth->bindParam(':time_generate', $this->getPlugin_time_generate(), \PDO::PARAM_STR);
        $sth->bindParam(':plugin_id', $this->getPlugin_id(), \PDO::PARAM_STR);
          
        try {
            $sth->execute();
            $this->updateDescription();
            return true;
        } catch (\PDOException $e) {
            echo "Error ".$e->getMessage();
            return false;
        }
    }

    /**
     *
     * @return boolean
     */
    private function updateDescription()
    {
        $sth = $this->db->db->prepare('DELETE FROM `mod_export_description` WHERE `description_plugin_id` = :plugin_id');
        $sth->bindParam(':plugin_id', $this->getPlugin_id(), \PDO::PARAM_INT);
       try {
            $sth->execute();
            $this->insertDescription($this->getPlugin_id());
        } catch (\PDOException $e) {
            echo "Error ".$e->getMessage();
            return false;
        }
        
        return true;
    }
    
    /**
     * Delete a pluginpack
     *
     * @param int id The pluginpack id to delete
     */
    public function delete($id)
    {
        $query = 'DELETE FROM mod_export_pluginpack WHERE plugin_id = :id';
        $sth = $this->db->db->prepare($query);
        $sth->bindParam(':id', $id, \PDO::PARAM_INT);
        try {
            $sth-execute();
        } catch (\PDOException $e) {
        }
    }
    
    /**
     * Delete a list of pluginpacks
     *
     * @param array id The list of pluginpack id to delete
     */
    public function deleteByIds($listIds)
    {
        if (count($listIds) === 0) {
            return;
        }
        $query = 'DELETE FROM mod_export_pluginpack WHERE plugin_id IN (' . join(',', $listIds) . ')';
        try {
            $this->db->db->query($query);
        } catch (\PDOException $e) {}
    }

    /**
     * Get list of plugin packs
     *
     * @param array $parameters
     * @return array
     */
    public function getList($parameters = array())
    {
        $fields = '*';
        if (count($parameters)) {
            $fields = implode(',',$parameters);
        }
        $query = 'SELECT ' . $fields
            . ' FROM mod_export_pluginpack';

        $result = $this->db->db->query($query);
        $pluginPacks = $result->fetchAll();

        return $pluginPacks;
    }
}
