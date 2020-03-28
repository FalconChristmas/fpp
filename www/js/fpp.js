
STATUS_IDLE = "0";
STATUS_PLAYING = "1";
STATUS_STOPPING_GRACEFULLY = "2";
STATUS_STOPPING_GRACEFULLY_AFTER_LOOP = "3";


// Globals
gblCurrentPlaylistIndex = 0;
gblCurrentPlaylistEntryType = '';
gblCurrentPlaylistEntrySeq = '';
gblCurrentPlaylistEntrySong = '';
gblCurrentLoadedPlaylist  = '';
gblCurrentLoadedPlaylistCount = 0;

var max_retries = 60;
var retry_poll_interval_arr = [];

var minimalUI = 0;

var statusTimeout = null;
var lastStatus = '';

function PadLeft(string,pad,length) {
    return (new Array(length+1).join(pad)+string).slice(-length);
}

function SecondsToHuman(seconds) {
    var m = parseInt(seconds / 60);
    var s = parseInt(seconds % 60);
    var human = PadLeft(m, '0', 2) + ':' + PadLeft(s, '0', 2);

    return human;
}

function versionToNumber(version)
{
    // convert a version string like 2.7.1-2-dirty to "20701"
    if (version.charAt(0) == 'v') {
        version = version.substr(1);
    }
    if (version.indexOf("-") != -1) {
        version = version.substr(0, version.indexOf("-"));
    }
    var parts = version.split('.');
    
    while (parts.length < 3) {
        parts.push("0");
    }
    var number = 0;
    for (var x = 0; x < 3; x++) {
        var val = parseInt(parts[x]);
        if (val >= 9990) {
            return number * 10000 + 9999;
        } else if (val > 99) {
            val = 99;
        }
        number = number * 100 + val;
    }
    return number;
}

function RegexCheckData(regexStr, value, desc, hideValue = false) {
    var regex = new RegExp(regexStr);

    if (regex.test(value)) {
        return true;
    }

    if (hideValue)
        DialogError('Data Format Error', "ERROR: The new value does not match proper format: " + desc);
    else
        DialogError('Data Format Error', "ERROR: '" + value + "' does not match proper format: " + desc);
    return false;
}

// Compare two version numbers
function CompareFPPVersions(a, b) {
	// Turn any non-string version numbers into a string
	a = "" + a;
	b = "" + b;
    a = versionToNumber(a);
    b = versionToNumber(b);

	if (a > b) {
		return 1;
	} else if (a < b) {
		return -1;
	}

	return 0;
}

function InitializeTimeInputs(format = 'H:i:s') {
    $('.time').timepicker({
        'timeFormat': format,
        'typeaheadHighlight': false
    });
}

function InitializeDateInputs(format = 'yy-mm-dd') {
    $('.date').datepicker({
        'changeMonth': true,
        'changeYear': true,
        'dateFormat': format,
        'minDate': new Date(MINYEAR - 1, 1 - 1, 1),
        'maxDate': new Date(MAXYEAR, 12 - 1, 31),
        'showButtonPanel': true,
        'selectOtherMonths': true,
        'showOtherMonths': true,
        'yearRange': "" + MINYEAR +":" + MAXYEAR,
        'autoclose': true,
    });
}

function DeleteSelectedEntries(item) {
    $('#' + item).find('.selectedEntry').remove();
}

function AddTableRowFromTemplate(table) {
    $('#' + table).append($('#' + table).parent().parent().find('.fppTableRowTemplate').find('tr').parent().html());

    return $('#' + table + ' > tr').last();
}

function HandleTableRowMouseClick(event, row) {
    if ((event.target.nodeName == 'INPUT') ||
        (event.target.nodeName == 'TEXTAREA') ||
        (event.target.nodeName == 'SELECT'))
        return;

    event.preventDefault(); // prevent mouse move from highlighting text

    if (row.hasClass('selectedEntry')) {
        row.removeClass('selectedEntry');
    } else {
        if (event.shiftKey) {
            var na = row.nextAll().length;
            var nl = row.nextUntil('.selectedEntry').length;
            var pa = row.prevAll().length;
            var pl = row.prevUntil('.selectedEntry').length;

            if (pa == pl)
                pl = -1;

            if (na == nl)
                nl = -1;

            if ((pl >= 0) && (nl >= 0)) {
                if (nl > pl) {
                    row.prevUntil('.selectedEntry').addClass('selectedEntry');
                } else {
                    row.nextUntil('.selectedEntry').addClass('selectedEntry');
                }
            } else if (pl >= 0) {
                row.prevUntil('.selectedEntry').addClass('selectedEntry');
            } else if (nl >= 0) {
                row.nextUntil('.selectedEntry').addClass('selectedEntry');
            }
        } else {
            if (!event.ctrlKey) {
                row.parent().find('tr').removeClass('selectedEntry');
            }
        }

        row.addClass('selectedEntry');
    }
}

function Get(url, async) {
    var result = {};

    $.ajax({
        url: url,
        type: 'GET',
        async: async,
        dataType: 'json',
        success: function(data) {
            result = data;
        },
        error: function() {
            $.jGrowl('Error: Unable to get ' + url);
        }
    });

    return result;
}

function GetSync(url) {
    return Get(url, false);
}

function GetAsync(url) {
    return Get(url, true);
}

function SetupToolTips(delay = 100) {
    $(document).tooltip({
        content: function() {
            $('.ui-tooltip').hide();
            var id = $(this).attr('id');
            id = id.replace('_img', '_tip');
            return $('#' + id).html();
        },
        hide: { delay: delay }
    });
}

function ShowTableWrapper(tableName) {
    if ($('#' + tableName).parent().parent().hasClass('fppTableWrapperAsTable'))
        $('#' + tableName).parent().parent().attr('style', 'display: table');
    else
        $('#' + tableName).parent().parent().show();
}

function HideTableWrapper(tableName) {
    $('#' + tableName).parent().parent().hide();
}

function ShowPlaylistDetails() {
	$('#playlistDetailsWrapper').show();
	$('#btnShowPlaylistDetails').hide();
	$('#btnHidePlaylistDetails').show();
}

function HidePlaylistDetails() {
	$('#playlistDetailsWrapper').hide();
	$('#btnShowPlaylistDetails').show();
	$('#btnHidePlaylistDetails').hide();
}

function PopulateLists() {
    DisableButtonClass('playlistEditButton');
	PlaylistTypeChanged();
    PopulatePlaylists(false);
}

function PlaylistEntryTypeToString(type)
{
    switch (type ) {
        case 'both':        return 'Seq+Med';
        case 'branch':      return 'Branch';
        case 'command':     return 'Command';
        case 'dynamic':     return 'Dynamic';
        case 'event':       return 'Event';
        case 'image':       return 'Image';
        case 'media':       return 'Media';
        case 'mqtt':        return 'MQTT';
        case 'pause':       return 'Pause';
        case 'playlist':    return 'Playlist';
        case 'plugin':      return 'Plugin';
        case 'remap':       return 'Remap';
        case 'script':      return 'Script';
        case 'sequence':    return 'Sequence';
        case 'url':         return 'URL';
        case 'volume':      return 'Volume';
    }
}

function psiDetailsBegin() {
    return "<div class='psiDetailsWrapper'><div class='psiDetails'>";
}

function psiDetailsArgBegin() {
    return "<div class='psiDetailsArg'>";
}

function psiDetailsHeader(text) {
    return "<div class='psiDetailsHeader'>" + text + ":</div>";
}

function psiDetailsData(name, value, units = '', hide = false) {
    var str = '';
    var style = "";

    if (hide)
        style = " style='display: none;'";

    if (units == '') {
        return "<div class='psiDetailsData field_" + name + "'" + style + ">" + value + "</div>";
    }

    return "<div class='psiDetailsData'><span class='field_" + name + "'" + style + ">" + value + "</span> " + units + "</div>";
}

function psiDetailsArgEnd() {
    return "</div>";
}

function psiDetailsLF() {
    return "<div class='psiDetailsLF'></div>";
}

function psiDetailsEnd() {
    return "</div></div>";
}

function psiDetailsForEntrySimple(entry, editMode) {
    var pet = playlistEntryTypes[entry.type];
    var result = "";
    var keys = Object.keys(pet.args);
    for (var i = 0; i < keys.length; i++) {
        var a = pet.args[keys[i]];

        if (((!a.hasOwnProperty('simpleUI')) ||
             (!a.simpleUI)) &&
            (a.name != 'args')) {
            continue;
        }

        if ((editMode) &&
            (a.hasOwnProperty('statusOnly')) &&
            (a.statusOnly == true)) {
            continue;
        }

        if ((!a.optional) ||
            ((entry.hasOwnProperty(a.name)) &&
             (entry[a.name] != ''))) {
            var partialResult = '';

            if (a.type == 'args') {
                if ((entry[a.name].length == 1) &&
                    ($.isNumeric(entry[a.name][0]))) {
                    partialResult += entry[a.name][0];
                } else {
                    for (var x = 0; x < entry[a.name].length; x++) {
                        if (partialResult != '')
                            partialResult += ' ';

                        partialResult += "\"" + entry[a.name][x] + "\"";
                    }
                }
            } else if (a.type == 'array') {
                var akeys = Object.keys(entry[a.name]);
                if ((akeys.length == 1) &&
                    ($.isNumeric(entry[a.name][akeys[0]]))) {
                    partialResult += entry[a.name][akeys[0]];
                } else {
                    for (var x = 0; x < akeys.length; x++) {
                        if (partialResult != '')
                            partialResult += ' ';

                        partialResult += "\"" + entry[a.name][akeys[x]] + "\"";
                    }
                }
            } else {
                if (a.hasOwnProperty('contents')) {
                    var ckeys = Object.keys(a.contents);
                    for (var x = 0; x < ckeys.length; x++) {
                        if (a.contents[ckeys[x]] == entry[a.name]) {
                            partialResult += ckeys[x];
                        }
                    }
                } else {
                    partialResult += entry[a.name];
                }

                if (a.hasOwnProperty('unit')) {
                    partialResult += " " + a.unit;
                }
            }

            if (partialResult != '') {
                if (result != '')
                    result += "&nbsp;&nbsp;&nbsp;<b>|</b>&nbsp;&nbsp;&nbsp;";

                result += partialResult;
            }
        }
    }

    result += "<br>";

    return result;
}

function psiDetailsForEntry(entry, editMode) {
    var pet = playlistEntryTypes[entry.type];
    var result = "";

    result += psiDetailsBegin();

    var children = [];
    var childrenToShow = [];
    var divs = 0;
    var keys = Object.keys(pet.args);
    for (var i = 0; i < keys.length; i++) {
        var a = pet.args[keys[i]];

        if ((editMode) &&
            (a.hasOwnProperty('statusOnly')) &&
            (a.statusOnly == true)) {
            continue;
        }

        if ((children.includes(a.name)) &&
            (!childrenToShow.includes(a.name))) {
            continue;
        }

        if ((!a.optional) &&
            (!entry.hasOwnProperty(a.name))) {
            if (a.hasOwnProperty("default")) {
                entry[a.name] = a.default;
            } else {
                if (a.type == 'int')
                    entry[a.name] = 0;
                else if (a.type == 'bool')
                    entry[a.name] = false;
                else
                    entry[a.name] = "";
            }
        }

        if ((!a.optional) ||
            ((entry.hasOwnProperty(a.name)) &&
             (entry[a.name] != ''))) {
            if (typeof a['children'] === 'object') {
                var val = entry[a.name];
                var ckeys = Object.keys(a.children);
                for (var c = 0; c < ckeys.length; c++) {
                    for (var x = 0; x < a.children[ckeys[c]].length; x++) {
                        if (!children.includes(a.children[ckeys[c]][x]))
                            children.push(a.children[ckeys[c]][x]);

                        if (val == ckeys[c]) {
                            childrenToShow.push(a.children[ckeys[c]][x]);
                        }
                    }
                }
            }

            if (i > 0)
                result += psiDetailsLF();

            if (a.type == 'args') {
                for (var x = 0; x < entry[a.name].length; x++) {
                    if (x > 0)
                        result += psiDetailsLF();

                    result += psiDetailsArgBegin();
                    result += psiDetailsHeader('Arg #' + (x+1));
                    result += psiDetailsData(a.name + '_' + (x+1), entry[a.name][x]);
                    result += psiDetailsArgEnd();
                }
            } else if (a.type == 'array') {
                var keys = Object.keys(entry[a.name]);
                for (var x = 0; x < keys.length; x++) {
                    if (x > 0)
                        result += psiDetailsLF();

                    result += psiDetailsArgBegin();
                    result += psiDetailsHeader('Extra Data #' + (x+1));
                    result += psiDetailsData(a.name + '_' + (x+1), entry[a.name][x]);
                    result += psiDetailsArgEnd();
                }
            } else {
                var units = '';
                if (a.hasOwnProperty('unit')) {
                    units = a.unit;
                }

                result += psiDetailsArgBegin();
                result += psiDetailsHeader(a.description);

                if (a.hasOwnProperty('contents')) {
                    result += psiDetailsData(a.name, entry[a.name], '', true);

                    var ckeys = Object.keys(a.contents);
                    for (var x = 0; x < ckeys.length; x++) {
                        if (a.contents[ckeys[x]] == entry[a.name]) {
                            result += ckeys[x] + ' ' + units;
                        }
                    }
                } else {
                    result += psiDetailsData(a.name, entry[a.name], units);
                }

                result += psiDetailsArgEnd();
            }
        }
    }

    result += psiDetailsEnd();

    return result;
}

function psiDetailsForEntryBranch(entry, editMode)
{
    var result = "";

    result += psiDetailsBegin();

    var branchStr = "Invalid Config";
    if (entry.trueNextItem < 999) {
        branchStr = entry.startTime + " < X < " + entry.endTime;
        branchStr += ", True: " + BranchItemToString(entry.trueNextBranchType, entry.trueNextSection, entry.trueNextItem);

        if (entry.falseNextItem < 999) {
            branchStr += ", False: " + BranchItemToString(entry.falseNextBranchType, entry.falseNextSection, entry.falseNextItem);
        }
    }

    result += psiDetailsHeader('Test');
    result += psiDetailsData('test', branchStr);
    result += psiDetailsEnd();

    var keys = Object.keys(entry);
    for (var i = 0; i < keys.length; i++) {
        var a = entry[keys[i]];
        if (keys[i] == 'compInfo') {
            var akeys = Object.keys(a);
            for (var x = 0; x < akeys.length; x++) {
                var aa = entry[keys[i]][akeys[x]];
                result += "<span style='display:none;' class='field_compInfo_" + akeys[x] + "'>" + aa + "</span>";
            }
        } else {
            result += "<span style='display:none;' class='field_" + keys[i] + "'>" + a + "</span>";
        }
    }


    return result;
}

function VerbosePlaylistItemDetailsToggled() {
    if ($('#verbosePlaylistItemDetails').is(':checked')) {
        $('.psiData').show();
        $('.psiDataSimple').hide();
    } else {
        $('.psiDataSimple').show();
        $('.psiData').hide();
    }
}

function GetPlaylistDurationDiv(entry) {
    var h = "";
    var s = 0;

	if ((entry.hasOwnProperty('duration')) &&
        (entry.duration > 0)) {
        h = "Length: " + SecondsToHuman(entry.duration);
        s = entry.duration;
    }

    return "<div class='psiDuration'><span class='humanDuration'>" + h + "</span><span class='psiDurationSeconds'>" + s + "</span></div>";
}

function GetPlaylistRowHTML(ID, entry, editMode)
{
    var HTML = "";
    var rowNum = ID + 1;

    if (editMode)
        HTML += "<tr class='playlistRow'>";
    else
        HTML += "<tr id='playlistRow" + rowNum + "' class='playlistRow'>";

    HTML += "<td class='colPlaylistNumber";

    if (editMode)
        HTML += " colPlaylistNumberDrag";

    if (editMode)
        HTML += " playlistRowNumber'>" + rowNum + ".</td>";
    else
        HTML += " playlistRowNumber' id='colEntryNumber" + rowNum + "'>" + rowNum + ".</td>";

    var pet = playlistEntryTypes[entry.type];
    var deprecated = "";

    if ((typeof pet.deprecated === "number") &&
        (pet.deprecated == 1)) {
        deprecated = "<font color='red'><b>*</b></font>";
        $('#deprecationWarning').show();
    }

    HTML += "<td><div class='psi'><div class='psiHeader' >" + PlaylistEntryTypeToString(entry.type) + ":" + deprecated + "<span style='display: none;' class='entryType'>" + entry.type + "</span></div><div class='psiData'>";

    if (entry.type == 'dynamic') {
        HTML += psiDetailsForEntry(entry, editMode);

		if (entry.hasOwnProperty('dynamic'))
            HTML += psiDetailsForEntry(entry.dynamic, editMode);
    } else if (entry.type == 'branch') {
        HTML += psiDetailsForEntryBranch(entry, editMode);
    } else {
        HTML += psiDetailsForEntry(entry, editMode);
    }

    HTML += "</div>";

    HTML += "<div class='psiDataSimple'>";
    if (entry.type == 'dynamic') {
        HTML += psiDetailsForEntrySimple(entry, editMode);

		if (entry.hasOwnProperty('dynamic'))
            HTML += psiDetailsForEntrySimple(entry.dynamic, editMode);
    } else {
        HTML += psiDetailsForEntrySimple(entry, editMode);
    }
    HTML += "</div>";

    HTML += GetPlaylistDurationDiv(entry) + "</div></td></tr>";

    return HTML;
}

