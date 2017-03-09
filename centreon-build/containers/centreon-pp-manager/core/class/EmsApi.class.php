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
 * Class for EMS to discover on filesystem
 *
 * @author Centreon
 * @version 1.0.0
 * @package centreon-license-manager
 */
class EmsApi
{
    /**
     * Constructor
     *
     */
    public function __construct()
    {

    }

    /**
     *
     * @param string $path
     * @return array
     */
    public function getPluginPackFiles($path)
    {
        $pluginPackFiles = glob($path);

        $newestPluginPackFiles = array();
        foreach ($pluginPackFiles as $pluginPackFile) {
            $pluginPackFileName = basename($pluginPackFile);
            preg_match('/^(.*)-((\d+)|(\d+\.\d+)|(\d+\.\d+\.\d+))\.json$/', $pluginPackFileName, $matches);

            if (!isset($matches[1]) && !isset($matches[2])) {
                continue;
            }

            $pluginPackName = $matches[1];
            $pluginPackVersion = $matches[2];

            if (isset($newestPluginPackFiles[$pluginPackName])
                && isset($newestPluginPackFiles[$pluginPackName]['version'])
            ) {
                $comparedVersion = version_compare(
                    $pluginPackVersion,
                    $newestPluginPackFiles[$pluginPackName]['version'],
                    '>'
                );

                if ($comparedVersion == 1) {
                    $newestPluginPackFiles[$pluginPackName]['file'] = $pluginPackFile;
                    $newestPluginPackFiles[$pluginPackName]['version'] = $pluginPackVersion;
                }
            } else {
                $newestPluginPackFiles[$pluginPackName]['file'] = $pluginPackFile;
                $newestPluginPackFiles[$pluginPackName]['version'] = $pluginPackVersion;
            }
        }

        $lastPluginPackFiles = array();
        foreach ($newestPluginPackFiles as $name => $value) {
            $lastPluginPackFiles[$name] = $value['file'];
        }

        return $lastPluginPackFiles;
    }

    /**
     * Get plugin pack information
     *
     * @param int $path - The path of the plugin pack json file
     * @param array $aFilters
     * @return array
     * @throws Exception
     */
    public function readPluginPackFile($path, $aFilters)
    {
        $pluginPackInformation = array();

        $content = json_decode(file_get_contents($path), true);

        if (isset($content['information'])
            && isset($content['information']['name'])
            && isset($content['information']['slug'])
            && (isset($content['information']['icon']) || is_null($content['information']['icon']))
            && isset($content['information']['version'])
            && isset($content['information']['status'])
            && isset($content['information']['description'])
            && isset($content['information']['update_date'])
        ) {

            // Filter on slug
            if (isset($aFilters['slug']) && !empty($aFilters['slug']) &&
                !stristr($content['information']['slug'], $aFilters['slug'])) {
                return $pluginPackInformation;
            }

            // Filter on name, tag or description
            if (isset($aFilters['search']) && !empty($aFilters['search'])) {
                $searchFilters = preg_split('/,|\s+/', $aFilters['search']);
                $countDoNotMatch = 0;
                foreach ($searchFilters as $searchFilter) {
                    if (!stristr($content['information']['name'], $searchFilter) &&
                        !$this->array_key($content['information']['tags'], 'value', $searchFilter) &&
                        !$this->array_key($content['information']['description'], 'valueAssoc', $searchFilter)
                    ) {

                        $countDoNotMatch++;

                    }
                }

                if (count($searchFilters) == $countDoNotMatch) {
                    return $pluginPackInformation;
                }
            }


            // Filter on category
            if (isset($aFilters['category'])
                && !empty($aFilters['category'])
                && !stristr($content['information']['discovery_category'], $aFilters['category'])
            ) {
                return $pluginPackInformation;

            }

            // Filter on status
            if (isset($aFilters['status'])
                && !empty($aFilters['status'])
                && !stristr($content['information']['status'], $aFilters['status'])
            ) {
                return $pluginPackInformation;

            }

            // Filter on last update
            if (isset($aFilters['operator']) && isset($aFilters['lastUpdate'])
                && !empty($aFilters['operator']) && !empty($aFilters['lastUpdate'])
            ) {
                $checkedDate = false;
                $lastUpdateDatetime = \DateTime::createFromFormat('Y-m-d', $aFilters['lastUpdate']);
                if ($lastUpdateDatetime === false) {
                    throw new \Exception('Wrong date format on last update filter');
                }
                $lastUpdateFilter = $lastUpdateDatetime->getTimestamp();
                $content['information']['update_date'] = (int)$content['information']['update_date'];

                switch ($aFilters['operator']) {
                    case 'gt':
                        $checkedDate = ($content['information']['update_date'] > $lastUpdateFilter) ? true : false;
                        break;
                    case 'lt':
                        $checkedDate = ($content['information']['update_date'] < $lastUpdateFilter) ? true : false;
                        break;
                }

                if (!$checkedDate) {
                    return $pluginPackInformation;
                }
            }

            $pluginPackInformation = array(
                'name' => $content['information']['name'],
                'slug' => $content['information']['slug'],
                'icon_file' => $content['information']['icon'],
                'version' => $content['information']['version'],
                'status' => $content['information']['status'],
                'description' => $content['information']['description'],
                'installed' => false,
                'uptodate' => false,
                'can_be_installed' => true
            );
        }

        return $pluginPackInformation;
    }


    /**
     * Method to search keyword by key or value in array
     * @param $aArray array
     * @param $sType string
     * @param $sSearch string
     * @return bool
     *
     * if $sType = key => search in key
     * if $sType = value => search in value
     * if $sType = valueAssoc => (spacial case of array multidimensionnal) search in value
     */
    private function array_key($aArray, $sType, $sSearch)
    {
        if (is_array($aArray)) {
            if ($sType == 'value') {
                foreach ($aArray as $value) {
                    if (stristr($value, $sSearch)) {
                        return true;
                    }
                }
            } elseif ($sType == 'key') {
                foreach ($aArray as $key => $value) {
                    if (stristr($key, $sSearch)) {
                        return true;
                    }
                }
            } elseif ($sType == 'valueAssoc') {
                foreach ($aArray as $key => $array) {
                    foreach ($array as $value) {
                        if (stristr($value, $sSearch)) {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
}
