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

class Export
{
    protected $iconObj;

    /**
     *
     * @var array 
     */
    protected $object = array();
    
    /**
     *
     * @var array 
     */
    protected $icons = array();

    /**
     * 
     */
    public function __construct($iconObj)
    {
        $this->iconObj = $iconObj;
    }
    
    /**
     * 
     * @param Plugins $pluginPack
     * @return array
     */
    public function exportConfiguration($pluginPack)
    {
        $slug = trim($pluginPack->getPlugin_slug());
        if (empty($slug)) {
            $slug = $pluginPack->getPlugin_name();
        }

        $aHostTpl = $pluginPack->exportHostTemplates();
        $this->manageIcons($aHostTpl);

        $aServiceTpl = $pluginPack->exportServiceTemplates();
        $this->manageIcons($aServiceTpl);

        $this->object = array(
            'schema_version'  => 1,
            'information' => array(
                'slug' => $slug,
                'name' => $pluginPack->getPlugin_Display_Name(),
                'icon' => $pluginPack->getPlugin_icon_base64(),
                'version' => $pluginPack->getPlugin_version(),
                'update_date' => $pluginPack->getPlugin_last_update(),
                'status' => $pluginPack->getPlugin_status(),
                'status_message' => $pluginPack->getPlugin_status_message(),
                "tags" => $pluginPack->getTagAsArray(),
                'discovery_category' => $pluginPack->getPlugin_discovery_category(),
                'author' => $pluginPack->getPlugin_author(),
                'email' => $pluginPack->getPlugin_email(),
                'website' => $pluginPack->getPlugin_website(),
                'changelog' => $pluginPack->getPlugin_change_log(),
                'monitoring_procedure' => $pluginPack->getPlugin_monitoring_procedure(),
                'description' => $pluginPack->getDescriptionAsArray(),
                'dependencies' => $pluginPack->getPlugin_dependencies(),
                'requirement' => $pluginPack->getPlugin_requirement()
            ),
            'timeperiods'  => $pluginPack->getTimeperiodAsArray(),
            'commands' => $pluginPack->getCommandAsArray(),
            'service_templates' => $aServiceTpl,
            'host_templates' => $aHostTpl,
            'images' => $this->icons
        );    
        
        return $this->object; 
    }
    
    /**
     * 
     * @param type $objects
     */
    public function manageIcons(&$objects)
    {
        foreach ($objects as $key => &$object) {
            $used = false;
            if (!empty($object['icon'])) {
                $explodedIconPath = explode('/', $object['icon']);
                $iconName = end($explodedIconPath);

                foreach ($this->icons as $icon) {
                    if ($icon['name'] == $iconName) {
                        $used = true;
                    }
                }

                if (!$used) {
                    $iconBinary = $this->iconObj->convertIcon($object['icon']);
                    $this->icons[] = array(
                        'name' => $iconName,
                        'icon' => $iconBinary
                    );
                }

                $object['icon'] = $iconName;
            }
        }
    }
}