function BranchItemToString(branchType, nextSection, nextIndex) {
    if (typeof branchType == "undefined") {
        branchType = "Index";
    }
    if (branchType == "None") {
        return "None";
    } else if (branchType == "" || branchType == "Index") {
        var r = "Index: "
        if (nextSection != "") {
            r = r + nextSection + "/";
        }
        r = r + nextIndex;
        return r;
    } else if (branchType == "Offset") {
        return "Offset: " + nextIndex;
    }
}

var oldPlaylistEntryType = '';
function PlaylistTypeChanged() {
	var type = $('#pe_type').val();

	$('.playlistOptions').hide();
    $('#pbody_' + type).show();

    var oldSequence = '';
    if ((oldPlaylistEntryType == 'sequence') ||
        (oldPlaylistEntryType == 'both')) {
        oldSequence = $('.arg_sequenceName').val();
    }

    var oldMedia = '';
    if ((oldPlaylistEntryType == 'media') ||
        (oldPlaylistEntryType == 'both')) {
        oldMedia = $('.arg_mediaName').val();
    }

    $('#playlistEntryOptions').html('');
    $('#playlistEntryCommandOptions').html('');
    PrintArgInputs('playlistEntryOptions', true, playlistEntryTypes[type].args);

    if (oldPlaylistEntryType == '') { // First load on page defaults to 'both'
        if ($('.arg_sequenceName option').length == 0) {
            if ($('.arg_mediaName option').length >= 0) {
                oldPlaylistEntryType = 'both';
                $('#pe_type').val('media');
                PlaylistTypeChanged();
                return;
            }
        } else {
            if ($('.arg_mediaName option').length == 0) {
                oldPlaylistEntryType = 'both';
                $('#pe_type').val('sequence');
                PlaylistTypeChanged();
                return;
            }
        }
    }

	if (type == 'both')
	{
		$("#autoSelectWrapper").show();
		$("#autoSelectMatches").prop('checked', true);
	}

    if (oldSequence != '') {
        $('.arg_sequenceName').val(oldSequence);
    }

    if (oldMedia != '') {
        $('.arg_mediaName').val(oldMedia);
    }

    oldPlaylistEntryType = type;
}

function PlaylistNameOK(name) {
    var tmpName = name.replace(/[^-a-zA-Z0-9_ ]/g,'');
    if (name != tmpName) {
        DialogError('Invalid Playlist Name', 'You may use only letters, numbers, spaces, hyphens, and underscores in playlist names.');
        return 0;
    }

    return 1;
}

function LoadPlaylistDetails(name) {
    $.get('/api/playlist/' + name
    ).done(function(data) {
        PopulatePlaylistDetails(data, 1);
        RenumberPlaylistEditorEntries();
        UpdatePlaylistDurations();
        VerbosePlaylistItemDetailsToggled();
        $("#tblPlaylistLeadInHeader tr").get(0).scrollIntoView();
    }).fail(function() {
        DialogError('Error loading playlist', 'Error loading playlist details!');
    });
}

function CreateNewPlaylist() {
	var name = $('#txtNewPlaylistName').val().replace(/ /,'_');

    if (!PlaylistNameOK(name))
        return;

    if (playListArray.includes(name)) {
		DialogError('Playlist name conflict', "Found existing playlist named '" + name + "'.  Loading existing playlist.");
        $('#playlistSelect option[value="' + name + '"]').prop('selected', true);
        LoadPlaylistDetails(name);
        return;
    }

    SetPlaylistName(name);
    $('#tblPlaylistLeadIn').html("<tr id='tblPlaylistLeadInPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>");
    $('#tblPlaylistLeadIn').show();
    $('#tblPlaylistLeadInHeader').show();

    $('#tblPlaylistMainPlaylist').html("<tr id='tblPlaylistMainPlaylistPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>");
    $('#tblPlaylistMainPlaylist').show();
    $('#tblPlaylistMainPlaylistHeader').show();

    $('#tblPlaylistLeadOut').html("<tr id='tblPlaylistLeadOutPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>");
    $('#tblPlaylistLeadOut').show();
    $('#tblPlaylistLeadOutHeader').show();

    EnableButtonClass('playlistEditButton');
    DisableButtonClass('playlistExistingButton');
    DisableButtonClass('playlistDetailsEditButton');
}

function EditPlaylist() {
    var name = $('#playlistSelect').val();
    EnableButtonClass('playlistEditButton');
    DisableButtonClass('playlistDetailsEditButton');

    LoadPlaylistDetails(name);
}

function EnableButtonClass(c) {
    $('.' + c).addClass('buttons');
    $('.' + c).removeClass('disableButtons');
    $('.' + c).removeAttr("disabled");
}

function DisableButtonClass(c) {
    $('.' + c).removeClass('buttons');
    $('.' + c).addClass('disableButtons');
    $('.' + c).attr("disabled", "disabled");
}

function RenumberPlaylistEditorEntries() {
    var id = 1;
    var sections = ['LeadIn', 'MainPlaylist', 'LeadOut'];
    for (var s = 0; s < sections.length; s++) {
        $('#tblPlaylist' + sections[s] + ' tr.playlistRow').each(function() {
            $(this).find('.playlistRowNumber').html('' + id + '.');
            id++;
        });
    }
}

function UpdatePlaylistDurations() {
    var sections = ['LeadIn', 'MainPlaylist', 'LeadOut'];
    for (var s = 0; s < sections.length; s++) {
        var duration = 0;

        $('#tblPlaylist' + sections[s] + ' tr.playlistRow').each(function() {
            if ($(this).find('.psiDurationSeconds').length)
                duration += parseFloat($(this).find('.psiDurationSeconds').html());
        });

        var items = $('#tblPlaylist' + sections[s] + ' tr.playlistRow').length;
        $('.playlistItemCount' + sections[s]).html(items);
        if (items == 1)
            items = items.toString() + " item";
        else
            items = items.toString() + " items";

        $('.playlistItemCountWithLabel' + sections[s]).html(items);

        $('.playlistDuration' + sections[s]).html(SecondsToHuman(duration));

        if (sections[s] == 'MainPlaylist')
            $('#playlistDuration').html(duration);
    }
}

function GetSequenceDuration(sequence, updateUI, row) {
    var durationInSeconds = 0;
    var file = sequence.replace(/.fseq$/, '');
    $.ajax({
        url: "/api/sequence/" + encodeURIComponent(file) + '/meta',
        type: 'GET',
        async: updateUI,
        dataType: 'json',
        success: function(data) {
            durationInSeconds = 1.0 * data.NumFrames / (1000 / data.StepTime);
            if (updateUI) {
                var humanDuration = SecondsToHuman(durationInSeconds);

                row.find('.psiDurationSeconds').html(durationInSeconds);
                row.find('.humanDuration').html('<b>Length: </b>' + humanDuration);

                UpdatePlaylistDurations();
            }
        },
        error: function() {
            $.jGrowl('Error: Unable to get metadata for ' + sequence);
        }
    });

    return durationInSeconds;
}

function SetPlaylistItemMetaData(row) {
    var type = row.find('.entryType').html();
    var file = row.find('.field_mediaName').html();

    if (((type == 'both') || (type == 'media')) &&
        (typeof file != 'undefined')) {
        file = $('<div/>').html(file).text(); // handle any & or other chars that got converted
        $.get('/api/media/' + encodeURIComponent(file) + '/duration').success(function(mdata) {
            var duration = -1;

            if ((mdata.hasOwnProperty(file)) &&
                (mdata[file].hasOwnProperty('duration'))) {
                duration = mdata[file].duration;
            }

            if (type == 'both') {
                var sDuration = GetSequenceDuration(row.find('.field_sequenceName').html(), false, '');

                // Playlist/PlaylistEntryBoth.cpp ends whenever shortest item ends
                if ((duration > sDuration) || (duration < 0))
                    duration = sDuration;
            }

            var humanDuration = SecondsToHuman(duration);

            row.find('.psiDurationSeconds').html(duration);
            row.find('.humanDuration').html('<b>Length: </b>' + humanDuration);

            UpdatePlaylistDurations();
        }).fail(function() {
            $.jGrowl('Error: Unable to get metadata for ' + file);
        });
    } else if (type == 'sequence') {
        GetSequenceDuration(row.find('.field_sequenceName').html(), true, row);
    } else if (type == 'playlist') {
        $.ajax({
            url: "/api/playlist/" + row.find('.field_name').html(),
            type: 'GET',
            async: false,
            dataType: 'json',
            success: function(data) {
                if (data.hasOwnProperty('playlistInfo')) {
                    var duration = data.playlistInfo.total_duration;
                    var humanDuration = SecondsToHuman(duration);

                    row.find('.psiDurationSeconds').html(duration);
                    row.find('.humanDuration').html('<b>Length: </b>' + humanDuration);

                    UpdatePlaylistDurations();
                }
            }
        });
    }
}

function PopulatePlaylistItemDuration(row, editMode) {
    var type = row.find('.entryType').html();

    if (!editMode) {
        var duration = row.find('.psiDurationSeconds').html();
        if (duration != "0")
            return;
    }

    SetPlaylistItemMetaData(row);

    if (type == 'pause') {
        var duration = parseFloat(row.find('.field_duration').html());
        row.find('.psiDurationSeconds').html(duration);
        row.find('.humanDuration').html('<b>Length: </b>' + SecondsToHuman(duration));
        UpdatePlaylistDurations();
    }
}

function AddPlaylistEntry(mode) {
    if (mode && !$('#tblPlaylistDetails').find('.playlistSelectedEntry').length) {
        DialogError('No playlist item selected', "Error: No playlist item selected.");
        return;
    }

    $('#tblPlaylistMainPlaylistPlaceHolder').remove();

    var type = $('#pe_type').val();
    var pet = playlistEntryTypes[type];

    var pe = {};
    pe.type = type;
    pe.enabled = 1;  // no way to disable currently, so force this
    pe.playOnce = 0; // Not currently used by player

    var keys = Object.keys(pet.args);
    for (var i = 0; i < keys.length; i++) {
        var a = pet.args[keys[i]];

        var style = $('#playlistEntryOptions').find('.arg_' + a.name).parent().parent().attr('style');
        if ((typeof style != 'undefined') && (style.includes('display: none;'))) {
            continue;
        }

        if (a.type == 'int') {
            pe[a.name] = parseInt($('#playlistEntryOptions').find('.arg_' + a.name).val());
        } else if (a.type == 'float') {
            pe[a.name] = parseFloat($('#playlistEntryOptions').find('.arg_' + a.name).val());
        } else if (a.type == 'bool') {
            pe[a.name] = $('#playlistEntryOptions').find('.arg_' + a.name).is(':checked') ? 'true' : 'false';
        } else if ((a.type == 'time') || (a.type == 'date')) {
            pe[a.name] = $('#playlistEntryOptions').find('.arg_' + a.name).val();
        } else if (a.type == 'array') {
            var f = {};
            for (x = 0; x < a.keys; x++) {
                f[a.keys[x]] = $('#playlistEntryOptions').find('.arg_' + a.name + '_' + a.keys[x]).val();
            }
            pe[a.name] = f;
        } else if (a.type == 'args') {
            var arr = [];
            for (x = 1; x <= 20; x++) {
                if ($('#playlistEntryCommandOptions_arg_' + x).length) {
                    arr.push($('#playlistEntryCommandOptions_arg_' + x).val());
                }
            }
            pe[a.name] = arr;
        } else if (a.type == 'string') {
            pe[a.name] = $('#playlistEntryOptions').find('.arg_' + a.name).val();
        } else {
            pe[a.name] = $('#playlistEntryOptions').find('.arg_' + a.name).html();
        }
    }

    var newRow;
    var html = GetPlaylistRowHTML(0, pe, 1);
    if (mode == 1) { // replace
        var row = $('#tblPlaylistDetails').find('.playlistSelectedEntry');
        $(row).after(html);
        $(row).removeClass('playlistSelectedEntry');
        newRow = $(row).next();
        newRow.addClass('playlistSelectedEntry');
        $(row).remove();
    } else if (mode == 2) { // insert before
        var row = $('#tblPlaylistDetails').find('.playlistSelectedEntry');
        $(row).before(html);
        $(row).removeClass('playlistSelectedEntry');
        newRow = $(row).prev();
        newRow.addClass('playlistSelectedEntry');
    } else if (mode == 3) { // insert after
        var row = $('#tblPlaylistDetails').find('.playlistSelectedEntry');
        $(row).after(html);
        $(row).removeClass('playlistSelectedEntry');
        newRow = $(row).next();
        newRow.addClass('playlistSelectedEntry');
    } else {
        $('#tblPlaylistMainPlaylist').append(html);

        $('#tblPlaylistDetails tbody tr').removeClass('playlistSelectedEntry');

        newRow = $('#tblPlaylistMainPlaylist > tr').last();
        $(newRow).addClass('playlistSelectedEntry');
    }

    RenumberPlaylistEditorEntries();

    PopulatePlaylistItemDuration($(newRow), 1);

    if (type == 'pause')
        UpdatePlaylistDurations();

    VerbosePlaylistItemDetailsToggled();
}

function GetPlaylistEntry(row) {
    var e = {};
    e.type = $(row).find('.entryType').html();
    e.enabled = 1;  // no way to disable currently, so force this
    e.playOnce = 0; // Not currently used by player

    var pet = playlistEntryTypes[e.type];
    var haveDuration = 0;

    var keys = Object.keys(pet.args);
    for (var i = 0; i < keys.length; i++) {
        var a = pet.args[keys[i]];

        if (a.type == 'int') {
            e[a.name] = parseInt($(row).find('.field_' + a.name).html());

            if (a.name == 'duration')
                haveDuration = 1;
        } else if (a.type == 'float') {
            e[a.name] = parseFloat($(row).find('.field_' + a.name).html());

            if (a.name == 'duration')
                haveDuration = 1;
        } else if (a.type == 'bool') {
            e[a.name] = ($(row).find('.field_' + a.name).html() == 'true') ? true : false;
        } else if (a.type == 'array') {
            var f = {};
            for (var x = 0; x < a.keys.length; x++) {
                f[a.keys[x]] = parseInt($(row).find('.field_' + a.name + '_' + a.keys[x]).html());
            }
            e[a.name] = f;
        } else if (a.type == 'args') {
            var arr = [];
            for (x = 1; x <= 20; x++) {
                if ($(row).find('.field_args_' + x).length) {
                    arr.push($(row).find('.field_args_' + x).html());
                }
            }
            e[a.name] = arr;
        } else if (a.type == 'string') {
            var v = $(row).find('.field_' + a.name).html();
            if (parseInt(v) == v) {
                e[a.name] = parseInt(v);
            } else {
                e[a.name] = v;
            }
        } else {
            e[a.name] = $(row).find('.field_' + a.name).html();
        }
    }

    if ((!haveDuration) && ($(row).find('.psiDurationSeconds').html() != "0"))
        e['duration'] = parseFloat($(row).find('.psiDurationSeconds').html());

    return e;
}

function SavePlaylist(filter, callback) {
    var name = $('#txtPlaylistName').val();
    if (name == "") {
        alert("Playlist name cannot be empty");
        return;
    }

    return SavePlaylistAs(name, filter, callback);
}

function SetPlaylistName(name) {
    $('#txtPlaylistName').val(name);
    $('#txtPlaylistName').prop('size', name.length);
}

function SavePlaylistAs(name, filter, callback) {
    if (!PlaylistNameOK(name))
        return 0;

    var itemCount = 0;
    var pl = {};
    pl.name = name;
    pl.version = 3;   // v1 == CSV, v2 == JSON, v3 == deprecated some things
    pl.repeat = 0;    // currently unused by player
    pl.loopCount = 0; // currently unused by player

    var leadIn = [];
    $('#tblPlaylistLeadIn > tr:not(.unselectable)').each(function() {
        leadIn.push(GetPlaylistEntry(this));
    });
    if (leadIn.length)
        pl.leadIn = leadIn;

    var mainPlaylist = [];
    $('#tblPlaylistMainPlaylist > tr:not(.unselectable)').each(function() {
        mainPlaylist.push(GetPlaylistEntry(this));
    });
    if (mainPlaylist.length)
        pl.mainPlaylist = mainPlaylist;

    var leadOut = [];
    $('#tblPlaylistLeadOut > tr:not(.unselectable)').each(function() {
        leadOut.push(GetPlaylistEntry(this));
    });
    if (leadOut.length)
        pl.leadOut = leadOut;

    var playlistInfo = {};
    playlistInfo.total_duration = parseFloat($('#playlistDuration').html());
    playlistInfo.total_items = mainPlaylist.length;
    pl.playlistInfo = playlistInfo;

    var str = JSON.stringify(pl, true);
    $.ajax({
        url: "/api/playlist/" + name,
        type: 'POST',
        contentType: 'application/json',
        data: str,
        async: false,
        dataType: 'json',
        success: function(data) {
            var rowSelected = $('#tblPlaylistDetails').find('.playlistSelectedEntry').length;

            PopulateLists();
            EnableButtonClass('playlistEditButton');

            if (rowSelected) {
                EnableButtonClass('playlistDetailsEditButton');
            } else {
                DisableButtonClass('playlistDetailsEditButton');
            }

            SetPlaylistName(name);
            $.jGrowl("Playlist Saved");
        },
        error: function() {
            DialogError('Unable to save playlist', "Error: Unable to save playlist.");
        }
    });

    return 1;
}

