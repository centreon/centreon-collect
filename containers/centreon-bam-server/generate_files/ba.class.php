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

require_once dirname(__FILE__) . '/subclass/ba_command.class.php';
require_once dirname(__FILE__) . '/subclass/ba_timeperiod.class.php';
require_once dirname(__FILE__) . '/subclass/ba_host.class.php';
require_once dirname(__FILE__) . '/subclass/ba_service.class.php';
require_once dirname(__FILE__) . '/subclass/ba_contactgroup.class.php';
require_once dirname(__FILE__) . '/subclass/ba_contact.class.php';
require_once dirname(__FILE__) . '/subclass/ba_dependency.class.php';
require_once dirname(__FILE__) . '/subclass/ba_escalation.class.php';
require_once dirname(__FILE__) . '/lib/backend_ba.class.php';

class Ba extends AbstractObject {
    public function generateFromPollerId($poller_id, $localhost) {
        if (!BackendBa::getInstance()->hasBa()) {
            return 0;
        }

        BaCommand::getInstance()->generateObjects();
        BaTimeperiod::getInstance()->generateObjects();
        BaHost::getInstance()->generateObjects();
        BaService::getInstance()->generateObjects($localhost);
        BaContactgroup::getInstance()->generateObjects();
        BaDependency::getInstance()->generateObjects($localhost);
        BaEscalation::getInstance()->generateObjects($localhost);
    }

    public function reset() {
        BaCommand::getInstance()->reset();
        BaTimeperiod::getInstance()->reset();
        BaHost::getInstance()->reset();
        BaService::getInstance()->reset();
        BaContactgroup::getInstance()->reset();
        BaContact::getInstance()->reset();
        BaDependency::getInstance()->reset();
        BaEscalation::getInstance()->reset();
    }
}
