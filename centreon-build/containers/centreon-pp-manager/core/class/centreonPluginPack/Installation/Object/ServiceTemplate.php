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
namespace CentreonPluginPackManager\Installation\Object;

/**
 *
 * @author lionel
 */
class ServiceTemplate extends ObjectManager
{
    /**
     *
     * @var array
     */
    private $serviceTemplateList;

    /**
     *
     * @var CentreonServicetemplate
     */
    private $serviceTemplateObj;

    /**
     *
     * @var array
     */
    public $icons;

    /**
     *
     * @param \CentreonDB $database
     */
    public function __construct($database)
    {
        parent::__construct($database);
        $this->serviceTemplateObj = new \CentreonServicetemplates($database);
        $this->commandObj = new \CentreonCommand($database);
        $this->timeperiodObj = new \CentreonTimeperiod($database);
        $this->keyInJson = 'service_templates';
        $this->neededUtils = array('templateSorter');
    }

    /**
     *
     * @return array
     */
    public function getObjectParams()
    {
        return $this->serviceTemplateList;
    }

    /**
     * 
     * @param array $newObjectParams
     */
    public function setObjectParams($newObjectParams)
    {
        $this->serviceTemplateList = $newObjectParams;
    }
    
    /**
     * 
     */
    private function sortTemplate()
    {
        // Sort Host Template
        $this->utilsManager['templateSorter']->setDataToSort($this->serviceTemplateList);
        $this->utilsManager['templateSorter']->performSort();
        $this->serviceTemplateList = $this->utilsManager['templateSorter']->getDataToSort();
    }

    /**
     * Prepare install
     */
    public function prepareInstall()
    {
        
    }

    /**
     * Prepare update
     */
    public function prepareUpdate()
    {
        
    }

    /**
     * Prepare uninstall
     */
    public function prepareUninstall()
    {

    }

    /**
     * Install service templates
     */
    public function launchInstall()
    {
        $this->sortTemplate();
        $this->normalizeServiceParams();

        foreach ($this->serviceTemplateList as $serviceTemplate) {

            if (isset($serviceTemplate['parent_template']) && !empty($serviceTemplate['parent_template'])) {
                $serviceTemplate['service_template_model_stm_id'] =
                    $this->serviceTemplateObj->getServiceTemplateId($serviceTemplate['parent_template']);
                if (is_null($serviceTemplate['service_template_model_stm_id'])) {
                    throw new \Exception('Cannot get id of parent template ' . $serviceTemplate['parent_template']);
                }
            }

            $originalServiceTemplateId =
                $this->serviceTemplateObj->getServiceTemplateId($serviceTemplate['service_description']);

            if (is_null($originalServiceTemplateId)) {
                $serviceTemplateId = $this->serviceTemplateObj->insert($serviceTemplate);
            } else {
                $serviceTemplateId = $originalServiceTemplateId;
                $this->updateServiceTemplateInDb($originalServiceTemplateId, $serviceTemplate);
            }

            if (isset($serviceTemplate['macros'])) {
                $this->installServiceTemplateMacrosInDb($serviceTemplateId, $serviceTemplate['macros']);
            }

            $customServiceTemplate = array(
                'service_description' => $serviceTemplate['service_description'] . '-custom',
                'service_alias' => $serviceTemplate['service_alias'],
                'service_register' => '0',
                'service_locked' => '0',
                'service_activate' => array(
                    'service_activate' => '1'
                ),
                'service_template_model_stm_id' => $serviceTemplateId
            );

            $this->createCustomServiceTemplate($customServiceTemplate);
        }
    }

    /**
     * Update service templates
     */
    public function launchUpdate()
    {
        $this->launchInstall();
    }

    /**
     * remove service templates
     */
    public function launchUninstall()
    {
        foreach ($this->serviceTemplateList as $serviceTemplate) {
            $this->serviceTemplateObj->deleteServiceByDescription($serviceTemplate['name'] . '-custom');
            $this->serviceTemplateObj->deleteServiceByDescription($serviceTemplate['name']);
        }
    }

