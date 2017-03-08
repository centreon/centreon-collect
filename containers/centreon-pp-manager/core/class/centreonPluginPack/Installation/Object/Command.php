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
class Command extends ObjectManager
{
    /**
     *
     * @var array
     */
    private $commandList;

    /**
     *
     * @var CentreonCommand
     */
    private $commandObj;

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
        $this->commandObj = new \CentreonCommand($database);
        $this->keyInJson = 'commands';
        $this->additionnalParamsKeysInJson = array(
            'host_templates',
            'service_templates'
        );
    }

    /**
     *
     * @return array
     */
    public function getObjectParams()
    {
        return $this->commandList;
    }

    /**
     *
     */
    public function setObjectParams($newObjectParams)
    {
        $this->commandList = $newObjectParams;
    }

    /**
     * @return \CentreonCommand
     */
    public function getCommandObj()
    {
        return $this->commandObj;
    }

    /**
     * @param \CentreonCommand $commandObj the centreon command
     */
    public function setCommandObj($commandObj)
    {
        $this->commandObj = $commandObj;
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
     * Install commands in database
     */
    public function launchInstall()
    {
        // From JSON file
        foreach ($this->commandList as $command) {

            // Decode command line
            $command['line'] = json_decode(stripslashes($command['line']));

            // Test if exist in database
            $commandId = $this->commandObj->getCommandIdByName($command['name']);

            $parameters = array(
                'command_name' => $command['name'],
                'command_line' => $command['line'],
                'command_type' => $command['type']
            );

            // If already exist (in db)
            if (! is_null($commandId)) {
                // Update
                $this->commandObj->update($commandId, $parameters);
            } else {
                // Insert an element
                $this->commandObj->insert($parameters, true);
            }
        }
    }

    /**
     * Update commands in database
     */
    public function launchUpdate()
    {
        // Install new commands or update existed ones
        $this->launchInstall();
    }

    /**
     * Remove commands in database
     */
    public function launchUninstall()
    {
        // From JSON file
        foreach ($this->commandList as $command) {

            // Delete each command by name
            $this->commandObj->deleteCommandByName($command['name']);

        }
    }
    
    /**
     * 
     */
    public function checkLinkedObjects(&$linkedObjects)
    {
        $linkedObjects['commands'] = array();
        $linkedCommands = array(
            'hosts' => array(),
            'host_templates' => array(),
            'services' => array(),
            'service_templates' => array()
        );

        $pluginPackHostTemplateNames = array();
        foreach ($this->additionnalParams['host_templates'] as $hostTemplate) {
            $pluginPackHostTemplateNames[] = $hostTemplate['name'];
            $pluginPackHostTemplateNames[] = $hostTemplate['name'] . '-custom';
        }

        $pluginPackServiceTemplateNames = array();
        foreach ($this->additionnalParams['service_templates'] as $serviceTemplate) {
            $pluginPackServiceTemplateNames[] = $serviceTemplate['name'];
            $pluginPackServiceTemplateNames[] = $serviceTemplate['name'] . '-custom';
        }

        foreach ($this->commandList as $command) {
            $hostTemplates = $this->commandObj->getLinkedHostsByName($command['name'], true);
            if (count($hostTemplates)) {
                $linkedCommands['host_templates'][$command['name']] = $hostTemplates;
            }

            $hosts = $this->commandObj->getLinkedHostsByName($command['name'], false);
            if (count($hosts)) {
                $linkedCommands['hosts'][$command['name']] = $hosts;
            }

            $serviceTemplates = $this->commandObj->getLinkedServicesByName($command['name'], true);
            if (count($serviceTemplates)) {
                $linkedCommands['service_templates'][$command['name']] = $serviceTemplates;
            }

            $services = $this->commandObj->getLinkedServicesByName($command['name'], false);
            if (count($services)) {
                $linkedCommands['services'][$command['name']] = $services;
            }
        }

        // Remove dependencies on pp itself
        foreach ($this->commandList as $command) {
            // host templates
            if (isset($linkedCommands['host_templates'][$command['name']])) {
                $linkedCommands['host_templates'][$command['name']] =
                    array_diff($linkedCommands['host_templates'][$command['name']], $pluginPackHostTemplateNames);
                if (!count($linkedCommands['host_templates'][$command['name']])) {
                    unset($linkedCommands['host_templates'][$command['name']]);
                }
            }

            // service temapltes
            if (isset($linkedCommands['service_templates'][$command['name']])) {
                $linkedCommands['service_templates'][$command['name']] =
                    array_diff(
                        $linkedCommands['service_templates'][$command['name']],
                        $pluginPackServiceTemplateNames
                    );
                if (!count($linkedCommands['service_templates'][$command['name']])) {
                    unset($linkedCommands['service_templates'][$command['name']]);
                }
            }
        }
        
        $linkedObjects['commands'] = $linkedCommands;
    }
}
