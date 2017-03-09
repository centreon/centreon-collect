/**
 * For hiding duplicate redundant entry
 */
jQuery(function () {
	var hiddenEntryId = 0;
	var showEntryId = 0;

	jQuery("select[name=id_reporting_period]").change(function() {
		if (hiddenEntryId != 0 && hiddenEntryId != jQuery(this).val()) {
			showEntryId = hiddenEntryId;
		}
		hiddenEntryId = jQuery(this).val();
		jQuery("select[name^=reporting_timeperiods] option[value=" + hiddenEntryId + "]").hide();
		jQuery("select[name^=reporting_timeperiods] option[value=" + showEntryId + "]").show();
	});

	// for initial loading
	jQuery("select[name=id_reporting_period]").trigger('change');
});


