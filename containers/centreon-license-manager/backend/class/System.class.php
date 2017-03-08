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
 * Utils class for system call
 *
 * @author Centreon
 * @version 1.0.0
 * @package centreon-license-manager
 */
class System
{
    /**
     * Get the current hostname
     *
     * @return string The current hostname
     */
    public function getHostname()
    {
        return gethostname();
    }
    
    /**
     * Save the content to a file
     *
     * @param string $filename The filename to put content
     * @param string $data The data to put in file
     * @return boolean If the file is correctly write
     */
    public function filePutContents($filename, $data)
    {
        return @file_put_contents($filename, $data) !== false;
    }

    /**
     * Run a system command
     *
     * @param string $cmd The command to execute
     * @return string The command output
     */
    public function runCmd($cmd)
    {
        $output = shell_exec($cmd);
        if ($output === null) {
            throw new Exception('Error during execute command.');
        }
        return $output;
    }

    /**
     * Mock file_exists function
     *
     * @param string $filepath file to test
     * @return bool If the file exists
     */
    public function fileExists($filepath)
    {
        return @file_exists($filepath);
    }
    
    /**
     * Get the content to a file
     *
     * @param string $filename The filename to get content
     * @return string The content of the file
     */
    public function fileGetContents($filename)
    {
        return @file_get_contents($filename);
    }
    
    /**
     * Test if a path is a directory and readable
     *
     * @param string $dir The path to test
     * @return bool If the path is a directory
     */
    public function isDir($dir)
    {
        return is_dir($dir) && is_readable($dir);
    }
    
    /**
     * Get the list of file matching to pattern
     *
     * @param string $pattern The pattern to match
     * @return array The list of files
     */
    public function glob($pattern)
    {
        return glob($pattern);
    }
}