function RandomizePlaylistEntries() {
    $('#randomizeBuffer').html($('#tblPlaylistMainPlaylist').html());
    $('#tblPlaylistMainPlaylist').empty();

    var itemsLeft = $('#randomizeBuffer > tr').length;
    while (itemsLeft > 0) {
        var x = Math.floor(Math.random() * Math.floor(itemsLeft)) + 1;
        var item = $('#randomizeBuffer > tr:nth-child(' + x + ')').clone();
        $('#randomizeBuffer > tr:nth-child(' + x + ')').remove();

        $('#tblPlaylistMainPlaylist').append(item);

        itemsLeft = $('#randomizeBuffer > tr').length;
    }

    RenumberPlaylistEditorEntries();

//    $('.playlistEntriesBody').sortable('refresh').sortable('refreshPositions');
}

/**
 * Removes any of the following characters from the supplied name, can be used to cleanse playlist names, event names etc
 * Current needed for example it the case of the scheduler since it is still CSV and commas in a playlist name cause issues
 * Everything is currently replaced with a hyphen ( - )
 *
 * Currently unused in the front-end
 */
function RemoveIllegalChars(name) {

    // , (comma)
    // < (less than)
    // > (greater than)
    // : (colon)
    // " (double quote)
    // / (forward slash)
    // \ (backslash)
    // | (vertical bar or pipe)
    // ? (question mark)
    // * (asterisk)

    var illegalChars = [',', '<', '>', ':', '"', '/', '\\', '|', '?', '*'];

    for(ill_index = 0; ill_index < illegalChars.length; ++ill_index) {
        name = name.toString().replace(illegalChars[ill_index], " - ");
    }

    return name;
}

function CopyPlaylist()	{
    var name = $('#txtPlaylistName').val();

    $("#copyPlaylist_dialog").dialog({
        width: 400,
        buttons: {
            "Copy": function() {
                var new_playlist_name = $(this).find(".newPlaylistName").val();

                if (name == new_playlist_name) {
                    DialogError('Error, same name given', 'Identical name given.');
                    return;
                }

                if (!SavePlaylistAs(new_playlist_name, '', ''))
                    return;

                PopulateLists();
                SetPlaylistName(new_playlist_name);
                $(this).dialog("close");
            },
            Cancel: function() {
                $(this).dialog("close");
            }
        },
        open: function (event, ui) {
            //Generate a name for the new playlist
            $(this).find(".newPlaylistName").val(name + " - Copy");
        },
        close: function () {
            $(this).find(".newPlaylistName").val("New Playlist Name");
        }
    });
}

function RenamePlaylist()	{
    var name = $('#txtPlaylistName').val();

    $("#renamePlaylist_dialog").dialog({
        width: 400,
        buttons: {
            "Rename": function() {
                var new_playlist_name = $(this).find(".newPlaylistName").val();

                if (name == new_playlist_name) {
                    DialogError('Error, same name given', 'Identical name given.');
                    return;
                }

                if (!SavePlaylistAs(new_playlist_name, '', ''))
                    return;

                DeleteNamedPlaylist(name);
                PopulateLists();

                SetPlaylistName(new_playlist_name);
                $(this).dialog("close");
            },
            Cancel: function() {
                $(this).dialog("close");
            }
        },
        open: function (event, ui) {
            //Generate a name for the new playlist
            $(this).find(".newPlaylistName").val(name);
        },
        close: function () {
            $(this).find(".newPlaylistName").val("New Playlist Name");
        }
    });
}

function DeletePlaylist() {
    var name = $('#txtPlaylistName').val();

    DeleteNamedPlaylist(name);
}

function DeleteNamedPlaylist(name) {
    var postDataString = "";
    $.ajax({
        dataType: "json",
        url: "api/playlist/" + name,
        type: "DELETE",
        async: false,
        success: function(data) {
            PopulateLists();
            $.jGrowl("Playlist Deleted");
        },
        error: function() {
            DialogError('Error Deleting Playlist', "Error deleting '" + name + "' playlist");
        }
    });
}

function EditPlaylistEntry() {
    if (!$('#tblPlaylistDetails').find('.playlistSelectedEntry').length) {
        DialogError('No playlist item selected', "Error: No playlist item selected.");
        return;
    }

    $("#playlistDetails").get(0).scrollIntoView();

    var row = $('#tblPlaylistDetails').find('.playlistSelectedEntry');
    var type = $(row).find('.entryType').html();
    var pet = playlistEntryTypes[type];

    $('#pe_type').val(type);
    PlaylistTypeChanged();
    EnableButtonClass('playlistEditButton');

    var keys = Object.keys(pet.args);
    for (var i = 0; i < keys.length; i++) {
        var a = pet.args[keys[i]];

        if (a.type == 'bool') {
            if ($(row).find('.field_' + a.name).html() == 'true')
                $('.arg_' + a.name).prop('checked', true).trigger('change');
            else
                $('.arg_' + a.name).prop('checked', false).trigger('change');
        } else if (a.type == 'args') {
            var arr = [];
            for (x = 1; x <= 20; x++) {
                if ($(row).find('.field_args_' + x).length) {
                    $('#playlistEntryCommandOptions_arg_' + x).val($(row).find('.field_args_' + x).html());
                }
            }
        } else {
            $('.arg_' + a.name).val($(row).find('.field_' + a.name).html()).trigger('change');
        }
    }

    UpdateChildVisibility();
}

function RemovePlaylistEntry()	{
    if (!$('#tblPlaylistDetails').find('.playlistSelectedEntry').length) {
        DialogError('No playlist item selected', "Error: No playlist item selected.");
        return;
    }

    DisableButtonClass('playlistDetailsEditButton');
    $('#tblPlaylistDetails').find('.playlistSelectedEntry').remove();
    RenumberPlaylistEditorEntries();
    UpdatePlaylistDurations();
}

		function reloadPage()
		{
			location.reload(true);
		}

		function ManualGitUpdate()
		{
			SetButtonState("#ManualUpdate", "disable");
			document.body.style.cursor = "wait";

			$.get("fppxml.php?command=manualGitUpdate"
			).done(function() {
				document.body.style.cursor = "pointer";
				location.reload(true);
			}).fail(function() {
				SetButtonState("#ManualUpdate", "enable");
				document.body.style.cursor = "pointer";
				DialogError("Manual Git Update", "Update failed");
			});
		}

		function PingIP(ip, count)
		{
			if (ip == "")
				return;

				$('#helpText').html("Pinging " + ip + "<br><br>This will take a few seconds to load");
				$('#dialog-help').dialog({ height: 600, width: 800, position: { my: 'center', at: 'top', of: window}, title: "Ping " + ip });
//				$('#dialog-help').dialog( "moveToTop" );

				$.get("ping.php?ip=" + ip + "&count=" + count
				).done(function(data) {
						$('#helpText').html(data);
				}).fail(function() {
						$('#helpText').html("Error pinging " + ip);
				});
		}

		function PingE131IP(id)
		{
			var ip = $("[name='txtIP\[" + id + "\]']").val();

			PingIP(ip, 3);
		}

		function ViewReleaseNotes(version) {
				$('#helpText').html("Retrieving Release Notes");
				$('#dialog-help').dialog({ height: 800, width: 800, title: "Release Notes for FPP v" + version });
				$('#dialog-help').dialog( "moveToTop" );

				$.get("fppxml.php?command=viewReleaseNotes&version=" + version
				).done(function(data) {
						$('#helpText').html(
						"<center><input onClick='UpgradeFPPVersion(\"" + version + "\");' type='button' class='buttons' value='Upgrade'></center>" +
						"<pre style='white-space: pre-wrap; word-wrap: break-word;'>" + data + "</pre>"
						);
				}).fail(function() {
						$('#helpText').html("Error loading release notes.");
				});
		}

		function UpgradeFPPVersion(newVersion)
		{
			if (confirm('Do you wish to upgrade the Falcon Player?\n\nClick "OK" to continue.\n\nThe system will automatically reboot to complete the upgrade.\nThis can take a long time,  20-30 minutes on slower devices.'))
			{
                location.href="upgradefpp.php?version=v" + newVersion;
			}
		}

		function ChangeGitBranch(newBranch)
		{
            location.href="changebranch.php?branch=" + newBranch;
		}
	
		function SetUniverseCount(input)
		{
			var txtCount=document.getElementById("txtUniverseCount");
			var count = Number(txtCount.value);
			if(isNaN(count)) {
				count = 8;
			}
            
            if (count < UniverseCount) {
                while (count < UniverseCount) {
                    UniverseSelected = UniverseCount - 1;
                    DeleteUniverse(input);
                }
            } else {
                if (UniverseCount == 0) {
                    var data = {};
                    var channelData = {};
                    channelData.enabled = 0;
                    channelData.type = "universes";
                    channelData.universes = [];
                    var universe = {};
                    universe.active = 1;
                    universe.description = "";
                    universe.id = 1;
                    universe.startChannel = 1;
                    universe.channelCount = 512;
                    universe.type = 0;
                    universe.address = "";
                    universe.priority = 0;
                    universe.monitor = 1;
                    universe.deDuplicate = 0;
                    channelData.universes.push(universe);
                    if (input) {
                        data.channelInputs = [];
                        data.channelInputs.push(channelData);
                    } else {
                        data.channelOutputs = [];
                        data.channelOutputs.push(channelData);
                    }
                    populateUniverseData(data, false, input);
                }
                var selectIndex = UniverseCount - 1;
                var universe=Number(document.getElementById("txtUniverse[" + selectIndex + "]").value);
                var universeType=document.getElementById("universeType[" + selectIndex + "]").value;
                var unicastAddress=document.getElementById("txtIP[" + selectIndex + "]").value;
                var size=Number(document.getElementById("txtSize[" + selectIndex + "]").value);
                var ucount=Number(document.getElementById("numUniverseCount[" + selectIndex + "]").value);
                var startAddress=Number(document.getElementById("txtStartAddress[" + selectIndex + "]").value);
                var active=document.getElementById("chkActive[" + selectIndex + "]").value;
                var priority=Number(document.getElementById("txtPriority[" + selectIndex + "]").value);
                var monitor=document.getElementById("txtMonitor[" + selectIndex + "]").checked ? 1 : 0;
                var deDuplicate=document.getElementById("txtDeDuplicate[" + selectIndex + "]").checked ? 1 : 0;

                var tbody=document.getElementById("tblUniversesBody");  //get the table
                var origRow = tbody.rows[selectIndex];
                var origUniverseCount = UniverseCount;
                while (UniverseCount < count) {
                    var row = origRow.cloneNode(true);
                    tbody.appendChild(row);
                    UniverseCount++;
                }
                UniverseCount =  origUniverseCount;
                SetUniverseInputNames();
                while (UniverseCount < count) {
                    if (universe != 0) {
                        universe += ucount;
                        document.getElementById("txtUniverse[" + UniverseCount + "]").value = universe;
                    }
                    startAddress += size * ucount;
                    document.getElementById("txtStartAddress[" + UniverseCount + "]").value = startAddress;

                    if (!input) {
                        document.getElementById("tblUniversesBody").rows[UniverseCount].cells[13].innerHTML = "<input type='button' value='Ping' onClick='PingE131IP(" + UniverseCount + ");'/>";
                    }
                    updateUniverseEndChannel( document.getElementById("tblUniversesBody").rows[UniverseCount]);
                    UniverseCount++;
                }
                document.getElementById("txtUniverseCount").value = UniverseCount;
            }
		}

        function IPOutputTypeChanged(item, input) {
            var type = $(item).val();
            if (type == 4 || type == 5) { // DDP
                var univ = $(item).parent().parent().find("input.txtUniverse");
                univ.prop('disabled', true);
                var univc = $(item).parent().parent().find("input.numUniverseCount");
                univc.prop('disabled', true);
                var sz = $(item).parent().parent().find("input.txtSize");
                sz.prop('max', 512000);
                
                var monitor = $(item).parent().parent().find("input.txtMonitor");
                monitor.prop('disabled', false);

                var universe = $(item).parent().parent().find("input.txtUniverse");
                universe.prop('min', 1);
            } else { // 0,1 = E1.31, 2,3 = Artnet
                var univ = $(item).parent().parent().find("input.txtUniverse");
                univ.prop('disabled', false);
                if (parseInt(univ.val()) < 1) {
                    univ.val(1);
                }
                var univc = $(item).parent().parent().find("input.numUniverseCount");
                univc.prop('disabled', false);
                if (parseInt(univc.val()) < 1) {
                    univc.val(1);
                }
                var sz = $(item).parent().parent().find("input.txtSize");
                var val = parseInt(sz.val());
                if (val > 512) {
                    sz.val(512);
                }
                sz.prop('max', 512);
                
                if (!input) {
                    if ((type == 0) || (type == 2)) {
                        $(item).parent().parent().find("input.txtIP").val('');
                        $(item).parent().parent().find("input.txtIP").prop('disabled', true);
                    } else {
                        $(item).parent().parent().find("input.txtIP").prop('disabled', false);
                    }

                    var monitor = $(item).parent().parent().find("input.txtMonitor");
                    if (type == 0 || type == 2) {
                        monitor.prop('disabled', true);
                    } else {
                        monitor.prop('disabled', false);
                    }

                    var universe = $(item).parent().parent().find("input.txtUniverse");
                    if (type == 2 || type == 3) {
                        universe.prop('min', 0);
                    } else {
                        universe.prop('min', 1);
                    }
                }
            }
        }

