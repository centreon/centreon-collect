<?php
/**
 * CENTREON
 * Source Copyright 2005-2008 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@merethis.com
 *
 */

namespace CentreonExport;

/*
 *  Class that is used for writing & reading XML in utf_8 only!
 */
class CentreonXMLReader
{
    protected $requestObj;
    protected $requestType;
    protected $header;

    /*
     *  Constructor
     */
    function __construct($xmlRequest)
    {
        $this->requestObj = $xmlRequest;
        $this->requestType = array();
        //$this->setRequestType();
        $this->header;
    }

    /*
     *  Gets the type of request
     */
    public function getRequestType()
    {
        return $this->requestType;
    }

    /*
     *  Gets the header
     */
    public function getHeader()
    {
        return $this->header;
    }

    /*
     *  Sets the type of request
     */
    private function setRequestType()
    {
        foreach ($this->requestObj->children() as $request) {
            if ($request->getName() == "request") {
                $this->requestType[] = $request;
            } elseif ($request->getName() == "header") {
                $this->header= $request;
            }
        }
    }
}
?>