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