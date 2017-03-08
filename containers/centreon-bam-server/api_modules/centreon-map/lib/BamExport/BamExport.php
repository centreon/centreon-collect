<?php
/*
 * MERETHIS
 *
 * Source Copyright 2005-2009 MERETHIS
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@merethis.com
 *
*/

require_once 'Image/Graph.php';
require_once 'Image/Canvas.php';

define('REGULAR_TYPE', 0);
define('META_TYPE', 1);
define('BA_TYPE', 2);
define('LEVELMAX', 100);
define('LEVELMIN', 0);
define('DEFAULT_WIDTH', 600);
define('DEFAULT_HEIGHT', 400); 
    
/*
 *  This object builds the image
 */
class BamExport
{
    protected $_dbCentreon;
    protected $_dbBroker;
    protected $_ba;    
    protected $_kpi;
    protected $_width;
    protected $_height; 
    
    /*
     *  Constructor
     */
    public function __construct($ba_id)
    {
        if (!is_numeric($ba_id)) {
            throw new Exception('ID must be numeric');
        }        
        
        $this->_dbCentreon = new CentreonBAM_DB();
        $this->_dbBroker = new CentreonBAM_DB("ndo");
        $this->_ba = array();
        $this->_kpi = array();
        $this->_ba['id'] = $ba_id;
        $this->_initBa();
        $this->_initKpi(REGULAR_TYPE);
        $this->_initKpi(META_TYPE);
        $this->_initKpi(BA_TYPE);
        $this->_width = DEFAULT_WIDTH;
        $this->_height = DEFAULT_HEIGHT;
    }
    
    /*
     *  Sets width
     */
    public function setWidth($size)
    {
        if (!is_numeric($size)) {
            throw new Exception('Width must be numeric');
        }
        $this->_width = $size;
    }
    
    
    /*
     *  Sets height
     */
    public function setHeight($size)
    {
        if (!is_numeric($size)) {
            throw new Exception('Height must be numeric');
        }
        $this->_height = $size;
    }
    
    /*
     *  Renderer
     */
    public function render()
    {
        $canvas= Image_Canvas::factory('png', array('width'=>$this->_width,'height'=>$this->_height));
		$graph = Image_Graph::factory('graph', $canvas);
        $graph->setFontColor('#000000');        
		$graph->setBackgroundColor('#FFFFFF');
		$titleString = $this->_ba['name'] . ' : ' . $this->_ba['level'] . '%';
		$graph->add(
		    Image_Graph::vertical(
		        Image_Graph::factory('title', array($titleString, 12)),        
		        Image_Graph::vertical(
		            $plotArea = Image_Graph::factory('Image_Graph_Plotarea_Radar'),
		            $legend = Image_Graph::factory('legend', array('test', 5)),
		            80),
		    10)
		);   
		$legend->setPlotarea($plotArea);
		$plotArea->addNew('Image_Graph_Grid_Polar', IMAGE_GRAPH_AXIS_Y);
		$plotArea->setBackgroundColor('#FFFFFF');
		$ds1 = Image_Graph::factory('dataset');
		$ds2 = Image_Graph::factory('dataset');
		$ds3 = Image_Graph::factory('dataset');
		
		$cpt = 0;
		foreach ($this->_kpi as $key => $tab){
			$ds1->addPoint($this->_kpi[$key]["name"], $this->_kpi[$key]['criticalImpact']);			
			$ds2->addPoint($this->_kpi[$key]["name"], $this->_kpi[$key]['warningImpact']);
			$ds3->addPoint($this->_kpi[$key]["name"], $this->_kpi[$key]['level']);
			$cpt++;
		}
		
		if ($cpt < 3) {
			while ($cpt < 3){
				$ds1->addPoint(" ", 0);
				$ds2->addPoint(" ", 0);
				$ds3->addPoint(" ", 0);
				$cpt++;
			}
		}
		
		$plot1 = $plotArea->addNew('Image_Graph_Plot_Radar', $ds1);
		$plot1->setTitle('Critical (%)');	
		$plot1->setLineColor('red@1');    
		$plot1->setFillColor('red@0.5');
		
		$plot2 = $plotArea->addNew('Image_Graph_Plot_Radar', $ds2);
		$plot2->setTitle('Warning (%)');	
		$plot2->setLineColor('orange@1');
		$plot2->setFillColor('orange@0.6');

		$plot3 =& $plotArea->addNew('Image_Graph_Plot_Radar', $ds3);
		$plot3->setTitle('KPI Status (%)');	
		$plot3->setLineColor('green@1');    
		$plot3->setFillColor('green@0.7');
		
		$graph->done();
    }
    
