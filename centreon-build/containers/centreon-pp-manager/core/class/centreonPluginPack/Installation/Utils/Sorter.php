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
namespace CentreonPluginPackManager\Installation\Utils;

/**
 * Description of Sorter
 *
 * @author lionel
 */
class Sorter
{
    /**
     *
     * @var array 
     */
    private $dataToSort;
    
    /**
     * 
     */
    public function __construct()
    {
        ;
    }
    
    /**
     * 
     * @return array
     */
    public function getDataToSort()
    {
        return $this->dataToSort;
    }
    
    /**
     * 
     * @param array $newDataToSort
     */
    public function setDataToSort($newDataToSort)
    {
        $this->dataToSort = $newDataToSort;
    }

    /**
     * 
     * @param array $reverse
     * @throws \Exception
     */
    public function performSort($reverse = false)
    {
        if (isset($this->dataToSort)) {

            $datassorted = array();

            $datas = $this->dataToSort;
            
            $max_loop_iterations = 10000;
            $i=0;

            while (count($datas)) {
                // Simple code for if templates have wrong dependencies (ie : A needs B and B needs A)
                if ($i++ == $max_loop_iterations) {
                    throw new \Exception("Too many iterations for sorted templates in plugin pack JSON");
                    break;
                }

                foreach ($datas as $templateKey => $template) {
                    // No dependancy on the template
                    if (!isset($template['parent_template']) || empty($template['parent_template'])) {
                        $datassorted[] = $template;
                        unset($datas[$templateKey]);
                        break;
                    }

                    if (! is_array($template['parent_template'])) {
                        $template_arr = array($template['parent_template']);
                    } else {
                        $template_arr = $template['parent_template'];
                    }

                    foreach ($template_arr as $template_parent_template) {
                        // If no exist in pluginPackJson, add
                        if (!$this->templateExistsInTemplates($template_parent_template, $this->dataToSort)) {
                            $datassorted[] = $template;
                            unset($datas[$templateKey]);
                            break;
                        }

                        // Check dependancy of the template (check if dependency is OK)
                        if ($this->templateExistsInTemplates($template_parent_template, $datassorted)) {
                            $datassorted[] = $template;
                            unset($datas[$templateKey]);
                            break;
                        }
                    }
                }
            }
            
            // Write override the object
            if ($reverse) {
                $datassorted = array_reverse($datassorted);
            }
            $this->dataToSort = $datassorted;
        }

    }
    
    /**
     * Check if the template exists in templates
     * @param string $template_name
     * @param array $datas
     * @param string $keyInJsonToSort
     * @return boolean
     */
    public function templateExistsInTemplates($template_name, $datas)
    {
        foreach ($datas as $data) {
            if (isset($data['name']) && $template_name == $data['name']) {
                return true;
            }
        }
        return false;
    }
}
