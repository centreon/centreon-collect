'use strict';

var jQuery = require('jquery');

var LoginView = require('./login.view.js');

var LoginController = function () {
    jQuery("#license-manager").html("<span class='loading'>Loading ...</span>");
    var view = new LoginView();
    view.render();
};

module.exports = LoginController;