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
        var source = jQuery(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);

        return self;
    }
})