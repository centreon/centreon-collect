#
# rhel.py
#
# Copyright (C) 2010  Red Hat, Inc.  All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from installclass import BaseInstallClass
from constants import *
from product import *
from flags import flags
import os
import types

import installmethod
import yuminstall

class InstallClass(BaseInstallClass):
    # name has underscore used for mnemonics, strip if you dont need it
    id = "Centreon"
    name = N_("Centreon")
    _description = N_("The default installation of %s is a minimum install. "
                     "You can optionally select a different set of software "
                     "now.")
    _descriptionFields = (productName,)
    sortPriority = 10005
    hidden = 0

    bootloaderTimeoutDefault = 5
    bootloaderExtraArgs = ["crashkernel=auto"]

    tasks = [(N_("Desktop"),
              ["base", "core", "debugging",
               "directory-client", "java-platform", "network-file-system-client",
               "server-platform",
               "fonts", "print-client",
               "basic-desktop", "desktop-debugging", "desktop-platform",
               "general-desktop", "graphical-admin-tools", "input-methods",
               "legacy-x", "x11", "internet-applications", "internet-browser", "office-suite",
               "remote-desktop-clients"]),
              (N_("Minimal Desktop"),
              ["base", "core", "debugging",
               "directory-client", "java-platform", "network-file-system-client",
               "server-platform",
               "fonts", "print-client",
               "basic-desktop", "desktop-debugging", "desktop-platform",
               "input-methods",
               "legacy-x", "x11", "internet-browser",
               "remote-desktop-clients"]),
             (N_("Minimal"),
              ["core"]),
             (N_("Basic Server"),
              ["base", "console-internet", "core", "debugging",
               "directory-client", "hardware-monitoring", "java-platform", 
               "large-systems", "network-file-system-client",
               "performance", "perl-runtime", "server-platform"]),
             (N_("Database Server"),
              ["base", "console-internet", "core", "debugging",
               "directory-client", "hardware-monitoring", "java-platform", 
               "large-systems", "network-file-system-client",
               "performance", "perl-runtime", "server-platform",
               "mysql-client", "mysql", "postgresql-client", "postgresql",
               "system-admin-tools"]),
             (N_("Web Server"),
              ["base", "console-internet", "core", "debugging",
               "directory-client", "java-platform", 
               "network-file-system-client",
               "performance", "perl-runtime", "server-platform",
               "web-server", "web-servlet", "php", "turbogears",
               "mysql-client", "postgresql-client"]),
             (N_("Virtual Host"),
              ["base", "console-internet", "core", "debugging",
               "directory-client", "hardware-monitoring", "java-platform", 
               "large-systems", "network-file-system-client",
               "performance", "perl-runtime", "server-platform",
               "virtualization", "virtualization-client",
               "virtualization-platform"]),
             (N_("Software Development Workstation"),
              ["base", "core", "debugging",
               "directory-client", "java-platform", "network-file-system-client",
               "performance", "perl-runtime", "server-platform",
               "fonts", "print-client",
               "basic-desktop", "desktop-debugging", "desktop-platform",
               "general-desktop", "graphical-admin-tools", "input-methods",
               "legacy-x", "x11", "internet-browser", "graphics", "emacs", "tex",
               "remote-desktop-clients", "virtualization", "virtualization-client",
               "virtualization-platform",
               "desktop-platform-devel", "development", "eclipse",
               "server-platform-devel", "technical-writing",
               "additional-devel"])]
 

    def getPackagePaths(self, uri):
        if not type(uri) == types.ListType:
            uri = [uri,]

        return {productName: uri}

    def setInstallData(self, anaconda):
        BaseInstallClass.setInstallData(self, anaconda)
        BaseInstallClass.setDefaultPartitioning(self,
                                                anaconda.id.storage,
                                                anaconda.platform)

    def setSteps(self, anaconda):
        BaseInstallClass.setSteps(self, anaconda)
        anaconda.dispatch.skipStep("partition")
        anaconda.dispatch.skipStep("betanag",permanent=1)

    def getBackend(self):
        if flags.livecdInstall:
            import livecd
            return livecd.LiveCDCopyBackend
        else:
            return yuminstall.YumBackend

    def productMatches(self, oldprod):
        if oldprod is None:
            return False

        if oldprod.startswith(productName):
            return True

        productUpgrades = {
            "CentOS": ("CentOS release", ),
            "Centreon": ("Centreon", ),
            "Red Hat Enterprise Linux AS": ("Red Hat Linux Advanced Server", ),
            "Red Hat Enterprise Linux WS": ("Red Hat Linux Advanced Workstation",),
            # FIXME: this probably shouldn't be in a release...
            "Red Hat Enterprise Linux": ("Red Hat Linux Advanced Server",
                                         "Red Hat Linux Advanced Workstation",
                                         "Red Hat Enterprise Linux AS",
                                         "Red Hat Enterprise Linux ES",
                                         "Red Hat Enterprise Linux WS"),
            "Red Hat Enterprise Linux Server": ("Red Hat Enterprise Linux AS",
                                                "Red Hat Enterprise Linux ES",
                                                "Red Hat Enterprise Linux WS",
                                                "Red Hat Enterprise Linux"),
            "Red Hat Enterprise Linux Client": ("Red Hat Enterprise Linux WS",
                                                "Red Hat Enterprise Linux Desktop",
                                                "Red Hat Enterprise Linux"),
        }

        if productUpgrades.has_key(productName):
            acceptable = productUpgrades[productName]
        else:
            acceptable = ()

        for p in acceptable:
            if oldprod.startswith(p):
                return True

        return False

    def versionMatches(self, oldver):
        oldMajor = oldver.split(".")[0]
        newMajor = productVersion.split(".")[0]

        return oldMajor == newMajor

    def __init__(self):
        BaseInstallClass.__init__(self)
