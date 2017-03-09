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