    /*
     *
     */
    private function normalizeServiceParams()
    {
        foreach ($this->serviceTemplateList as &$serviceTemplate) {
            $normalizedServiceTemplate = array(
                'service_description' => $serviceTemplate['name'],
                'service_alias' => isset($serviceTemplate['alias']) ?
                    $serviceTemplate['alias'] :
                    $serviceTemplate['name'],
                'service_comment' => isset($serviceTemplate['_comment']) ?  $serviceTemplate['_comment'] : '',
                'service_active_checks_enabled' => array(
                    'service_active_checks_enabled' => isset($serviceTemplate['active_checks_enabled']) ?
                        $serviceTemplate['active_checks_enabled'] :
                        '2'
                ),
                'service_passive_checks_enabled' => array(
                    'service_passive_checks_enabled' => isset($serviceTemplate['passive_checks_enabled']) ?
                        $serviceTemplate['passive_checks_enabled'] :
                        '2'
                ),
                'service_is_volatile' => array(
                    'service_is_volatile' => isset($serviceTemplate['is_volatile']) ?
                        $serviceTemplate['is_volatile'] :
                        '2'
                ),
                'service_max_check_attempts' => isset($serviceTemplate['max_check_attempts']) ?
                    $serviceTemplate['max_check_attempts'] :
                    0,
                'service_normal_check_interval' => isset($serviceTemplate['normal_check_interval']) ?
                    $serviceTemplate['normal_check_interval'] :
                    0,
                'service_retry_check_interval' => isset($serviceTemplate['retry_check_interval']) ?
                    $serviceTemplate['retry_check_interval'] :
                    0,
                'service_max_check_attempts' => isset($serviceTemplate['max_check_attempts']) ?
                    $serviceTemplate['max_check_attempts'] :
                    0,
                'service_register' => '0',
                'service_locked' => '1',
                'service_activate' => array(
                    'service_activate' => '1'
                ),
                'macros' => isset($serviceTemplate['macros']) ?  $serviceTemplate['macros'] : array(),
                'command_command_id' => isset($serviceTemplate['command_name']) &&
                    !is_null($serviceTemplate['command_name']) &&
                    !empty($serviceTemplate['command_name']) ?
                    $this->commandObj->getCommandIdByName($serviceTemplate['command_name']) :
                    null,
                'timeperiod_tp_id' => isset($serviceTemplate['timeperiod']) &&
                    !is_null($serviceTemplate['timeperiod']) &&
                    !empty($serviceTemplate['timeperiod']) ?
                    $this->timeperiodObj->getTimperiodIdByName($serviceTemplate['timeperiod']) :
                    '',
                'parent_template' => isset($serviceTemplate['parent_template']) &&
                    !empty($serviceTemplate['parent_template']) ?
                    $serviceTemplate['parent_template'] . '-custom' :
                    null
            );

            if (
                isset($serviceTemplate['icon']) &&
                !is_null($serviceTemplate['icon']) &&
                !empty($serviceTemplate['icon'])
            ) {
                $normalizedServiceTemplate['esi_icon_image'] = $this->icons[
                    $this->pluginPackSlug . '-' . $serviceTemplate['icon']
                ]['mediaId'];
            }

            $serviceTemplate = $normalizedServiceTemplate;

        }
    }

    /**
     *
     * @param array $customServiceTemplates
     */
    private function createCustomServiceTemplate($customServiceTemplate)
    {
        $originalServiceTemplateId =
            $this->serviceTemplateObj->getServiceTemplateId($customServiceTemplate['service_description']);
        if (is_null($originalServiceTemplateId)) {
            $this->serviceTemplateObj->insert($customServiceTemplate);
        }
    }

    /**
     *
     * @param int $serviceTemplateId
     * @param array $macros
     */
    private function installServiceTemplateMacrosInDb($serviceTemplateId, $macros)
    {
        $compiledMacros = array(
            'input' => array(),
            'value' => array(),
            'password' => array(),
            'description' => array()
        );

        foreach ($macros as $macro) {
            preg_match('/^(?:\$)(?:_SERVICE)?(.*)(?:\$)$/', $macro['key'], $matches);
            if (!isset($matches[1])) {
                continue;
            }
            if (isset($macro['password'])) {
                if ($macro['password']) {
                    $password = '1';
                } else {
                    $password = null; //'0'
                }
            } else {
                $password = null;
            }
            $compiledMacros['input'][] = $matches[1];
            $compiledMacros['value'][] = isset($macro['value']) ? $macro['value'] : '';
            $compiledMacros['password'][] = $password;
            $compiledMacros['description'][] = isset($macro['description']) ? $macro['description'] : '';
        }

        $this->serviceTemplateObj->insertMacro(
            $serviceTemplateId,
            $compiledMacros['input'],
            $compiledMacros['value'],
            $compiledMacros['password'],
            $compiledMacros['description']
        );
    }

