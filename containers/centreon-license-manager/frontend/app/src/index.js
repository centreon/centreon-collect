'use strict';

var Backbone = require('backbone');
var jQuery = require('jquery');

var router = require('./commons/license-manager.router.js');
var TranslationService = require('./translation/translation.service.js');
var LicenceManagerService = require('./commons/license-manager.service.js');

function init() {
    if (window.locale) {
        TranslationService.setLocale(window.locale);
    }

    Backbone.history.start();

    var currentRoute = Backbone.history.getFragment();
    // check status only if we are on index page
    if (currentRoute == '') {
        LicenceManagerService.getStartingState(
            function (startingState) {
                var step = router.routesId[startingState.step];
                router.navigate(step, {trigger: true});
            }, function (err) {
                var errorContainer = jQuery('.error-msg')[0];
                jQuery(errorContainer).find('span').text(JSON.parse(err.responseText));
                jQuery(errorContainer).removeClass('hidden');
            }
        );
    }
}

jQuery(document).ready(function() {
    init();
});
