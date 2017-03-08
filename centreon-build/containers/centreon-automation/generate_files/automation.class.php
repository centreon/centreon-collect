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

require_once dirname(__FILE__) . '/subclass/discovery_command.class.php';

class Automation extends AbstractObject
{

    protected $broker = true;
    protected $engine = false;

    public function generateFromPollerId($poller_id, $localhost)
    {
        DiscoveryCommand::getInstance()->generateObjects();
    }

    public function reset()
    {
        DiscoveryCommand::getInstance()->reset();
    }
}
