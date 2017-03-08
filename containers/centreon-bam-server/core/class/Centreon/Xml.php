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

/*
 *  Class that is used for writing & reading XML in utf_8 only!
 */
class CentreonBam_Xml
{

    protected $_buffer;

    /**
     *  Constructor
     */
    function __construct()
    {
        $this->_buffer = new XMLWriter();
        $this->_buffer->openMemory();
        $this->_buffer->startDocument('1.0', 'UTF-8');
    }

    /**
     * Clean string
     * 
     * @param string $str
     * @return string
     */
    protected function cleanStr($str)
    {
        $str = preg_replace('/[\x00-\x09\x0B-\x0C\x0E-\x1F\x0D]/', "", $str);
        return $str;
    }

    /**
     *  Starts an element that contains other elements
     */
    public function startElement($element_tag)
    {
        $this->_buffer->startElement($element_tag);
    }

    /**
     *  Ends an element (closes tag)
     */
    public function endElement()
    {
        $this->_buffer->endElement();
    }

    /*
     *  Simply puts text
     */
    public function text($txt, $cdata = true, $encode = 0)
    {
        $txt = $this->cleanStr($txt);
        $txt = html_entity_decode($txt);
        if ($encode || !$this->is_utf8($txt)) {
            $this->_buffer->writeCData(utf8_encode($txt));
        } else {
            if ($cdata) {
                $this->_buffer->writeCData($txt);
            } else {
                $this->_buffer->text($txt);
            }
        }
    }

    /**
     * Checks if string is encoded
     *
     * @param string $string
     * @return boolean
     */
    protected function is_utf8($string)
    {
        if (strlen($string) > 1024) {
            $res = $this->is_utf8(substr($string, 0, 1024));
            $res += $this->is_utf8(substr($string, 1025));
        } else {
            $res = 0;
            $res += preg_match('%^(?:[\x09\x0A\x0D\x20-\x7E] |
                                     [\xC2-\xDF][\x80-\xBF] |
                                     \xE0[\xA0-\xBF][\x80-\xBF] |
                                     [\xE1-\xEC\xEE\xEF][\x80-\xBF]{2})*$%xs', $string);
            $res += preg_match('%^(?:\xED[\x80-\x9F][\x80-\xBF] |
                                    \xF0[\x90-\xBF][\x80-\xBF]{2})*$%xs', $string);
            $res += preg_match('%^(?:[\xF1-\xF3][\x80-\xBF]{3})*$%xs', $string);
            $res += preg_match('%^(?:\xF4[\x80-\x8F][\x80-\xBF]{2})*$%xs', $string);
        }
        return $res;
    }

    /**
     *  Creates a tag and writes data
     */
    public function writeElement($element_tag, $element_value, $encode = 0)
    {
        $this->startElement($element_tag);
        $element_value = $this->cleanStr($element_value);
        $element_value = html_entity_decode($element_value);
        if ($encode || !$this->is_utf8($element_value)) {
            $this->_buffer->writeCData(utf8_encode($element_value));
        } else {
            $this->_buffer->writeCData($element_value);
        }

        $this->endElement();
    }

    /**
     *  Writes attribute
     */
    public function writeAttribute($att_name, $att_value, $encode = false)
    {
        $att_value = $this->cleanStr($att_value);
        if ($encode) {
            $this->_buffer->writeAttribute($att_name, utf8_encode(html_entity_decode($att_value)));
        } else {
            $this->_buffer->writeAttribute($att_name, html_entity_decode($att_value));
        }
    }

    /**
     *  Output the whole XML buffer
     */
    public function output()
    {
        $this->_buffer->endDocument();
        print $this->_buffer->outputMemory(true);
    }
}