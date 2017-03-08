Automation.Models.GenericHost = Backbone.Model.extend({

    /* Receive Hosts ID */

    type: 'POST',
    dataType: 'JSON',

    initialize: function() {

    },

    setHosts: function(hostIp) {
        this.hosts = hostIp;
    },

    url: function() {
        return './api/internal.php?module=centreon-automation&object=centreon_automation_apply_host&action=apply';
    },

    fetch : function(opt) {
        
        var opt = opt || {};

        opt.data = JSON.stringify(this.hosts);
        opt.type = this.type;
        opt.dataType = this.dataType;

        return Backbone.Collection.prototype.fetch.call(this,opt);

    },

});