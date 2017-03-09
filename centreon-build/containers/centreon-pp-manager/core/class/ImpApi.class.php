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

require_once MODULE_PATH . '/core/class/Fingerprint.class.php';

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
     */
    public function __construct($dbconn, $impUrl, $fpLib = null, $restHttpLib = null)
    {
        $this->dbconn = $dbconn;
        $this->baseImpUrl = $impUrl;
        if (is_null($fpLib)) {
            $this->fingerprintLib = new \Fingerprint();
        } else {
            $this->fingerprintLib = $fpLib;
        }
        if (is_null($restHttpLib)) {
            $this->restHttpLib = new \CentreonRestHttp();
        } else {
            $this->restHttpLib = $restHttpLib;
        }
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
        if ($row === false || $row['value'] === '') {
            throw new LengthException('Error during getting IMP Company Token.');
        }
        return $row['value'];
    }

    /**
     * Check subscription validity
     *
     */
    public function checkSubscription()
    {
        /* Call IMP api for get the subscription status */
        $url = $this->baseImpUrl . '/license-manager/instance/subscription';

        $headers = array();
        try {
            $this->authenticate();
            $headers = array('centreon-imp-token: ' . $this->token);
        } catch (Exception $e) {
            if (!$e instanceof LengthException) {
                throw $e;
            }
        }

        $subscription = false;
        try {
            $returnData = $this->restHttpLib->call(
                $url,
                'GET',
                null,
                $headers
            );

            if (isset($returnData['subscription']) && count($returnData['subscription'])) {
                $subscription = true;
            }
        } catch (\Exception $e) {
            $subscription = false;
        }
        return $subscription;
    }

    /**
     * Get the list of pluginpack
     *
     * @param array $pagination The pagination
     * @param array $aFilters Filters
     * @param int $offset The slug offset
     * @return array The list for pluginpack
     * @throws Exception
     */
    public function getListPluginPack($pagination, $aFilters, $offset = 0)
    {
        $url = $this->baseImpUrl . '/pluginpack/pluginpack';

        /* Add pagination */
        $url .= '?page[offset]=' . $pagination['offset'];
        $url .= '&page[limit]=' . $pagination['limit'];

        $searchFilters = str_replace(' ', ',', $aFilters['search']);

        if (!empty($aFilters['search'])) {
            $url .= '&filter[search]=' . $searchFilters;
        }
        if (!empty($aFilters['category'])) {
            $url .= '&filter[category]=' . $aFilters['category'];
        }
        if (!empty($aFilters['status']) && strtolower($aFilters['status']) != 'all') {
            $url .= '&filter[status]=' . $aFilters['status'];
        }
        if (!empty($aFilters['lastUpdate']) && !empty($aFilters['operator'])) {
            $url .= '&filter[release_date.' . $aFilters['operator'] . ']=' . $aFilters['lastUpdate'];
        }

        /*
         * Manage too long URL
         * If there are too many slugs, the query is exploded
         * Currently, only 100 slugs can be sent
        */
        $tooManySlugs = false;
        if (!empty($aFilters['slugs'])) {
            $slugs = array_slice($aFilters['slugs'], $offset, 100);
            $offset += 100;
            if ($offset < count($aFilters['slugs'])) {
                $tooManySlugs = true;
            }
            $url .= '&filter[slugs]=' . implode(',', $slugs);
        }

        /* Build HTTP header*/
        $headers = array();
        try {
            $this->authenticate();
            $headers = array('centreon-imp-token: ' . $this->token);
        } catch (Exception $e) {
            if (!$e instanceof LengthException) {
                throw $e;
            }
        }

        /* Call middleware */
        $returnData = $this->restHttpLib->call(
            $url, 'GET', null, $headers
        );

        /* Use recursion if URL is too long */
        if ($tooManySlugs) {
            $nextReturnData = $this->getListPluginPack($pagination, $aFilters, $offset);
            $returnData['data'] = array_merge($returnData['data'], $nextReturnData['pluginpack']);
        }

        return array('pluginpack' => $returnData['data']);
    }

    /**
     * Get pluginpack informations
     *
     * @param int $pluginPackId The plugin pack id
     * @param int $pluginPackVersion The plugin pack version
     * $param string $action The action
     * @return array The pluginpack information
     * @throws Exception
     */
    public function getPluginPack($pluginPackId, $pluginPackVersion, $action = '')
    {
        $url = $this->baseImpUrl . '/pluginpack/pluginpack/' . $pluginPackId . '/' . $pluginPackVersion . '/data';

        if (!empty($action)) {
            $url .= "?action=$action";
        }

        $headers = array();
        try {
            $this->authenticate();
            $headers = array('centreon-imp-token: ' . $this->token);
        } catch (Exception $e) {
            if (!$e instanceof LengthException) {
                throw $e;
            }
        }

        $returnData = $this->restHttpLib->call(
            $url, 'GET', null, $headers
        );

        return $returnData['data'];
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
}
