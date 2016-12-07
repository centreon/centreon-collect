<?php

// Copy directory recursively
function xcopy($source, $dest) {
    if (stristr(PHP_OS, 'windows')) {
        exec("xcopy '$source' '$dest' /E /I");
    } else {
        exec("cp -r '$source' '$dest'", $output, $return);
    }
    if ($return == 0)
        return (true);
    else
        return (false);
}

// Replace slashes with platform-specific directory separator.
function xpath($path) {
    return str_replace('/', DIRECTORY_SEPARATOR, $path);
}

// Remove directory recursively.
function xrmdir($dir) {
    if (is_dir($dir)) {
        $objects = scandir($dir);
        foreach ($objects as $object) {
            if ($object != "." && $object != "..") {
                if (is_dir($dir."/".$object))
                    xrmdir($dir."/".$object);
                else
                    unlink($dir."/".$object);
            }
        }
        rmdir($dir);
    }
}

?>
