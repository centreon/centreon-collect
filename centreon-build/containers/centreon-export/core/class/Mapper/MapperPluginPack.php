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
namespace CentreonExport\Mapper;

require_once _CENTREON_PATH_ . '/lib/Slug.class.php';

class MapperPluginPack
{
    protected $pluginPackObject;
    protected $hostTemplateObject;
    protected $serviceTemplateObject;

    public function __construct($pluginPackObject, $hostTemplateObject, $serviceTemplateObject)
    {
        $this->pluginPackObject = $pluginPackObject;
        $this->hostTemplateObject = $hostTemplateObject;
        $this->serviceTemplateObject = $serviceTemplateObject;
    }

    /**
     * This method injects a form in a plugin object
     * @param \HTML_QuickForm $form
     * @return pluginPack $pluginPackObject
     */
    public function saveInObject($form)
    {
        $aDatas = $form->getSubmitValues();
        $this->hostTemplateParser($aDatas);
        $this->serviceTemplateParser($aDatas);

        if (!empty($aDatas['plugin_id'])) {
            $this->pluginPackObject->setPlugin_id($aDatas['plugin_id']);
            
            /* Load database values */
            $aHostTemplate = $this->hostTemplateObject->getList(true, true);
            $aServiceTemplate = $this->serviceTemplateObject->getList(true);
            $this->pluginPackObject->getDetail($aDatas['plugin_id'], $aHostTemplate, $aServiceTemplate);
        }

        $this->pluginPackObject->setPlugin_Name($aDatas['name']);
        $this->pluginPackObject->setPlugin_Display_Name($aDatas['display_name']);
        $sSlug = (string) new \Slug($aDatas['name'], array('max_length' => 255));
        $this->pluginPackObject->setPlugin_slug($sSlug);
        $this->pluginPackObject->setPlugin_Author($aDatas['author']);
        $this->pluginPackObject->setPlugin_email($aDatas['email']);
        $this->pluginPackObject->setPlugin_website($aDatas['website']);
        $this->pluginPackObject->setPlugin_compatibility($aDatas['compatibility']);
        $this->pluginPackObject->setPlugin_monitoring_procedure($aDatas['monitoring_procedure']);
        $this->pluginPackObject->setPlugin_discovery_category($aDatas['type_discovery']);
        $this->pluginPackObject->setPlugin_icon($aDatas['id_icon']);

        $this->pluginPackObject->setPlugin_status($aDatas['status']);

        if (is_null($this->pluginPackObject->getPlugin_version())) {
            $this->pluginPackObject->setPlugin_version('');
        }

        $this->pluginPackObject->setPlugin_description($aDatas['description']);
        
        if (count($aDatas['hostIncluded']) > 0) {
            $this->pluginPackObject->setHostIncluded($aDatas['hostIncluded']);
            $this->pluginPackObject->setHostIncludedParameters($aDatas['hostIncludedParams']);
        } else {
            $this->pluginPackObject->setHostIncluded(array());
            $this->pluginPackObject->setHostIncludedParameters(array());
        }
        
        if (count($aDatas['serviceIncluded']) > 0) {
            $this->pluginPackObject->setServiceIncluded($aDatas['serviceIncluded']);
            $this->pluginPackObject->setServiceIncludedParameters($aDatas['serviceIncludedParams']);
        } else {
            $this->pluginPackObject->setServiceIncluded(array());
            $this->pluginPackObject->setServiceIncludedParameters(array());
        }
        
        if (count($aDatas['hostExcluded']) > 0) {
            $this->pluginPackObject->setHostExcluded($aDatas['hostExcluded']);
        } else {
            $this->pluginPackObject->setHostExcluded(array());
        }
        
        if (count($aDatas['serviceExcluded']) > 0) {
            $this->pluginPackObject->setServiceExcluded($aDatas['serviceExcluded']);
        } else {
            $this->pluginPackObject->setServiceExcluded(array());
        }
        
        if (count($aDatas['command']) > 0) {
            $this->pluginPackObject->setCommand($aDatas['command']);
        } else {
            $this->pluginPackObject->setCommand(array());
        }
        
        if (count($aDatas['tag']) > 0) {
            $this->pluginPackObject->setPlugin_tags($aDatas['tag']);
        } else {
            $this->pluginPackObject->setPlugin_tags(array());
        }
        
        if (count($aDatas['nameDependency']) > 0) {
            $aDepend = array();
            foreach ($aDatas['nameDependency'] as $key => $plug_name) {
                $aDepend[] = array(
                    'name' => $plug_name,
                    'version' => $aDatas['versionDependency'][$key]
                );
            }
            $this->pluginPackObject->setPlugin_dependency($aDepend);
        } else {
            $this->pluginPackObject->setPlugin_dependency(array());
        }
        
        if (count($aDatas['nameRequirement']) > 0) {
            $aRequirement = array();
            foreach ($aDatas['nameRequirement'] as $key => $soft_name) {
                if (!empty($aDatas['versionRequirement'][$key])) {
                    $aRequirement[] = array(
                        'name' => $soft_name,
                        'version' => $aDatas['versionRequirement'][$key]
                    );
                }
            }
            $this->pluginPackObject->setPlugin_requirement($aRequirement);
        } else {
            $this->pluginPackObject->setPlugin_requirement(array());
        }
            
        return $this->pluginPackObject;
    }
    
    /**
     * 
     * @param array $aDatas
     */
    private function hostTemplateParser(&$aDatas)
    {
        // Host Included
        $hostIncludedParams = array();
        if (isset($aDatas['hostIncluded'])) {
            foreach ($aDatas['hostIncluded'] as $myHostIncluded => $fakeValue) {
                $hostIncludedParams[$myHostIncluded]['discovery_protocol'] = $aDatas['protocol_included_host'][$myHostIncluded];
                $hostIncludedParams[$myHostIncluded]['discovery_category'] = $aDatas['discovery_category_included_host'][$myHostIncluded];
                $hostIncludedParams[$myHostIncluded]['discovery_command'] = $aDatas['discovery_command_included_host'][$myHostIncluded];
                $hostIncludedParams[$myHostIncluded]['documentation'] = $aDatas['documentation_included_host'][$myHostIncluded];
            }
            $aDatas['hostIncludedParams'] = $hostIncludedParams;
        }
        
        // Remove obsoltete field
        unset($aDatas['protocol_included_host']);
        unset($aDatas['discovery_category_included_host']);
        unset($aDatas['discovery_command_included_host']);
        unset($aDatas['documentation_included_host']);
    }
    
    
    /**
     * 
     * @param array $aDatas
     */
    private function serviceTemplateParser(&$aDatas)
    {
        // Service Included
        $serviceIncludedParams = array();
        if (isset($aDatas['serviceIncluded'])) {
            foreach ($aDatas['serviceIncluded'] as $myServiceIncluded => $fakeValue) {
                $serviceIncludedParams[$myServiceIncluded] = $aDatas['discovery_command_included_service'][$myServiceIncluded];
            }
            $aDatas['serviceIncludedParams'] = $serviceIncludedParams;
        }
        
        // Remove obsoltete field
        unset($aDatas['discovery_command_included_service']);
    }
}
