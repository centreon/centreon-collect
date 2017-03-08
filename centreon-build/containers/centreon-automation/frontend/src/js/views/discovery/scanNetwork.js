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
                        jQuery('.listing-container').niceScroll({cursoropacitymin : 0.3});

                        jQuery(".waiting").remove();
                        jQuery(".action-container").removeClass('hide').addClass('show');
                    }
                })

                .error(function(err) {

                    jQuery(".waiting").remove();

                    var error = err.responseText.replace(/\"/g, "");
                    var msg = '<p class="msg"><strong>Error ' + err.status + '</strong> ' + error + '</p>';
                    
                    jQuery('.error-container').html(msg);
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

            jQuery(".scan-listing").append(row);
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
        var source = jQuery(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);
        return self;

    }
})
