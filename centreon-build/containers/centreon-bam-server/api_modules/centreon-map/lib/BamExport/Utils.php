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

class Utils
{
    public function formatString($str, $flag = NULL)
    {
        if (!isset($flag)) {
            $newStr = str_replace("#S#", "/", $str);
            $newStr = str_replace("#BS#", "\\", $newStr);
        }
        else {
            $newStr = str_replace("/", "#S#", $str);
            $newStr = str_replace("\\", "#BS#", $newStr);
        }
        return $newStr;
    }
}