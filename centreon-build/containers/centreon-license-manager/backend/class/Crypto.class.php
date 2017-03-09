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
class Crypto
{
    /**
     * Get a random string of n characters
     *
     * @param int $nbCharacters The lenght of random string
     * @return string The random string
     */
    public function randomString($nbCharacters = 10)
    {
        $number = '0123456789';
        $chars = 'qwertyuiopasdfghjklzxcvbnm';
        $fullchars = $number . $chars . strtoupper($chars);
        return substr(str_shuffle($fullchars), 0, $nbCharacters);
    }

    /**
     * Hash a string to sha256
     *
     * @param string $string The string to hash
     * @return string The hashed string
     */
    public function sha256($string)
    {
        return hash('sha256', $string);
    }
}
