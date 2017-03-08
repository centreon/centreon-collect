var Backbone = require('backbone');

var LoginController = require('../login/login.controller');
var SubscriptionController = require('../subscribe/subscribe.controller');
var InstallController = require('../install/install.controller');

var Router = Backbone.Router.extend({
    routes: {
        'login': LoginController,
        'subscribe': SubscriptionController,
        'install': InstallController
    },

    routesId : {
        1: 'login',
        2: 'subscribe',
        3: 'install'
    }
});

var _instance;

var SingletonRouter = function () {
    if (!_instance) {
        _instance = new Router();
    }
    return _instance;
};

module.exports = new SingletonRouter();
