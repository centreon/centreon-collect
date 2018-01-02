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

"""Module with the class for the Centreon TUI spoke."""

# import gettext
# _ = lambda x: gettext.ldgettext("centreon-anaconda-plugin", x)

# will never be translated
_ = lambda x: x
N_ = lambda x: x

# the path to addons is in sys.path so we can import things from centreon
from com_centreon_server_role.categories.centreon import CentreonCategory
from pyanaconda.ui.tui.spokes import NormalTUISpoke
from pyanaconda.ui.tui.spokes import EditTUISpoke
from pyanaconda.ui.tui.spokes import EditTUISpokeEntry as Entry
from pyanaconda.ui.common import FirstbootSpokeMixIn
from pyanaconda.ui.tui.simpleline import TextWidget, ColumnWidget

# export only the CentreonSpoke and CentreonEditSpoke classes
__all__ = ["CentreonSpoke"]


class CentreonSpoke(FirstbootSpokeMixIn, NormalTUISpoke):
    """
    Class for the Centreon TUI spoke that is a subclass of NormalTUISpoke. It
    is a simple example of the basic unit for Anaconda's text user interface.
    Since it is also inherited form the FirstbootSpokeMixIn, it will also appear
    in the Initial Setup (successor of the Firstboot tool).

    :see: pyanaconda.ui.tui.TUISpoke
    :see: pyanaconda.ui.common.FirstbootSpokeMixIn
    :see: pyanaconda.ui.tui.tuiobject.TUIObject
    :see: pyaanconda.ui.tui.simpleline.Widget

    """

    ### class attributes defined by API ###

    # title of the spoke
    title = N_("Centreon installation type")

    # category this spoke belongs to
    category = CentreonCategory

    def __init__(self, app, data, storage, payload, instclass):
        """
        :see: pyanaconda.ui.tui.base.UIScreen
        :see: pyanaconda.ui.tui.base.App
        :param app: reference to application which is a main class for TUI
                    screen handling, it is responsible for mainloop control
                    and keeping track of the stack where all TUI screens are
                    scheduled
        :type app: instance of pyanaconda.ui.tui.base.App
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

        NormalTUISpoke.__init__(self, app, data, storage, payload, instclass)
        self._selected = 0
        self._choices = ['central', 'centralwithoutdb', 'poller', 'database']

    def mandatory(self):
        return True

    def initialize(self):
        """
        The initialize method that is called after the instance is created.
        The difference between __init__ and this method is that this may take
        a long time and thus could be called in a separated thread.

        :see: pyanaconda.ui.common.UIObject.initialize

        """

        NormalTUISpoke.initialize(self)

    def refresh(self, args=None):
        """
        The refresh method that is called every time the spoke is displayed.
        It should update the UI elements according to the contents of
        self.data.

        :see: pyanaconda.ui.common.UIObject.refresh
        :see: pyanaconda.ui.tui.base.UIScreen.refresh
        :param args: optional argument that may be used when the screen is
                     scheduled (passed to App.switch_screen* methods)
        :type args: anything
        :return: whether this screen requests input or not
        :rtype: bool

        """

        NormalTUISpoke.refresh(self, args)

        self._window += [TextWidget(_("Available type"))]

        for idx, choice in enumerate(self._choices):
            number = TextWidget("%2d)" % (idx + 1))
            c = ColumnWidget([(3, [number]), (None, [TextWidget(choice)])], 1)
            self._window += [c]

        return True

    def apply(self):
        """
        The apply method that is called when the spoke is left. It should
        update the contents of self.data with values set in the spoke.

        """

        if self._selected == 1:
            self.data.addons.com_centreon_server_role.installation_type = "central"
        if self._selected == 2:
            self.data.addons.com_centreon_server_role.installation_type = "centralwithoutdb"
        if self._selected == 3:
            self.data.addons.com_centreon_server_role.installation_type = "poller"
        if self._selected == 4:
            self.data.addons.com_centreon_server_role.installation_type = "database"

    def execute(self):
        """
        The excecute method that is called when the spoke is left. It is
        supposed to do all changes to the runtime environment according to
        the values set in the spoke.

        """

        # nothing to do here
        pass

    @property
    def completed(self):
        """
        The completed property that tells whether all mandatory items on the
        spoke are set, or not. The spoke will be marked on the hub as completed
        or uncompleted acording to the returned value.

        :rtype: bool

        """

        if self._selected > 0:
            return True
        return False

    @property
    def status(self):
        """
        The status property that is a brief string describing the state of the
        spoke. It should describe whether all values are set and if possible
        also the values themselves. The returned value will appear on the hub
        below the spoke's title.

        :rtype: str

        """

        if self._selected > 0:
            return _("Type: %s") % self.data.addons.com_centreon_server_role.installation_type
        else:
            return _("Type not set")

    def input(self, args, key):
        """
        The input method that is called by the main loop on user's input.

        :param args: optional argument that may be used when the screen is
                     scheduled (passed to App.switch_screen* methods)
        :type args: anything
        :param key: user's input
        :type key: unicode
        :return: if the input should not be handled here, return it, otherwise
                 return INPUT_PROCESSED or INPUT_DISCARDED if the input was
                 processed succesfully or not respectively
        :rtype: bool|unicode

        """
        try:
            keyid = int(key)
        except ValueError:
            pass

        self._selected = keyid

        # no other actions scheduled, apply changes
        self.apply()

        # close the current screen (remove it from the stack)
        self.close()
        return True

    # def prompt(self, args=None):
    #    """
    #    The prompt method that is called by the main loop to get the prompt
    #    for this screen.
    #
    #    :see: pyanconda.ui.tui.simpleline.Prompt
    #
    #    :param args: optional argument that can be passed to App.switch_screen*
    #                 methods
    #    :type args: anything
    #    :return: text that should be used in the prompt for the input
    #    :rtype: instance of pyanconda.ui.tui.simpleline.Prompt or None
    #    """

    #    return _("Enter a new text or leave empty to use the old one: ")