    /**
     * 
     * @param int $serviceTemplateId
     * @param type $serviceTemplate
     */
    private function updateServiceTemplateInDb($serviceTemplateId, $serviceTemplate)
    {
        if ($this->serviceAliasHasChanged($serviceTemplateId, $serviceTemplate['service_alias'])) {
            // Update service names which inherit from non custom service template
            $this->updateServiceNamesInDb($serviceTemplate['service_description'], $serviceTemplate['service_alias']);

            $customServiceTemplateId =
                $this->serviceTemplateObj->getServiceTemplateId($serviceTemplate['service_description'] . '-custom');
            $oldServiceTemplate = $this->serviceTemplateObj->getParameters($serviceTemplateId, array('service_alias'));
            $oldServiceTemplateAlias = $oldServiceTemplate['service_alias'];
            $oldCustomServiceTemplate =
                $this->serviceTemplateObj->getParameters($customServiceTemplateId, array('service_alias'));
            $oldCustomServiceTemplateAlias = $oldCustomServiceTemplate['service_alias'];

            // If custom service template alias has not been changed by user, update it and its linked services
            if ($oldServiceTemplateAlias == $oldCustomServiceTemplateAlias) {

                $this->serviceTemplateObj->setServiceAlias($customServiceTemplateId, $serviceTemplate['service_alias']);

                // Update service names which inherit from custom service template
                $this->updateServiceNamesInDb(
                    $serviceTemplate['service_description'] . '-custom',
                    $serviceTemplate['service_alias']
                );
            }
        }

        $this->serviceTemplateObj->update($serviceTemplateId, $serviceTemplate);
    }

    /**
     * Update service name if service template alias has changed
     * @param type $serviceTemplateDescription
     * @param type $serviceTemplateAlias
     */
    private function updateServiceNamesInDb($serviceTemplateDescription, $serviceTemplateAlias)
    {
        $serviceIds = array();

        $hostTemplateNames =
            $this->serviceTemplateObj->getLinkedHostsByServiceDescription($serviceTemplateDescription, true);
        foreach ($this->pluginPackJson['host_templates'] as $hostTemplate) {
            $hostTemplateNames[] = $hostTemplate['host_name'];
            $hostTemplateNames[] = $hostTemplate['host_name'] . '-custom';
        }
        $hostTemplateNames = array_unique($hostTemplateNames);

        foreach ($hostTemplateNames as $hostTemplateName) {
            $serviceIds1 =
                $this->serviceTemplateObj->getServiceIdsLinkedToSTAndCreatedByHT(
                    $serviceTemplateDescription,
                    $hostTemplateName
                );
            $serviceIds = array_merge($serviceIds, $serviceIds1);
        }

        foreach ($serviceIds as $serviceId) {
            $this->serviceTemplateObj->setServiceDescription($serviceId, $serviceTemplateAlias);
        }
    }

    /**
     * 
     * @param type $serviceTemplateId
     * @param type $serviceTemplateAlias
     * @return boolean
     */
    private function serviceAliasHasChanged($serviceTemplateId, $serviceTemplateAlias)
    {
        $oldService = $this->serviceTemplateObj->getParameters($serviceTemplateId, array('service_alias'));

        if (isset($oldService['service_alias']) && $oldService['service_alias'] != $serviceTemplateAlias) {
            return true;
        }

        return false;
    }

    /**
     * 
     * @param array $linkedObject
     */
    public function checkLinkedObjects(&$linkedObject)
    {
        $linkedObject['services'] = array();
        $linkedObject['service_templates'] = array();

        $pluginPackServiceTemplateNames = array();
        foreach ($this->serviceTemplateList as $serviceTemplate) {
            $pluginPackServiceTemplateNames[] = $serviceTemplate['name'];
            $pluginPackServiceTemplateNames[] = $serviceTemplate['name'] . '-custom';
        }

        foreach ($pluginPackServiceTemplateNames as $serviceTemplateName) {
            $serviceTemplates = $this->serviceTemplateObj->getLinkedServicesByName($serviceTemplateName, true);
            if (count($serviceTemplates)) {
                $linkedObject['service_templates'][$serviceTemplateName] = $serviceTemplates;
            }
            $services = $this->serviceTemplateObj->getLinkedServicesByName($serviceTemplateName, false);
            if (count($services)) {
                $linkedObject['services'][$serviceTemplateName] = $services;
            }
        }

        // Remove dependencies on pp itself
        foreach ($pluginPackServiceTemplateNames as $serviceTemplateName) {
            if (isset($linkedObject['service_templates'][$serviceTemplateName])) {
                $linkedObject['service_templates'][$serviceTemplateName] =
                    array_diff(
                        $linkedObject['service_templates'][$serviceTemplateName],
                        $pluginPackServiceTemplateNames
                    );
                if (!count($linkedObject['service_templates'][$serviceTemplateName])) {
                    unset($linkedObject['service_templates'][$serviceTemplateName]);
                }
            }
        }
    }
}