function updateUniverseEndChannel(row) {
	var startChannel = parseInt($(row).find("input.txtStartAddress").val());
	var count = parseInt($(row).find("input.numUniverseCount").val());
	var size = parseInt($(row).find("input.txtSize").val());
	var end = startChannel + (count * size) - 1;

	$(row).find("span.numEndChannel").html(end);
}

        function populateUniverseData(data, reload, input) {
			var bodyHTML="";
			UniverseCount = 0;
            var inputStyle = "";
            var inputStr = 'Output';

            if (input) {
                inputStr = 'Input';
                inputStyle = "style='display: none;'";
            }

            var channelData = input ? data.channelInputs[0] : data.channelOutputs[0];
            
            UniverseCount = channelData.universes.length;
            for (var i = 0; i < channelData.universes.length; i++) {
                var universe = channelData.universes[i];
                var active = universe.active;
                var desc = universe.description;
                var uid = universe.id;
                var ucount = universe.universeCount;
                if (!ucount) {
                    ucount = 1;
                }
                var startAddress = universe.startChannel;
                var size = universe.channelCount;
                var type = universe.type;
                var unicastAddress =  universe.address;
                var priority =  universe.priority;
                unicastAddress = unicastAddress.trim();
                var endChannel = universe.startChannel + (ucount * size) - 1;

                var activeChecked = active == 1  ? "checked=\"checked\"" : "";
                var typeMulticastE131 = type == 0 ? "selected" : "";
                var typeUnicastE131 = type == 1 ? "selected": "";
                var typeBroadcastArtNet = type == 2 ? "selected" : "";
                var typeUnicastArtNet = type == 3 ? "selected": "";
                var typeDDPR = type == 4 ? "selected": "";
                var typeDDP1 = type == 5 ? "selected": "";
                var monitor = 1;
                if (universe.monitor != null) {
                    monitor = universe.monitor;
                }
                var deDuplicate = 0;
                if (universe.deDuplicate != null) {
                    deDuplicate = universe.deDuplicate;
                }

                var universeSize = 512;
                var universeCountDisable = "";
                var universeNumberDisable = "";
                var monitorDisabled = "";
                var ipDisabled = "";
                if (type == 4 || type == 5) {
                    universeSize = 512000;
                    universeCountDisable = " disabled";
                    universeNumberDisable = " disabled";
                }
                if (type == 0 || type == 2) {
                    monitorDisabled = " disabled";
                }
                var minNum = 1;
                if (type == 2 || type == 3) {
                    minNum = 0;
                }
                if (type == 0 || type == 2) {
                    ipDisabled = " disabled";
                    unicastAddress = "";
                }

                bodyHTML += "<tr>" +
                            "<td><span class='rowID' id='rowID'>" + (i+1).toString() + "</span></td>" +
                            "<td><input class='chkActive' type='checkbox' " + activeChecked +"/></td>" +
                            "<td><input class='txtDesc' type='text' size='24' maxlength='64' value='" + desc + "'/></td>";
                bodyHTML += "<td><select class='universeType' style='width:150px'";

                if (input) {
                    bodyHTML += ">" +
                                "<option value='0' " + typeMulticastE131 + ">E1.31 - Multicast</option>" +
                                "<option value='1' " + typeUnicastE131 + ">E1.31 - Unicast</option>" +
                                "<option value='2' " + typeBroadcastArtNet + ">ArtNet</option>";
                } else {
                    bodyHTML += " onChange='IPOutputTypeChanged(this, " + input + ");'>" +
                                "<option value='0' " + typeMulticastE131 + ">E1.31 - Multicast</option>" +
                                "<option value='1' " + typeUnicastE131 + ">E1.31 - Unicast</option>" +
                                "<option value='2' " + typeBroadcastArtNet + ">ArtNet - Broadcast</option>" +
                                "<option value='3' " + typeUnicastArtNet + ">ArtNet - Unicast</option>" +
                                "<option value='4' " + typeDDPR + ">DDP - Raw Channel Numbers</option>" +
                                "<option value='5' " + typeDDP1 + ">DDP - One Based</option>";
                }

                bodyHTML += "</select></td>";
                bodyHTML += "<td " + inputStyle + "><input class='txtIP' type='text' value='" + unicastAddress + "' size='16' maxlength='32' " + ipDisabled + "></td>";
                bodyHTML += "<td><input class='txtStartAddress' type='number' min='1' max='1048576' value='" + startAddress.toString() + "' onChange='updateUniverseEndChannel($(this).parent().parent());' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'/></td><td><span class='numEndChannel'>" + endChannel.toString() + "</span></td>";

                bodyHTML += "<td><input class='txtUniverse' type='number' min='" + minNum + "' max='63999' value='" + uid.toString() + "'" + universeNumberDisable + "/></td>";

                bodyHTML += "<td><input class='numUniverseCount' type='number' min='1' max='250' value='" + ucount.toString() + "'" + universeCountDisable + " onChange='updateUniverseEndChannel($(this).parent().parent());' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'/></td>";

                bodyHTML += "<td><input class='txtSize' type='number'  min='1'  max='" + universeSize + "' value='" + size.toString() + "' onChange='updateUniverseEndChannel($(this).parent().parent());' onkeypress='this.onchange();' onpaste='this.onchange();' oninput='this.onchange();'></td>";
                bodyHTML += "<td " + inputStyle + "><input class='txtPriority' type='number' min='0' max='9999' value='" + priority.toString() + "'/></td>";
                bodyHTML += "<td " + inputStyle + "><input class='txtMonitor' id='txtMonitor' type='checkbox' size='4' maxlength='4' " + (monitor == 1 ? "checked" : "" ) + monitorDisabled + "/></td>" +
                            "<td " + inputStyle + "><input class='txtDeDuplicate' id='txtDeDuplicate' type='checkbox' size='4' maxlength='4' " + (deDuplicate == 1 ? "checked" : "" ) + "/></td>" +
                            "<td " + inputStyle + "><input type=button onClick='PingE131IP(" + i.toString() + ");' value='Ping'></td>" +
                            "</tr>";
            }

            if (!input) {
                var ecb = $('#E131Enabled');
                if ( channelData.enabled == 1) {
                    ecb.prop('checked', true);
                } else {
                    ecb.prop('checked', false)
                }
            }
            $('#tblUniversesBody').html(bodyHTML);

            $('#txtUniverseCount').val(UniverseCount);

            SetUniverseInputNames(); // in co-universes.php
		}
        function getUniverses(reload, input)
        {
            var url = "fppjson.php?command=getChannelOutputs&file=universeOutputs";
            if (input) {
                url = "fppjson.php?command=getChannelOutputs&file=universeInputs";
            }
            $.getJSON(url, function(data) {
                        populateUniverseData(data, reload, input)
                      }).fail(function() {
                              UniverseCount = 0;
                              $('#txtUniverseCount').val(UniverseCount);
                      })
        }

		function getPixelnetDMXoutputs(reload)
		{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=getPixelnetDMXoutputs&reload=" + reload;
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
 			var innerHTML="";
			xmlhttp.onreadystatechange = function () {
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
					var entries = xmlDoc.getElementsByTagName('PixelnetDMXentries')[0];
					if(entries.childNodes.length> 0)
					{
						innerHTML = "<tr>" +
                            "<th>#</th>" +
                            "<th>Active</th>" +
                            "<th>Type</th>" +
                            "<th>Start</th>" +
                            "</tr>";

							PixelnetDMXcount = entries.childNodes.length;
							for(i=0;i<PixelnetDMXcount;i++)
							{
								var active = entries.childNodes[i].childNodes[0].textContent;
								var type = entries.childNodes[i].childNodes[1].textContent;
								var startAddress = entries.childNodes[i].childNodes[2].textContent;

								var activeChecked = active == 1  ? "checked=\"checked\"" : "";
								var pixelnetChecked = type == 0 ? "selected" : "";
								var dmxChecked = type == 1 ? "selected" : "";
								
								innerHTML += 	"<tr class=\"rowUniverseDetails\">" +
								              "<td>" + (i+1).toString() + "</td>" +
															"<td><input name=\"FPDchkActive[" + i.toString() + "]\" id=\"FPDchkActive[" + i.toString() + "]\" type=\"checkbox\" " + activeChecked +"/></td>" +
															"<td><select id=\"pixelnetDMXtype[" + i.toString() + "]\" name=\"pixelnetDMXtype[" + i.toString() + "]\" style=\"width:150px\">" +
															      "<option value=\"0\" " + pixelnetChecked + ">Pixelnet</option>" +
															      "<option value=\"1\" " + dmxChecked + ">DMX</option></select></td>" + 
															"<td><input name=\"FPDtxtStartAddress[" + i.toString() + "]\" id=\"FPDtxtStartAddress[" + i.toString() + "]\" type=\"text\" size=\"8\" value=\"" + startAddress.toString() + "\"/></td>" +
															"</tr>";

							}
					}
					else
					{
						innerHTML = "No Results Found";	
					}
					var results = document.getElementById("tblOutputs");
					results.innerHTML = innerHTML;	
				}
			};
			
			xmlhttp.send();			
		}

        function SetUniverseRowInputNames(row, id) {
            var fields = Array('chkActive', 'txtDesc', 'txtStartAddress',
                               'txtUniverse', 'numUniverseCount', 'txtSize', 'universeType', 'txtIP',
                               'txtPriority', 'txtMonitor', 'txtDeDuplicate');
            row.find('span.rowID').html((id + 1).toString());
            
            for (var i = 0; i < fields.length; i++)
            {
                row.find('input.' + fields[i]).attr('name', fields[i] + '[' + id + ']');
                row.find('input.' + fields[i]).attr('id', fields[i] + '[' + id + ']');
                row.find('select.' + fields[i]).attr('name', fields[i] + '[' + id + ']');
                row.find('select.' + fields[i]).attr('id', fields[i] + '[' + id + ']');
            }
        }
		function SetUniverseInputNames()
		{
            var id = 0;
			$('#tblUniversesBody tr').each(function() {
                SetUniverseRowInputNames($(this), id);
				id += 1;
			});
		}

		function InitializeUniverses()
		{
			UniverseSelected = -1;
			UniverseCount = 0;
		}
		
		function DeleteUniverse(input)
		{
            if (UniverseSelected >= 0) {
                var selectedIndex = UniverseSelected;
                for(i = UniverseSelected + 1; i < UniverseCount; i++, selectedIndex++) {
                    
                    document.getElementById("txtUniverse[" + selectedIndex + "]").value = document.getElementById("txtUniverse[" + i + "]").value
                    document.getElementById("txtDesc[" + selectedIndex + "]").value = document.getElementById("txtDesc[" + i + "]").value
                    document.getElementById("universeType[" + selectedIndex + "]").value = document.getElementById("universeType[" + i + "]").value;
                    var universeType = document.getElementById("universeType[" + selectedIndex + "]").value;
                    document.getElementById("txtStartAddress[" + selectedIndex + "]").value  = document.getElementById("txtStartAddress[" + i + "]").value;
                    document.getElementById("numUniverseCount[" + selectedIndex + "]").value  = document.getElementById("numUniverseCount[" + i + "]").value;

                    var checkb = document.getElementById("chkActive[" + i + "]");
                    document.getElementById("chkActive[" + selectedIndex + "]").checked = checkb.checked;
                    document.getElementById("txtSize[" + selectedIndex + "]").value = document.getElementById("txtSize[" + i + "]").value;
                    document.getElementById("txtIP[" + selectedIndex + "]").value = document.getElementById("txtIP[" + i + "]").value;
                    document.getElementById("txtPriority[" + selectedIndex + "]").value = document.getElementById("txtPriority[" + i + "]").value;
                    document.getElementById("txtMonitor[" + selectedIndex + "]").checked = document.getElementById("txtMonitor[" + i + "]").checked;
                    document.getElementById("txtDeDuplicate[" + selectedIndex + "]").checked = document.getElementById("txtDeDuplicate[" + i + "]").checked;
                    if ((universeType == '1') || (universeType == '3')) {
                        document.getElementById("txtIP[" + selectedIndex + "]").disabled = false;
                    } else {
                        document.getElementById("txtIP[" + selectedIndex + "]").disabled = true;
                    }
                    updateUniverseEndChannel(document.getElementById("tblUniversesBody").rows[selectedIndex]);
                }
                document.getElementById("tblUniversesBody").deleteRow(UniverseCount-1);
                UniverseCount--;
                document.getElementById("txtUniverseCount").value = UniverseCount;
                UniverseSelected = -1;
            }
        
        }
		
		function CloneUniverses(cloneNumber)
		{
			var selectIndex = (UniverseSelected).toString();
			if(!isNaN(cloneNumber))
			{
				if((UniverseSelected + cloneNumber -1) < UniverseCount)
				{
                    var universeDesc=document.getElementById("txtDesc[" + selectIndex + "]").value;
					var universeType=document.getElementById("universeType[" + selectIndex + "]").value;
					var unicastAddress=document.getElementById("txtIP[" + selectIndex + "]").value;
					var size=Number(document.getElementById("txtSize[" + selectIndex + "]").value);
                    var uCount=Number(document.getElementById("numUniverseCount[" + selectIndex + "]").value);
                    var universe=Number(document.getElementById("txtUniverse[" + selectIndex + "]").value)+uCount;
					var startAddress=Number(document.getElementById("txtStartAddress[" + selectIndex + "]").value)+ size * uCount;
					var active=document.getElementById("chkActive[" + selectIndex + "]").value;
					var priority=Number(document.getElementById("txtPriority[" + selectIndex + "]").value);
                    var monitor=document.getElementById("txtMonitor[" + selectIndex + "]").checked ? 1 : 0;
                    var deDuplicate=document.getElementById("txtDeDuplicate[" + selectIndex + "]").checked ? 1 : 0;

					for(z=0;z<cloneNumber;z++,universe+=uCount)
					{
                        var i = z+UniverseSelected+1;
                        i = i.toString();
                        document.getElementById("txtDesc[" + i + "]").value = universeDesc;
						document.getElementById("txtUniverse[" + i + "]").value	= universe.toString();
						document.getElementById("universeType[" + i + "]").value = universeType;
						document.getElementById("txtStartAddress[" + i + "]").value	= startAddress.toString();
                        document.getElementById("numUniverseCount[" + i + "]").value = uCount.toString();
						document.getElementById("chkActive[" + i + "]").value = active;
						document.getElementById("txtSize[" + i + "]").value = size.toString();
						document.getElementById("txtIP[" + i + "]").value = unicastAddress;
						document.getElementById("txtPriority[" + i + "]").value = priority;
                        document.getElementById("txtMonitor[" + i + "]").checked = (monitor == 1);
                        document.getElementById("txtDeDuplicate[" + i + "]").checked = (deDuplicate == 1);
						if((universeType == '1') || (universeType == '3'))
						{
							document.getElementById("txtIP[" + i + "]").disabled = false;
						}
						else
						{
							document.getElementById("txtIP[" + i + "]").disabled = true;
						}
						updateUniverseEndChannel(document.getElementById("tblUniversesBody").rows[i]);
						startAddress+=size*uCount;
					}
				}
			} else {
				DialogError("Clone Universe", "Error, invalid number");
			}
		}
        function CloneUniverse()
        {
            var answer = prompt ("How many universes to clone from selected universe?","1");
            var cloneNumber = Number(answer);
            CloneUniverses(cloneNumber);
        }

		function ClonePixelnetDMXoutput()
		{
			var answer = prompt ("How many outputs to clone from selected output?","1");
			var cloneNumber = Number(answer);
			var selectIndex = (PixelnetDMXoutputSelected-1).toString();
			if(!isNaN(cloneNumber))
			{
				if((PixelnetDMXoutputSelected + cloneNumber -1) < 12)
				{
					var active=document.getElementById("FPDchkActive[" + selectIndex + "]").value;
					var pixelnetDMXtype=document.getElementById("pixelnetDMXtype[" + selectIndex + "]").value;
					var size = pixelnetDMXtype == "0" ? 4096:512;
					var startAddress=Number(document.getElementById("FPDtxtStartAddress[" + selectIndex + "]").value)+ size;
					for(i=PixelnetDMXoutputSelected;i<PixelnetDMXoutputSelected+cloneNumber;i++)
					{
						document.getElementById("pixelnetDMXtype[" + i + "]").value	 = pixelnetDMXtype;
						document.getElementById("FPDtxtStartAddress[" + i + "]").value	 = startAddress.toString();
						document.getElementById("FPDchkActive[" + i + "]").value = active;
						startAddress+=size;
					}
					$.jGrowl("" + cloneNumber + " Outputs Cloned");
				}
			} else {
				DialogError("Clone Output", "Error, invalid number");
			}
		}

        function postUniverseJSON(input) {
            var postData = {};
            
            var output = {};
            output.type = "universes";
            if (!input) {
                output.enabled = document.getElementById("E131Enabled").checked ? 1 : 0;
            } else {
                output.enabled = 1;
            }
            output.startChannel = 1;
            output.channelCount = -1;
            output.universes = [];
            
            var i;
            for(i = 0; i < UniverseCount; i++) {
                var universe = {};
                universe.active = document.getElementById("chkActive[" + i + "]").checked ? 1 : 0;
                universe.description = document.getElementById("txtDesc[" + i + "]").value;;
                universe.id = parseInt(document.getElementById("txtUniverse[" + i + "]").value);
                universe.startChannel = parseInt(document.getElementById("txtStartAddress[" + i + "]").value);
                universe.universeCount = parseInt(document.getElementById("numUniverseCount[" + i + "]").value);

                universe.channelCount = parseInt(document.getElementById("txtSize[" + i + "]").value);
                universe.type = parseInt(document.getElementById("universeType[" + i + "]").value);
                universe.address = document.getElementById("txtIP[" + i + "]").value;
                universe.priority = parseInt(document.getElementById("txtPriority[" + i + "]").value);
                if (!input) {
                    universe.monitor = document.getElementById("txtMonitor[" + i + "]").checked ? 1 : 0;
                    universe.deDuplicate = document.getElementById("txtDeDuplicate[" + i + "]").checked ? 1 : 0;
                }
                output.universes.push(universe);
            }
            if (input) {
                postData.channelInputs = [];
                postData.channelInputs.push(output);
            } else {
                postData.channelOutputs = [];
                postData.channelOutputs.push(output);
            }
            var fileName = input ? 'universeInputs' : 'universeOutputs';
            var postDataString = 'command=setChannelOutputs&file='+ fileName +'&data={' + encodeURIComponent(JSON.stringify(postData)) + '}';
            
            $.post("fppjson.php", postDataString).done(function(data) {
                                                       $.jGrowl("E1.31 Universes Saved");
                                                       SetRestartFlag(2);
                                                       CheckRestartRebootFlags();
                                                 }).fail(function() {
                                                        DialogError('Save Universes', "Error: Unable to save E1.31 Universes.");
                                                  });
            
        }

		function validateUniverseData()
		{
			var i;
			var txtUniverse;
			var txtStartAddress;
			var txtSize;
			var universeType;
			var txtPriority;
			var result;
			var returnValue=true;
			for(i=0;i<UniverseCount;i++)
			{

                // unicast address
                universeType=document.getElementById("universeType[" + i + "]").value;
                if(universeType == 1 || universeType == 3 || universeType == 4 || universeType == 5)
                {
                    if(!validateIPaddress("txtIP[" + i + "]"))
                    {
                        returnValue = false;
                    }
                }
				// universe
                if (universeType >= 0 && universeType <= 3) {
                    txtUniverse=document.getElementById("txtUniverse[" + i + "]");
                    var minNum = 1;
                    if (universeType >= 2 && universeType <= 3)
                        minNum = 0;

                    if(!validateNumber(txtUniverse,minNum,63999))
                    {
                        returnValue = false;
                    }
                }
				// start address
				txtStartAddress=document.getElementById("txtStartAddress[" + i + "]");				
				if(!validateNumber(txtStartAddress,1,1048576))
				{
					returnValue = false;
				}
                // size
                txtSize=document.getElementById("txtSize[" + i + "]");
                var max = 512;
                if (universeType == 4 || universeType == 5) {
                    max = 512000;
                }
                if(!validateNumber(txtSize,1,max))
                {
                    returnValue = false;
                }

				// priority
				txtPriority=document.getElementById("txtPriority[" + i + "]");
				if(!validateNumber(txtPriority,0,9999))
				{
					returnValue = false;
				}
			}
			return returnValue;
		}
		
		function validateIPaddress(id)
		{
			var ipb = document.getElementById(id);
			var ip = ipb.value;
			var isHostnameRegex = /[a-z]/i;
			var isHostname = ip.match(isHostnameRegex);
			if ((ip == "") || ((isHostname == null) || (isHostname.length > 0)) || (/^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.test(ip)))
			{
				ipb.style.border = "#000 0px none";
				return true;
			}

			ipb.style.border = "#F00 2px solid";
			return false;
		}  

		function validateNumber(textbox,minimum,maximum)   
		{  
			result = true;
			value = Number(textbox.value);
			if(isNaN(value))
			{
				textbox.style.border="red solid 1px";
				textbox.value = ""; 
				result = false;
			}
			if(value >= minimum && value <= maximum)
			{
				return true;	
			}
			else
			{
				textbox.style.border="red solid 1px";
				textbox.value = ""; 
				result = false;
			alert(textbox.value + ' is not between ' + minimum + ' and ' + maximum);
			}
		}
		
		function ReloadPixelnetDMX()
		{
			getPixelnetDMXoutputs("TRUE");
		} 

		function ExtendSchedule(minutes)
		{
			var seconds = minutes * 60;
			var url = "api/command/Extend Schedule/" + seconds;
			$.get(url).done(function(data) {
					$.jGrowl(data);
					if (statusTimeout != null)
						clearTimeout(statusTimeout);
					GetFPPStatus();
				}).fail(function() {
					$.jGrowl("Failed to extend schedule.");
				});
		}

		function ExtendSchedulePopup()
		{
			var minutes = prompt ("Extend running scheduled playlist by how many minutes?","1");
			if (minutes === null) {
				$.jGrowl("Extend cancelled");
				return;
			}

			minutes = Number(minutes);

			var minimum = -3 * 60;
			var maximum = 12 * 60;

			if ((minutes > maximum) ||
				(minutes < minimum)) {
				DialogError("Extend Schedule", "Error: Minutes is not between the minimum " + minimum + " and maximum " + maximum);
			} else {
				ExtendSchedule(minutes);
			}
		}


