/**
 * Created by kduret on 31/10/2016.
 */

function setO(_i) {
    document.forms['form'].elements['o'].value = _i;
}
jQuery(function () {
    var $ = jQuery;
    var resetTimeout = null;
    /* Reset export form */
    function resetExport() {
        var $form = $('#export-plugin-pack form');
        $form.find('[name=status]').val('stable');
        $form.find('[name=status_message]').val(null);
        $form.find('[name=changelog]').val(null);
        $form.find('[name=version]').val(null);
        $form.find('.pp-field-error').text('').hide();
        $('.md-preview').html('');
    }

    /* Event on click export */
    $('.pp-export').on('click', function(e) {
        var $link = $(e.currentTarget);
        var linkData = $link.data();
        var ppId = linkData.ppid;
        var ppName = linkData.ppname;

        resetExport();
        $('#export-plugin-pack form [name="pluginpack_id"]').val(ppId);
        $('#export-plugin-pack .pp-name').text(ppName);
        $('#export-plugin-pack').centreonPopin('open');
    });

    $('#export-plugin-pack').centreonPopin();

    $('#export-plugin-pack').on('click', '.bt_success', function () {
        /* Prepare data for send */
        var i = 0;
        var data = {};
        var hasError = false;
        var fields = $('#export-plugin-pack form').serializeArray();
        var version = "1.0";
        $('#export-plugin-pack .pp-general-error').text('').hide();
        /* Transform and validate field */
        for (i; i < fields.length; i++) {
            data[fields[i].name] = fields[i].value;
            if (fields[i].name === 'version') {
                if ($.trim(fields[i].value) === '') {
                    $('#export-plugin-pack form input[name="version"]')
                        .parent().find('span').text('Version is required').show();
                    hasError = true;
                }
            } else if (fields[i].name === 'changelog') {
                if ($.trim(fields[i].value) === '') {
                    $('#export-plugin-pack form textarea[name="changelog"]')
                        .parent().find('span').text('Changelog is required').show();
                    hasError = true;
                }
            }
        }

        if (hasError) {
            return;
        }

        hasVersion();

        /* Check if version already exists */
        function hasVersion() {
            $.ajax({
                url: './api/internal.php?object=centreon_export_plugin_pack&action=hasVersion',
                data: JSON.stringify({
                    id: $('#export-plugin-pack form input[name="pluginpack_id"]').val(),
                    version: $('#export-plugin-pack form input[name="version"]').val()
                }),
                dataType: 'json',
                type: 'post',
                processData: false,
                success: function (data) {
                    if (data === true) {
                        $('#export-plugin-pack form input[name="version"]')
                            .parent().find('span').text('This version already exists.').show();
                    } else {
                        saveVersion();
                    }
                }
            });
        }

        /* Save and valid a new version */
        function saveVersion() {
            $.ajax({
                url: './api/internal.php?object=centreon_export_plugin_pack&action=addVersion',
                data: JSON.stringify(data),
                dataType: 'json',
                type: 'post',
                processData: false,
                success: function (data) {
                    if (data.success) {
                        var name = $('#export-plugin-pack .pp-name').text();
                        var version = $('#export-plugin-pack form input[name="version"]').val();
                        var fullname = 'pluginpack_' + name + '-' + version + '.json';
                        var blob = new Blob([JSON.stringify(data.data)], {type: 'application/json'});
                        saveAs(blob, fullname);
                        $('#export-plugin-pack').centreonPopin('close');
                    }
                },
                error: function () {
                    $('#export-plugin-pack .pp-general-error').text('Error during generate new version.').show();
                }
            });
        }
    });

    $('#export-plugin-pack').on('click', '.bt_default', function () {
        $('#export-plugin-pack').centreonPopin('close');
    });

    $('#export-plugin-pack form input').on('focus', function () {
        var $element = $(this);
        $element.parent().find('span').text('').hide();
    });
    $('#export-plugin-pack form textarea').on('focus', function () {
        var $element = $(this);
        $element.parent().find('span').text('').hide();
    });
    $('#export-plugin-pack form textarea').on('change keyup paste', function () {
        var $element = $(this);
        if (resetTimeout !== null) {
            clearTimeout(resetTimeout);
        }
        resetTimeout = setTimeout(function () {
            $('#export-plugin-pack .md-preview').html(
                marked($element.val())
            );
        }, 1000);
    });

    /* Reset export all form */
    function resetExportAll() {
        var $form = $('#export-all-plugin-pack form');
        $form.find('[name=increment_version]').val(null);
        $form.find('[name=changelog]').val(null);
        $form.find('.pp-field-error').text('').hide();
        $('.md-preview').html('');
    }

    /* HideChangelog */
    function manageExportAllFields() {
        var checked = $('#export-all-plugin-pack input[name="increment_version"]').prop('checked');
        if (checked) {
            $('#export-all-plugin-pack form').find('.toHide').show();
        } else {
            $('#export-all-plugin-pack form').find('.toHide').hide();
        }
    }

    /* Init export all popin */
    $('#export-all-plugin-pack').centreonPopin();

    /* Event on click export all */
    $('#exportAll').on('click', function() {
        resetExportAll();
        manageExportAllFields();
        $('#export-all-plugin-pack').centreonPopin('open');
    });

    $('#export-all-plugin-pack').on('click', '.bt_default', function () {
        $('#export-all-plugin-pack').centreonPopin('close');
    });

    $('#export-all-plugin-pack form textarea').on('change keyup paste', function () {
        var $element = $(this);
        if (resetTimeout !== null) {
            clearTimeout(resetTimeout);
        }
        resetTimeout = setTimeout(function () {
            $('#export-all-plugin-pack .md-preview').html(
                marked($element.val())
            );
        }, 1000);
    });

    $('#export-all-plugin-pack').on('click', 'input[name="increment_version"]', function () {
        manageExportAllFields();
    });

    /* Export all plugin packs */
    $('#export-all-plugin-pack').on('click', '.bt_success', function () {
        var increment_version = $('#export-all-plugin-pack input[name="increment_version"]').prop('checked');
        var changelog = $('#export-all-plugin-pack form textarea[name="changelog"]').val();
        if (increment_version === true && changelog.trim() === '') {
            $('#export-all-plugin-pack form textarea[name="changelog"]')
                .parent().find('span').text('Changelog is required').show();
            resetExportAll();
            return false;
        }
        $.ajax({
            url: './api/internal.php?object=centreon_export_plugin_pack&action=exportAll',
            data: JSON.stringify({
                increment_version: increment_version,
                changelog: changelog
            }),
            dataType: 'json',
            type: 'post',
            processData: false,
            success: function (data) {
                if (data.success) {
                    var zip = new JSZip();
                    var packFolder = zip.folder('centreon-packs');
                    data.data.forEach(function(pluginPack) {
                        var name = pluginPack.information.slug;
                        var version = pluginPack.information.version;
                        var fullname = 'pluginpack_' + name + '-' + version + '.json';
                        packFolder.file(fullname, JSON.stringify(pluginPack));
                    });
                    zip.generateAsync({type:"blob"})
                    .then(function(content) {
                        saveAs(content, "centreon-packs.zip");
                        $('#export-all-plugin-pack').centreonPopin('close');
                        resetExportAll();
                    });
                 } else {
                    $('#export-all-plugin-pack .pp-general-error').text('Error during global export.').show();
                }
            },
            error: function () {
                $('#export-all-plugin-pack .pp-general-error').text('Error during global export.').show();
            }
        });
    });
});

