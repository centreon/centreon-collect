'use strict';

var jQuery = require('jquery');

var SubscriptionView = require('./subscribe.view.js');

var SubscriptionController = function () {
    jQuery("#license-manager").html("<span class='loading'>Loading ...</span>");
    var view = new SubscriptionView();
    view.render();
};

module.exports = SubscriptionController;