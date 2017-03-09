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

/**
 * Description of PluginPackManagerFactory
 *
 * @author lionel
 */
class PluginPackManagerFactory
{
    /**
     *
     * @var type 
     */
    private $dbconn;
    
    /**
     * 
     * @param type $dbconn
     */
    public function __construct($dbconn)
    {
        $this->dbconn = $dbconn;
    }
    
    /**
     * 
     * @param string $impUrl
     * @param type $fpLib
     * @param type $restHttpLib
     * @return \ImpApi
     */
    public function newImpApi($impUrl, $fpLib = null, $restHttpLib = null)
    {
        return new ImpApi($this->dbconn, $impUrl, $fpLib, $restHttpLib);
    }

    /**
     *
     * @return \EmsApi
     */
    public function newEmsApi()
    {
        return new EmsApi();
    }
}
