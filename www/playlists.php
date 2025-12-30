<!DOCTYPE html>
<html lang="en">

<head>
    <?php
    include 'common/htmlMeta.inc';
    require_once 'config.php';
    require_once 'common.php';

    //ini_set('display_errors', 'On');
    error_reporting(E_ALL);

    include 'common/menuHead.inc'; ?>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <title>
        <? echo htmlspecialchars($pageTitle); ?>
    </title>
    <?
    if (isset($_GET['playlist'])) {
        ?>
        <script>
            var initialPlaylist = "<? echo htmlspecialchars($_GET['playlist']); ?>";
        </script>
        <?
    }
    ?>

    <script>
        console.log('Playlists.php loaded - version 2025-12-29-v2');
        function CopyPlaylist() {
            var name = $('#txtPlaylistName').val();

            DoModalDialog({
                id: "CopyPlaylistDialog",
                title: "Copy Playlist",
                body: '<span>Enter name for new playlist:</span><input id="newPlaylistName" name="newPlaylistName" type="text" style="z-index:1000; width: 95%" class="newPlaylistName" value="New Playlist Name">',
                class: "modal-m",
                backdrop: true,
                keyboard: true,
                buttons: {
                    "Copy": function () {
                        var new_playlist_name = $("#CopyPlaylistDialog").find(".newPlaylistName").val();

                        if (new_playlist_name === "") {
                            DialogError('No name given', 'The playlist name cannot be empty.');
                            return;
                        }

                        if (name == new_playlist_name) {
                            DialogError('Error, same name given', 'Identical name given.');
                            return;
                        }

                        if (!SavePlaylistAs(new_playlist_name, '', ''))
                            return;

                        PopulateLists({
                            onPlaylistArrayLoaded: function () {
                                $('#playlistEditor').removeClass('hasPlaylistDetailsLoaded');
                                onPlaylistArrayLoaded();
                            }
                        });

                        SetPlaylistName(new_playlist_name);
                        CloseModalDialog("CopyPlaylistDialog");
                    },
                    "Cancel": function () {
                        CloseModalDialog("CopyPlaylistDialog");
                    }
                },
                open: function (event, ui) {
                    //Generate a name for the new playlist
                    $("#CopyPlaylistDialog").find(".newPlaylistName").val(name + " - Copy");
                },
                close: function () {
                    $("#CopyPlaylistDialog").find(".newPlaylistName").val("New Playlist Name");
                }
            })
        }

        function RenamePlaylist() {
            var name = $('#txtPlaylistName').val();
            DoModalDialog({
                id: "RenamePlaylistDialog",
                title: "Rename Playlist",
                body: '<span>Enter name for new playlist:</span><input id="newPlaylistName" name="newPlaylistName" type="text" style="z-index:10000; width: 95%" class="newPlaylistName" value="New Playlist Name">',
                class: "modal-m",
                backdrop: true,
                keyboard: true,
                buttons: {
                    "Rename": function () {
                        var new_playlist_name = $("#RenamePlaylistDialog").find(".newPlaylistName").val();

                        if (name == new_playlist_name) {
                            DialogError('Error, same name given', 'Identical name given.');
                            return;
                        }

                        if (!SavePlaylistAs(new_playlist_name, '', ''))
                            return;

                        DeleteNamedPlaylist(name);
                        PopulateLists();

                        SetPlaylistName(new_playlist_name);
                        CloseModalDialog("RenamePlaylistDialog");
                        location.reload();
                    },
                    Cancel: function () {
                        CloseModalDialog("RenamePlaylistDialog");
                    }
                },
                open: function (event, ui) {
                    //Generate a name for the new playlist
                    $("#RenamePlaylistDialog").find(".newPlaylistName").val(name);
                },
                close: function () {
                    $("#RenamePlaylistDialog").find(".newPlaylistName").val("New Playlist Name");
                }
            });
        }

        function DeletePlaylist(options) {
            var name = $('#txtPlaylistName').val();

            DeleteNamedPlaylist(name, options);
        }

        function DeleteNamedPlaylist(name, options) {
            var postDataString = "";
            $.ajax({
                dataType: "json",
                url: "api/playlist/" + name,
                type: "DELETE",
                async: false,
                success: function (data) {
                    PopulateLists(options);
                    $.jGrowl("Playlist Deleted", {
                        themeState: 'success'
                    });
                },
                error: function (xhr, status, error) {
                    DialogError('Error Deleting Playlist', "Error deleting '" + name + "' playlist: " + error);
                }
            });
        }

        function formatTime(seconds) {
            if (!seconds) return '0:00';
            var mins = Math.floor(seconds / 60);
            var secs = Math.floor(seconds % 60);
            return mins + ':' + (secs < 10 ? '0' : '') + secs;
        }

        function ExportPlaylist() {
            var name = $('#txtPlaylistName').val();

            DoModalDialog({
                id: "ExportPlaylistDialog",
                title: "Export Playlist",
                body: '<div class="form-group"><label for="exportFormat">Select Export Format:</label><select id="exportFormat" class="form-control"><option value="json">JSON</option><option value="txt">Text (TXT)</option><option value="xls">Excel (CSV)</option></select></div>',
                class: "modal-m",
                backdrop: true,
                keyboard: true,
                buttons: {
                    "Export": function () {
                        var format = $("#exportFormat").val();

                        $.ajax({
                            dataType: "json",
                            url: "api/playlist/" + name,
                            type: "GET",
                            success: function (data) {
                                var content = "";
                                var filename = name;
                                var mimeType = "";

                                if (format === "json") {
                                    content = JSON.stringify(data, null, 2);
                                    filename += ".json";
                                    mimeType = "application/json";
                                } else if (format === "txt") {
                                    content = "Playlist: " + data.name + "\n";
                                    content += "Description: " + (data.desc || "") + "\n";
                                    content += "Random: " + (data.random || "0") + "\n";
                                    content += "\nEntries:\n";
                                    content += "==========\n\n";

                                    var totalTime = 0;
                                    var itemNumber = 0;

                                    var sectionNames = ["LeadIn", "MainPlaylist", "LeadOut"];
                                    var sectionLabels = ["Lead In", "Main", "Lead Out"];

                                    for (var s = 0; s < sectionNames.length; s++) {
                                        var rows = $('#tblPlaylist' + sectionNames[s] + ' tr.playlistRow');
                                        if (rows.length > 0) {
                                            content += "--- " + sectionLabels[s] + " ---\n";
                                            rows.each(function () {
                                                itemNumber++;
                                                content += itemNumber + ". ";

                                                var duration = parseFloat($(this).find('.psiDurationSeconds').html()) || 0;
                                                totalTime += duration;

                                                var type = $(this).find('.entryType').html();
                                                var name = "";

                                                if (type === "both" || type === "media") {
                                                    var seqName = $(this).find('.field_sequenceName').text();
                                                    var mediaName = $(this).find('.field_mediaName').text();
                                                    name = seqName || mediaName;
                                                    content += "Media: " + name;
                                                } else if (type === "sequence") {
                                                    name = $(this).find('.field_sequenceName').text();
                                                    content += "Sequence: " + name;
                                                } else if (type === "pause") {
                                                    content += "Pause: " + formatTime(duration);
                                                } else if (type === "playlist") {
                                                    name = $(this).find('.field_name').text();
                                                    content += "Playlist: " + name;
                                                } else if (type === "command") {
                                                    name = $(this).find('.field_command').text();
                                                    content += "Command: " + name;
                                                } else if (type === "script") {
                                                    var scriptName = $(this).find('.field_scriptName').text();
                                                    var scriptArgs = $(this).find('.field_scriptArgs').text();
                                                    content += "Script: " + scriptName;
                                                    if (scriptArgs) {
                                                        content += " (" + scriptArgs + ")";
                                                    }
                                                } else if (type === "plugin") {
                                                    name = $(this).find('.field_pluginHost').text();
                                                    content += "Plugin: " + name;
                                                } else {
                                                    content += type || "Unknown";
                                                }

                                                if (duration > 0 && type !== "pause") {
                                                    content += " (" + formatTime(duration) + ")";
                                                }
                                                content += "\n";
                                            });
                                            content += "\n";
                                        }
                                    }

                                    content += "Total Time: " + formatTime(totalTime) + "\n";
                                    filename += ".txt";
                                    mimeType = "text/plain";
                                } else if (format === "xls") {
                                    content = "#,Section,Type,Name/Details,Duration\n";

                                    var totalTime = 0;
                                    var itemNumber = 0;

                                    var sectionNames = ["LeadIn", "MainPlaylist", "LeadOut"];
                                    var sectionLabels = ["Lead In", "Main", "Lead Out"];

                                    for (var s = 0; s < sectionNames.length; s++) {
                                        var rows = $('#tblPlaylist' + sectionNames[s] + ' tr.playlistRow');
                                        rows.each(function () {
                                            itemNumber++;
                                            var duration = parseFloat($(this).find('.psiDurationSeconds').html()) || 0;
                                            totalTime += duration;

                                            var type = $(this).find('.entryType').html();
                                            var name = "";

                                            if (type === "both" || type === "media") {
                                                var seqName = $(this).find('.field_sequenceName').text();
                                                var mediaName = $(this).find('.field_mediaName').text();
                                                name = seqName || mediaName;
                                            } else if (type === "sequence") {
                                                name = $(this).find('.field_sequenceName').text();
                                            } else if (type === "pause") {
                                                name = "Pause";
                                            } else if (type === "playlist") {
                                                name = $(this).find('.field_name').text();
                                            } else if (type === "command") {
                                                name = $(this).find('.field_command').text();
                                            } else if (type === "script") {
                                                var scriptName = $(this).find('.field_scriptName').text();
                                                var scriptArgs = $(this).find('.field_scriptArgs').text();
                                                name = scriptName;
                                                if (scriptArgs) {
                                                    name += " (" + scriptArgs + ")";
                                                }
                                            } else if (type === "plugin") {
                                                name = $(this).find('.field_pluginHost').text();
                                            }

                                            var row = itemNumber + ",";
                                            row += '"' + sectionLabels[s] + '","' + type + '","' + name + '","' + formatTime(duration) + '"\n';
                                            content += row;
                                        });
                                    }

                                    content += "Total,,," + formatTime(totalTime) + "\n";
                                    filename += ".csv";
                                    mimeType = "text/csv";
                                }

                                // Create download
                                var blob = new Blob([content], { type: mimeType });
                                var link = document.createElement('a');
                                link.href = window.URL.createObjectURL(blob);
                                link.download = filename;
                                link.click();

                                $.jGrowl("Playlist exported successfully", {
                                    themeState: 'success'
                                });

                                CloseModalDialog("ExportPlaylistDialog");
                            },
                            error: function (xhr, status, error) {
                                DialogError('Error Exporting Playlist', "Error exporting '" + name + "' playlist: " + error);
                            }
                        });
                    },
                    "Cancel": function () {
                        CloseModalDialog("ExportPlaylistDialog");
                    }
                }
            });
        }


        function LoadInitialPlaylist() {
            $('#playlistSelect').val(initialPlaylist).trigger('change');
        }

        function handleDeleteButtonClick(name = "") {
            if (name == "") {
                name = $('#txtPlaylistName').val();
            }
            DoModalDialog({
                id: "DeletePlaylistDialog",
                title: "Delete Playlist?",
                body: 'Are you sure you want to delete the playlist `' + name + '`?',
                class: "modal-sm",
                backdrop: true,
                keyboard: false,
                buttons: {
                    "Delete": function () {
                        DeleteNamedPlaylist(name, {
                            onPlaylistArrayLoaded: function () {
                                $('#playlistEditor').removeClass('hasPlaylistDetailsLoaded');
                                onPlaylistArrayLoaded();
                            }
                        });
                        location.reload();
                        CloseModalDialog("DeletePlaylistDialog");
                    },
                    "Cancel": function () {
                        CloseModalDialog("DeletePlaylistDialog");
                    }
                }
            })
        }

        function onPlaylistArrayLoaded() {
            $('.playlistSelectBody').html('');
            $('.playlistSelectCount').html(playListArray.length);
            $.each(playListArray, function (i, playList) {
                var $playlistCol = $('<div class="card-group col-md-4"/>');
                var $playlistCard = $('<div class="card has-shadow playlistCard buttonActionsParent"/>');
                var $playlistName = String(playList.name);
                var $playlistDescription = String(playList.description);
                var $playlistDuration = String(SecondsToHuman(playList.total_duration));
                var $playlistItems = String(playList.total_items);
                var $playlistClass = playList.valid ? 'class="card-title"' :
                    'class="card-title playlist-warning" title="' + playList.messages.join(' ') + '"';
                var $playlistCardHeading = $('<h3 ' + $playlistClass + '>' + $playlistName + '</h3>');
                var $playlistCardDescription = $('<div class="text-center"/><p class="card-text mb-2 text-muted">' +
                    $playlistDescription + '</p></div>');
                var $playlistCardDuration = $('<div class="text-left"/><p class="card-text mb-2 text-muted"><span class="fw-bold">Total Duration: </span>' + $playlistDuration + '</p></div>');
                var $playlistCardItems = $('<div class="text-left"/><p class="card-text mb-2 text-muted"><span class="fw-bold">Total Items: </span>' + $playlistItems + '</p></div>');
                var $playlistActions = $("<div class='buttonActions' />");
                var $playlistEditButton = $(
                    '<button class="playlistCardEditButton circularButton circularButton-sm circularEditButton">Edit</button>'
                );
                var $playlistDelete = $(
                    "<button class='circularButton circularButton-sm circularDeleteButton'>Delete</button>");
                $playlistCard.on("click", function (e) {
                    $('#playlistSelect').val($playlistName).trigger('change');
                    e.stopPropagation();
                })
                $playlistDelete.on("click", function (e) {
                    handleDeleteButtonClick($playlistName);
                    e.stopPropagation();
                });
                $playlistActions.append($playlistEditButton);
                $playlistActions.append($playlistDelete);
                $playlistCol.append($playlistCard);
                $playlistCard.append($playlistCardHeading);
                $playlistCard.append($playlistCardDescription);
                $playlistCard.append($playlistActions);
                $playlistCard.append($playlistCardDuration);
                $playlistCard.append($playlistCardItems);

                $('.playlistSelectBody').append($playlistCol)
            })

        }

        $(function () {
            $('#playlistSelect').on('change', function () {
                $('.playlistEditorHeaderTitle').html($(this).val());
            })

            $('.playlistAddNewBtn').on("click", function () {
                $('#txtAddPlaylistName').val(""); // BUG #1391
                DoModalDialog({
                    id: "AddPlaylistDialog",
                    backdrop: true,
                    keyboard: true,
                    title: "Add a New Playlist",
                    body: $("#playlistAdd"),
                    class: "modal-m",
                    focus: "txtAddPlaylistName",
                    buttons: {
                        "Add Playlist": {
                            click: function () {
                                if ($("#txtAddPlaylistName").val() === "") {
                                    DialogError('No name given', 'The playlist name cannot be empty.');
                                    return;
                                }
                                //check if playlist name already in use
                                var playlistName = $("#txtAddPlaylistName").val();
                                var existingPlaylist = null;
                                for (var i = 0; i < playListArray.length; i++) {
                                    if (playListArray[i].name === playlistName) {
                                        existingPlaylist = playListArray[i];
                                        break;
                                    }
                                }
                                if (existingPlaylist) {
                                    DialogError('Playlist Name in Use', 'The playlist name already exists.');
                                    return;
                                }

                                SavePlaylistAs(
                                    $("#txtAddPlaylistName").val(),
                                    {
                                        desc: $("#txtAddPlaylistDesc").val(),
                                        random: $("#randomizeAddPlaylist").val(),
                                        empty: true
                                    },
                                    function () {
                                        onPlaylistArrayLoaded();
                                        $('#playlistSelect').val($(
                                            "#txtAddPlaylistName").val()).trigger(
                                                'change');
                                        LoadPlaylistDetails($("#txtAddPlaylistName").val());
                                        //Set Page header to new playlist name
                                        $('.playlistEditorHeaderTitle').html($("#txtAddPlaylistName").val());

                                        CloseModalDialog("AddPlaylistDialog");
                                    }
                                );
                            },
                            class: 'btn-success'
                        },
                        "Close": function () {
                            CloseModalDialog("AddPlaylistDialog");
                        }
                    }
                });
            })

            $('.editPlaylistBtn').on("click", function () {
                $("#verbosePlaylistItemDetailsRow .printSettingLabelCol").removeClass("col-xxxl-2");
                $("#verbosePlaylistItemDetailsRow .printSettingLabelCol").removeClass("col-lg-3");
                $("#verbosePlaylistItemDetailsRow .printSettingLabelCol").removeClass("col-md");
                $("#verbosePlaylistItemDetailsRow .printSettingLabelCol").addClass("col-md-4");
                DoModalDialog({
                    id: "EditPlaylistDetailsDialog",
                    backdrop: true,
                    keyboard: true,
                    title: "Edit Playlist Details",
                    body: $("#playlistEdit"),
                    class: "modal-lg",
                    buttons: {
                        "Save": {
                            click: function () {
                                SavePlaylist('', function () {
                                    CloseModalDialog("EditPlaylistDetailsDialog");
                                });
                            },
                            class: 'btn-success'
                        },
                        "Close": function () {
                            CloseModalDialog("EditPlaylistDetailsDialog");
                        }
                    }
                });
            })
            $('.playlistEditorHeaderTitleEditButton').on("click", function () {
                RenamePlaylist();
            })
            $('.playlistEntriesAddNewBtn').on("click", function () {
                // Refresh dropdown to pick up any newly added sequences (only when filter is active)
                if ($('#filterUsedSequences').is(':checked')) {
                    PlaylistTypeChanged();
                    // Re-trigger auto-select matching after rebuilding the form
                    SequenceChanged();
                }

                var playlistEntriesAddNewFooter = $('<div class="modal-actions"/>');
                //  <a href="#" onclick="AddPlaylistEntry(2);" class="dropdown-item" value="Insert Before">Insert Before</a>
                // <a href="#" onclick="AddPlaylistEntry(3);" class="dropdown-item" value="Insert After">Insert After</a>
                var playlistEntriesAddNewAddBtn = $('<button class="buttons btn-success">Add</button>').on(
                    "click",
                    function () {
                        AddPlaylistEntry();
                        $('#playlistEntryProperties').fppDialog('close');
                    })
                var playlistEntriesAddNewInsertBeforeBtn = $(
                    '<a href="#" class="dropdown-item" value="Insert Before">Before Selection</a>').on(
                        "click",
                        function () {
                            AddPlaylistEntry(2);
                            $('#playlistEntryProperties').fppDialog('close');
                        })
                var playlistEntriesAddNewInsertAfterBtn = $(
                    '<a href="#" class="dropdown-item" value="Insert After">After Selection</a>').on(
                        "click",
                        function () {
                            AddPlaylistEntry(3);
                            $('#playlistEntryProperties').fppDialog('close');
                        })
                var playlistEntriesAddNewInsertBtn = $(
                    '<div class="dropdown"><button class="btn btn-outline-success dropdown-toggle" type="button" id="dropdownMenuButton" data-bs-toggle="dropdown" aria-haspopup="true" aria-expanded="false">Insert</button></div>'
                );
                var playlistEntriesAddNewInsertBtnMenu = $(
                    '<div class="dropdown-menu" aria-labelledby="dropdownMenuButton"></div>').append(
                        playlistEntriesAddNewInsertBeforeBtn).append(playlistEntriesAddNewInsertAfterBtn);
                playlistEntriesAddNewInsertBtn.append(playlistEntriesAddNewInsertBtnMenu);

                playlistEntriesAddNewFooter.append(playlistEntriesAddNewInsertBtn).append(
                    playlistEntriesAddNewAddBtn);
                $('#playlistEntryProperties').fppDialog({
                    title: 'New Entry',
                    width: 800,
                    footer: playlistEntriesAddNewFooter
                });
            });

            $('.savePlaylistBtn').on("click", function () {
                SavePlaylist($('#txtPlaylistName').val())
            })
            $('.playlistEditorBackButton').on("click", function () {
                if (isCurrentPlaylistModified()) {
                    if (confirm("Leave without saving changes?") == false) {
                        return;
                    }
                }

                // Clear playlist DOM tables to prevent data being put in new playlists
                $('#tblPlaylistLeadIn').html("<tr id='tblPlaylistLeadInPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>");
                $('#tblPlaylistMainPlaylist').html("<tr id='tblPlaylistMainPlaylistPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>");
                $('#tblPlaylistLeadOut').html("<tr id='tblPlaylistLeadOutPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>");

                //logic to reload window playlist details to pick up changes
                PopulateLists({
                    onPlaylistArrayLoaded: function () {
                        $('#playlistEditor').removeClass('hasPlaylistDetailsLoaded');
                        onPlaylistArrayLoaded();
                    }
                });

                // Update history state when clicking back button
                history.pushState({ view: 'list' }, '', window.location.href);
            })

            PopulateLists({
                onPlaylistArrayLoaded: onPlaylistArrayLoaded
            });

            if (typeof initialPlaylist !== 'undefined') {
                LoadInitialPlaylist();
            } else {
                $('#playlistSelect').prepend('<option value="" disabled selected>Select a Playlist</option>');
            }

            // Handle browser back/forward button
            window.addEventListener('popstate', function (e) {
                if (e.state) {
                    if (e.state.view === 'list') {
                        // Go back to list view
                        if ($('#playlistEditor').hasClass('hasPlaylistDetailsLoaded')) {
                            // Clear playlist tables to prevent stale data
                            $('#tblPlaylistLeadIn').html("<tr id='tblPlaylistLeadInPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>");
                            $('#tblPlaylistMainPlaylist').html("<tr id='tblPlaylistMainPlaylistPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>");
                            $('#tblPlaylistLeadOut').html("<tr id='tblPlaylistLeadOutPlaceHolder' class='unselectable'><td>&nbsp;</td></tr>");
                            $('#playlistEditor').removeClass('hasPlaylistDetailsLoaded');
                            PopulateLists({
                                onPlaylistArrayLoaded: onPlaylistArrayLoaded
                            });
                        }
                    } else if (e.state.view === 'editor' && e.state.playlist) {
                        // Go forward to editor view
                        if (!$('#playlistEditor').hasClass('hasPlaylistDetailsLoaded')) {
                            $('#playlistSelect').val(e.state.playlist).trigger('change');
                        }
                    }
                }
            });

            // Set initial history state
            history.replaceState({ view: 'list' }, '', location.href);

        })
    </script>
    <style>
        .ui-resizable-se {
            bottom: 20px;
            right: 25px;
        }
    </style>