    /*
     *  Debug mode
     */
    public function debug()
    {
        var_dump($this->_ba);
        print '<br/>======================================<br/>';
        var_dump($this->_kpi);
    }

    /*
     *  Initializes BA properties
     */
    protected function _initBa()
    {        
        $query = "SELECT * FROM mod_bam WHERE ba_id = '".$this->_ba['id']."' LIMIT 1";
        $res = $this->_dbCentreon->query($query);
        if (!$res->numRows()) {
            throw new Exception('Business Activity ID ('.$this->_ba['id'].') could not be found');
        }
        while ($row = $res->fetchRow()) {
            $this->_ba['name'] = $row['name'];
            $this->_ba['warningThreshold'] = $row['level_w'];
            $this->_ba['criticalThreshold'] = $row['level_c'];
            $this->_ba['level'] = $row['current_level'];
        }
    }
    
    /*
     *  Initializes KPI properties
     */
    protected function _initKpi($type = REGULAR_TYPE)
    {
        if ($type == REGULAR_TYPE) {
            $query = "SELECT h.host_name, s.service_description, kpi.kpi_id, ".
            		"kpi.drop_warning, kpi.drop_critical, kpi.drop_unknown, kpi.current_status " .
					"FROM mod_bam_kpi kpi, host h, service s " .
					"WHERE kpi.activate = '1' " .
					"AND kpi.id_ba = '" . $this->_ba['id'] . "' " .
                	"AND kpi.kpi_type = '0' " .
                	"AND kpi.host_id = h.host_id " .
                	"AND kpi.service_id = s.service_id";
        }
        elseif ($type == META_TYPE) {
            $query = "SELECT kpi.kpi_id, meta.meta_name, " .
                    "kpi.drop_warning, kpi.drop_critical, kpi.drop_unknown, kpi.current_status " .
					"FROM mod_bam_kpi kpi, meta_service meta " .
					"WHERE kpi.activate = '1' " .
					"AND kpi.id_ba = '" . $this->_ba['id'] . "' " .
                	"AND kpi.kpi_type = '1' " .
                	"AND kpi.meta_id = meta.meta_id";
        }
        elseif ($type == BA_TYPE) {
            $query = "SELECT kpi.kpi_id, ba.name, " .
                	"kpi.drop_warning, kpi.drop_critical, kpi.drop_unknown, kpi.current_status " .
					"FROM mod_bam_kpi kpi, mod_bam ba " .
					"WHERE kpi.activate = '1' " .
					"AND kpi.id_ba = '" . $this->_ba['id'] . "' " .
                	"AND kpi.kpi_type = '2' " .
                	"AND kpi.id_indicator_ba = ba.ba_id";
        }
        $res = $this->_dbCentreon->query($query);

        while ($row = $res->fetchRow()) {
            $kpiId = $row['kpi_id'];
            if ($type == REGULAR_TYPE) {
                $this->_kpi[$kpiId]['name'] = Utils::formatString($row['host_name'] . ' / ' . $row['service_description']);
            }
            elseif ($type == META_TYPE) {
                $this->_kpi[$kpiId]['name'] = Utils::formatString($row['meta_name']);
            }
            elseif ($type == BA_TYPE) {
                $this->_kpi[$kpiId]['name'] = Utils::formatString($row['name']);
            }
            $this->_kpi[$kpiId]['warningImpact'] = $row['drop_warning'];
            $this->_kpi[$kpiId]['criticalImpact'] = $row['drop_critical'];
            $this->_kpi[$kpiId]['unknownImpact'] = $row['drop_unknown'];
            switch($row['current_status']) {
        		case "0" : 
                    $this->_kpi[$kpiId]['level'] = LEVELMAX;
        		    break;
        		case "1" :
        		    $this->_kpi[$kpiId]['level'] = LEVELMAX - $row["drop_warning"];
        		    break;
        		case "2" : 
        		    $this->_kpi[$kpiId]['level'] = LEVELMAX - $row["drop_critical"];
        		    break;
        		case "3" :
        		    $this->_kpi[$kpiId]['level'] = LEVELMAX - $row["drop_unknown"];
        		    break;		
	        }
        }
    }
}