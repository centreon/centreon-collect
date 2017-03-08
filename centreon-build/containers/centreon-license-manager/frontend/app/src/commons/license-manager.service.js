'use strict';

var Backbone = require('backbone');
var jQuery = require('jquery');
var TranslationService = require('../translation/translation.service.js');

var apiUrl = '/centreon/include/common/webServices/rest/internal.php?object=centreon_license_manager&action=';
var apiUrl1 = '/license-manager/instance/customer';
var actions = {
    states: 'stepStatus',
    login: 'registerInstance',
    subscribe: 'subscription',
    license: 'license',
    customer: 'customer'
};

var LicenceManagerService = Backbone.Model.extend({

    getStartingState: function (callback, failCallback) {
        var url = window.location.origin + apiUrl + actions.states;
        var parameters = {
            url: url,
            type: 'GET',
            dataType: 'json'
        };
        this.call(parameters, callback, failCallback);
    },

    login: function (login, password, callback, failCallback) {
        var url = window.location.origin + apiUrl + actions.login;
        var parameters = {
            url: url,
            type: 'POST',
            dataType: 'json',
            data: JSON.stringify({
                username: login,
                password: password
            }),
            processData: false
        };
        this.call(parameters, callback, failCallback);
    },

    getSubscriptions: function (callback, failCallback) {
        var url = window.location.origin + apiUrl + actions.subscribe;
        var parameters = {
            url: url,
            type: 'GET',
            dataType: 'json'
        };
        this.call(parameters, callback, failCallback);
    },

    subscribe: function (subscriptionId, callback, failCallback) {
        var url = window.location.origin + apiUrl + actions.subscribe;
        var parameters = {
            url: url,
            type: 'PATCH',
            dataType: 'json',
            data: JSON.stringify({
                subscriptionId: parseInt(subscriptionId)
            }),
            processData: false
        }
        this.call(parameters, callback, failCallback);
    },

    downloadLicence: function (callback, failCallback) {
        var url = window.location.origin + apiUrl + actions.license;
        var parameters = {
            url: url,
            type: 'GET',
            dataType: 'json'
        };
        this.call(parameters, callback, failCallback);
    },

    getCustomer: function (callback, failCallback) {
        var url = window.location.origin + apiUrl + actions.customer;
        var parameters = {
            url: url,
            type: 'GET',
            dataType: 'json'
        };
        this.call(parameters, callback, failCallback);
    },

    call: function (parameters, callback, failCallback) {
        var self = this;
        jQuery.ajax(parameters)
            .done(callback)
            .fail(function (err) {
                var errorMessage = self.getTranslation(err.status);
                if (errorMessage) {
                    err.responseText = JSON.stringify(errorMessage);
                }
                typeof failCallback === 'function' && failCallback(err);
            });
    },

    getTranslation: function(word) {
        var currentLocale = TranslationService.polyglot.locale();
        var translations = TranslationService.polyglot;

        var translationKey = currentLocale + '.error.' + word;
        var translationValue = false;
        if (translations.has(translationKey)) {
            translationValue = translations.t(translationKey);
        }

        return translationValue;
    }

});

var _instance;

var SingletonService = function () {
    if (!_instance) {
        _instance = new LicenceManagerService();
    }
    return _instance;
};

module.exports = new SingletonService();
