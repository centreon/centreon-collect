var Backbone = require('backbone');
var jQuery = require('jquery');
var _ = require('underscore');

var LicenceManagerService = require('../commons/license-manager.service.js');
var TranslationService = require('../translation/translation.service.js');
var loginTemplate = require('./login.html');

var LoginForm = Backbone.View.extend({
    className: 'login-subview',

    el: '#license-manager',

    events: {
        'submit #login-form': 'login'
    },

    initialize: function() {
    },

    render: function() {
        var template = _.template(loginTemplate);
        var viewContents = _.extend(
            {},
            this.getTranslations(),
            this.getViewData()
        );
        this.$el.html(template(viewContents));
        return this;
    },

    getTranslations: function() {
        var currentLocale = TranslationService.polyglot.locale();
        var translations = TranslationService.polyglot;
        return {
            introMessage: translations.t(currentLocale + '.login.introMessage'),
            email: translations.t(currentLocale + '.login.email'),
            password: translations.t(currentLocale + '.login.password'),
            connect: translations.t(currentLocale + '.login.connect'),
            errorMessage: translations.t(currentLocale + '.login.errorMessage'),
            subscriptionText: translations.t(currentLocale + '.login.subscriptionText'),
            subscription: translations.t(currentLocale + '.login.subscription'),
            subscriptionLink: translations.t(currentLocale + '.login.subscriptionLink')
        };
    },

    getViewData: function() {
        return {
            subscriptionLink: '#'
        };
    },

    login: function(e) {
        e.preventDefault();
        var login = this.$el.find('*[name="email"]').val();
        var password = this.$el.find('*[name="password"]').val();
        var errorContainer = this.$el.find('.error-msg')[0];

        LicenceManagerService.login(
            login,
            password,
            function () {
                var router = require('../commons/license-manager.router.js');
                router.navigate('subscribe', {trigger: true});
            },
            function (jqXHR) {
                if (jqXHR.status == '409') {
                    var router = require('../commons/license-manager.router.js');
                    router.navigate('subscribe', {trigger: true});
                }
                jQuery(errorContainer).find('span').text(JSON.parse(jqXHR.responseText));
                jQuery(errorContainer).removeClass('hidden');
            }
        );
    }
});

module.exports = LoginForm;
