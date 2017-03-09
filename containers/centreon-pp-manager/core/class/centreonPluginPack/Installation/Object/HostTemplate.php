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
class HostTemplate extends ObjectManager
{
    /**
     *
     * @var array
     */
    private $hostTemplateList;

    /**
     *
     * @var \CentreonHosttemplate
     */
    private $hostTemplateObj;
    
    /**
     *
     * @var \CentreonServicetemplates 
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
        $this->hostTemplateObj = new \CentreonHosttemplates($database);
        $this->commandObj = new \CentreonCommand($database);
        $this->serviceTemplateObj = new \CentreonServicetemplates($database);
        $this->keyInJson = 'host_templates';
        $this->neededUtils = array('templateSorter');

    }

    /**
     *
     * @return array
     */
    public function getIcons()
    {
        return $this->icons;
    }

    /**
     *
     */
    public function setIcons($icons)
    {
        $this->icons = $icons;
    }

    /**
     *
     * @return array
     */
    public function getObjectParams()
    {
        return $this->hostTemplateList;
    }

    /**
     * 
     * @param array $newObjectParams
     */
    public function setObjectParams($newObjectParams)
    {
        $this->hostTemplateList = $newObjectParams;
    }
    
    /**
     * 
     */
    private function sortTemplate()
    {
        // Sort Host Template
        $this->utilsManager['templateSorter']->setDataToSort($this->hostTemplateList);
        $this->utilsManager['templateSorter']->performSort();
        $this->hostTemplateList = $this->utilsManager['templateSorter']->getDataToSort();
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
     * Install host templates
     * @param bool $createServiceTemplateRelation
     */
    public function launchInstall($createServiceTemplateRelation = true)
    {
        $this->sortTemplate();
        $this->normalizeHostParams();

        foreach ($this->hostTemplateList as $hostTemplate) {
            $originalHostTemplateId = $this->hostTemplateObj->getHostId($hostTemplate['host_name']);

            if (is_null($originalHostTemplateId)) {
                $hostTemplateId = $this->hostTemplateObj->insert($hostTemplate);
            } else {
                $this->hostTemplateObj->update($originalHostTemplateId, $hostTemplate);
                $hostTemplateId = $originalHostTemplateId;
            }

            foreach ($hostTemplate['parent_template'] as &$htpl) {
                $htpl .= '-custom';
            }
            $this->setParentHostTemplatesInDb($hostTemplateId, $hostTemplate['parent_template']);

            $this->installHostTemplateMacrosInDb($hostTemplateId, $hostTemplate['macros']);

            $customHostTemplate = array(
                'host_name' => $hostTemplate['host_name'] . '-custom',
                'host_alias' => $hostTemplate['host_alias'],
                'host_register' => '0',
                'host_locked' => '0',
                'host_activate' => array(
                    'host_activate' => '1'
                ),
                'parent_template' => array($hostTemplate['host_name']),
                'services' => $hostTemplate['services']
            );

            $this->createCustomHostTemplate($customHostTemplate, $createServiceTemplateRelation);

        }
    }

    /**
     * Update host templates
     */
    public function launchUpdate()
    {
        $this->launchInstall(false);
    }

    /**
     * Remove host templates
     */
    public function launchUninstall()
    {
        foreach ($this->hostTemplateList as $hostTemplate) {
            $this->hostTemplateObj->deleteHostByName($hostTemplate['name'] . '-custom');
            $this->hostTemplateObj->deleteHostByName($hostTemplate['name']);
        }
    }

    /**
     *
     */
    private function normalizeHostParams()
    {
        foreach ($this->hostTemplateList as &$hostTemplate) {
            $normalizedHostTemplate = array(
                'host_name' => $hostTemplate['name'],
                'host_alias' => isset($hostTemplate['alias']) ? $hostTemplate['alias'] : $hostTemplate['name'],
                'host_comment' => isset($hostTemplate['_comment']) ?  $hostTemplate['_comment'] : '',
                'host_active_checks_enabled' => array(
                    'host_active_checks_enabled' => isset($hostTemplate['active_checks_enabled']) ?
                    $hostTemplate['active_checks_enabled'] :
                    '2'
                ),
                'host_passive_checks_enabled' => array(
                    'host_passive_checks_enabled' => isset($hostTemplate['passive_checks_enabled']) ?
                        $hostTemplate['passive_checks_enabled'] :
                        '2'
                ),
                'host_max_check_attempts' => isset($hostTemplate['max_check_attempts']) ?
                    $hostTemplate['max_check_attempts'] :
                    null,
                'host_normal_check_interval' => isset($hostTemplate['normal_check_interval']) ?
                    $hostTemplate['normal_check_interval'] :
                    null,
                'host_retry_check_interval' => isset($hostTemplate['retry_check_interval']) ?
                    $hostTemplate['retry_check_interval'] :
                    null,
                'host_register' => '0',
                'host_locked' => '1',
                'host_activate' => array(
                    'host_activate' => '1'
                ),
                'parent_template' => isset($hostTemplate['parent_template']) ?
                    $hostTemplate['parent_template'] :
                    array(),
                'macros' => isset($hostTemplate['macros']) ?  $hostTemplate['macros'] : array(),
                'command_command_id' => isset($hostTemplate['command_name']) &&
                    !is_null($hostTemplate['command_name']) &&
                    !empty($hostTemplate['command_name']) ?
                    $this->commandObj->getCommandIdByName($hostTemplate['command_name']) : null,
                'timeperiod_tp_id' => isset($hostTemplate['timeperiod']) &&
                    !is_null($hostTemplate['timeperiod']) &&
                    !empty($hostTemplate['timeperiod']) ?
                    $this->timeperiodObj->getTimperiodIdByName($hostTemplate['timeperiod']) : '',
                'services' => isset($hostTemplate['services']) ?  $hostTemplate['services'] : array()
            );

            foreach ($hostTemplate['parent_template'] as &$parentTemplate) {
                $parentTemplate .= '-custom';
            }

            if (isset($hostTemplate['icon']) && !is_null($hostTemplate['icon']) && !empty($hostTemplate['icon'])) {
                $normalizedHostTemplate['ehi_icon_image'] = $this->icons[
                    $this->pluginPackSlug . '-' . $hostTemplate['icon']
                ]['mediaId'];
            }

            $hostTemplate = $normalizedHostTemplate;
        }
    }

    /**
     *
     * @param int $hostTemplateId
     * @param array $macros
     */
    private function installHostTemplateMacrosInDb($hostTemplateId, $macros)
    {
        $compiledMacros = array(
            'input' => array(),
            'value' => array(),
            'password' => array(),
            'description' => array()
        );

        foreach ($macros as $macro) {
            preg_match('/^(?:\$)(?:_HOST)?(.*)(?:\$)$/', $macro['key'], $matches);
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

        $this->hostTemplateObj->insertMacro(
            $hostTemplateId,
            $compiledMacros['input'],
            $compiledMacros['value'],
            $compiledMacros['password'],
            $compiledMacros['description']
        );
    }

    /**
     *
     * @param array $customHostTemplates
     * @param bool $createServiceTemplateRelation
     */
    private function createCustomHostTemplate($customHostTemplate, $createServiceTemplateRelation)
    {
        $originalHostTemplateId = $this->hostTemplateObj->getHostId($customHostTemplate['host_name']);
        if (is_null($originalHostTemplateId)) {
            $customHostTemplateId = $this->hostTemplateObj->insert($customHostTemplate);
            $this->setParentHostTemplatesInDb($customHostTemplateId, $customHostTemplate['parent_template']);

            if ($createServiceTemplateRelation) {
                // Link to service templates
                $this->linkServiceTemplatesToHostTemplateInDb($customHostTemplateId, $customHostTemplate['services']);
            }
        }
    }

    /**
     *
     * @param int $hostId
     * @param array $parentHostTemplates
     * @throws \Exception
     */
    private function setParentHostTemplatesInDb($hostId, $parentHostTemplates)
    {
        $hostTemplateIds = array();

        foreach ($parentHostTemplates as $parentHostTemplate) {
            $parentHostTemplateId = $this->hostTemplateObj->getHostId($parentHostTemplate);
            if (is_null($parentHostTemplateId)) {
                throw new \Exception("Host template " . $parentHostTemplate . " can't be retrieve");
            }
            $hostTemplateIds[] = $parentHostTemplateId;
        }

        $this->hostTemplateObj->setTemplates($hostId, $hostTemplateIds);
    }

    /**
     *
     * @param int $hostTemplateId
     * @param array $serviceTemplates
     */
    private function linkServiceTemplatesToHostTemplateInDb($hostTemplateId, $serviceTemplates)
    {
        foreach ($serviceTemplates as $serviceTemplate) {
            $serviceTemplateId = $this->serviceTemplateObj->getServiceTemplateId($serviceTemplate . '-custom');
            if (!is_null($serviceTemplateId)) {

                $pluginpackId = $this->getPluginPackId();

                if (!$this->checkExistingRelation($pluginpackId, $hostTemplateId, $serviceTemplateId)) {

                    $this->insertHostServiceRelation($pluginpackId, $hostTemplateId, $serviceTemplateId);
                    $this->hostTemplateObj->insertRelHostService($hostTemplateId, $serviceTemplateId);

                }


            }
        }
    }

    /**
     *
     */
    public function checkExistingRelation($pluginpackId, $hostTemplateId, $serviceTemplateId)
    {
        $ExistingRelation = "SELECT * "
            . "FROM `mod_ppm_host_service_relation` "
            . "WHERE `pluginpack_id` = " . $pluginpackId . " "
            . "AND `host_id` = " . $hostTemplateId . " "
            . "AND `service_id` = ".$serviceTemplateId;
        $result = $this->dbManager->query($ExistingRelation);
        if ($result->fetchRow()) {
            return true;
        } else {
            return false;
        }
    }


    /**
     *
     */
    public function insertHostServiceRelation($pluginpackId, $hostTemplateId, $serviceTemplateId)
    {
        $insertHostService = "INSERT INTO `mod_ppm_host_service_relation` (`pluginpack_id`, `host_id`, `service_id`) "
         . "VALUES (".$pluginpackId.", ".$hostTemplateId.", ".$serviceTemplateId.")";
        $this->dbManager->query($insertHostService);
    }

    /**
     * 
     */
    public function checkLinkedObjects(&$linkedObject)
    {
        $linkedObject['hosts'] = array();
        $linkedObject['host_templates'] = array();

        $pluginPackHostTemplateNames = array();
        foreach ($this->hostTemplateList as $hostTemplate) {
            $pluginPackHostTemplateNames[] = $hostTemplate['name'];
            $pluginPackHostTemplateNames[] = $hostTemplate['name'] . '-custom';
        }

        
        foreach ($pluginPackHostTemplateNames as $hostTemplateName) {
            $hostTemplates = $this->hostTemplateObj->getLinkedHostsByName($hostTemplateName, true);
            if (count($hostTemplates)) {
                $linkedObject['host_templates'][$hostTemplateName] = $hostTemplates;
            }
            $hosts = $this->hostTemplateObj->getLinkedHostsByName($hostTemplateName, false);
            if (count($hosts)) {
                $linkedObject['hosts'][$hostTemplateName] = $hosts;
            }
        }

        // Remove dependencies on pp itself
        foreach ($pluginPackHostTemplateNames as $hostTemplateName) {
            if (isset($linkedObject['host_templates'][$hostTemplateName])) {
                $linkedObject['host_templates'][$hostTemplateName] =
                    array_diff($linkedObject['host_templates'][$hostTemplateName], $pluginPackHostTemplateNames);
                if (!count($linkedObject['host_templates'][$hostTemplateName])) {
                    unset($linkedObject['host_templates'][$hostTemplateName]);
                }
            }
        }
    }
}
