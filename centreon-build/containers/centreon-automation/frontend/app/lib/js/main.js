Automation.Router = Backbone.Router.extend({
    step: 1,

    routes: {
        '': 'HomePage',
        'discovery/step_:step': 'Discovery'
    },

    initialize: function(options) {
        this.genericView = new Automation.Views.genericView();
    },

    HomePage: function () {
        new Automation.Views.HomePage().render();
        new Automation.Views.introSide().render();
    },

    Discovery: function (step) {
        new Automation.Views.steps(step).render();
        this.genericView.redirect(step);
    }

});
Automation.Collections.hosts = Backbone.Collection.extend({
    type: 'POST',
    dataType: 'JSON',

    initialize: function(val) {

        this.range = {
            'range' : val,
        };
    },

    url: function() {
        return './include/common/webServices/rest/internal.php?module=centreon-automation&object=centreon_automation_network_scan&action=status';
    },



    fetch : function(opt) {
        var opt = opt || {};

        opt.data = JSON.stringify(this.range);
        opt.type = this.type;
        opt.dataType = this.dataType;


        return Backbone.Collection.prototype.fetch.call(
            this,
            opt
        );


    },
});


Automation.Models.GenericHost = Backbone.Model.extend({

    /* Receive Hosts ID */

    type: 'POST',
    dataType: 'JSON',

    initialize: function() {

    },

    setHosts: function(hostIp) {
        this.hosts = hostIp;
    },

    url: function() {
        return './api/internal.php?module=centreon-automation&object=centreon_automation_apply_host&action=apply';
    },

    fetch : function(opt) {
        
        var opt = opt || {};

        opt.data = JSON.stringify(this.hosts);
        opt.type = this.type;
        opt.dataType = this.dataType;

        return Backbone.Collection.prototype.fetch.call(this,opt);

    },

});
Automation.Models.Range = Backbone.Model.extend({

    /* Receive Hosts ID */

    type: 'POST',
    dataType: 'JSON',

    initialize: function () {
    },

    setRange: function (ip) {
        this.range = ip;
    },

    url: function () {
        return './api/internal.php?module=centreon-automation&object=centreon_automation_network_scan&action=scan';
    },

    fetch: function (opt) {
        var opt = opt || {};

        opt.data = JSON.stringify(this.range);
        opt.type = this.type;
        opt.dataType = this.dataType;

        return Backbone.Collection.prototype.fetch.call(this, opt);
    },

    /* Validate Range */

patterns: {
        privateIp: '(^(127|10)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)))($|(\/((3[0-2]|2[4-9]))$))'+
            '|(^172\\.(1[6-9]|2[0-9]]|3[0-1]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?))($|(\/((3[0-2]|2[4-9]))$))'+
            '|(^192\\.168\\.)(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)))($|(\/((3[0-2]|2[4-9]))$))',
        publicIp: '^([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])$',
    },

    pattern: function (value, pattern) {
        return new RegExp(pattern, "gm").test(value) ? true : false;
    },

    isPrivate: function (value) {
        var self = this;
        return this.pattern(value, self.patterns.privateIp);
    },

    isPublic: function (value) {
        var self = this;
        return this.pattern(value, self.patterns.publicIp);
    },

    error: function (ip) {

        var errors = this.errors = {};
        var ipAddr = ip;

        if (ipAddr != null) {
            //IP Address is required
            if (ipAddr == '') errors.ipAddress = 0;
            //You insert a Public IP Address, Are you sure ?
            else if (!this.isPrivate(ipAddr) && (this.isPublic(ipAddr))) errors.ipAddress = 2;
            //Invalid IP format
            else if (!this.isPrivate(ipAddr) && (!this.isPublic(ipAddr))) errors.ipAddress = 0;
            //Valid IP format
            else errors.ipAddress = 1;
        };
        
        if (!_.isEmpty(errors)) {
            return errors;
        }
    },

    validate: function () {
        var ipAddr = this.attributes.range;
        if (!ipAddr.error()) return ipAddr;
    },

});
Automation.Views.genericView = Backbone.View.extend({
    
    initialize: function (step) {
        this.step = step;
    },

    redirect: function (step) {

        var step = parseInt(step);

        switch (step) {
         case 1:
             new Automation.Views.homeNetwork().render();
             new Automation.Views.inputRange().render();
             break;

          case 2:
             new Automation.Views.scanNetwork().render();
         }
    }
});

Automation.Views.homeNetwork = Backbone.View.extend({
    el : '.view-container',

    events : {
    },

    initialize: function() {
        var self = this;

        Automation.event.on('hostId', function(val) {

            self.hosts = new Automation.Collections.hosts(val);
            self.getStatus();

        });
    },

    getStatus: function() {

    },

    render : function() {
        var self = this;

        var t = Automation.Templates.homeNetwork();
        var source = $Automation(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);

        return self;
    }
})
Automation.Views.inputRange = Backbone.View.extend({
    el: '.range-view',

    events: {
        'click #sendRange': 'validate',
        'keyup': 'keyAction'
    },

    initialize: function () {
        var self = this;

        self.step = 2;
        self.check_ip = "";
        this.model = new Automation.Models.Range();
    },


    keyAction: function (e) {

        var self = this;

        var log = "";

        this.ip = self.$el.find('input').val();

        switch (this.model.error(this.ip).ipAddress) {
            case 0:
                log = "cross.svg";
                self.$el.find('span.error-msg').empty();
                $Automation('#inputRange').removeClass('highlight-error error');
                break;
            case 1:
                log = "checkmark.svg";
                break;
            case 2:
                log = "warning.svg";
        }

        self.check_ip = '<img src="./modules/centreon-automation/frontend/app/img/icons/' + log + '" alt="validation icon"/>';

        $Automation('.check-icon').html(self.check_ip);

        var code = e.keyCode || e.which;
        if (code == 13) {
            this.validate();
        }
    },

    validate: function () {
        var self = this;

        this.ip = self.$el.find('input').val();
        this.model.setRange(this.ip);

        if (this.model.error(this.ip).ipAddress!=0) {

            $Automation('#inputRange').removeClass('highlight-error error');
            self.$el.find('span.error-msg').empty();

                this.model.fetch().done(function (val) {

                Automation.router.navigate("/discovery/step_" + self.step, {trigger: true});
                Automation.event.trigger("hostId", val);
                }).error(function (val) {

                    var errormsg = '';
                    errormsg += val.status + ' ' + val.statusText + ' : ' +val.responseText;
                    alert(errormsg);

                });


        } else {
            $Automation('#inputRange').addClass('highlight-error error');
            self.$el.find('span.error-msg').text('Invalid format');
        }
    },

    render: function () {
        var self = this;

        var t = Automation.Templates.inputRange();
        var source = $Automation(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);

        return self;

    }
});



