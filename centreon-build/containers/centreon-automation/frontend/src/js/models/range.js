Automation.Models.Range = Backbone.Model.extend({

    /* Receive Hosts ID */

    type: 'POST',
    dataType: 'JSON',

    initialize: function () {
    },

    setRange: function (ip) {
        this.range = ip;
    },

    url: function () {
        return './api/internal.php?module=centreon-automation&object=centreon_automation_network_scan&action=scan';
    },

    fetch: function (opt) {
        var opt = opt || {};

        opt.data = JSON.stringify(this.range);
        opt.type = this.type;
        opt.dataType = this.dataType;

        return Backbone.Collection.prototype.fetch.call(this, opt);
    },

    /* Validate Range */

patterns: {
        privateIp: '(^(127|10)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)))($|(\/((3[0-2]|2[4-9]))$))'+
            '|(^172\\.(1[6-9]|2[0-9]]|3[0-1]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?))($|(\/((3[0-2]|2[4-9]))$))'+
            '|(^192\\.168\\.)(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)))($|(\/((3[0-2]|2[4-9]))$))',
        publicIp: '^([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])$',
    },

    pattern: function (value, pattern) {
        return new RegExp(pattern, "gm").test(value) ? true : false;
    },

    isPrivate: function (value) {
        var self = this;
        return this.pattern(value, self.patterns.privateIp);
    },

    isPublic: function (value) {
        var self = this;
        return this.pattern(value, self.patterns.publicIp);
    },

    error: function (ip) {

        var errors = this.errors = {};
        var ipAddr = ip;

        if (ipAddr != null) {
            //IP Address is required
            if (ipAddr == '') errors.ipAddress = 0;
            //You insert a Public IP Address, Are you sure ?
            else if (!this.isPrivate(ipAddr) && (this.isPublic(ipAddr))) errors.ipAddress = 2;
            //Invalid IP format
            else if (!this.isPrivate(ipAddr) && (!this.isPublic(ipAddr))) errors.ipAddress = 0;
            //Valid IP format
            else errors.ipAddress = 1;
        };
        
        if (!_.isEmpty(errors)) {
            return errors;
        }
    },

    validate: function () {
        var ipAddr = this.attributes.range;
        if (!ipAddr.error()) return ipAddr;
    },

});