var Backbone = require('backbone');
var alert = require('sweetalert');
var _ = require('underscore');

var subscribeTemplate = require('./subscribe.html');
var errorTemplate = require('../error/error.html');
var TranslationService = require('../translation/translation.service.js');
var LicenceManagerService = require('../commons/license-manager.service.js');

var Subscriptions = Backbone.View.extend({
    className: 'subscribe-subview',

    el: '#license-manager',

    events: {
        //'submit #subscription-choice-form': 'subscribe'
        'click input[name="submitSubscribe"]': 'subscribe'
    },

    initialize: function() {
    },

    render: function() {
        var template;
        var viewContents = {};
        var self = this;
        this.getViewData(function (data) {
            template = _.template(subscribeTemplate);
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
            notFound: translations.t(currentLocale + '.index.notFound'),
            subscriptionLink: translations.t(currentLocale + '.login.subscriptionLink'),
            link: translations.t(currentLocale + '.subscribe.link'),
            subscribe: translations.t(currentLocale + '.subscribe.subscribe'),
            introMessage: translations.t(currentLocale + '.subscribe.introMessage'),
            warningMessage: translations.t(currentLocale + '.subscribe.warningMessage'),
            irreversible: translations.t(currentLocale + '.subscribe.irreversible'),
            errorMessage: translations.t(currentLocale + '.subscribe.errorMessage'),
            get5Plugins: translations.t(currentLocale + '.subscribe.get5Plugins'),
            nbPlugins: translations.t(currentLocale + '.subscribe.nbPlugins'),
            ppInstall: translations.t(currentLocale + '.subscribe.ppInstall'),
            discoverIMP: translations.t(currentLocale + '.subscribe.discoverIMP'),
            subscribeText: translations.t(currentLocale + '.subscribe.subscribeText'),
            CentreonPP: translations.t(currentLocale + '.subscribe.CentreonPP'),
            usingPPMText: translations.t(currentLocale + '.subscribe.usingPPMText'),
            warningTitle: translations.t(currentLocale + '.subscribe.warningTitle'),
            confirmButton: translations.t(currentLocale + '.subscribe.confirmButton'),
            warningText: translations.t(currentLocale + '.subscribe.warningText'),
            cancelButton: translations.t(currentLocale + '.subscribe.cancelButton')
        };
    },

    getViewData: function(callback, failCallback) {
        var self = this;
        var content = _.extend(
            {
                subscriptions: []
            },
            self.getTranslations()
        );

        LicenceManagerService.getCustomer(function (customerInfos) {
            content = _.extend(
                content,
                customerInfos
            );

            LicenceManagerService.getSubscriptions(function (userSubscriptions) {
                content = _.extend(
                    content,
                    userSubscriptions
                );
                callback(content);
            }, function (jqXHR) {
                failCallback(jqXHR);
            });

        }, function (jqXHR) {
            failCallback(jqXHR);
        });
    },

    subscribe: function(e) {
        var self = this;

        e.preventDefault();
        var subscriptionId = this.$el.find('input[name=subscription]:checked').val();
        var errorContainer = this.$el.find('.error-msg')[0];

        if (subscriptionId) {

            swal({
                    title: self.getTranslations().warningTitle,
                    text: self.getTranslations().warningText,
                    type: "warning",
                    showCancelButton: true,
                    confirmButtonColor: "#88B917",
                    confirmButtonText: self.getTranslations().confirmButton,
                    cancelButtonText: self.getTranslations().cancelButton,
                    animation: "slide-from-bottom",
                    closeOnConfirm: true
                },
                function(isConfirm) {
                    if(isConfirm) {
                        LicenceManagerService.subscribe(
                            subscriptionId,
                            function () {

                                var form = jQuery('#subscription-choice-form');
                                var router = require('../commons/license-manager.router.js');
                                router.navigate('install', {trigger: true});

                            },
                            function () {
                                jQuery(errorContainer).find('span').text(self.getTranslations().notFound);
                                jQuery(errorContainer).removeClass('hidden');
                            }
                        );
                    }
                }
            );
        } else {
            jQuery(errorContainer).removeClass('hidden');
        }
    }
});

module.exports = Subscriptions;
