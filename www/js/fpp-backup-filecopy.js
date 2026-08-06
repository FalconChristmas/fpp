var fppFileCopy = fppFileCopy || {};

fppFileCopy.config = {
    direction: '#backup\\.Direction',
    usbDevice: '#backup\\.USBDevice',
    pathSelect: '#backup\\.PathSelect',
    host: '#backup\\.Host',
    remoteStorage: '#backup\\.RemoteStorage',
    deleteExtra: '#backup\\.DeleteExtra',
    copyText: '#copyText',
    showUSB: '.copyUSB',
    showHost: '.copyHost',
    showHostDevice: '.copyHostDevice',
    showPathSelect: '.copyPathSelect',
    showPath: '.copyPath',
    showBackups: '.copyBackups',
    showCompressed: '.sendCompressed',
    popupModalId: 'copyPopup_Modal',
    popupCloseBtnId: 'copyPopup_ModalCloseButton',
    copyPopupBodyId: 'copyPopup',
    onDevicesLoaded: null,
    onCopyDone: null,
    hostChangeFunction: null,
    usbChangeFunction: null
};

fppFileCopy.directionChanged = function () {
    var direction = $(fppFileCopy.config.direction).val();

    $(fppFileCopy.config.showUSB).hide();
    $(fppFileCopy.config.showHost).hide();
    $(fppFileCopy.config.showHostDevice).hide();
    $(fppFileCopy.config.showPathSelect).hide();
    $(fppFileCopy.config.showPath).hide();
    $(fppFileCopy.config.showCompressed).hide();

    // The "Backups" folder can be copied in either direction (see
    // copy_settings_to_storage.sh's "Backups" case, which isn't gated on
    // direction), so the checkbox should be available regardless of which
    // Copy Type is selected -- not just TOUSB.
    $(fppFileCopy.config.showBackups).show();

    switch (direction) {
        case 'TOUSB':
            $(fppFileCopy.config.showUSB).show();
            $(fppFileCopy.config.showPath).show();
            fppFileCopy.getRestoreDeviceDirectories();
            break;
        case 'FROMUSB':
            $(fppFileCopy.config.showUSB).show();
            $(fppFileCopy.config.showPathSelect).show();
            fppFileCopy.getBackupDeviceDirectories();
            break;
        case 'TOLOCAL':
            $(fppFileCopy.config.showPath).show();
            break;
        case 'FROMLOCAL':
            $(fppFileCopy.config.showPathSelect).show();
            fppFileCopy.getBackupDirsViaAPI(window.location.host, '', true);
            break;
        case 'TOREMOTE':
            $(fppFileCopy.config.showHost).show();
            $(fppFileCopy.config.showHostDevice).show();
            $(fppFileCopy.config.showPath).show();
            $(fppFileCopy.config.showCompressed).show();
            fppFileCopy.checkRemoteHasRsyncdEnabled($(fppFileCopy.config.host).val());
            fppFileCopy.getRemoteHostUSBStorage();
            break;
        case 'FROMREMOTE':
            $(fppFileCopy.config.showHost).show();
            $(fppFileCopy.config.showHostDevice).show();
            $(fppFileCopy.config.showPathSelect).show();
            $(fppFileCopy.config.showCompressed).show();
            fppFileCopy.getBackupHostBackupDirs();
            fppFileCopy.checkRemoteHasRsyncdEnabled($(fppFileCopy.config.host).val());
            fppFileCopy.getRemoteHostUSBStorage();
            break;
    }
};

fppFileCopy.getBackupDevices = function () {
    $(fppFileCopy.config.usbDevice).html('<option>Loading...</option>');
    $(fppFileCopy.config.usbDevice).parent().closest('td').addClass('fpp-backup-action-loading');

    $.get('api/backups/devices').done(function (data) {
        var options = '';
        for (var i = 0; i < data.length; i++) {
            var desc = data[i].name;
            if (data[i].vendor != '')
                desc += ' - ' + data[i].vendor;
            if (data[i].model != '') {
                if (data[i].vendor != '')
                    desc += ' ';
                else
                    desc += ' - ';
                desc += ' ' + data[i].model;
            }
            desc += ' - ' + data[i].size + 'GB';
            options += "<option value='" + data[i].name + "'>" + desc + "</option>";
        }
        $(fppFileCopy.config.usbDevice).html(options);
        $(fppFileCopy.config.usbDevice).parent().closest('td').removeClass('fpp-backup-action-loading');

        if (options != '') {
            var dir = $(fppFileCopy.config.direction).val();
            if (dir == 'FROMUSB')
                fppFileCopy.getBackupDeviceDirectories();
            else if (dir == 'TOUSB')
                fppFileCopy.getRestoreDeviceDirectories();
        }

        if (fppFileCopy.config.onDevicesLoaded)
            fppFileCopy.config.onDevicesLoaded(data);
    }).fail(function () {
        $(fppFileCopy.config.usbDevice).html('');
        $(fppFileCopy.config.usbDevice).parent().closest('td').removeClass('fpp-backup-action-loading');
    });
};