var playListArray = [];
function GetPlaylistArray()
{
    $.ajax({
        dataType: "json",
        url: "api/playlists",
        async: false,
        success: function(data) {
            playListArray = data;
        },
        error: function() {
            DialogError('Load Playlists', 'Error loading list of playlists');
        }
    });
}

var sequenceArray = [];
function GetSequenceArray()
{
    $.ajax({
        dataType: "json",
        url: "api/sequence",
        async: false,
        success: function(data) {
            sequenceArray = data;
        },
        error: function() {
            DialogError('Load Sequences', 'Error loading list of sequences');
        }
    });
}

  function GetFiles(dir)
  {
    var xmlhttp=new XMLHttpRequest();
    var url = "fppxml.php?command=getFiles&dir=" + dir;
    $('#tbl' + dir).empty();
    xmlhttp.open("GET",url,false);
    xmlhttp.setRequestHeader('Content-Type', 'text/xml');
    xmlhttp.send();

    var xmlDoc=xmlhttp.responseXML; 
    var files = xmlDoc.getElementsByTagName('Files')[0];
    if(files.childNodes.length> 0)
    {
      var innerhtml = '';
      for(i=0; i<files.childNodes.length; i++)
      {
        // Thanks: http://stackoverflow.com/questions/5396560/how-do-i-convert-special-utf-8-chars-to-their-iso-8859-1-equivalent-using-javasc
        var encodedstring = decodeURIComponent(escape(files.childNodes[i].childNodes[0].textContent));
        var name = "";
        try{
            // If the string is UTF-8, this will work and not throw an error.
            name=decodeURIComponent(escape(encodedstring));
        }catch(e){
            // If it isn't, an error will be thrown, and we can asume that we have an ISO string.
            name=encodedstring;
        }

        var time = files.childNodes[i].childNodes[1].textContent.replace(/ /g, '&nbsp;');
        var fileInfo = files.childNodes[i].childNodes[2].textContent;

          var tableRow = "<tr class='fileDetails' id='fileDetail_" + i + "'><td class ='fileName'>" + name + "</td><td class='fileExtraInfo'>" + fileInfo + "</td><td class ='fileTime'>" + time + "</td></tr>";
        $('#tbl' + dir).append(tableRow);
      }
    }
  }

	function moveFile(file)
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=moveFile&file=" + encodeURIComponent(file);
			xmlhttp.open("GET",url,false);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
	}

	function updateFPPStatus()
	{
		var status = GetFPPStatus();
	}
	
	function IsFPPDrunning()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=isFPPDrunning";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.onreadystatechange = function () 
			{
				if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
				{
					var xmlDoc=xmlhttp.responseXML; 
					var status = xmlDoc.getElementsByTagName('Status')[0];
					var retValue='false';
					if(status.childNodes.length> 0)
					{
						ret = status.childNodes[0].textContent;
						if(ret == 'true')
						{
							SetButtonState('#btnDaemonControl','enable');
							$("#btnDaemonControl").attr('value', 'Stop FPPD');
							$('#daemonStatus').html("FPPD is running.");
						}
						else if (ret == 'updating')
						{
							SetButtonState('#btnDaemonControl','disable');
							$("#btnDaemonControl").attr('value', 'Start FPPD');
							$('#daemonStatus').html("FPP is currently updating.");
						} 
						else
						{
							SetButtonState('#btnDaemonControl','enable');
							$("#btnDaemonControl").attr('value', 'Start FPPD');
							$('#daemonStatus').html("FPPD is stopped.");
							$('.schedulerStartTime').hide();
							$('.schedulerEndTime').hide();
						} 
					}
				}
			};
			xmlhttp.send();
	}

	function SetupUIForMode(fppMode)
	{
		if (fppMode == 1) // Bridge Mode
		{
			$("#playerModeInfo").hide();
			$("#remoteModeInfo").hide();
			$("#bridgeModeInfo").show();
		}
		else if (fppMode == 8) // Remote Mode
		{
			$("#playerModeInfo").hide();
			$("#remoteModeInfo").show();
			$("#bridgeModeInfo").hide();
		}
		else // Player or Master Modes
		{
			$("#playerModeInfo").show();
			$("#remoteModeInfo").hide();
			$("#bridgeModeInfo").hide();
		}
	}

    var temperatureUnit = false;
    function changeTemperatureUnit() {
        if (temperatureUnit) {
            temperatureUnit = false;
        } else {
            temperatureUnit = true;
        }
    }
	function GetFPPStatus()	{
		$.ajax({
			url: 'fppjson.php?command=getFPPstatus',
			dataType: 'json',
			success: function(response, reqStatus, xhr) {	
				
				if(response && typeof response === 'object') {

					if(response.status_name == 'stopped') {
						
						$('#fppTime').html('');
						SetButtonState('#btnDaemonControl','enable');
						$("#btnDaemonControl").attr('value', 'Start FPPD');
						$('#daemonStatus').html("FPPD is stopped.");
						$('#txtPlayerStatus').html(status);
						$('#playerTime').hide();
						$('#txtSeqFilename').html("");
						$('#txtMediaFilename').html("");
						$('#schedulerStatus').html("");
						$('.schedulerStartTime').hide();
						$('.schedulerEndTime').hide();
					
					} else if(response.status_name == 'updating') {

						$('#fppTime').html('');
						SetButtonState('#btnDaemonControl','disable');
						$("#btnDaemonControl").attr('value', 'Start FPPD');
						$('#daemonStatus').html("FPP is currently updating.");
						$('#txtPlayerStatus').html(status);
						$('#playerTime').hide();
						$('#txtSeqFilename').html("");
						$('#txtMediaFilename').html("");
						$('#schedulerStatus').html("");
						$('.schedulerStartTime').hide();
						$('.schedulerEndTime').hide();

					} else {

							SetButtonState('#btnDaemonControl','enable');
							$("#btnDaemonControl").attr('value', 'Stop FPPD');
							$('#daemonStatus').html("FPPD is running.");

						parseStatus(response);

					}

					lastStatus = response.status;
				}

			},
			complete: function() {
				clearTimeout(statusTimeout);
				statusTimeout = setTimeout(GetFPPStatus, 1000);
			}
		})
	}

	var firstStatusLoad = 1;
	function parseStatus(jsonStatus) {
		var fppStatus = jsonStatus.status;
		var fppMode = jsonStatus.mode;
        var status = "Idle";
        if (jsonStatus.status_name == "testing") {
            status = "Testing";
        }
		if (fppStatus == STATUS_IDLE ||
			fppStatus == STATUS_PLAYING ||
			fppStatus == STATUS_STOPPING_GRACEFULLY ||
			fppStatus == STATUS_STOPPING_GRACEFULLY_AFTER_LOOP ) {
		
			$("#btnDaemonControl").attr('value', 'Stop FPPD');
			$('#daemonStatus').html("FPPD is running.");
		}

		var Volume = parseInt(jsonStatus.volume);
		$('#volume').html(Volume);
		$('#slider').slider('value', Volume);
		SetSpeakerIndicator(Volume);

        AdjustFPPDModeFromStatus(fppMode);
        
        if (jsonStatus.hasOwnProperty('warnings')) {
            var txt = "<hr><center><b>Abnormal Conditions - May Cause Poor Performance</b></center>";
            for (var i = 0; i < jsonStatus.warnings.length; i++) {
                txt += "<font color='red'><center>" + jsonStatus.warnings[i] + "</center></font>";
            }
            document.getElementById('warningsDiv').innerHTML = txt;
            $('#warningsRow').show();
        } else {
            $('#warningsRow').hide();
        }

		if (fppMode == 1) {
			// Bridge Mode
			$('#fppTime').html(jsonStatus.time);

			if (firstStatusLoad || $('#e131statsLiveUpdate').is(':checked'))
				GetUniverseBytesReceived();
		} else if (fppMode == 8) {
			// Remote Mode
			$('#fppTime').html(jsonStatus.time);

			if(jsonStatus.time_elapsed) {
				status = "Syncing to Master: Elapsed: " + jsonStatus.time_elapsed + "&nbsp;&nbsp;&nbsp;&nbsp;Remaining: " + jsonStatus.time_remaining;
			}

			$('#txtRemoteStatus').html(status);
			$('#txtRemoteSeqFilename').html(jsonStatus.sequence_filename);
			$('#txtRemoteMediaFilename').html(jsonStatus.media_filename);
		} else {
			// Player/Master Mode
			var nextPlaylist = jsonStatus.next_playlist;
			var nextPlaylistStartTime = jsonStatus.next_playlist_start_time;
			var currentPlaylist = jsonStatus.current_playlist;

			if (fppStatus == STATUS_IDLE) {
				// Not Playing Anything
				gblCurrentPlaylistIndex =0;
				gblCurrentPlaylistEntryType = '';
				gblCurrentPlaylistEntrySeq = '';
				gblCurrentPlaylistEntrySong = '';
				$('#txtPlayerStatus').html(status);
				$('#playerTime').hide();
				$('#txtSeqFilename').html("");
				$('#txtMediaFilename').html("");
				$('#schedulerStatus').html("Idle");
				$('.schedulerStartTime').hide();
				$('.schedulerEndTime').hide();
								
				// Enable Play
				SetButtonState('#btnPlay','enable');
                SetButtonState('#btnStopNow','disable');
                SetButtonState('#btnPrev','disable');
				SetButtonState('#btnNext','disable');
				SetButtonState('#btnStopGracefully','disable');
				SetButtonState('#btnStopGracefullyAfterLoop','disable');
				$('#playlistSelect').removeAttr("disabled");
				UpdateCurrentEntryPlaying(0);
			} else if (currentPlaylist.playlist != "") {
				// Playing a playlist
				var playerStatusText = "Playing ";
                if (jsonStatus.current_song != "") {
                    playerStatusText += " - <strong>'" + jsonStatus.current_song + "'</strong>";
                    if (jsonStatus.current_sequence != "") {
                        playerStatusText += "/";
                    }
                }
                if (jsonStatus.current_sequence != "") {
                    if (jsonStatus.current_song == "") {
                        playerStatusText += " - ";
                    }
                    playerStatusText += "<strong>'" + jsonStatus.current_sequence + "'</strong>";
                }
                var repeatMode = jsonStatus.repeat_mode;
				if ((gblCurrentLoadedPlaylist != currentPlaylist.playlist) ||
					(gblCurrentPlaylistIndex != currentPlaylist.index) ||
					(gblCurrentPlaylistEntryType != currentPlaylist.type) ||
					(gblCurrentPlaylistEntrySeq != jsonStatus.current_sequence) ||
					(gblCurrentPlaylistEntrySong != jsonStatus.current_song)) {
					$('#playlistSelect').val(currentPlaylist.playlist);
					PopulatePlaylistDetailsEntries(false,currentPlaylist.playlist);

					gblCurrentPlaylistEntryType = currentPlaylist.type;
					gblCurrentPlaylistEntrySeq = jsonStatus.current_sequence;
					gblCurrentPlaylistEntrySong = jsonStatus.current_song;
				}

				SetButtonState('#btnPlay','enable');
				SetButtonState('#btnStopNow','enable');
                SetButtonState('#btnPrev','enable');
                SetButtonState('#btnNext','enable');
				SetButtonState('#btnStopGracefully','enable');
				SetButtonState('#btnStopGracefullyAfterLoop','enable');
				$('#playlistSelect').attr("disabled");

				if(fppStatus == STATUS_STOPPING_GRACEFULLY) {
					playerStatusText += " - Stopping Gracefully";
				} else if(fppStatus == STATUS_STOPPING_GRACEFULLY_AFTER_LOOP) {
					playerStatusText += " - Stopping Gracefully After Loop";
				}

				$('#txtPlayerStatus').html(playerStatusText);
				$('#playerTime').show();
				$('#txtTimePlayed').html(jsonStatus.time_elapsed );
				$('#txtTimeRemaining').html(jsonStatus.time_remaining );
				$('#txtSeqFilename').html(jsonStatus.current_sequence);
				$('#txtMediaFilename').html(jsonStatus.current_song);

//				if(currentPlaylist.index != gblCurrentPlaylistIndex && 
//					currentPlaylist.index <= gblCurrentLoadedPlaylistCount) {
// FIXME, somehow this doesn't refresh on the first page load, so refresh
// every time for now
if (1) {
							
							UpdateCurrentEntryPlaying(currentPlaylist.index);
							gblCurrentPlaylistIndex = currentPlaylist.index;
					}

				if (repeatMode) {
					$("#chkRepeat").prop( "checked", true );
				} else{
					$("#chkRepeat").prop( "checked", false );
				}

				if (jsonStatus.scheduler != "") {
					if (jsonStatus.scheduler.status == "playing") {
						var pl = jsonStatus.scheduler.currentPlaylist;
						$('#schedulerStatus').html("Playing <b>'" + pl.playlistName + "'</b>");

						$('.schedulerStartTime').show();
						$('#schedulerStartTime').html(pl.actualStartTimeStr);
						$('.schedulerEndTime').show();
						$('#schedulerEndTime').html(pl.actualEndTimeStr);
						$('#schedulerStopType').html(pl.stopTypeStr);

						if ((fppStatus == STATUS_STOPPING_GRACEFULLY) ||
							(fppStatus == STATUS_STOPPING_GRACEFULLY_AFTER_LOOP)) {
							$('.schedulerExtend').hide();
						} else {
							$('.schedulerExtend').show();
						}

					} else if (jsonStatus.scheduler.status == "manual") {
						var pl = jsonStatus.scheduler.currentPlaylist;
						$('#schedulerStatus').html("Playing <b>'" + pl.playlistName + "'</b> (manually started)");
						$('.schedulerStartTime').hide();
						$('.schedulerEndTime').hide();
					} else {
						$('#schedulerStatus').html("Idle");
						$('.schedulerStartTime').hide();
						$('.schedulerEndTime').hide();
					}
				} else {
					$('#schedulerStatus').html("Idle");
					$('#schedulerStartTime').html("N/A");
					$('#schedulerEndTime').html("N/A");
				}
            } else if (jsonStatus.current_sequence != "") {
                //  Playing a sequence via test mode
                var playerStatusText = "Playing <strong>'" + jsonStatus.current_sequence + "'</strong>";
                SetButtonState('#btnPlay','disable');
                SetButtonState('#btnPrev','enable');
                SetButtonState('#btnNext','enable');
                SetButtonState('#btnStopNow','enable');
                SetButtonState('#btnStopGracefully','enable');
                SetButtonState('#btnStopGracefullyAfterLoop','enable');
                
                $('#txtPlayerStatus').html(playerStatusText);
				$('#playerTime').show();
                $('#txtTimePlayed').html(jsonStatus.time_elapsed );
                $('#txtTimeRemaining').html(jsonStatus.time_remaining );
				$('#txtSeqFilename').html(jsonStatus.current_sequence);
				$('#txtMediaFilename').html(jsonStatus.current_song);

			}

			$('#fppTime').html(jsonStatus.time);

			var npl = jsonStatus.scheduler.nextPlaylist;
			if (npl.scheduledStartTimeStr != "")
				$('#nextPlaylist').html("'" + npl.playlistName + "' on " + npl.scheduledStartTimeStr);
			else
				$('#nextPlaylist').html("No playlist scheduled.");
		}

        if (jsonStatus.hasOwnProperty('sensors')) {
            var sensorText = "<table id='sensorTable'>";
            for (var i = 0; i < jsonStatus.sensors.length; i++) {
                if ((jsonStatus.sensors.length < 4) || ((i % 2) == 0)) {
                    sensorText += "<tr>";
                }
                sensorText += "<th>";
                sensorText += jsonStatus.sensors[i].label;
                sensorText += "</th><td";
                if (jsonStatus.sensors[i].valueType == "Temperature") {
                    sensorText += " onclick='changeTemperatureUnit()'>";
                    var val = jsonStatus.sensors[i].value;
                    if (temperatureUnit) {
                        val *= 1.8;
                        val += 32;
                        sensorText += val.toFixed(1);
                        sensorText += "F";
                    } else {
                        sensorText += val.toFixed(1);
                        sensorText += "C";
                    }
                } else {
                    sensorText += ">";
                    sensorText += jsonStatus.sensors[i].formatted;
                }
                sensorText += "</td>";
                
                if ((jsonStatus.sensors.length > 4) && ((i % 2) == 1)) {
                    sensorText += "<tr>";
                }
            }
            sensorText += "</table>";
            var sensorData = document.getElementById("sensorData");
            if (typeof sensorData != "undefined" && sensorData != null) {
                sensorData.innerHTML = sensorText;
            }
        }

		firstStatusLoad = 0;
	}

	function GetUniverseBytesReceived()
	{	
		var html='';
		var html1='';
    var xmlhttp=new XMLHttpRequest();
		var url = "fppxml.php?command=getUniverseReceivedBytes";
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.onreadystatechange = function () {
			if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
			{
					var xmlDoc=xmlhttp.responseXML; 
					var receivedBytes = xmlDoc.getElementsByTagName('receivedBytes')[0];
					if(receivedBytes && receivedBytes.childNodes.length> 0)
					{
						html =  "<table>";
						html += "<tr id=\"rowReceivedBytesHeader\"><td>Universe</td><td>Start Address</td><td>Packets</td><td>Bytes</td><td>Errors</td></tr>";

                        var i;
                        var maxRows = receivedBytes.childNodes.length / 2;
                        if (maxRows < 32) {
                            maxRows = 32;
                        }
						for(i=0;i<receivedBytes.childNodes.length;i++)
						{
								if(i==maxRows)
								{
									html += "</table>";
									html1 = html;
									html =  "<table>";
									html += "<tr id=\"rowReceivedBytesHeader\"><td>Universe</td><td>Start Address</td><td>Packets</td><td>Bytes</td><td>Errors</td></tr>";
								}
								var universe = receivedBytes.childNodes[i].childNodes[0].textContent;
								var startChannel = receivedBytes.childNodes[i].childNodes[1].textContent;
								var bytes = receivedBytes.childNodes[i].childNodes[2].textContent;
								var packets = receivedBytes.childNodes[i].childNodes[3].textContent;
                                var errors = receivedBytes.childNodes[i].childNodes[4].textContent;
								html += "<tr><td>" + universe + "</td>";
								html += "<td>" + startChannel + "</td><td>" + packets + "</td><td>" + bytes + "</td><td>" + errors + "</td></tr>";
						}
						html += "</table>";
					}
					if(receivedBytes && receivedBytes.childNodes.length>32)
					{
						$("#bridgeStatistics1").html(html1);
						$("#bridgeStatistics2").html(html);
					}
					else
					{
						$("#bridgeStatistics1").html(html);
						$("#bridgeStatistics2").html('');
					}					
			}
		};
		xmlhttp.send();
	}
	
	function UpdateCurrentEntryPlaying(index,lastIndex)
	{
		$('#tblPlaylistDetails tbody tr').removeClass('PlaylistRowPlaying');
		$('#tblPlaylistDetails tbody td').removeClass('PlaylistPlayingIcon');

		if((index >= 0) && ($('#playlistRow' + index).length))
		{
			$("#colEntryNumber" + index).addClass("PlaylistPlayingIcon");
			$("#playlistRow" + index).addClass("PlaylistRowPlaying");
		}
	}
	
	function SetIconForCurrentPlayingEntry(index)
	{
	}
	
	
	function SetButtonState(button,state)
	{
			// Enable Button
			if(state == 'enable')
			{
				$(button).addClass('buttons');
				$(button).removeClass('disableButtons');
				$(button).removeAttr("disabled");
			}
			else
			{
				$(button).removeClass('buttons');
				$(button).addClass('disableButtons');
				$(button).attr("disabled", "disabled");
			}
	}
	
	function StopGracefully()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=stopGracefully";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
	}

	function StopGracefullyAfterLoop()
	{
		var xmlhttp=new XMLHttpRequest();
		var url = "fppxml.php?command=stopGracefullyAfterLoop";
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.send();
	}

	function StopNow()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=stopNow";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
	}

	function ToggleSequencePause()
	{
		$.get("fppjson.php?command=toggleSequencePause");
	}

	function SingleStepSequence()
	{
		$.get("fppjson.php?command=singleStepSequence");
	}

	function SingleStepSequenceBack()
	{
		$.get("fppjson.php?command=singleStepSequenceBack");
	}

	function StopFPPD()
	{
    	var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=stopFPPD";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
	}

	function SetSettingRestart(key, value) {
		var restartFPPD = 1;

		if ((key == 'LogLevel') ||
			(key == 'LogMask'))
		{
			restartFPPD = 0;
		}

		SetSetting(key, value, restartFPPD, 0);
	}

	function SetSettingReboot(key, value) {
		SetSetting(key, value, 0, 1);
	}

