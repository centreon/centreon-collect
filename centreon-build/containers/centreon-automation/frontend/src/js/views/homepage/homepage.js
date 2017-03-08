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
        var source = jQuery(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);

    }
});
