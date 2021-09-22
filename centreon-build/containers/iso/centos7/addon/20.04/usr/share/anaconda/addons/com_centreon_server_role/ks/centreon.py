#
# Copyright (C) 2013  Red Hat, Inc.
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions of
# the GNU General Public License v.2, or (at your option) any later version.
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY expressed or implied, including the implied warranties of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.  You should have received a copy of the
# GNU General Public License along with this program; if not, write to the
# Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.  Any Red Hat trademarks that are incorporated in the
# source code or documentation are not subject to the GNU General Public
# License and may only be used or replicated with the express permission of
# Red Hat, Inc.
#
# Red Hat Author(s): Vratislav Podzimek <vpodzime@redhat.com>
#

"""Module with the CentreonData class."""

import os.path

from pyanaconda.addons import AddonData
from pyanaconda.iutil import execWithRedirect, getSysroot

from pykickstart.options import KSOptionParser
from pykickstart.errors import KickstartParseError, formatErrorMsg

# export CentreonData class to prevent Anaconda's collect method from taking
# AddonData class instead of the CentreonData class
# :see: pyanaconda.kickstart.AnacondaKSHandler.__init__
__all__ = ["CentreonData"]

CENTREON_FILE_PATH = "/root/centreon_addon_output.txt"


class CentreonData(AddonData):
    """
    Class parsing and storing data for the Centreon addon.

    :see: pyanaconda.addons.AddonData

    """

    def __init__(self, name):
        """
        :param name: name of the addon
        :type name: str

        """

        # We remove centos banner
        execWithRedirect("rm", ["-f", "/usr/share/anaconda/pixmaps/rnotes/en/centos-artwork.png",
                                "/usr/share/anaconda/pixmaps/rnotes/en/centos-cloud.png",
                                "/usr/share/anaconda/pixmaps/rnotes/en/centos-core.png",
                                "/usr/share/anaconda/pixmaps/rnotes/en/centos-promotion.png",
                                "/usr/share/anaconda/pixmaps/rnotes/en/centos-virtualization.png"])

        AddonData.__init__(self, name)
        self.installation_type = ""

    def __str__(self):
        """
        What should end up in the resulting kickstart file, i.e. the %addon
        section containing string representation of the stored data.

        """

        addon_str = "%%addon %s" % self.name
        addon_str += " --type='%s'" % self.installation_type
        addon_str += "\n%%end\n"
        return addon_str

    def handle_header(self, lineno, args):
        """
        The handle_header method is called to parse additional arguments in the
        %addon section line.

        args is a list of all the arguments following the addon ID. For
        example, for the line:

            %addon com_centreon_server_role --type="central"

        handle_header will be called with args=['--reverse', '--arg2="example"']

        :param lineno: the current line number in the kickstart file
        :type lineno: int
        :param args: the list of arguments from the %addon line
        :type args: list
        """

        op = KSOptionParser()
        op.add_option("--type", dest="installation_type", default="")
        (opts, extra) = op.parse_args(args=args, lineno=lineno)

        # Reject any additional arguments.
        if extra:
            msg = "Unhandled arguments on %%addon line for %s" % self.name
            if lineno != None:
                raise KickstartParseError(formatErrorMsg(lineno, msg=msg))
            else:
                raise KickstartParseError(msg)

        # Store the result of the option parsing
        self.installation_type = opts.installation_type

    def handle_line(self, line):
        """
        The handle_line method that is called with every line from this addon's
        %addon section of the kickstart file.

        :param line: a single line from the %addon section
        :type line: str

        """

        # simple example, we just append lines to the text attribute
        # We don't care ;)
        if self.text is "":
            self.text = line.strip()
        else:
            self.text += " " + line.strip()

    def finalize(self):
        """
        The finalize method that is called when the end of the %addon section
        (i.e. the %end line) is processed. An addon should check if it has all
        required data. If not, it may handle the case quietly or it may raise
        the KickstartValueError exception.

        """

        # no actions needed in this addon
        pass

    def setup(self, storage, ksdata, instclass, payload):
        """
        The setup method that should make changes to the runtime environment
        according to the data stored in this object.

        :param storage: object storing storage-related information
                        (disks, partitioning, bootloader, etc.)
        :type storage: blivet.Blivet instance
        :param ksdata: data parsed from the kickstart file and set in the
                       installation process
        :type ksdata: pykickstart.base.BaseHandler instance
        :param instclass: distribution-specific information
        :type instclass: pyanaconda.installclass.BaseInstallClass
        :param payload: object managing packages and environment groups
                        for the installation
        :type payload: any class inherited from the pyanaconda.packaging.Payload
                       class
        """

        widget_list = ['centreon-widget-graph-monitoring', 'centreon-widget-hostgroup-monitoring',
                       'centreon-widget-host-monitoring', 'centreon-widget-servicegroup-monitoring',
                       'centreon-widget-service-monitoring', 'centreon-widget-engine-status',
                       'centreon-widget-grid-map', 'centreon-widget-live-top10-cpu-usage',
                       'centreon-widget-live-top10-memory-usage', 'centreon-widget-tactical-overview'
                       ]
        if self.installation_type == 'central':
            ksdata.packages.packageList.extend(
                ['centreon-release', 'centos-release-scl', 'centreon', 'mariadb-server'])
            ksdata.packages.packageList.extend(widget_list)
        if self.installation_type == 'centralwithoutdb':
            ksdata.packages.packageList.extend(['centreon-release', 'centos-release-scl', 'centreon-base-config-centreon-engine'])
            ksdata.packages.packageList.extend(widget_list)
        if self.installation_type == 'poller':
            ksdata.packages.packageList.extend(['centreon-release', 'centreon-poller-centreon-engine'])
        if self.installation_type == 'database':
            ksdata.packages.packageList.extend(['centreon-release', 'centreon-database', 'mariadb-server'])
        if self.installation_type == 'centreonmap':
            ksdata.packages.packageList.extend(
                ['centreon-release', 'centos-release-scl', 'centreon-map-release', 'centreon-map4-server', 'mariadb-server'])
        if self.installation_type == 'centreonmbi':
            ksdata.packages.packageList.extend(
                ['centreon-release', 'centos-release-scl', 'centreon-mbi-release', 'centreon-bi-reporting-server', 'mariadb-server'])

    def execute(self, storage, ksdata, instclass, users, payload):
        """
        The execute method that should make changes to the installed system. It
        is called only once in the post-install setup phase.

        :see: setup
        :param users: information about created users
        :type users: pyanaconda.users.Users instance

        """

        # Disable selinux
        execWithRedirect("sed", ["-i", "s/=enforcing/=disabled/", getSysroot() + "/etc/selinux/config"])

        # Disable firewall
        execWithRedirect("systemctl", ["disable", "firewalld"], root=getSysroot())

        # NTP.
        execWithRedirect("timedatectl", ["set-ntp", "true"], root=getSysroot())

        # MariaDB-Server
        if self.installation_type == 'central' or self.installation_type == 'database' or self.installation_type == 'centreonmbi' or self.installation_type == 'pollerdisplay':
            limit_mariadb_path = os.path.normpath(getSysroot() + '/etc/systemd/system/mariadb.service.d/limits.conf')
            with open(limit_mariadb_path, "w") as fobj:
                fobj.write("[Service]\nLimitNOFILE=32000\n")
            execWithRedirect("systemctl", ["enable", "mariadb"], root=getSysroot())

        # httpd and PHP.
        if self.installation_type == 'central' or self.installation_type == 'centralwithoutdb' or self.installation_type == 'pollerdisplay':
            execWithRedirect("systemctl", ["enable", "httpd24-httpd"], root=getSysroot())
            execWithRedirect("systemctl", ["enable", "rh-php72-php-fpm"], root=getSysroot())
            centreon_ini_path = os.path.normpath(getSysroot() + '/etc/opt/rh/rh-php72/php.d/50-centreon.ini')
            with open(centreon_ini_path, "a") as fobj:
                fobj.write("date.timezone=" + ksdata.timezone.timezone + "\n")

        # SNMP related services.
        if self.installation_type == 'central' or self.installation_type == 'centralwithoutdb' or self.installation_type == 'poller' or self.installation_type == 'pollerdisplay':
            execWithRedirect("systemctl", ["enable", "snmpd"], root=getSysroot())
            execWithRedirect("systemctl", ["enable", "snmptrapd"], root=getSysroot())
            execWithRedirect("systemctl", ["enable", "centreontrapd"], root=getSysroot())