</head>

<body>
    <div id="bodyWrapper">
        <?php
        $activeParentMenuItem = 'content';
        include 'menu.inc'; ?>
        <div class="mainContainer">
            <h1 class="title">Playlists</h1>
            <div class='pageContent'>

                <div id="playlistEditor">
                    <div class="playlistSelectContainer">
                        <div class="playlistSelectHeader">

                            <div class="row">
                                <div class="col-md">
                                    <h2>Your Playlists <div class="badge badge-light ms-1 playlistSelectCount"></div>
                                    </h2>
                                    <select id='playlistSelect'
                                        class="hidden form-control form-control-lg form-control-rounded has-shadow"
                                        onChange='EditPlaylist();'>
                                    </select>
                                </div>
                                <div class="col-md-auto ms-lg-auto">
                                    <button
                                        class="playlistAddNewBtn buttons btn-outline-success btn-rounded  btn-icon-add"><i
                                            class="fas fa-plus"></i> New Playlist
                                    </button>
                                </div>
                            </div>

                        </div>

                        <div class="playlistSelectBody row">
                            <div class="col-md-4 skeleton-loader">
                                <div class="sk-block sk-card"></div>
                            </div>
                            <div class="col-md-4 skeleton-loader">
                                <div class="sk-block sk-card"></div>
                            </div>
                            <div class="col-md-4 skeleton-loader">
                                <div class="sk-block sk-card"></div>
                            </div>
                            <div class="col-md-4 skeleton-loader">
                                <div class="sk-block sk-card"></div>
                            </div>

                        </div>
                    </div>

                    <div class="playlistCreateContainer hidden">
                        <b>Enter new playlist name</b><br />
                        <input id="txtNewPlaylistName" class="default-value form-control" type="text"
                            value="Enter Playlist Name" size="40" maxlength="64" onChange='CreateNewPlaylist();' />

                    </div>
                    <div class='clear'></div>

                    <a name='editor'></a>
                    <div class="playlistEditorContainer">
                        <div class="playlistEditorHeader">
                            <div>
                                <button class="playlistEditorBackButton has-shadow">
                                    <i class="fas fa-chevron-left"></i><i class="fas fa-grip-horizontal"></i>
                                </button>
                            </div>

                            <div class="playlistEditorHeaderTitleWrapper">
                                <h2 class="playlistEditorHeaderTitle">
                                </h2>
                                <div class="playlistEditorHeaderTitleEditButtonWrapper">
                                    <button
                                        class="circularButton circularEditButton playlistEditorHeaderTitleEditButton">Edit</button>
                                </div>
                            </div>
                            <div class="form-actions playlistEditorHeaderActions">
                                <button class="buttons editPlaylistBtn">
                                    <i class="fas fa-cog"></i>
                                </button>
                                <div class="dropdown pe-2">
                                    <button class="buttons dropdown-toggle playlistEditButton" type="button"
                                        id="playlistEditMoreButton" data-bs-toggle="dropdown" aria-haspopup="true"
                                        aria-expanded="false">
                                        Playlist Actions
                                    </button>
                                    <div class="dropdown-menu playlistEditMoreButtonMenu"
                                        aria-labelledby="playlistEditMoreButton">

                                        <a href="#" value="Copy" onclick="CopyPlaylist();" class="dropdown-item">Copy
                                            Playlist</a>
                                        <a href="#" value="Rename" onclick="RenamePlaylist();"
                                            class="dropdown-item ">Rename Playlist</a>
                                        <a href="#" value="Randomize" onclick="RandomizePlaylistEntries();"
                                            class="dropdown-item ">Randomize Playlist</a>
                                        <a href="#" value="Export" onclick="ExportPlaylist();"
                                            class="dropdown-item ">Export Playlist</a>
                                        <a href="#" value="Reset" onclick="EditPlaylist();" class="dropdown-item ">Reset
                                            Playlist</a>
                                        <a href="#" value="Delete" onclick="handleDeleteButtonClick();"
                                            class="dropdown-item ">Delete Playlist</a>
                                    </div>
                                </div>
                                <button class="buttons btn-success savePlaylistBtn">
                                    Save Playlist <i class="fa fa-exclamation-triangle savePlaylistBtnHasChange"
                                        aria-hidden="true"></i>
                                </button>
                            </div>

                        </div>
                        <div class="playlistEditorPanel backdrop">
                            <? include_once 'playlistEditor.php'; ?>
                        </div>
                    </div>


                </div>
                <div class="playlistAdd hidden" id="playlistAdd">
                    <div class="form-group">
                        <label for="txtAddPlaylistName">Playlist Name:</label>
                        <input type="text" id="txtAddPlaylistName" class="pl_title form-control" />
                    </div>
                    <div class="form-group">
                        <label for="txtAddPlaylistDesc">Playlist Description:</label>
                        <input type="text" id="txtAddPlaylistDesc" class="pl_description form-control" />
                    </div>
                    <div class="form-group flow">
                        <label for="randomizeAddPlaylist">Randomize:</label>
                        <select id='randomizeAddPlaylist' class="form-control">
                            <option value='0'>Off</option>
                            <option value='1'>Once per load</option>
                            <option value='2'>Every iteration</option>
                        </select>
                    </div>
                </div>
                <div class="playlistEdit hidden" id="playlistEdit">
                    <div class="row">
                        <div class="col-4"><label for="txtPlaylistName">Playlist Name:</label></div>
                        <div class="col-auto"><input type="text" id="txtPlaylistName" class="pl_title form-control" />
                        </div>
                    </div>
                    <div class="row">
                        <div class="col-4"><label for="txtPlaylistDesc">Playlist Description:</label></div>
                        <div class="col-8"><input type="text" id="txtPlaylistDesc"
                                class="pl_description form-control" /></div>
                    </div>
                    <div class="row">
                        <div class="col-4"><label for="randomizePlaylist">Randomize:</label></div>
                        <div class="col-auto"><select id='randomizePlaylist' class="form-control">
                                <option value='0'>Off</option>
                                <option value='1'>Once per load</option>
                                <option value='2'>Every iteration</option>
                            </select></div>
                    </div>
                    <? PrintSetting('verbosePlaylistItemDetails', 'VerbosePlaylistItemDetailsToggled'); ?>
                </div>

            </div>
        </div>
        <?php include 'common/footer.inc'; ?>
    </div>
</body>

</html>