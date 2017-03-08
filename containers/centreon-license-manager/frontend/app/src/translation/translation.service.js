'use strict';

var Backbone = require('backbone');
var Polyglot = require('node-polyglot');

var TranslationService = Backbone.Model.extend({

    translations: {
        fr: JSON.parse(require('./fr.json')),
        en: JSON.parse(require('./en.json'))
    },

    initialize: function () {
        this.polyglot = new Polyglot({
            locale: 'en',
            phrases: this.translations
        });
    },

    setLocale: function (lang) {
        if(this.translations[lang]) {
            this.polyglot.locale(lang);
            return true;
        } else {
            console.error('Unavailable translation: ', lang);
            return false;
        }
    }

});

var _instance;

var SingletonService = function () {
    if (!_instance) {
        _instance = new TranslationService();
    }
    return _instance;
};

module.exports = new SingletonService();