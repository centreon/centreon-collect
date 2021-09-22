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

"""Module with the CentreonSpoke class."""

# import gettext
# _ = lambda x: gettext.ldgettext("centreon-anaconda-plugin", x)

# will never be translated
_ = lambda x: x
N_ = lambda x: x

# the path to addons is in sys.path so we can import things from centreon
from com_centreon_server_role.categories.centreon import CentreonCategory
from pyanaconda.ui.gui import GUIObject
from pyanaconda.ui.gui.spokes import NormalSpoke
from pyanaconda.ui.common import FirstbootSpokeMixIn

# export only the spoke, no helper functions, classes or constants
__all__ = ["CentreonSpoke"]

class CentreonSpoke(FirstbootSpokeMixIn, NormalSpoke):
    """
    Class for the Centreon spoke. This spoke will be in the Centreon
    category and thus on the Summary hub. It is a very simple example of a unit
    for the Anaconda's graphical user interface. Since it is also inherited form
    the FirstbootSpokeMixIn, it will also appear in the Initial Setup (successor
    of the Firstboot tool).


    :see: pyanaconda.ui.common.UIObject
    :see: pyanaconda.ui.common.Spoke
    :see: pyanaconda.ui.gui.GUIObject
    :see: pyanaconda.ui.common.FirstbootSpokeMixIn
    :see: pyanaconda.ui.gui.spokes.NormalSpoke

    """

    ### class attributes defined by API ###

    # list all top-level objects from the .glade file that should be exposed
    # to the spoke or leave empty to extract everything
    builderObjects = ["CentreonSpokeWindow", "buttonImage"]

    # the name of the main window widget
    mainWidgetName = "CentreonSpokeWindow"

    # name of the .glade file in the same directory as this source
    uiFile = "centreon.glade"

    # category this spoke belongs to
    category = CentreonCategory

    # spoke icon (will be displayed on the hub)
    # preferred are the -symbolic icons as these are used in Anaconda's spokes
    #icon = "face-cool-symbolic"
    icon = "utilities-system-monitor-symbolic"

    # title of the spoke (will be displayed on the hub)
    title = N_("_INSTALLATION TYPE")

    ### methods defined by API ###
    def __init__(self, data, storage, payload, instclass):
        """
        :see: pyanaconda.ui.common.Spoke.__init__
        :param data: data object passed to every spoke to load/store data
                     from/to it
        :type data: pykickstart.base.BaseHandler
        :param storage: object storing storage-related information
                        (disks, partitioning, bootloader, etc.)
        :type storage: blivet.Blivet
        :param payload: object storing packaging-related information
        :type payload: pyanaconda.packaging.Payload
        :param instclass: distribution-specific information
        :type instclass: pyanaconda.installclass.BaseInstallClass

        """

        NormalSpoke.__init__(self, data, storage, payload, instclass)

    def initialize(self):
        """
        The initialize method that is called after the instance is created.
        The difference between __init__ and this method is that this may take
        a long time and thus could be called in a separated thread.

        :see: pyanaconda.ui.common.UIObject.initialize

        """

        # Radio buttons
        NormalSpoke.initialize(self)
        self._rb_type_central = self.builder.get_object("type_central")
        self._rb_type_centralwithoutdb = self.builder.get_object("type_centralwithoutdb")
        self._rb_type_poller = self.builder.get_object("type_poller")
        self._rb_type_database = self.builder.get_object("type_database")

    def refresh(self):
        """
        The refresh method that is called every time the spoke is displayed.
        It should update the UI elements according to the contents of
        self.data.

        :see: pyanaconda.ui.common.UIObject.refresh

        """

        if self._rb_type_central.get_active():
            self._rb_type_central.set_active(True)
        if self._rb_type_centralwithoutdb.get_active():
            self._rb_type_centralwithoutdb.set_active(True)
        if self._rb_type_poller.get_active():
            self._rb_type_poller.set_active(True)
        if self._rb_type_database.get_active():
            self._rb_type_database.set_active(True)

    def apply(self):
        """
        The apply method that is called when the spoke is left. It should
        update the contents of self.data with values set in the GUI elements.

        """

        if self._rb_type_central.get_active():
            self.data.addons.com_centreon_server_role.installation_type = 'central'
        if self._rb_type_centralwithoutdb.get_active():
            self.data.addons.com_centreon_server_role.installation_type = 'centralwithoutdb'
        if self._rb_type_poller.get_active():
            self.data.addons.com_centreon_server_role.installation_type = 'poller'
        if self._rb_type_database.get_active():
            self.data.addons.com_centreon_server_role.installation_type = 'database'

    def execute(self):
        """
        The excecute method that is called when the spoke is left. It is
        supposed to do all changes to the runtime environment according to
        the values set in the GUI elements.

        """

        # nothing to do here
        pass

    @property
    def ready(self):
        """
        The ready property that tells whether the spoke is ready (can be visited)
        or not. The spoke is made (in)sensitive based on the returned value.

        :rtype: bool

        """

        # this spoke is always ready
        return True

    @property
    def completed(self):
        """
        The completed property that tells whether all mandatory items on the
        spoke are set, or not. The spoke will be marked on the hub as completed
        or uncompleted acording to the returned value.

        :rtype: bool

        """

        return bool(self.data.addons.com_centreon_server_role.installation_type)

    @property
    def mandatory(self):
        """
        The mandatory property that tells whether the spoke is mandatory to be
        completed to continue in the installation process.

        :rtype: bool

        """

        return True

    @property
    def status(self):
        """
        The status property that is a brief string describing the state of the
        spoke. It should describe whether all values are set and if possible
        also the values themselves. The returned value will appear on the hub
        below the spoke's title.

        :rtype: str

        """

        installation_type = self.data.addons.com_centreon_server_role.installation_type

        # If --reverse was specified in the kickstart, reverse the text
        #if self.data.addons.com_centreon_server_role.reverse:
        #    text = text[::-1]

        if installation_type:
            return _("Type: %s") % installation_type
        else:
            return _("Type not set")


    ### handlers ###
    #def on_entry_icon_clicked(self, entry, *args):
    #    """Handler for the textEntry's "icon-release" signal."""
    #
    #    entry.set_text("")

    #def on_main_button_clicked(self, *args):
    #    """Handler for the mainButton's "clicked" signal."""

        # every GUIObject gets ksdata in __init__
    #    dialog = CentreonDialog(self.data)

        # show dialog above the lightbox
    #    with self.main_window.enlightbox(dialog.window):
    #        dialog.run()

class CentreonDialog(GUIObject):
    """
    Class for the sample dialog.

    :see: pyanaconda.ui.common.UIObject
    :see: pyanaconda.ui.gui.GUIObject

    """

    builderObjects = ["sampleDialog"]
    mainWidgetName = "sampleDialog"
    uiFile = "centreon.glade"

    def __init__(self, *args):
        GUIObject.__init__(self, *args)

    def initialize(self):
        GUIObject.initialize(self)

    def run(self):
        """
        Run dialog and destroy its window.

        :returns: respond id
        :rtype: int

        """

        ret = self.window.run()
        self.window.destroy()

        return ret
