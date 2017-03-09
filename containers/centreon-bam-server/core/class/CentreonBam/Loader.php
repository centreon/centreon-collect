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

/**
 * 
 * @author Sylvestre HO
 * Object that will load all other required php class files
 */
class CentreonBam_Loader
{
    protected $_namespace;
    protected $_dir;
    
    /**
     * 
     * @param $dir
     * @param $namespace
     * @return unknown_type
     */
    public function __construct($dir, $namespace = 'CentreonBam_')
    {
        $this->_dir = rtrim($dir, "/");
        $this->_namespace = $namespace;
        $this->_load();
    }
    
    /**
     * Load files that are within the specified directory
     */
    protected function _load()
    {
        if (!is_dir($this->_dir)) {
            throw new Exception(sprintf("%s is not a directory", $this->_dir));
        }
        $handle = opendir($this->_dir);        
        while (false !== ($file = readdir($handle))) {
            if (($classSuffix = $this->_isPhpFile($file)) !== null) {                
                if (!class_exists($this->_namespace . $classSuffix)) {
                    if (!file_exists($this->_dir . "/" . $file)) {
                        throw new Exception(sprintf("% does not exist", $this->_dir . "/" . $file));
                    }
                    require_once $this->_dir . "/" . $file;
                }
            }
        }
        closedir($handle);
    }
    
    /**
     * Returns filename without extension if file is php file
     * Otherwise, returns null    
     */
    protected function _isPhpFile($fileName)
    {        
        $tab = explode(".", $fileName);        
        if ($fileName != "." && $fileName != ".." && is_array($tab) && isset($tab[1])) {
            if (strtolower($tab[1]) == "php") {
                return $tab[0];
            }
        }
        return null;
    }
}
?>