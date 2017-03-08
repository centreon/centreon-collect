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

class BaTimeperiod extends AbstractObject {
    protected $generate_filename = 'centreon-bam-timeperiod.cfg';
    protected $object_name = 'timeperiod';
    protected $attributes_write = array(
        'timeperiod_name',
        'alias',
        'sunday',
        'monday',
        'tuesday',
        'wednesday',
        'thursday',
        'friday',
        'saturday',
    );

    public function generateObjects() {
        if ($this->checkGenerate(0)) {
            return 0;
        }

        Engine::getInstance()->addCfgPath($this->generate_filename);

        $object = array();
        $object['timeperiod_name'] = 'centreon-bam-timeperiod';
        $object['alias'] = 'centreon-bam-timeperiod';
        $object['sunday'] = '00:00-24:00';
        $object['monday'] = '00:00-24:00';
        $object['tuesday'] = '00:00-24:00';
        $object['wednesday'] = '00:00-24:00';
        $object['thursday'] = '00:00-24:00';
        $object['friday'] = '00:00-24:00';
        $object['saturday'] = '00:00-24:00';
        $this->generateObjectInFile($object, 0);
    }
}
