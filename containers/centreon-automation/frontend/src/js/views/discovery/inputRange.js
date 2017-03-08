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
                jQuery('#inputRange').removeClass('highlight-error error');
                break;
            case 1:
                log = "checkmark.svg";
                break;
            case 2:
                log = "warning.svg";
        }

        self.check_ip = '<img src="./modules/centreon-automation/frontend/app/img/icons/' + log + '" alt="validation icon"/>';

        jQuery('.check-icon').html(self.check_ip);

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

            jQuery('#inputRange').removeClass('highlight-error error');
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
            jQuery('#inputRange').addClass('highlight-error error');
            self.$el.find('span.error-msg').text('Invalid format');
        }
    },

    render: function () {
        var self = this;

        var t = Automation.Templates.inputRange();
        var source = jQuery(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);

        return self;

    }
});


