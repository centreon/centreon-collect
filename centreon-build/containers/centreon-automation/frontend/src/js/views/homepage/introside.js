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
        var source = jQuery(t).html();
        var tpl = Handlebars.compile(source);
        self.$el.html(tpl);

        return this;

    }

});
