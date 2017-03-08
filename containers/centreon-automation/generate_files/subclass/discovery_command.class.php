<?php
/*
 * Centreon
 *
 * Source Copyright 2005-2016 Centreon
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@centreon.com
 *
 */

class DiscoveryCommand extends AbstractObject
{
    private $commands = null;
    protected $generate_filename = 'discovery.cfg';
    protected $object_name = 'command';
    protected $attributes_select = '
        command_id,
        command_name,
        command.command_line
    ';
    protected $attributes_write = array(
        'command_name',
        'command_line',
    );

    private function getCommands()
    {
        $stmt = $this->backend_instance->db->prepare("
            SELECT $this->attributes_select
            FROM command
            WHERE command_type = 4
        ");
        $stmt->execute();
        $this->commands = $stmt->fetchAll(PDO::FETCH_GROUP|PDO::FETCH_UNIQUE|PDO::FETCH_ASSOC);
    }

    public function generateObjects()
    {
        if (is_null($this->commands)) {
            $this->getCommands();
        }

        foreach ($this->commands as $command_id => $command) {
            if ($this->checkGenerate($command_id)) {
                continue;
            }

            $object = array(
                'command_name' => $command['command_name'],
                'command_line' => $command['command_line']
            );

            $this->generateObjectInFile($object, $command_id);
        }
    }


    public function reset()
    {
    }
}
