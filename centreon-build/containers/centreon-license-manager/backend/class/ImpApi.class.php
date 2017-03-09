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

/**
 * Class for call IMP API
 *
 * @author Centreon
 * @version 1.0.0
 * @package centreon-license-manager
 */
class ImpApi
{
    /**
     * @var CentreonDB The database connection
     */
    private $dbconn;
    /**
     * @var string The base url for IMP API endpoint
     */
    private $baseImpUrl;
    /**
     * @var Fingerprint The library for get the fingerprint
     */
    private $fingerprintLib;
    /**
     * @var CentreonRestHttp The library for call HTTP REST API
     */
    private $restHttpLib;
    /**
     * @var System The library for system call
     */
    private $systemLib;
    /**
     * @var Translate The library for translation
     */
    private $translateLib;
    /**
     * @var string The authentication token
     */
    private $token = null;

    /**
     * Constructor
     *
     * @param CentreonDB $dbconn The database connection
     * @param string $impUrl The url for IMP Api
     * @param Fingerprint $fpLib The fingerprint lib
     * @param CentreonRestHttp $restHttpLib The HTTP REST lib
     * @param System $systemLib The system call lib
     * @param Translate $translateLib The translation lib
     */
    public function __construct(
        $dbconn,
        $impUrl,
        $fpLib = null,
        $restHttpLib = null,
        $systemLib = null,
        $translateLib = null
    ) {
        $this->dbconn = $dbconn;
        $this->baseImpUrl = $impUrl;
        if (is_null($fpLib)) {
            $this->fingerprintLib = new Fingerprint();
        } else {
            $this->fingerprintLib = $fpLib;
        }
        if (is_null($restHttpLib)) {
            $this->restHttpLib = new CentreonRestHttp('application/json', 'license-manager.log');
        } else {
            $this->restHttpLib = $restHttpLib;
        }
        if (is_null($systemLib)) {
            $this->systemLib = new System();
        } else {
            $this->systemLib = $systemLib;
        }
        if (is_null($translateLib)) {
            $this->translateLib = new Translate();
        } else {
            $this->translateLib = $translateLib;
        }
    }

    /**
     * Register the current instance on IMP
     *
     * @param string $email The email from IMP
     * @param string $password The password from IMP
     * @return bool If the registrastion is valid
     */
    public function registerInstance($email, $password)
    {
        /* Call IMP api for register the instance */
        $url = $this->baseImpUrl . '/license-manager/instance';
        $method = 'POST';
        $data = array(
            'email' => $email,
            'password' => $password,
            'fingerprint' => $this->fingerprintLib->getFingerprint(),
            'name' => $this->systemLib->getHostname()
        );

        $returnData = $this->restHttpLib->call($url, $method, $data);

        /* Save the company token */
        $this->saveCompanyToken($returnData['company']);

        return true;
    }

    /**
     * Get the current register status
     *
     * @return array The status step
     */
    public function getStatus()
    {
        /* Test if this instance has a company token */
        try {
            $this->getCompanyToken();
        } catch (LengthException $e) {
            return array(
                'step' => 1,
                'message' => 'This instance is not registered to IMP.'
            );
        }

        $this->authenticate();

        /* Call IMP api for get the subscription status */
        $url = $this->baseImpUrl . '/license-manager/instance/subscription';
        $method = 'GET';

        try {
            $returnData = $this->restHttpLib->call($url, $method, null, array('centreon-imp-token: ' . $this->token));
        } catch (RestNotFoundException $e) {
            return array(
                'step' => 2,
                'message' => 'This instance has not a subscription.'
            );
        }

        if (!$this->checkLicense()) {
            return array(
                'step' => 3,
                'message' => 'This instance has not a license.'
            );
        }

        return array(
            'step' => 3,
            'message' => 'This instance has a license.'
        );
    }

    /**
     * Get the company token from database
     *
     * @throws Exception Cannot get the company token
     * @return string The company token
     */
    private function getCompanyToken()
    {
        $query = 'SELECT `value` FROM `options` WHERE `key` = "impCompanyToken"';
        $res = $this->dbconn->query($query);
        if (PEAR::isError($res)) {
            throw new Exception('Error during getting IMP Company Token.');
        }
        $row = $res->fetchRow();
        if (is_null($row) || $row['value'] === '') {
            throw new LengthException('Error during getting IMP Company Token.');
        }
        return $row['value'];
    }

    /**
     * Save the company token for this instance
     *
     * @throws Exception This instance is already register.
     * @throws Exception Error during save the company token.
     * @param string $token The company token
     */
    private function saveCompanyToken($token)
    {
        /* Verify if the company is already saved */
        $companyToken = null;
        try {
            $companyToken = $this->getCompanyToken();
        } catch (Exception $e) {
        }
        if (false === is_null($companyToken)) {
            throw new Exception('This instance is already registered.');
        }

        /* Save the company tokem */
        $query = 'UPDATE `options` SET `value` = "' .
            $this->dbconn->escape($token) .
            '" WHERE `key` = "impCompanyToken"';
        if (PEAR::isError($this->dbconn->query($query))) {
            throw new Exception('Error during save the company token.');
        }
    }

