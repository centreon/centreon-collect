<?php
/**
 * Copyright 2016 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

require_once realpath(dirname(__FILE__) . '/../../centreon-license-manager.conf.php');
require_once _CENTREON_PATH_ . "/www/api/class/webService.class.php";
require_once _CENTREON_PATH_ . "/www/class/centreonRestHttp.class.php";
require_once _MODULE_PATH_ . '/backend/class/System.class.php';
require_once _MODULE_PATH_ . '/backend/class/Crypto.class.php';
require_once _MODULE_PATH_ . '/backend/class/ImpApi.class.php';
require_once _MODULE_PATH_ . '/backend/class/Fingerprint.class.php';
require_once _MODULE_PATH_ . '/backend/class/Translate.class.php';

/**
 * Class for endpoints of centreon license manager
 * @author Centreon
 * @version 1.0.0
 * @package centreon-license-manager
 */
class CentreonLicenseManager extends CentreonWebService
{
    protected $db;
    protected $impApi;

    /**
     * Constructor
     */
    public function __construct()
    {
        $this->db = new CentreonDB();
        $this->impApi = new ImpApi($this->db, CENTREON_IMP_API_URL);
        parent::__construct();
    }
    
    /**
     * Action register instance
     **/
    public function postRegisterInstance()
    {
        if (!isset($this->arguments['username'])
            || !isset($this->arguments['password'])) {
            throw new RestBadRequestException('Missing arguments');
        }

        try {
            $this->impApi->registerInstance(
                $this->arguments['username'],
                $this->arguments['password']
            );
        } catch (RestNotFoundException $e) {
            throw new RestNotFoundException(_('Cannot access to IMP portal'));
        } catch (RestUnauthorizedException $e) {
            throw new RestUnauthorizedException(_('Invalid credentials'));
        }

        return array('success' => true);
    }
    
    /**
     * Step position for subscribe
     */
    public function getStepStatus()
    {
        try {
            return $this->impApi->getStatus();
        } catch (RestNotFoundException $e) {
            throw new RestNotFoundException(_('Cannot access to IMP portal'));
        }
    }

    /**
     * Get available subscription list
     */
    public function getSubscription()
    {
        try {
            return $this->impApi->getSubscription();
        } catch (RestNotFoundException $e) {
            throw new RestNotFoundException(_('Cannot access to IMP portal'));
        }
    }
    
    /**
     * Set a subscription to the instance
     */
    public function patchSubscription()
    {
        /* Get parameters */
        if (!isset($this->arguments['subscriptionId'])) {
            throw new RestBadRequestException('Missing arguments');
        }
        /* Test arguments */
        if (!is_int($this->arguments['subscriptionId'])) {
            throw new RestBadRequestException('Bad type arguments');
        }
        
        return $this->impApi->setSubscription($this->arguments['subscriptionId']);
    }

    /**
     * Get license for the instance
     */
    public function getLicense()
    {
        return $this->impApi->getLicense();
    }

   /**
     * Get customer linked to the instance
     */
    public function getCustomer()
    {
        return $this->impApi->getCustomer();
    }
}
