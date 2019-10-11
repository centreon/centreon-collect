=================
Centreon Clib 19.10
=================

*********
Bug fixes
*********

Asynchronous Commands
=====================
Fix a race condition in some async command exec.

************
Enhancements
************

Switch to C++11
===============
Centreon clib now is C++11 compatible.

Cmake cleanup
=============

The build directory is gone away. CMake is used as intended, this solves issues
with some ide (like kdevelop)...

