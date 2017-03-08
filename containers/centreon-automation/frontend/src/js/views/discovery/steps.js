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

        var currentStep = jQuery('#step_' + self.step);
        var previousStep = jQuery('#step_' + (self.step - 1));

        if (self.step != 'undefined' || self.step != '0') {
            currentStep.addClass('current-step').removeClass('next-step');
            previousStep.removeClass('current-step').addClass('previous-step');
        }
    },

    render : function() {

        var self = this;

        var t = Automation.Templates.steps({step : self.step});
        var source = jQuery(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);

        self.displayCurrentStep();

        return self;

    }
});



