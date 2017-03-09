var Backbone = require('backbone');
var jQuery = require('jquery');
var _ = require('underscore');

var downloadTemplate = require('./download.html');
var TranslationService = require('../translation/translation.service.js');
var LicenceManagerService = require('../commons/license-manager.service.js');

var Downloads = Backbone.View.extend({
    className: 'download-subview',

    el: '#license-manager',

    events: {
    },

    initialize: function() {
        this.render();
    },

    bindDownloadAction: function() {
        var self = this;
        this.$el.find('li').one('click', function () {
            self.downloadLicence(this);
        });
    },

    render: function() {
        var template = _.template(downloadTemplate);
        var self = this;
        this.getViewData(function (data) {
            var viewContents = data;
            self.$el.html(template(viewContents));

            self.bindDownloadAction();
            return self;
        });
    },

    getTranslations: function() {
        var currentLocale = TranslationService.polyglot.locale();
        var translations = TranslationService.polyglot;
        return {
            welcome: translations.t(currentLocale + '.index.welcome'),
            notFound: translations.t(currentLocale + '.index.notFound'),
            introMessage: translations.t(currentLocale + '.download.introMessage'),
            downloadMessage: translations.t(currentLocale + '.download.downloadMessage')
        };
    },

    getViewData: function(callback) {
        var self = this;
        var content = _.extend(
            {
                downloads: []
            },
            self.getTranslations()
        );

        LicenceManagerService.getCustomer(
            function (customerInfos) {
                content = _.extend(
                    content,
                    customerInfos
                );

                callback(content);
            }, function(err) {
                callback({'errorMessage': self.getTranslations().notFound});
            }
        );
    },

    downloadLicence: function (object) {
        var self = this;

        var listElement = jQuery(object);
        var errorContainer = this.$el.find('.error-msg')[0];

        LicenceManagerService.downloadLicence(
            function (success) {
                if (success) {
                    listElement.addClass('downloaded');
                    var router = require('../commons/license-manager.router.js');
                    router.navigate('install', {trigger: true});
                } else {
                    console.log('fail !');
                }
            },
            function() {
                jQuery(errorContainer).find('span').text(self.getTranslations().notFound);
                jQuery(errorContainer).removeClass('hidden');
            }
        );
    }
});

module.exports = Downloads;
