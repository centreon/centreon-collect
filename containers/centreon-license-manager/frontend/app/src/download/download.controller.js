'use strict';

var jQuery = require('jquery');

var DownloadView = require('./download.view.js');

var SubscriptionController = function () {
    jQuery("#license-manager").html("Loading");
    var view = new DownloadView();
    view.render();
};

module.exports = SubscriptionController;