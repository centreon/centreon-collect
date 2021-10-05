# -*- coding: utf-8 -*-

import gtk
import gui
import logging
from iw_gui import InstallWindow
import kickstart
import flags

import pprint

import gettext
_ = lambda x: gettext.ldgettext("anaconda", x)

logger = logging.getLogger("anaconda")


class CesChoiceWindows(InstallWindow):
    def getScreen(self, anaconda):
        self.intf = anaconda.intf
        self.anaconda = anaconda
        # Set selinux and firewall to disable
        self.anaconda.id.security.setSELinux(False)
        self.anaconda.id.firewall.enabled = False
        # Enable onboot to eth0
        devices = anaconda.id.network.netdevices
        for (key, val) in devices.iteritems():
            if key == 'eth0':
                dev = devices.get(key, None)
                if dev.get('ONBOOT') == 'no':
                    dev.set(('ONBOOT', 'yes'))

        self.anaconda.dispatch.skipStep("tasksel", skip=1, permanent=1)

        self.choise = 'centralwithdb'

        (self.xml, self.align) = gui.getGladeWidget('ceschoice.glade',
                'ceschoice_align')

        # Radio buttons
        self.xml.get_widget('ceschoice_centralwithdb'). \
            connect('toggled', self.choise_action, 'centralwithdb')
        self.xml.get_widget('ceschoice_centralwithoutdb'). \
            connect('toggled', self.choise_action, 'centralwithoutdb')
        self.xml.get_widget('ceschoice_poller'). \
            connect('toggled', self.choise_action, 'poller')
        self.xml.get_widget('ceschoice_database'). \
            connect('toggled', self.choise_action, 'database')

        # Help buttons
        self.xml.get_widget('info_centralwithdb'). \
            connect('button-press-event', self.information, 'centralwithdb')
        self.xml.get_widget('info_centralwithoutdb'). \
            connect('button-press-event', self.information, 'centralwithoutdb')
        self.xml.get_widget('info_poller'). \
            connect('button-press-event', self.information, 'poller')
        self.xml.get_widget('info_database'). \
            connect('button-press-event', self.information, 'database')

        return self.align

    def information(self, widget, event, informationType):
        message = ''
        if informationType == 'centralwithdb':
            message = _('A central server with a web UI and a database will be installed.')
        elif informationType == 'centralwithoutdb':
            message = _('A central server with a web UI will be installed.')
        elif informationType == 'poller':
            message = _('A poller server with a monitoring engine will be installed.')
        elif informationType == 'database':
            message = _('A server with only a database will be installed.')

        dialog = gtk.Dialog(_('Information'))
        dialog.add_button('gtk-ok', 1)
        dialog.set_position(gtk.WIN_POS_CENTER)
        gui.addFrame(dialog)

        dialog.vbox.pack_start(gui.WrappingLabel(message))
        dialog.show_all()
        dialog.run()
        dialog.destroy()

    def choise_action(self, widget, choise):
        if widget.get_active():
            self.choise = choise

    def getPrev(self):
        pass

    def getNext(self):
        ksFile = None

        if self.choise == 'database':
            ksFile = '/tmp/updates/kickstart/ksDatabase.cfg'
        elif self.choise == 'poller':
            ksFile = '/tmp/updates/kickstart/ksPoller-CentreonEngine.cfg'
        elif self.choise == 'centralwithoutdb':
            ksFile = '/tmp/updates/kickstart/ksCentral-CentreonEngine.cfg'
        elif self.choise == 'centralwithdb':
            ksFile = '/tmp/updates/kickstart/ksCentralDb-CentreonEngine.cfg'

        if ksFile is not None:
            self.anaconda.isKickstart = True
            kickstart.preScriptPass(self.anaconda, ksFile)
            ksdata = kickstart.parseKickstart(self.anaconda, ksFile)
            self.anaconda.id.setKsdata(ksdata)