Automation.Views.scanNetwork = Backbone.View.extend({
    el: '.discovery-content',

    events: {
        'click #SearchA': 'addPing',
        'click #SearchB': 'validate'
    },

    initialize: function (step) {
        var self = this;

        self.step = step;

        Automation.event.on('hostId', function (val) {
            self.hosts = new Automation.Collections.hosts(val);
            self.getStatus();
        });

        this.model = new Automation.Models.GenericHost();
    },

    getStatus: function () {
        var self = this;

            self.hosts.fetch()
                .done(function (val) {

                    self.appendHosts(val.objects);

                    if (!val.completed) {
                        setTimeout(function () {
                            self.getStatus();
                        }, 3000);

                    } else {
                        $Automation('.listing-container').niceScroll({cursoropacitymin : 0.3});

                        $Automation(".waiting").remove();
                        $Automation(".action-container").removeClass('hide').addClass('show');
                    }
                })

                .error(function(err) {

                    $Automation(".waiting").remove();

                    var error = err.responseText.replace(/\"/g, "");
                    var msg = '<p class="msg"><strong>Error ' + err.status + '</strong> ' + error + '</p>';
                    
                    $Automation('.error-container').html(msg);
                }) 
    },

    displayIcon: function(val) {
        
        var icon = "";
        
        if (val === 1) {
            icon = "<img src='./modules/centreon-automation/frontend/app/img/icons/in_icon.png' alt='displayed in centreon' />";
        }

        return icon;
    },

    appendHosts: function(hosts) {
        var self = this;

        for (var i = 0; i < hosts.length; i++) {

            var stringTabValue = hosts[i].inCentreon + ',' + hosts[i].ip + ',' + hosts[i].fqdn;

            var row =
                '<tr>' +
                    '<td>' + self.displayIcon(hosts[i].inCentreon) + '</td>' +
                     '<td>' + '<input name="' + hosts[i].ip + '" value="' + stringTabValue + '"type="checkbox" /> ' +
                        hosts[i].ip +
                    '</td>' +
                    '<td>' + hosts[i].fqdn + '</td>' +
                '<td>';

            if (hosts[i].inCentreon == 1) {
                row += hosts[i].nbService + ' active service(s)';
            }

            row += '</td></tr>';

            $Automation(".scan-listing").append(row);
        }
    },

    addPing: function () {
        var self = this;

        self.step++;
        
        var inputs = self.$el.find('input');
        var tabHostSelected = [];

        for (var i = 0, l = inputs.length; i < l; i++) {
            if ((inputs[i].name == "host[]") && (inputs[i].checked)) {
                tabHostSelected.push(inputs[i].value);
            }
        }
        
        Automation.router.navigate("/discovery/step/" + self.step, {trigger: true});

        this.model.setHosts(tabHostSelected);
        this.model.fetch().done(function (val) {
            window.location = val;
        });
    },


    render: function () {
        var self = this;

        var t = Automation.Templates.scanNetwork();
        var source = $Automation(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);
        return self;

    }
})

Automation.Views.steps = Backbone.View.extend({
    el : '.side-container',

    initialize: function (step) {
        var self = this;
        self.step = step;
    },

    events: {
    },

    displayCurrentStep : function() {
        var self = this;

        var currentStep = $Automation('#step_' + self.step);
        var previousStep = $Automation('#step_' + (self.step - 1));

        if (self.step != 'undefined' || self.step != '0') {
            currentStep.addClass('current-step').removeClass('next-step');
            previousStep.removeClass('current-step').addClass('previous-step');
        }
    },

    render : function() {

        var self = this;

        var t = Automation.Templates.steps({step : self.step});
        var source = $Automation(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);

        self.displayCurrentStep();

        return self;

    }
});




Automation.Views.HomePage = Automation.Views.genericView.extend({

    el: '.view-container',

    initialize: function () {
        this.step = 1;
    },

    events: {

        'click #DiscoveryAction' : 'Discovery', 
        'click #AddHostAction' : 'AddHost'
    },

    Discovery : function() {
        var self = this;

        Automation.router.navigate("/discovery/step_" + self.step, {trigger: true});
    },

    AddHost : function() {
        Automation.router.navigate('/addHost', {trigger: true});
    },

    render : function() {
        
        var self  = this;
        var t = Automation.Templates.homePage();
        var source = $Automation(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);

    }
});

Automation.Views.introSide = Backbone.View.extend({

    el: '.side-container',

    events: {

    },

    initialize : function() {
        return this;
    },


    render : function() {
        var self = this;

        var t = Automation.Templates.introSide();
        var source = $Automation(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);

        return this;

    }

});
