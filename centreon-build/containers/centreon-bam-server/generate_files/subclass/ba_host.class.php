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

class BaHost extends AbstractObject {
    protected $generate_filename = 'centreon-bam-host.cfg';
    protected $object_name = 'host';
    protected $attributes_write = array(
        'host_name',
        'alias',
        'address',
        'check_command',
        'max_check_attempts',
        'check_interval',
        'active_checks_enabled',
        'passive_checks_enabled',
        'check_period',
        'notification_interval',
        'notification_period',
        'notification_options',
        'notifications_enabled',
        '_HOST_ID',
        'register'
    );

    public function generateObjects() {
        if ($this->checkGenerate(0)) {
            return 0;
        }

        Engine::getInstance()->addCfgPath($this->generate_filename);

        $host_id = BackendBA::getInstance()->getBaHostId();

        $object = array();
        $object['host_name'] = '_Module_BAM_' .  $this->backend_instance->getPollerId();
        $object['alias'] = 'Centreon BAM Module';
        $object['address'] = '127.0.0.1';
        $object['check_command'] = 'centreon-bam-host-alive';
        $object['max_check_attempts'] = '3';
        $object['check_interval'] = '1';
        $object['active_checks_enabled'] = '0';
        $object['passive_checks_enabled'] = '0';
        $object['check_period'] = 'centreon-bam-timeperiod';
        $object['contact_groups'] = 'centreon-bam-contactgroup';
        $object['notification_interval'] = '60';
        $object['notification_period'] = 'centreon-bam-timeperiod';
        $object['notification_options'] = 'd';
        $object['notification_enabled'] = '0';
        $object['_HOST_ID'] = $host_id;
        $object['register'] = '1';

        $this->generateObjectInFile($object, 0);

        Host::getInstance()->addHost($host_id, $object);
    }
}