fppFileCopy.getBackupDeviceDirectories = function () {
    var dev = $(fppFileCopy.config.usbDevice).val();
    if (!dev) {
        $(fppFileCopy.config.pathSelect).html("<option value=''>No USB Device Selected</option>");
        return;
    }
    $(fppFileCopy.config.pathSelect).html('<option>Loading...</option>');
    $.get('api/backups/list/' + dev).done(function (data) {
        fppFileCopy.populateBackupDirs(data);
    }).fail(function () {
        $(fppFileCopy.config.pathSelect).html('');
    });
};

fppFileCopy.getRestoreDeviceDirectories = function () {
    var dev = $(fppFileCopy.config.usbDevice).val();
    if (!dev) {
        $(fppFileCopy.config.pathSelect).html("<option value=''>No USB Device Selected</option>");
        return;
    }
    $('#usbDirectories').html('');
    $.get('api/backups/list/' + dev).done(function (data) {
        var options = '';
        for (i = 0; i < data.length; i++) {
            if (data[i].substring(0, 5) != 'ERROR')
                options += "<option value='" + data[i] + "'>" + data[i] + "</option>";
        }
        $('#usbDirectories').html(options);
    });
};

fppFileCopy.usbDeviceChanged = function () {
    var direction = $(fppFileCopy.config.direction).val();
    if (direction == 'FROMUSB')
        fppFileCopy.getBackupDeviceDirectories();
    else if (direction == 'TOUSB')
        fppFileCopy.getRestoreDeviceDirectories();
};

fppFileCopy.getBackupDirsViaAPI = function (host, remoteStorageSelected, excludeRoot) {
    $(fppFileCopy.config.pathSelect).html('<option>Loading...</option>');
    var url = 'api/backups/list';
    if (remoteStorageSelected && remoteStorageSelected !== 'none') {
        url = 'api/backups/list/' + encodeURIComponent(remoteStorageSelected);
    }
    if (host && host !== window.location.host) {
        url += (url.indexOf('?') === -1 ? '?' : '&') + 'ip=' + encodeURIComponent(host);
    }
    $(fppFileCopy.config.pathSelect).parent().closest('td').addClass('fpp-backup-action-loading');
    $.get(url).done(function (data) {
        fppFileCopy.populateBackupDirs(data, excludeRoot);
        $(fppFileCopy.config.pathSelect).parent().closest('td').removeClass('fpp-backup-action-loading');
    }).fail(function () {
        $(fppFileCopy.config.pathSelect).html('');
        $(fppFileCopy.config.pathSelect).parent().closest('td').removeClass('fpp-backup-action-loading');
    });
};

fppFileCopy.getBackupHostBackupDirs = function (remoteStorageSelected) {
    var selStorage = (remoteStorageSelected && remoteStorageSelected !== 'none') ? remoteStorageSelected : '';
    fppFileCopy.getBackupDirsViaAPI($(fppFileCopy.config.host).val(), selStorage, true);
    fppFileCopy.checkRemoteHasRsyncdEnabled($(fppFileCopy.config.host).val());
};

fppFileCopy.populateBackupDirs = function (data, excludeRoot) {
    var options = '';
    for (var i = 0; i < data.length; i++) {
        if (excludeRoot && data[i] == '/') continue;
        if (data[i].substring(0, 5) != 'ERROR')
            options += "<option value='" + data[i] + "'>" + data[i] + "</option>";
        else
            options += "<option value=''>" + data[i] + "</option>";
    }
    $(fppFileCopy.config.pathSelect).html(options);
};

