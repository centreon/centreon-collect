<?php
/**
 * CENTREON
 *
 * Source Copyright 2016 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */

namespace CentreonPluginPackManager\Installation\Utils;

use CentreonPluginPackManager\Installation\Exception\InvalidDependencyException;
use CentreonPluginPackManager\Installation\Exception\DependencyNotExistedException;
use CentreonPluginPackManager\Installation\Exception\SchemaNotReadableException;

/**
 * Dependencies sorter
 *
 * @author Emmanuel Haguet <ehaguet@centreon.com>
 */
class DependencySorter
{

    /**
     * @var CentreonPluginPack_iProvider
     */
    private $pluginPackProvider;

    /**
     * @var array
     */
    private $checkedDependencies;

    /**
     * @var array
     */
    private $sortedDependencies;

    /**
     * @var string
     */
    private $keyInJson;

    /**
     * DependencySorter constructor.
     * @param CentreonPluginPack_iProvider|null $pluginPackProvider
     */
    public function __construct($pluginPackProvider = null)
    {
        $this->keyInJson = 'schema_version';
        $this->pluginPackProvider = $pluginPackProvider;
        $this->checkedDependencies = array();
        $this->sortedDependencies = array();
    }

    /**
     *
     * @return string
     */
    public function getKeyInJson()
    {
        return $this->keyInJson;
    }

    /**
     * Get sorted dependencies
     *
     * @param $pp  array('slug' => '', 'name' => '', 'version' => '')
     * @return array sorted plugin packs
     * @throws InvalidDependencyException
     * @throws SchemaNotReadableException
     * @throws DependencyNotExistedException
     */
    public function getSortedDependencies($pp)
    {
        /* Retrocompatibility */
        $pp['slug'] = isset($pp['slug']) ? $pp['slug'] : $pp['name'];

        // Initialize dependency check
        if (!isset($this->checkedDependencies[$pp['slug']])) {
            $this->checkedDependencies[$pp['slug']] = array(
                'temporary' => false,
                'marked' => false,
                'version' => '0.0.0'
            );
        }

        if ($this->checkedDependencies[$pp['slug']]['temporary']) {
            throw new InvalidDependencyException("Dependency loop found when processing PP : " . $pp['slug']);
        }

        // if n is not marked (i.e. has not been visited yet)
        if (!$this->checkedDependencies[$pp['slug']]['marked']) {
            // mark n temporarily
            $this->checkedDependencies[$pp['slug']]['temporary'] = true;

            $pluginPackContent = $this->pluginPackProvider->getPluginPackJsonFileOfLastVersion($pp);

            /* Retrocompatibility of schema version */
            $pluginPackContent[$this->keyInJson] = isset($pluginPackContent[$this->keyInJson]) ?
                $pluginPackContent[$this->keyInJson] : 0;

            // Check if schema version is readable
            if ($pluginPackContent[$this->keyInJson] > 1) {
                throw new SchemaNotReadableException(
                    'Cannot upgrade PP ' . $pp['slug'] . ' : You need to upgrade PPM.'
                );
            }

            // for each node m with an edge from n to m do
            foreach ($pluginPackContent['information']['dependencies'] as $dependency) {
                $this->getSortedDependencies($dependency);

                $dependency_slug = isset($dependency['slug']) ? $dependency['slug'] : $dependency['name'];

                // Retrocompatibility.
                if (!isset($dependency['slug'])) {
                    $dependency['slug'] = $dependency['name'];
                }

                $pluginPackContentOfDependency =
                    $this->pluginPackProvider->getPluginPackJsonFileOfLastVersion($dependency);

                $dependency_version = $pluginPackContentOfDependency['information']['version'];

                // Check if version minimal is respected
                if (version_compare($dependency['version'], $dependency_version, '>')) {
                    throw new DependencyNotExistedException(
                        "Could not find dependency " . $dependency_slug . " >= " . $dependency['version']
                        . " for PP " . $pp['slug'] . " : Most recent version is " . $dependency_version
                    );
                }
            }

            // mark n permanently
            $this->checkedDependencies[$pp['slug']]['marked'] = true;

            // unmark n temporarily
            $this->checkedDependencies[$pp['slug']]['temporary'] = false;

            // add n to head of L
            $this->sortedDependencies[] = array(
                'slug' => $pp['slug'],
                'version' => $pluginPackContent['information']['version']
            );

            return $this->sortedDependencies;
        }
    }
}