function SetSetting(key, value, restart, reboot) {
    $.ajax({
        url: "fppjson.php?command=setSetting&key=" + key + "&value=" + value,
        timeout: 1000,
        async: false
    }).success(function() {
        if ((key != 'restartFlag') && (key != 'rebootFlag'))
            $.jGrowl(key + " setting saved.");

        CheckRestartRebootFlags();
    }).fail(function() {
        DialogError('Save Setting', "Failed to save " + key + " setting.");
        CheckRestartRebootFlags();
    });
}

	function ClearRestartFlag() {
		settings['restartFlag'] = 0;
		SetSetting('restartFlag', 0, 0, 0);
	}

	function SetRestartFlag(newValue) {
		// 0 - no restart needed
		// 1 - full restart is needed
		// 2 - quick restart is OK
		if ((newValue == 2) &&
			(settings['restartFlag'] == 1))
			return;

		settings['restartFlag'] = newValue;
		SetSettingRestart('restartFlag', newValue);
	}

	function ClearRebootFlag() {
		settings['rebootFlag'] = 0;
		SetSetting('rebootFlag', 0, 0, 0);
	}

	function SetRebootFlag() {
		settings['rebootFlag'] = 1;
		SetSettingReboot('rebootFlag', 1);
	}

	function CheckRestartRebootFlags() {
		if (settings['disableUIWarnings'] == 1)
		{
			$('#restartFlag').hide();
			$('#rebootFlag').hide();
			return;
		}

		if (settings['restartFlag'] >= 1)
			$('#restartFlag').show();
		else
			$('#restartFlag').hide();

		if (settings['rebootFlag'] == 1)
		{
			$('#restartFlag').hide();
			$('#rebootFlag').show();
		}
		else
		{
			$('#rebootFlag').hide();
		}
	}

function GetFPPDUptime()
	{
		$.get("fppxml.php?command=getFPPDUptime");
	}

function RestartFPPD() {
		var args = "";

		if (settings['restartFlag'] == 2)
			args = "&quick=1";

		$('html,body').css('cursor','wait');
		$.get("fppxml.php?command=restartFPPD" + args
		).done(function() {
			$('html,body').css('cursor','auto');
			$.jGrowl('FPPD Restarted');
			ClearRestartFlag();
		}).fail(function() {
			$('html,body').css('cursor','auto');

            //If fail, the FPP may have rebooted (eg.FPPD triggering a reboot due to disabling soundcard or activating Pi Pixel output
            //start polling and see if we can wait for the FPP to come back up
            //restartFlag will already be cleared, but to keep things simple, just call the original code
            retries = 0;
            retries_max = max_retries;//avg timeout is 10-20seconds, polling resolves after 6-10 polls
            //attempt to poll every second, AJAX block for the default browser timeout if host is unavail
            retry_poll_interval_arr['restartFPPD'] = setInterval(function () {
                poll_result = false;
                if (retries < retries_max) {
                    // console.log("Polling @ " + retries);
                    //poll for FPPDRunning as it's the simplest command to run and doesn't put any extra processing load on the backend
                    $.ajax({
                            url: "fppxml.php?command=isFPPDrunning",
                            timeout: 1000,
                            async: true
                        }
                    ).success(
                        function () {
                            poll_result = true;
                            //FPP is up then
                            clearInterval(retry_poll_interval_arr['restartFPPD']);
                            //run original code for success
                            $.jGrowl('FPPD Restarted');
                            ClearRestartFlag();
                        }
                    ).fail(
                        function () {
                            poll_result = false;
                            retries++;
                            //If on first try throw up a FPP is rebooting notification
                            if(retries === 1){
                                //Show FPP is rebooting notification for 10 seconds
                                $.jGrowl('FPP is rebooting..',{ life: 10000 });
                            }
                        }
                    );

                    // console.log("Polling Result " + poll_result);
                } else {
                    //run original code
                    clearInterval(retry_poll_interval_arr['restartFPPD']);
                    DialogError("Restart FPPD", "Error restarting FPPD");
                }
            }, 2000);
		});
	}

function zeroPad(num, places) {
	var zero = places - num.toString().length + 1;
	return Array(+(zero > 0 && zero)).join("0") + num;
}
	
function ControlFPPD()
	{
    var xmlhttp=new XMLHttpRequest();
		var btnVal = $("#btnDaemonControl").attr('value');
		if(btnVal == "Stop FPPD")
		{
			var url = "fppxml.php?command=stopFPPD";
		}
		else
		{
			var url = "fppxml.php?command=startFPPD";
		}
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.onreadystatechange = function () {
			if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
			{
				var i = 19;
			}
		};
		xmlhttp.send();
	}
	
function PopulatePlaylists(sequencesAlso)
{
    var playlistOptionsText="";
    GetPlaylistArray();

    if (sequencesAlso)
        playlistOptionsText += "<option disabled>---------- Playlists ---------- </option>";

    for(j = 0; j < playListArray.length; j++)
    {
        playlistOptionsText +=  "<option value=\"" + playListArray[j] + "\">" + playListArray[j] + "</option>";
    }

    if (sequencesAlso) {
        GetSequenceArray();

        playlistOptionsText += "<option disabled>---------- Sequences ---------- </option>";
        for(j = 0; j < sequenceArray.length; j++)
        {
            playlistOptionsText +=  "<option value=\"" + sequenceArray[j] + ".fseq\">" + sequenceArray[j] + ".fseq</option>";
        }
    }

    $('#playlistSelect').html(playlistOptionsText);
}

function PlayPlaylist(Playlist, goToStatus = 0)
{
    $.get("/api/command/Start Playlist/" + Playlist + "/0").success(function() {
        if (goToStatus)
	        location.href="index.php";
        else
            $.jGrowl("Playlist Started");
    });
}

function StartPlaylistNow()
	{
		var Playlist =  $("#playlistSelect").val();
        var xmlhttp=new XMLHttpRequest();
		var repeat = $("#chkRepeat").is(':checked')?'checked':'unchecked';
		var url = "fppxml.php?command=startPlaylist&playList=" + Playlist + "&repeat=" + repeat + "&playEntry=" + PlayEntrySelected + "&section=" + PlaySectionSelected ;
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.send();
	}

function StopEffect()
{
	if (RunningEffectSelectedId < 0)
		return;

	var url = "fppxml.php?command=stopEffect&id=" + RunningEffectSelectedId;
	var xmlhttp=new XMLHttpRequest();
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');
	xmlhttp.send();

	RunningEffectSelectedId = -1;
	RunningEffectSelectedName = "";
	SetButtonState('#btnStopEffect','disable');

	GetRunningEffects();
}

var gblLastRunningEffectsXML = "";

function GetRunningEffects()
{
	var url = "fppxml.php?command=getRunningEffects";
	var xmlhttp=new XMLHttpRequest();
	xmlhttp.open("GET",url,true);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');

	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200)
		{
			var xmlDoc=xmlhttp.responseXML;
			var xmlText = new XMLSerializer().serializeToString(xmlDoc);

			$('#tblRunningEffectsBody').html('');
			if (xmlText != gblLastRunningEffectsXML)
			{
				xmlText = gblLastRunningEffectsXML;

				var entries = xmlDoc.getElementsByTagName('RunningEffects')[0];

				if(entries.childNodes.length> 0)
				{
					for(i=0;i<entries.childNodes.length;i++)
					{
						id = entries.childNodes[i].childNodes[0].textContent;
						name = entries.childNodes[i].childNodes[1].textContent;

						if (name == RunningEffectSelectedName)
						    $('#tblRunningEffectsBody').append('<tr class="effectSelectedEntry"><td width="5%">' + id + '</td><td width="95%">' + name + '</td></tr>');
                        else
							$('#tblRunningEffectsBody').append('<tr><td width="5%">' + id + '</td><td width="95%">' + name + '</td></tr>');
					}

					setTimeout(GetRunningEffects, 1000);
				}
			}
		}
	}

	xmlhttp.send();
}

	function RebootPi()
	{
		if (confirm('REBOOT the Falcon Player?'))
		{
			ClearRestartFlag();
			ClearRebootFlag();

			//Delay reboot for 1 second to allow flags to be cleared
            setTimeout(function () {
                var xmlhttp = new XMLHttpRequest();
                var url = "fppxml.php?command=rebootPi";
                xmlhttp.open("GET", url, true);
                xmlhttp.setRequestHeader('Content-Type', 'text/xml');
                xmlhttp.send();

                //Show FPP is rebooting notification for 10 seconds
                $.jGrowl('FPP is rebooting..', {life: 10000});
            }, 1000);
		} 
	}

	function ShutdownPi()
	{
		if (confirm('SHUTDOWN the Falcon Player?'))
		{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=shutdownPi";
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
		} 
	}

function UpgradePlaylist(data, editMode)
{
    var sections = ['leadIn', 'mainPlaylist', 'leadOut'];
    var events = GetSync('/api/events');
    var error = "";

    for (var s = 0; s < sections.length; s++) {
        if (typeof data[sections[s]] != 'object') {
            continue;
        }

        for (i = 0; i < data[sections[s]].length; i++) {
            var type = data[sections[s]][i]['type'];
            var o = data[sections[s]][i];
            var n = {};

            n.enabled = o.enabled;
            n.playOnce = o.playOnce;

            // Changes for both Status UI and Edit Mode.  These are needed in the status UI
            // when new fields replace old fields and where the PlaylistEntry* classes also
            // handle these conversions.
            if (type == 'branch') {
                if ((typeof o.startTime === 'undefined') ||
                    (typeof o.endTime === 'undefined')) {
                    n = o;
                    n.startTime =
                        PadLeft(o.compInfo.startHour, '0', 2) + ':' +
                        PadLeft(o.compInfo.startMinute, '0', 2) + ':' +
                        PadLeft(o.compInfo.startSecond, '0', 2);

                    n.endTime =
                        PadLeft(o.compInfo.endHour, '0', 2) + ':' +
                        PadLeft(o.compInfo.endMinute, '0', 2) + ':' +
                        PadLeft(o.compInfo.endSecond, '0', 2);

                    delete n.compInfo;
                    data[sections[s]][i] = n;
                }
            } else if (type == 'dynamic') {
                if ((o.subType == 'file') &&
                    (typeof o.dataFile != 'string') &&
                    (typeof o.data == 'string')) {
                    n = o;
                    n.dataFile = n.data;
                    data[sections[s]][i] = n;
                } else if ((o.subType == 'plugin') &&
                    (typeof o.pluginName != 'string') &&
                    (typeof o.data == 'string')) {
                    n = o;
                    n.pluginName = n.data;
                    data[sections[s]][i] = n;
                } else if ((o.subType == 'url') &&
                    (typeof o.url != 'string') &&
                    (typeof o.data == 'string')) {
                    n = o;
                    n.url = n.data;
                    data[sections[s]][i] = n;
                }
            }

            // Changes needed only during edit mode when we are upgrading a playlist
            if (editMode) {
                if (type == 'event') {
                    var id = PadLeft(o.majorID, '0', 2) + '_' + PadLeft(o.minorID, '0', 2);
                    if (typeof events[id] === 'object') {
                        n.type = 'command';
                        n.command = events[id].command;
                        n.args = events[id].args;

                        data[sections[s]][i] = n;
                    } else {
                        DialogError("Converting deprecated Event", "The Event playlist entry type has been deprecated.  FPP tries to automatically convert these to the new FPP Command playlist entry type, but the specified event ID '" + id + "' could not be found.  The existing playlist on-disk will not be modified, but the deprecated Event has been removed from the playlist editor.");
                    }
                } else if (type == 'mqtt') {
                    n.type = 'command';
                    n.command = "MQTT";

                    var args = [];
                    args.push(o.topic);
                    args.push(o.message);

                    n.args = args;

                    data[sections[s]][i] = n;
// 'Run Script' command does not support blocking yet. If this
// is done, then PlaylistEntryScript.cpp can be deprecated.
//                } else if (type == 'script') {
//                    n.type = 'command';
//                    n.command = 'Run Script';
//
//                    var args = [];
//                    args.push(o.scriptName);
//                    args.push(o.scriptArgs);
//
//                    n.args = args;
//
//                    data[sections[s]][i] = n;
                } else if (type == 'volume') {
                    n.type = 'command';
                    n.command = 'Volume Adjust';
                    n.args = [ o.volume ];

                    data[sections[s]][i] = n;
                }
            }
        }
    }

    return data;
}

function PopulatePlaylistDetails(data, editMode)
{
    var innerHTML = "";
    var entries = 0;

    data = UpgradePlaylist(data, editMode);

    if (!editMode)
        $('#deprecationWarning').hide(); // will re-show if we find any

    var sections = ['leadIn', 'mainPlaylist', 'leadOut'];
    for (var s = 0; s < sections.length; s++) {
        var idPart = sections[s].charAt(0).toUpperCase() + sections[s].slice(1);

        if (data.hasOwnProperty(sections[s]) && data[sections[s]].length > 0)
        {
            innerHTML = "";
            for (i = 0; i < data[sections[s]].length; i++)
            {
                innerHTML += GetPlaylistRowHTML(entries, data[sections[s]][i], editMode);
                entries++;
            }
            $('#tblPlaylist' + idPart).html(innerHTML);
            $('#tblPlaylist' + idPart + 'Header').show();

            if (!data[sections[s]].length)
                $('#tblPlaylist' + idPart).html("<tr id='tblPlaylist" + idPart + "PlaceHolder' class='unselectable'><td>&nbsp;</td></tr>");

            $('#tblPlaylist' + idPart + ' > tr').each(function() {
                PopulatePlaylistItemDuration($(this), editMode);
            });
        }
        else
        {
            $('#tblPlaylist' + idPart).html("");
            if (editMode) {
                $('#tblPlaylist' + idPart + 'Header').show();
                $('#tblPlaylist' + idPart).html("<tr id='tblPlaylist" + idPart + "PlaceHolder' class='unselectable'><td>&nbsp;</td></tr>");
            } else {
                $('#tblPlaylist' + idPart + 'Header').hide();
            }
        }
    }

    if (!editMode) {
        gblCurrentLoadedPlaylist = data.name;
        gblCurrentLoadedPlaylistCount = entries;
        UpdatePlaylistDurations();
    }

    SetPlaylistName(data.name);
}

