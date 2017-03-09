'use strict';

var jQuery = require('jquery');

var InstallView = require('./install.view.js');

var SubscriptionController = function () {
    jQuery("#license-manager").html("Loading");
    var view = new InstallView();
    view.render();
};

module.exports = SubscriptionController;
