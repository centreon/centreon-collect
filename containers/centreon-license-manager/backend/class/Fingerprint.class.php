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
 * Class contains method for generate and get fingerprint
 *
 * @author Centreon
 * @version 1.0.0
 * @package centreon-license-manager
 */
class Fingerprint
{
    private $systemLib;
    private $cryptoLib;
    
    /**
     * The constructor
     *
     * @param System $systemLib The system library
     * @param Crypto $cryptoLib The crypto library
     */
    public function __construct($systemLib = null, $cryptoLib = null)
    {
        $this->systemLib = $systemLib;
        $this->cryptoLib = $cryptoLib;
        if (is_null($this->systemLib)) {
            $this->systemLib = new System();
        }
        if (is_null($this->cryptoLib)) {
            $this->cryptoLib = new Crypto();
        }
    }
    
    /**
     * Generate and get fingerprint
     *
     * @return string The machine fingerprint
     */
    public function getFingerprint()
    {
        $os = $this->getOs();
        $commandName = 'os' . ucfirst($os);
        $macs = call_user_func(array($this, $commandName));
        $macs = array_map(array($this, 'cleanupMac'), $macs);
        $macs = array_map(array($this, 'hex2bin'), $macs);
        sort($macs);
        $stringMacs = join('', $macs);
        return $this->cryptoLib->sha256($stringMacs);
    }
    
    /**
     * Get an authentication string for authenticate by fingerprint
     *
     * @return string The string
     */
    public function getAuth()
    {
        $fp = $this->getFingerprint();
        $salt = $this->cryptoLib->randomString(10);
        return $salt . $this->cryptoLib->sha256($salt . $fp);
    }
    
    /**
     * Parse return output for getting mac address
     *
     * @return array The list of mac address
     */
    private function osLinux()
    {
        $addresses = array();
        if (false === $this->systemLib->isDir('/sys/class/net')) {
            throw new Exception('Error during getting fingerprint.');
        }
        $files = $this->systemLib->glob('/sys/class/net/*/address');
        foreach ($files as $file) {
            $mac = trim($this->systemLib->fileGetContents($file));
            if ($mac !== '00:00:00:00:00:00') {
                $addresses[] = $mac;
            }
        }
        if (count($addresses) === 0) {
            throw new Exception('Error during getting fingerprint.');
        }
        return $addresses;
    }
    
    /**
     * Get current OS
     *
     * @return string The os typw
     */
    private function getOs()
    {
        if (stristr(PHP_OS, 'LINUX')) {
            return 'linux';
        }
        throw new Exception('Operating system if not supported.');
    }
    
    /**
     * Run command
     *
     * @param string $cmd The command to execute
     * @return string The command output
     */
    private function runCmd($cmd)
    {
        try {
            return $this->systemLib->runCmd($cmd);
        } catch (Exception $e) {
            throw new Exception('Error during getting fingerprint.');
        }
    }
    
    /**
     * Cleanup the mac address
     *
     * Remove :  and to lower the string
     *
     * @param string $mac The mac address to cleanup
     * @return string The cleanup mac address
     */
    private function cleanupMac($mac)
    {
        return strtolower(str_replace(':', '', $mac));
    }
    
    /**
     * Convert hexa to binary
     *
     * @param string $string The string to convert
     * @return string The string converted
     */
    private function hex2bin($string)
    {
        $n = strlen($string);
        $sbin = '';
        for ($i = 0; $i < $n; $i += 2) {
            $a = substr($string, $i, 2);
            $c = pack('H*', $a);
            $sbin .= $c;
        }
        return $sbin;
    }
}