function PopulatePlaylistDetailsEntries(playselected,playList)
{
    var type;
    var pl;
    var xmlhttp=new XMLHttpRequest();
    var innerHTML="";
    var fromMemory = "";

    if (playselected == true)
    {
        pl = $('#playlistSelect :selected').text();
    }
    else
    {
        pl = playList;
        fromMemory = '&fromMemory=1';
    }

    PlayEntrySelected = 0;
    PlaySectionSelected = '';

    $.ajax({
        url: 'fppjson.php?command=getPlayListEntries&pl=' + pl + '&mergeSubs=1' + fromMemory,
        dataType: 'json',
        success: function(data, reqStatus, xhr) {
            PopulatePlaylistDetails(data, 0);
            VerbosePlaylistItemDetailsToggled();
        }
    });
}

function SelectPlaylistDetailsEntryRow(index)
{
		PlayEntrySelected  = index;
}

function SetVolume(value)
{
			var xmlhttp=new XMLHttpRequest();
			var url = "fppxml.php?command=setVolume&volume=" + value;
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
}

function SetFPPDmode()
{
	$.get("fppxml.php?command=setFPPDmode&mode=" + $('#selFPPDmode').val()
	).done(function() {
		$.jGrowl("fppMode Saved");
		RestartFPPD();
	}).fail(function() {
		DialogError("FPP Mode Change", "Save Failed");
	});
}

function SetE131interface()
{
			var xmlhttp=new XMLHttpRequest();
			var iface = $('#selE131interfaces').val();	
			var url = "fppxml.php?command=setE131interface&iface=" + iface;
			xmlhttp.open("GET",url,true);
			xmlhttp.setRequestHeader('Content-Type', 'text/xml');
			xmlhttp.send();
			$.jGrowl("E1.31 Interface Saved");
}

function GetVolume()
{
    var xmlhttp=new XMLHttpRequest();
		var url = "fppxml.php?command=getVolume";
		xmlhttp.open("GET",url,true);
		xmlhttp.setRequestHeader('Content-Type', 'text/xml');
		xmlhttp.onreadystatechange = function () {
			if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
			{
					var xmlDoc=xmlhttp.responseXML; 
					var Volume = parseInt(xmlDoc.getElementsByTagName('Volume')[0].childNodes[0].textContent);
					if ((Volume < 0) || (Volume == "NaN"))
					{
						Volume = 75;
						SetVolume(Volume);
					}
					$('#volume').html(Volume);
					$('#slider').slider('value', Volume);
					SetSpeakerIndicator(Volume);
			}
		};
		xmlhttp.send();

}

function AdjustFPPDModeFromStatus(mode) {
    var cur = $("#selFPPDmode").val();
    SetupUIForMode(mode);
    if (mode != cur) {
        if(mode == 1) // Bridge Mode
        {
            $("#selFPPDmode").prop("selectedIndex",3);
            $("#textFPPDmode").text("Bridge");
        } else if (mode == 8) { // Remote Mode
            $("#selFPPDmode").prop("selectedIndex",2);
            $("#textFPPDmode").text("Player (Remote)");
        } else { // Player or Master modes
            if (mode == 2) { // Player
                $("#selFPPDmode").prop("selectedIndex",0);
                $("#textFPPDmode").text("Player (Standalone)");
            } else {
                $("#selFPPDmode").prop("selectedIndex",1);
                $("#textFPPDmode").text("Player (Master)");
            }
        }
    }
}

function GetFPPDmode()
{
    var xmlhttp=new XMLHttpRequest();
    var url = "fppxml.php?command=getFPPDmode";
    xmlhttp.open("GET",url,true);
    xmlhttp.setRequestHeader('Content-Type', 'text/xml');
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status==200)
        {
            var xmlDoc=xmlhttp.responseXML;
            var mode = parseInt(xmlDoc.getElementsByTagName('mode')[0].childNodes[0].textContent);
            SetupUIForMode(mode);
            if(mode == 1) // Bridge Mode
            {
                $("#selFPPDmode").prop("selectedIndex",3);
                $("#textFPPDmode").text("Bridge");
            } else if (mode == 8) { // Remote Mode
                $("#selFPPDmode").prop("selectedIndex",2);
                $("#textFPPDmode").text("Player (Remote)");
            } else { // Player or Master modes
                if (mode == 2) { // Player
                    $("#selFPPDmode").prop("selectedIndex",0);
                    $("#textFPPDmode").text("Player (Standalone)");
                } else {
                    $("#selFPPDmode").prop("selectedIndex",1);
                    $("#textFPPDmode").text("Player (Master)");
                }
            }
        }
    };
    xmlhttp.send();
}

var helpOpen = 0;
function HelpClosed() {
    helpOpen = 0;
}

function DisplayHelp()
{
    if (helpOpen) {
        helpOpen = 0;
        $('#dialog-help').dialog('close');
        return;
    }

    var tmpHelpPage = helpPage;

	if ((helpPage == 'help/settings.php') && $('#tabs').length) {
		var tab = $("#tabs li.ui-tabs-active a").attr('href').replace('#tab-', '');
        if (tab != '') {
            tmpHelpPage = helpPage.replace('.php', '-' + tab + '.php');
        }
	}

	$('#helpText').html("No help file exists for this page yet");
	$('#helpText').load(tmpHelpPage);
	$('#dialog-help').dialog({ height: 600, width: 1000, title: "Help - Hit F1 or ESC to close", close: HelpClosed });
	$('#dialog-help').dialog( "moveToTop" );
    helpOpen = 1;
}

function GetGitOriginLog()
{
	$('#logText').html("Loading list of changes from github.");
	$('#logText').load("fppxml.php?command=getGitOriginLog");
	$('#logViewer').dialog({ height: 600, width: 800, title: "Git Changes" });
	$('#logViewer').dialog( "moveToTop" );
}

function GetVideoInfo(file)
{
    $('#fileText').html("Getting Video Info.");

    $.get("/api/media/" + file + "/meta", function(data) {
        $('#fileText').html('<pre>' + syntaxHighlight(JSON.stringify(data, null, 2)) + '</pre>');
        $('#fileViewer').dialog({ height: 600, width: 800, title: "Video Info" });
        $('#fileViewer').dialog( "moveToTop" );
    });
}

function PlayFileInBrowser(dir, file)
{
	window.open("fppxml.php?command=getFile&play=1&dir=" + dir + "&filename=" + file);
}

function CopyFile(dir, file)
{
	var newFile = prompt("New Filename:", file);

	var postData = "command=copyFile&dir=" + dir + "&filename=" + encodeURIComponent(file) + "&newfilename=" + encodeURIComponent(newFile);

	$.post("fppjson.php", postData).done(function(data) {
		if (data.status == 'success')
			GetFiles(dir);
		else
			DialogError("File Copy Failed", "Error: File Copy failed.");
	}).fail(function() {
		DialogError("File Copy Failed", "Error: File Copy failed.");
	});
}

function RenameFile(dir, file)
{
	var newFile = prompt("New Filename:", file);

	var postData = "command=renameFile&dir=" + dir + "&filename=" + encodeURIComponent(file) + "&newfilename=" + encodeURIComponent(newFile);

	$.post("fppjson.php", postData).done(function(data) {
		if (data.status == 'success')
			GetFiles(dir);
		else
			DialogError("File Rename Failed", "Error: File Rename failed.");
	}).fail(function() {
		DialogError("File Rename Failed", "Error: File Rename failed.");
	});
}

function DownloadFile(dir, file)
{
	location.href="fppxml.php?command=getFile&dir=" + dir + "&filename=" + file;
}

function DownloadFiles(dir, files)
{
    if (files.length == 1) {
        DownloadFile(dir, files[0]);
    } else {
        for (var i = 0; i < files.length; i++) {
            window.open("fppxml.php?command=getFile&dir=" + dir + "&filename=" + files[i]);
        }
    }
}

function DownloadZip(dir)
{
	location.href="fppxml.php?command=getZip&dir=" + dir;
}

function ViewImage(file)
{
	var url = "fppxml.php?command=getFile&dir=Images&filename=" + file + '&attach=0';
	window.open(url, '_blank');
}

function ViewFile(dir, file)
{
	$('#fileText').html("Loading...");
	$.get("fppxml.php?command=getFile&dir=" + dir + "&filename=" + file, function(text) {
		var ext = file.split('.').pop();
		if (ext != "html")
			$('#fileText').html("<pre>" + text.replace(/</g, '&lt;').replace(/>/g, '&gt;') + "</pre>");
	});

	$('#fileViewer').dialog({ height: 600, width: 800, title: "File Viewer: " + file });
	$('#fileViewer').dialog( "moveToTop" );
}

function DeleteFile(dir, row, file)
{
	if (file.indexOf("/") > -1)
	{
		alert("You can not delete this file.");
		return;
	}

    $.get("fppxml.php?command=deleteFile&dir=" + dir + "&filename=" + encodeURIComponent(file)
    ).done(function() {
        $(row).remove();
    }).fail(function() {
        DialogError("ERROR", "Error deleting file: " + file);
    });
}

function SaveUSBDongleSettings()
{
	var usbDonglePort = $('#USBDonglePort').val();
	var usbDongleType = $('#USBDongleType').val();
	var usbDongleBaud = $('#USBDongleBaud').val();

	var xmlhttp=new XMLHttpRequest();
	var url = "fppxml.php?command=saveUSBDongle&port=" + usbDonglePort +
				"&type=" + usbDongleType +
				"&baud=" + usbDongleBaud;
	xmlhttp.open("GET",url,false);
	xmlhttp.setRequestHeader('Content-Type', 'text/xml');

	xmlhttp.onreadystatechange = function () {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) 
		{
			alert("You must restart FPPD for changes to take effect.");
		}
	};
	xmlhttp.send();
}

function SetupSelectableTableRow(info)
{
	$('#' + info.tableName + ' > tbody').on('mousedown', 'tr', function(event,ui){
		$('#' + info.tableName + ' > tbody > tr').removeClass('fppTableSelectedEntry');
		$(this).addClass('fppTableSelectedEntry');
		var items = $('#' + info.tableName + ' > tbody > tr');
		info.selected  = items.index(this);

		for (var i = 0; i < info.enableButtons.length; i++) {
			SetButtonState('#' + info.enableButtons[i], "enable");
		}
		for (var i = 0; i < info.disableButtons.length; i++) {
			SetButtonState('#' + info.disableButtons[i], "disable");
		}
	});

    if (info.hasOwnProperty('sortable') && info.sortable) {
        $('#' + info.tableName + ' > tbody').sortable({
            update: function(event, ui) {
                if ((info.hasOwnProperty('sortableCallback')) &&
                    (info.sortableCallback != '')) {
                    window[info.sortableCallback]();
                }
            },
            scroll: true
        }).disableSelection();
    }
}

function DialogOK(title, message)
{
	$('#dialog-popup').dialog({
		mocal: true,
		height: 'auto',
		width: 400,
		autoResize: true,
		closeOnEscape: false,
		buttons: {
			Ok: function() {
				$(this).dialog("close");
			}
		}
	});
	$('#dialog-popup').dialog('option', 'title', title);
	$('#dialog-popup').html("<center>" + message + "</center>");
	$('#dialog-popup').show();
	$('#dialog-popup').draggable();
}

// Simple wrapper for now, but we may highlight this somehow later
function DialogError(title, message)
{
	DialogOK(title, message);
}

// page visibility prefixing
function getHiddenProp(){
    var prefixes = ['webkit','moz','ms','o'];
    
    // if 'hidden' is natively supported just return it
    if ('hidden' in document) return 'hidden';
    
    // otherwise loop over all the known prefixes until we find one
    for (var i = 0; i < prefixes.length; i++){
        if ((prefixes[i] + 'Hidden') in document) 
            return prefixes[i] + 'Hidden';
    }

    // otherwise it's not supported
    return null;
}

// return page visibility
function isHidden() {
    var prop = getHiddenProp();
    if (!prop) return false;
    
    return document[prop];
}

function bindVisibilityListener() {
	var visProp = getHiddenProp();
	if (visProp) {
	  var evtname = visProp.replace(/[H|h]idden/,'') + 'visibilitychange';
	  document.addEventListener(evtname, handleVisibilityChange);
	}
}

function handleVisibilityChange() {
    if (isHidden() && statusTimeout != null) {
        clearTimeout(statusTimeout);
        statusTimeout = null;
    } else {
         GetFPPStatus();
    }
}

// syntaxHighlight() from https://stackoverflow.com/questions/4810841/pretty-print-json-using-javascript
function syntaxHighlight(json) {
    json = json.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    return json.replace(/("(\\\\u[a-zA-Z0-9]{4}|\\\\[^u]|[^\\\\"])*"(\s*:)?|\b(true|false|null)\b|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?)/g, function (match) {
        var cls = 'jsNumber';
        if (/^"/.test(match)) {
            if (/:$/.test(match)) {
                cls = 'jsKey';
            } else {
                cls = 'jsString';
            }
        } else if (/true|false/.test(match)) {
            cls = 'jsBoolean';
        } else if (/null/.test(match)) {
            cls = 'jsNull';
        }
        return '<span class="' + cls + '">' + match + '</span>';
    });
}

function CommandToJSON(commandSelect, tblCommand, json, addArgTypes = false) {
    var args = new Array()
    var argTypes = new Array()
    var commandVal = $('#' + commandSelect).val();;
    json['command'] = commandVal;
    if (commandVal != "" && !(typeof commandVal == "undefined")) {
        json['multisyncCommand'] = $("#" + tblCommand + "_multisync").is(":checked");
        json['multisyncHosts'] = $("#" + tblCommand + "_multisyncHosts").val();
        for (var x = 1; x < 20; x++) {
            var inp =  $("#" + tblCommand + "_arg_" + x);
            var val = inp.val();
            if (inp.attr('type') == 'checkbox') {
                if (inp.is(":checked")) {
                    args.push("true");
                } else {
                    args.push("false");
                }
                if (addArgTypes) {
                    argTypes.push(inp.data("arg-type"));
                }
            } else if (inp.attr('type') == 'number' || inp.attr('type') == 'text') {
                args.push(val);
                var adj =  $("#" + tblCommand + "_arg_" + x + "_adjustable");
                if (adj.attr('type') == "checkbox") {
                    if (adj.is(":checked")) {
                        if (typeof json['adjustable'] == "undefined") {
                            json['adjustable'] = {};
                        }
                        json['adjustable'][x] = inp.attr('type');
                    } else {
                        if (typeof json['adjustable'] != "undefined") {
                            delete json['adjustable'][x];
                            if (jQuery.isEmptyObject(json['adjustable'])) {
                                delete json['adjustable'];
                            }
                        }
                    }
                }
                if (addArgTypes) {
                    argTypes.push(inp.data("arg-type"));
                }
            } else if (typeof val != "undefined") {
                args.push(val);
                if (addArgTypes) {
                    argTypes.push(inp.data("arg-type"));
                }
            }
        }
    }
    json['args'] = args;
    if (addArgTypes) {
        json['argTypes'] = argTypes;
    }
    return json;
}

function LoadCommandArg() {
    LoadCommandList($('.arg_command'));
}

var commandList = "";
var extraCommands = "";
function LoadCommandList(commandSelect) {
    if (typeof commandSelect === "string") {
        commandSelect = $('#' + commandSelect);
    }
    if (commandList == "") {
        $.ajax({
            dataType: "json",
            url: "api/commands",
            async: false,
            success: function(data) {
               commandList = data;
               if (extraCommands != "") {
                    $.each( extraCommands, function(key, val) {
                        commandList.push(val);
                    });
               }
               $.each( data, function(key, val) {
                   option = "<option value='" + val['name'] + "'>" + val['name'] + "</option>";
                   commandSelect.append(option);
               });
            }});
    } else {
        $.each( commandList, function(key, val) {
            option = "<option value='" + val['name'] + "'>" + val['name'] + "</option>";
            commandSelect.append(option);
        });
    }
}

function UpdateChildVisibility() {
    if (typeof playlistEntryTypes === "undefined") {
        return;
    }
    var pet = playlistEntryTypes[$('#pe_type').val()];
    var keys = Object.keys(pet.args);
    var shown = [];

    for (var i = 0; i < keys.length; i++) {
        var a = pet.args[keys[i]];
        if (typeof a['children'] === 'object') {
            var val = $('.arg_' + a.name).val();
            var ckeys = Object.keys(a.children);
            for (var c = 0; c < ckeys.length; c++) {
                for (var x = 0; x < a.children[ckeys[c]].length; x++) {
                    if (val == ckeys[c]) {
                        $('.arg_row_' + a.children[ckeys[c]][x]).show();
                        shown.push(a.children[ckeys[c]][x]);
                    } else {
                        if (!shown.includes(a.children[ckeys[c]][x])) {
                            $('.arg_row_' + a.children[ckeys[c]][x]).hide();
                        }
                    }
                }
            }
        }
    }
}

function CommandArgChanged() {
    $('#playlistEntryCommandOptions').html('');
    CommandSelectChanged('playlistEntryOptions_arg_1', 'playlistEntryCommandOptions');
}

var allowMultisyncCommands = false;
function OnMultisyncChanged(mscheck, tblCommand) {
    var b = $(mscheck).is(":checked");
    if (b) {
        $("#" + tblCommand + "_multisyncHosts_row").show();
    } else {
        $("#" + tblCommand + "_multisyncHosts_row").hide();
    }
}