    /**
     * Authenticate this instance to IMP
     *
     * @throws Exception Authentication error
     */
    protected function authenticate()
    {
        $url = $this->baseImpUrl . '/auth/instance';
        $method = 'POST';
        $data = array(
            'fingerprint' => $this->fingerprintLib->getAuth(),
            'companyToken' => $this->getCompanyToken()
        );
        $returnData = $this->restHttpLib->call($url, $method, $data);
        if (!isset($returnData['token'])) {
            throw new Exception('Error during authentication.');
        }
        $this->token = $returnData['token'];
    }

    /**
     * Get valid subscription list which are not assigned
     *
     * @return array The subscription list
     */
    public function getSubscription()
    {
        $this->authenticate();

        /* Call IMP api to get the subscription list */
        $url = $this->baseImpUrl . '/license-manager/company/subscription';
        $method = 'GET';
        $returnData = $this->restHttpLib->call($url, $method, null, array('centreon-imp-token: ' . $this->token));

        /* Convert data for return */
        $subscriptions = array();
        foreach ($returnData['data'] as $subscription) {
            list($type, $nbvalue, $value) = explode('_', $subscription['attributes']['type']);
            $subscriptions[] = array(
                'id' => $subscription['id'],
                'name' => sprintf(
                    "Centreon %s %s %d %s",
                    $type,
                    $this->translateLib->getTranslate('for'),
                    $nbvalue,
                    strtolower($this->translateLib->getTranslate($value))
                ),
                'period' => sprintf(
                    "%s %s %s %s",
                    $this->translateLib->getTranslate('from'),
                    date_format(
                        date_create($subscription['attributes']['start']),
                        $this->translateLib->getTranslate('Y/m/d')
                    ),
                    $this->translateLib->getTranslate('to'),
                    date_format(
                        date_create($subscription['attributes']['end']),
                        $this->translateLib->getTranslate('Y/m/d')
                    )
                )
            );
        }

        return array(
            'subscriptions' => $subscriptions
        );
    }

    /**
     * Set a subscription for the instance
     *
     * @param int subscriptionId The subscription id
     * @return array
     */
    public function setSubscription($subscriptionId)
    {
        $this->authenticate();

        /* Call IMP api for subscribe */
        $url = $this->baseImpUrl . '/license-manager/instance/subscription';
        $method = 'PATCH';
        $returnData = $this->restHttpLib->call(
            $url,
            $method,
            array(
                'data' => array(
                    'id' => $this->fingerprintLib->getFingerprint(),
                    'type' => 'instance',
                    'attributes' => array(
                        'subscription_id' => $subscriptionId
                    )
                )
            ),
            array('centreon-imp-token: ' . $this->token)
        );

        return array(
            'success' => true
        );
    }

    /**
     * Get a license for the instance
     *
     * @return array
     */
    public function getLicense()
    {
        $this->authenticate();

        /* Call IMP api for subscribe */
        $url = $this->baseImpUrl . '/license-manager/instance/license';
        $method = 'GET';
        $returnData = $this->restHttpLib->call($url, $method, null, array('centreon-imp-token: ' . $this->token));

        if (!isset($returnData['licenseRaw'])) {
            throw new RestConflictException('Cannot get license.');
        }

        if (!is_dir(_CENTREON_LICENSE_PATH_)) {
            throw new RestConflictException('Cannot find license directory.');
        }

        $licenseCreation = $this->systemLib->filePutContents(
            _CENTREON_LICENSE_PATH_ . '/centreon-automation.license',
            $returnData['licenseRaw']
        );
        if ($licenseCreation === false) {
            throw new RestConflictException('Cannot write license file.');
        }

        return array(
            'success' => true
        );
    }

    /**
     * Check license for the instance
     *
     * @return array
     */
    public function checkLicense()
    {
        return $this->systemLib->fileExists(_CENTREON_LICENSE_PATH_ . '/centreon-automation.license');
    }

    /**
     * Get customer linked to the instance
     *
     * @return array
     */
    public function getCustomer()
    {
        $this->authenticate();

        /* Call IMP api for subscribe */
        $url = $this->baseImpUrl . '/license-manager/instance/customer';
        $method = 'GET';
        $returnData = $this->restHttpLib->call($url, $method, null, array('centreon-imp-token: ' . $this->token));

        if (!isset($returnData['data'])) {
            throw new RestConflictException('Cannot get customer.');
        }

        return array(
            'customer' => $returnData['data']
        );
    }
}
