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