fppFileCopy.getRemoteHostUSBStorage = function () {
    var host = $(fppFileCopy.config.host).val();
    if (!host) return;
    var requestUrl = 'api/backups/devices?ip=' + encodeURIComponent(host);
    var defaultOption = "<option value='none' selected>Default FPP Storage</option>";
    $(fppFileCopy.config.remoteStorage).parent().closest('td').addClass('fpp-backup-action-loading');
    $.get(requestUrl).done(function (data) {
        var options = '';
        for (var i = 0; i < data.length; i++) {
            var desc = data[i].name;
            if (data[i].vendor != '') desc += ' - ' + data[i].vendor;
            if (data[i].model != '') {
                if (data[i].vendor != '') desc += ' ';
                else desc += ' - ';
                desc += ' ' + data[i].model;
            }
            desc += ' - ' + data[i].size + 'GB';
            options += "<option value='" + data[i].name + "'>" + desc + "</option>";
        }
        $(fppFileCopy.config.remoteStorage).html(defaultOption + options);
        $(fppFileCopy.config.remoteStorage).parent().closest('td').removeClass('fpp-backup-action-loading');
        if (options) fppFileCopy.getBackupHostBackupDirs($(fppFileCopy.config.remoteStorage).val());
    }).fail(function () {
        $(fppFileCopy.config.remoteStorage).html(defaultOption);
        $(fppFileCopy.config.remoteStorage).parent().closest('td').removeClass('fpp-backup-action-loading');
    });
};

fppFileCopy.checkRemoteHasRsyncdEnabled = function (host) {
    if (!host) return;
    $.ajax({
        url: 'api/settings/Service_rsync?ip=' + encodeURIComponent(host),
        type: 'GET',
        success: function (data) {
            if (typeof (data) !== 'undefined') {
                var rsyncd_setting_value = data['value'];
                if (rsyncd_setting_value === 0 || rsyncd_setting_value === '0') {
                    $('.host_rsyncd_warning').remove();
                    var hostHref = "<a href='http://" + host + "/settings.php#settings-services' target='_blank'>" + host + "</a>";
                    $(fppFileCopy.config.host).after('<span class="host_rsyncd_warning"><b>WARNING!</b> Rsync server is disabled on remote host, please enable rsync under FPP Settings -> Services -> OS Services on ' + hostHref + ' </span');
                } else {
                    $('.host_rsyncd_warning').remove();
                }
            }
        },
        error: function () {
            $.jGrowl('Error occurred reading the Service_rsync value from host - ' + host, { themeState: 'danger' });
        }
    });
};

fppFileCopy.closeCopyDialog = function () {
    var cd_remoteHost = $(fppFileCopy.config.host).val();
    var cd_remoteStorage = $(fppFileCopy.config.remoteStorage).val();

    if (cd_remoteHost && cd_remoteStorage && cd_remoteStorage !== 'none') {
        $.post('api/backups/devices/unmount/' + encodeURIComponent(cd_remoteStorage) + '/remote_filecopy?ip=' + encodeURIComponent(cd_remoteHost));
    }

    CloseModalDialog(fppFileCopy.config.popupModalId);
};

fppFileCopy.copyDone = function () {
    EnableModalDialogCloseButton(fppFileCopy.config.popupModalId);
    if (fppFileCopy.config.onCopyDone) {
        fppFileCopy.config.onCopyDone();
    }
};

fppFileCopy.copyTimeoutError = function () {
    var logUrl = 'api/file/Logs/fpp_backup_filecopy.log';
    var timeoutMsg = '!!! Attempting to track file copy process via its fallback log file...\n The file copy is still running in the background and will complete in due course.\n\n';
    var iterations = 0;
    var iterationsWithNoDataWarningsIssued = 1;
    var noNewDataIterationCount = 600;
    var last_response_len = 0;
    var outputArea = $(fppFileCopy.config.copyText);

    outputArea.val(timeoutMsg);

    var tailInterval = setInterval(function () {
        $.get(logUrl, function (text) {
            if (text === 'File does not exist.') {
                clearInterval(tailInterval);
                fppFileCopy.copyDone();
            } else {
                if (last_response_len === 0) {
                    last_response_len = text.length;
                }
                if (last_response_len === text.length) {
                    iterations += 1;
                } else {
                    iterations = 0;
                    last_response_len = text.length;
                }
                var noNewDataError = '';
                if (iterations === (noNewDataIterationCount * iterationsWithNoDataWarningsIssued)) {
                    iterationsWithNoDataWarningsIssued += 1;
                    noNewDataError = '\n!!! WARNING: No new log entries received in over ' + Math.floor(iterations / 60) + ' minutes, still waiting... !!!\n';
                }
                outputArea.val(timeoutMsg + text + noNewDataError);
                outputArea.scrollTop(outputArea.prop('scrollHeight'));

                if (text.includes('unmounted from') || text.length === 0) {
                    clearInterval(tailInterval);
                    fppFileCopy.copyDone();
                }
            }
        });
    }, 1000);
};
