
var Backbone = require('backbone');
var jQuery = require('jquery');
var _ = require('underscore');

var installTemplate = require('./install.html');
var errorTemplate = require('../error/error.html');
var TranslationService = require('../translation/translation.service.js');
var LicenceManagerService = require('../commons/license-manager.service.js');

var install = Backbone.View.extend({
    className: '',

    el: '#license-manager',

    events: {
    },

    initialize: function() {
    },

    render: function() {
        var template;
        var viewContents = {};

        var self = this;
        this.getViewData(function (data) {
            template = _.template(installTemplate);
            viewContents = data;
            self.$el.html(template(viewContents));

            return self;
        }, function(jqXHR) {
            template = _.template(errorTemplate);

            viewContents.errorMessage = JSON.parse(jqXHR.responseText);
            self.$el.html(template(viewContents));
        });
    },

    getTranslations: function() {
        var currentLocale = TranslationService.polyglot.locale();
        var translations = TranslationService.polyglot;
        return {
            welcome: translations.t(currentLocale + '.index.welcome'),
            introMessage: translations.t(currentLocale + '.install.introMessage'),
            CentreonPP: translations.t(currentLocale + '.install.CentreonPP'),
            ppInstall: translations.t(currentLocale + '.install.ppInstall')
        };
    },

    getViewData: function(callback, failCallback) {
        var self = this;
        var content = _.extend(
            {},
            self.getTranslations()
        );
        var errorContainer = this.$el.find('.error-msg')[0];

        LicenceManagerService.getCustomer(function (customerInfos) {
            content = _.extend(
                content,
                customerInfos
            );

            callback(content);
        }, function (jqXHR) {
            failCallback(jqXHR);
        });
    }
});

module.exports = install;