var remoteIpList = null;
function GetRemotes() {
    if (remoteIpList == null) {
        $.ajax({
        dataType: "json",
        async: false,
        url: "api/remotes",
        success: function(data) {
               remoteIpList = data;
             }
         });
    }
    return remoteIpList;
}
function CommandSelectChanged(commandSelect, tblCommand, configAdjustable = false, argPrintFunc = PrintArgInputs)
{
    for (var x = 1; x < 25; x++) {
        $('#' + tblCommand + '_arg_' + x + '_row').remove();
        $("#" + tblCommand + "_multisync_row").remove();
        $("#" + tblCommand + "_multisyncHosts_row").remove();
    }
    var command = $('#' + commandSelect).val();
    if (typeof command == "undefined"  ||  command == null) {
        return;
    }
    var co = commandList.find(function(element) {
                              return element["name"] == command;
                              });
   if (typeof co == "undefined"  ||  co == null) {
       $.ajax({
       dataType: "json",
       async: false,
       url: "api/commands/" + command,
       success: function(data) {
              co = data;
            }
        });
   }
    var line = "<tr id='" + tblCommand + "_multisync_row' ";
    if (!allowMultisyncCommands || command == "") {
        line += "style='display:none'";
    }
    line += "><td>Multicast:</td><td><input type='checkbox' id='" + tblCommand
        + "_multisync' onChange='OnMultisyncChanged(this, \"" +  tblCommand + "\");'></input></td></tr>";
    $('#' + tblCommand).append(line);
    line = "<tr id='" + tblCommand + "_multisyncHosts_row' style='display:none'><td>Hosts:</td><td><input style='width:100%;' type='text' id='" + tblCommand + "_multisyncHosts'";
    line += " list='" + tblCommand + "_multisyncHosts_list'></input>";
    line += "<datalist id='"+ tblCommand + "_multisyncHosts_list'>";
    remotes = GetRemotes();
    $.each( remotes, function (k, v) {
           line += "<option value='" + k + "'>" + v + "</option>\n";
           });
    line += "</datalist></td></tr>";
    
    $('#' + tblCommand).append(line);

    argPrintFunc(tblCommand, configAdjustable, co['args']);

}
function SubCommandChanged(subCommandV, configAdjustable = false, argPrintFunc = PrintArgInputs) {
    var subCommand = $(subCommandV);
    if (typeof subCommandV === "string") {
        subCommand = $("#" + subCommandV);
    }
    var val = subCommand.val();
    var url = subCommand.data("url");
    var count = subCommand.data("count");
    var tblCommand = subCommand.data('tblcommand');
    
    for (var x = count+1; x < 25; x++) {
        $('#' + tblCommand + '_arg_' + x + '_row').remove();
    }
    $.ajax({
           dataType: "json",
           async: false,
           url: url + val,
           success: function(data) {
            argPrintFunc(tblCommand, false, data['args'], count+1);
           }
    });

}
function PrintArgsInputsForEditable(tblCommand, configAdjustable, args, startCount = 1) {
    var count = startCount;
    var initFuncs = [];
    var haveTime = 0;
    var haveDate = 0;
    var children = [];

//    $.each( args,
    var valFunc = function( key, val ) {
        if (val['type'] == 'args') {
            return;
        }
        
        if ((val.hasOwnProperty('statusOnly')) &&
            (val.statusOnly == true)) {
            return;
        }
        var ID = tblCommand + "_arg_" + count;
        var line = "<tr id='" + ID + "_row' class='arg_row_" + val['name'] + "'><td>";
        var subCommandInitFunc = null;
        if (children.includes(val['name']))
            line += "&nbsp;&nbsp;&nbsp;&nbsp;&bull;&nbsp;";

        var typeName = val['type'];
        if (typeName == "datalist") {
            typeName = "string";
        }

        var dv = "";
        if (typeof val['default'] != "undefined") {
            dv = val['default'];
        }
        var contentListPostfix = "";
        if (val['type'] == "subcommand") {
            line += val["description"] + ":</td><td>";
            line += "<select class='playlistDetailsSelect arg_" + val['name'] + "' name='parent_" + val['name'] + "' id='" + ID + "'";
            line += " onChange='SubCommandChanged(this, " + configAdjustable + ", PrintArgsInputsForEditable)'";
            line += " data-url='" + val['contentListUrl'] + "'";
            line += " data-count='" + count + "'";
            line += " data-tblcommand='" + tblCommand + "'";
            line += " data-arg-type='subcommand'";
            line += ">";
            subCommandInitFunc = function () { SubCommandChanged(ID, configAdjustable, PrintArgsInputsForEditable);};
            $.each( val['contents'], function( key, v ) {
                   line += '<option value="' + v + '"';
                   if (v == dv) {
                        line += " selected";
                   }

                    if (Array.isArray(val['contents']))
                        line += ">" + v + "</option>";
                    else
                        line += ">" + key + "</option>";
             });
             line += "</select>";
        } else {
            line += val["description"] + " (" + typeName + "):</td><td>";
            line += "<input class='arg_" + val['name'] + "' id='" + ID  + "' type='text' size='40' maxlength='200' data-arg-type='" + typeName + "' ";
            if (val['type'] == "datalist" ||  (typeof val['contentListUrl'] != "undefined") || (typeof val['contents'] != "undefined")) {
                line += " list='" + ID + "_list' value='" + dv + "'";
            } else if (val['type'] == "bool") {
                if (dv == "true" || dv == "1") {
                    line += " value='true'";
                } else {
                    line += " value='false'";
                }
            } else if (val['type'] == "time") {
                line += " value='00:00:00'";
            } else if (val['type'] == "date") {
                line += " value='2020-12-25'";
            } else if ((val['type'] == "int") || (val['type'] == "float")) {
                if (dv != "") {
                    line += " value='" + dv + "'";
                } else if (typeof val['min'] != "undefined") {
                    line += " value='" + val['min'] + "'";
                }
            } else if (dv != "") {
                line += " value='" + dv + "'";
            }
            line += ">";
            if ((val['type'] == "int") || (val['type'] == "float")) {
                if (typeof val['unit'] === 'string') {
                    line += ' ' + val['unit'];
                }
            }
            line +="</input>";
            if (val['type'] == "datalist" || (typeof val['contentListUrl'] != "undefined") || (typeof val['contents'] != "undefined")) {
                line += "<datalist id='" + ID + "_list'>";
                $.each(val['contents'], function( key, v ) {
                       line += '<option value="' + v + '"';
                       line += ">" + v + "</option>";
                       });
                line += "</datalist>";
                contentListPostfix = "_list";
            }
        }

        line += "</td></tr>";
        $('#' + tblCommand).append(line);
        if (typeof val['contentListUrl'] != "undefined") {
            var selId = "#" + tblCommand + "_arg_" + count + contentListPostfix;
            $.ajax({
                   dataType: "json",
                   url: val['contentListUrl'],
                   async: false,
                   success: function(data) {
                       if (Array.isArray(data)) {
                            data.sort();
                            $.each( data, function( key, v ) {
                              var line = '<option value="' + v + '"';
                              if (v == dv) {
                                line += " selected";
                              }
                              line += ">" + v + "</option>";
                              $(selId).append(line);
                            });
                       } else {
                            $.each( data, function( key, v ) {
                                   var line = '<option value="' + key + '"';
                                   if (key == dv) {
                                        line += " selected";
                                   }
                                   line += ">" + v + "</option>";
                                   $(selId).append(line);
                            });
                       }
                   }
                   });
        }
        if (subCommandInitFunc != null) {
            subCommandInitFunc();
        }
        count = count + 1;
    };
    $.each( args, valFunc);
}

function PrintArgInputs(tblCommand, configAdjustable, args, startCount = 1) {
    var count = startCount;
    var initFuncs = [];
    var haveTime = 0;
    var haveDate = 0;
    var children = [];

    $.each( args, function( key, val ) {
         if (val['type'] == 'args')
            return;

         if ((val.hasOwnProperty('statusOnly')) &&
             (val.statusOnly == true)) {
            return;
         }

         var ID = tblCommand + "_arg_" + count;
         var line = "<tr id='" + ID + "_row' class='arg_row_" + val['name'] + "'><td>";
         var subCommandInitFunc = null;
           
         if (children.includes(val['name']))
            line += "&nbsp;&nbsp;&nbsp;&nbsp;&bull;&nbsp;";

         line += val["description"] + ":</td><td>";
         
         var dv = "";
         if (typeof val['default'] != "undefined") {
            dv = val['default'];
         }
         var contentListPostfix = "";
         if (val['type'] == "string") {
            if (typeof val['init'] === 'string') {
                initFuncs.push(val['init']);
            }

            if (typeof val['contents'] !== "undefined") {
                line += "<select class='playlistDetailsSelect arg_" + val['name'] + "' name='parent_" + val['name'] + "' id='" + ID + "'";

                if (typeof val['children'] === 'object') {
                    line += " onChange='UpdateChildVisibility();";
                    if (typeof val['onChange'] === 'string') {
                        line += ' ' + val['onChange'] + '();';
                        initFuncs.push(val['onChange']);
                    }

                    line += "'";

                    var ckeys = Object.keys(val['children']);
                    for (var c = 0; c < ckeys.length; c++) {
                        for (var x = 0; x < val['children'][ckeys[c]].length; x++) {
                            children.push(val['children'][ckeys[c]][x]);
                        }
                    }
                } else {
                    if (typeof val['onChange'] === 'string') {
                        line += " onChange='" + val['onChange'] + "();'";
                        initFuncs.push(val['onChange']);
                    }
                }

                line += ">";
                $.each( val['contents'], function( key, v ) {
                       line += '<option value="' + v + '"';
                       if (v == dv) {
                            line += " selected";
                       }

                        if (Array.isArray(val['contents']))
                            line += ">" + v + "</option>";
                        else
                            line += ">" + key + "</option>";
                });
                line += "</select>";
            } else if ((typeof val['contentListUrl'] == "undefined") &&
                       (typeof val['init'] == "undefined")) {
                line += "<input class='arg_" + val['name'] + "' id='" + ID  + "' type='text' size='40' maxlength='200' value='" + dv + "'";

                if (typeof val['placeholder'] === 'string') {
                    line += " placeholder='" + val['placeholder'] + "'";
                }

                line += "></input>";
                if (configAdjustable && val['adjustable']) {
                    line += "&nbsp;<input type='checkbox' id='" + ID + "_adjustable' class='arg_" + val['name'] + "'>Adjustable</input>";
                }
            } else {
                // Has a contentListUrl OR a init script
                line += "<select class='playlistDetailsSelect arg_" + val['name'] + "' id='" + ID + "'";

                if (typeof val['children'] === 'object') {
                    line += " onChange='UpdateChildVisibility();";
                    if (typeof val['onChange'] === 'string') {
                        line += ' ' + val['onChange'] + '();';
                        initFuncs.push(val['onChange']);
                    }

                    line += "'";

                    var ckeys = Object.keys(val['children']);
                    for (var c = 0; c < ckeys.length; c++) {
                        for (var x = 0; x < val['children'][ckeys[c]].length; x++) {
                            children.push(val['children'][ckeys[c]][x]);
                        }
                    }
                } else {
                    if (typeof val['onChange'] === 'string') {
                        line += " onChange='" + val['onChange'] + "();'";
                        initFuncs.push(val['onChange']);
                    }
                }

                line += ">";
                if (val['allowBlanks']) {
                    line += "<option value=''></option>";
                }
                line += "</select>";
            }
         } else if (val['type'] == "datalist") {
            line += "<input class='arg_" + val['name'] + "' id='" + ID  + "' type='text' size='40' maxlength='200' value='" + dv + "' list='" + ID + "_list'></input>";
            line += "<datalist id='" + ID + "_list'>";
            $.each( val['contents'], function( key, v ) {
                   line += '<option value="' + v + '"';
                   line += ">" + v + "</option>";
            })
            line += "</datalist>";
            contentListPostfix = "_list";
         } else if (val['type'] == "bool") {
            line += "<input type='checkbox' class='arg_" + val['name'] + "' id='" + ID  + "' value='true'";
             if (dv == "true" || dv == "1") {
                line += " checked";
             }
            line += "></input>";
         } else if (val['type'] == "color") {
            line += "<input type='color' class='arg_" + val['name'] + "' id='" + ID  + "' value='" + dv + "'></input>";
         } else if (val['type'] == "time") {
            haveTime = 1;
            line += "<input class='time center arg_" + val['name'] + "' id='" + ID + "' type='text' size='8' value='00:00:00'/>";
         } else if (val['type'] == "date") {
            haveDate = 1;
            line += "<input class='date center arg_" + val['name'] + "' id='" + ID + "' type='text' size='10' value='2020-01-01'/>";
         } else if ((val['type'] == "int") || (val['type'] == "float")) {
             line += "<input type='number' class='arg_" + val['name'] + "' id='" + ID  + "' min='" + val['min'] + "' max='" + val['max'] + "'";
             if (dv != "") {
                line += " value='" + dv + "'";
             } else if (typeof val['min'] != "undefined") {
                 line += " value='" + val['min'] + "'";
             }
             if (typeof val['step'] === "number") {
                line += " step='" + val['step'] + "'";
             }
             line += "></input>";

             if (typeof val['unit'] === 'string') {
                line += ' ' + val['unit'];
             }
             if (configAdjustable && val['adjustable']) {
                line += "&nbsp;<input type='checkbox' id='" + ID + "_adjustable' class='arg_" + val['name'] + "'>Adjustable</input>";
             }
         } else if (val['type'] == "subcommand") {
             line += "<select class='playlistDetailsSelect arg_" + val['name'] + "' name='parent_" + val['name'] + "' id='" + ID + "'";
             line += " onChange='SubCommandChanged(this, " + configAdjustable + ")'";
             line += " data-url='" + val['contentListUrl'] + "'";
             line += " data-count='" + count + "'";
             line += " data-tblcommand='" + tblCommand + "'";
             line += ">";
             subCommandInitFunc = function () { SubCommandChanged(ID, configAdjustable);};
             $.each( val['contents'], function( key, v ) {
                   line += '<option value="' + v + '"';
                   if (v == dv) {
                        line += " selected";
                   }

                    if (Array.isArray(val['contents']))
                        line += ">" + v + "</option>";
                    else
                        line += ">" + key + "</option>";
             });
             line += "</select>";
         }
         
         line += "</td></tr>";
         $('#' + tblCommand).append(line);
         if (typeof val['contentListUrl'] != "undefined") {
            var selId = "#" + tblCommand + "_arg_" + count + contentListPostfix;
            $.ajax({
                   dataType: "json",
                   url: val['contentListUrl'],
                   async: false,
                   success: function(data) {
                       if (Array.isArray(data)) {
                            data.sort();
                            $.each( data, function( key, v ) {
                              var line = '<option value="' + v + '"'
                              if (v == dv) {
                                   line += " selected";
                              }
                              line += ">" + v + "</option>";
                              $(selId).append(line);
                           })
                       } else {
                            $.each( data, function( key, v ) {
                              var line = '<option value="' + key + '"'
                              if (key == dv) {
                                   line += " selected";
                              }
                              line += ">" + v + "</option>";
                              $(selId).append(line);
                           })
                       }
                   }
                   });
         }
         if (subCommandInitFunc != null) {
           subCommandInitFunc();
         }
         count = count + 1;
    });

    if (haveTime) {
        InitializeTimeInputs();
    }

    if (haveDate) {
        InitializeDateInputs();
    }

    UpdateChildVisibility();

    for (var i = 0; i < initFuncs.length; i++) {
        if (typeof window[initFuncs[i]] == 'function' ) {
            window[initFuncs[i]]();
        }
    }
}


function PopulateExistingCommand(json, commandSelect, tblCommand, configAdjustable = false, argPrintFunc = PrintArgInputs) {
    if (typeof json != "undefined") {
        $('#' + commandSelect).val(json["command"]);
        CommandSelectChanged(commandSelect, tblCommand, configAdjustable, argPrintFunc);
        if (allowMultisyncCommands) {
            var to = typeof json['multisyncCommand'] ;
            
            if (typeof json['multisyncCommand'] != "undefined") {
                var val = json['multisyncCommand'];
                $("#" + tblCommand + "_multisync").prop("checked", val);
                if (val) {
                    val = json['multisyncHosts']
                    $("#" + tblCommand + "_multisyncHosts_row").show();
                    $("#" + tblCommand + "_multisyncHosts").val(val);
                }
            }
        } else {
            $("#" + tblCommand + "_multisync_row").hide();
            $("#" + tblCommand + "_multisyncHosts_row").hide();
        }
        
    
        if (typeof json['args'] != "undefined") {
            var count = 1;
            $.each( json['args'], function( key, v ) {
                   var inp =  $("#" + tblCommand + "_arg_" + count);
                   if (inp.attr('type') == 'checkbox') {
                       var checked = false;
                       if (v == "true" || v == "1") {
                           checked = true;
                       }
                       inp.prop( "checked", checked);
                   } else {
                       inp.val(v);
                   }
                   
                   if (inp.data('url') != null) {
                      SubCommandChanged(tblCommand + "_arg_" + count, configAdjustable, argPrintFunc);
                   }
                   
                   if (typeof json['adjustable'] != "undefined"
                       && typeof json['adjustable'][count] != "undefined") {
                       $("#" + tblCommand + "_arg_" + count + "_adjustable").prop("checked", true);
                   }
                   count = count + 1;
            });
        }
    }
}
