<?php
/**
 * CENTREON
 *
 * Source Copyright 2005-2015 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */

namespace CentreonExport;

class DiscoveryValidator
{
    static $aTypeValidator = array(
        'OID' => 'sliceAvanced',
        'TCP' => 'sliceBasic',
        'UDP' => 'sliceBasic'
    );
    
    static $sCommandSeparator = ";;";
    
    static $sAttributSeparator = ',';
    
    static $sValueSeparator = '=';
    
    static $aDatas = array();
    
    
    /**
     * 
     * @param string $sListCommand
     */
    static function sliceDiscoveryValidator($sListCommand)
    {
        $sListCommand = trim($sListCommand);
        if (empty($sListCommand)) {
            return "";
        }
        
        $aCommands = explode(static::$sCommandSeparator, $sListCommand);
        
        foreach ($aCommands as $key => $command) {
            $sName = strtoupper(substr($command, 0, 3));
            $sCommand = substr($command, 4);
            
            if (array_key_exists($sName, static::$aTypeValidator)) {
                $method = static::$aTypeValidator[$sName];
                static::$aDatas[$sName] = self::$method($sName, $sCommand);
            }
        }
        
        return static::DiscoveryValidatorToArray(static::$aDatas);
    }
    
    /**
     * 
     * @param string $sName
     * @param string $oneCommand
     * @return array
     */
    static public function sliceBasic($sName, $oneCommand)
    {
        $aArray = explode(static::$sAttributSeparator, $oneCommand);
        return $aArray;
    }
    
    /**
     * 
     * @param string $sName
     * @param string $oneCommand
     * @return array
     */
    static public function sliceAvanced($sName, $oneCommand)
    {
        $aArray = explode(static::$sValueSeparator, $oneCommand);
        return array(strtolower($sName) => $aArray[0], 'match' => $aArray[1]);
        
    }
    
    /**
     * 
     * @return array
     */
    static function DiscoveryValidatorToArray($aDatas)
    {        
        $aFinal = array();
        if (count($aDatas) > 0) {
            foreach (array_keys(static::$aTypeValidator) as $validator) {
                if (array_key_exists($validator, $aDatas)) {
                    if (static::$aTypeValidator[$validator] == 'sliceBasic') {
                        $aElement = array();
                        foreach (array_values($aDatas[$validator]) as $value) {
                            $aElement[] = array('port' => $value);
                        };      
                        $aFinal['check_'.strtolower($validator)] = $aElement;
                    } elseif (static::$aTypeValidator[$validator] == 'sliceAvanced') {     
                        $aFinal['check_'.strtolower($validator)] = $aDatas[$validator];
                    }
                }
            }
        }
        return $aFinal;
    }
}
