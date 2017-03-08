window.Automation = {
    init: function() {
        this.router = new Automation.Router();
        Backbone.history.start();
    },

    Models: {},
    Collections: {},
    Views: {},
    Router: {},
    event: {},
};

_.extend(Automation.event, Backbone.Events);

$Automation(document).ready(function() {
    Automation.init();
})
