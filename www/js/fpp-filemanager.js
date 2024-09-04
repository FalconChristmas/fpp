function GetFiles (dir) {
	$.ajax({
		dataType: 'json',
		url: 'api/files/' + dir,
		success: function (data) {
			let i = 0;

			if (data.files.length > 0) {
				$('#tbl' + dir)
					.find('tbody')
					.html('');
			} else {
				$('#tbl' + dir)
					.find('tbody')
					.html(
						"<tr class='unselectableRow'><td colspan=8 align='center'>No files found.</td></tr>"
					);
			}
			data.files.forEach(function (f) {
				var detail = f.sizeHuman;
				if ('playtimeSeconds' in f) {
					detail = f.playtimeSeconds;
				}

				var thumbSize = 0;
				if (
					settings.hasOwnProperty('fileManagerThumbnailSize') &&
					settings['fileManagerThumbnailSize'] > 0
				) {
					thumbSize = settings['fileManagerThumbnailSize'];
				}

				var tableRow = '';
				if (dir == 'Images' && thumbSize > 0) {
					if (parseInt(f.sizeBytes) > 0) {
						tableRow =
							"<tr class='fileDetails' id='fileDetail_" +
							i +
							"'><td class ='filenameColumn fileName'>" +
							f.name.replace(/&/g, '&amp;').replace(/</g, '&lt;') +
							"</td><td class='fileExtraInfo'>" +
							detail +
							"</td><td class ='fileTime'>" +
							f.mtime +
							"</td><td><img style='display: block; max-width: " +
							thumbSize +
							'px; max-height: ' +
							thumbSize +
							"px; width: auto; height: auto;' src='api/file/" +
							dir +
							'/' +
							f.name +
							"' onClick=\"ViewImage('" +
							f.name +
							'\');" /></td></tr>';
					} else {
						tableRow =
							"<tr class='fileDetails unselectableRow' id='fileDetail_" +
							i +
							"'><td class ='filenameColumn fileName'>" +
							f.name.replace(/&/g, '&amp;').replace(/</g, '&lt;') +
							"</td><td class='fileExtraInfo'>" +
							detail +
							"</td><td class ='fileTime'>" +
							f.mtime +
							'</td><td>Empty Subdir</td></tr>';
					}
				} else {
					tableRow =
						"<tr class='fileDetails' id='fileDetail_" +
						i +
						"'><td class ='filenameColumn fileName'>" +
						f.name.replace(/&/g, '&amp;').replace(/</g, '&lt;') +
						"</td><td class='fileExtraInfo'>" +
						detail +
						"</td><td class ='fileTime'>" +
						f.mtime +
						'</td></tr>';
				}

				$('#tbl' + dir)
					.find('tbody')
					.append(tableRow);
				++i;
			});
		},
		error: function (x, t, e) {
			DialogError(
				'Load Files',
				'Error loading list of files in ' +
					dir +
					' directory' +
					show_details([x, t, e])
			);
		},
		complete: function () {
			SetupTableSorter('tbl' + dir);
			UpdateFileCount(dir);
		}
	});
}

function GetAllFiles () {
	GetFiles('Sequences');
	GetFiles('Music');
	GetFiles('Videos');
	GetFiles('Images');
	GetFiles('Effects');
	GetFiles('Scripts');
	GetFiles('Logs');
	GetFiles('Uploads');
	GetFiles('Crashes');
	GetFiles('Backups');

	pluginFileExtensions.forEach(ext => {
		GetFiles(ext);
	});

	if (document.readyState === 'loading') {
		document.addEventListener('DOMContentLoaded', AddStickyHeaderWidget);
	} else {
		AddStickyHeaderWidget();
	}
}

function GetSequenceInfo (file) {
	$('#fileText').html('Getting Sequence Info.');

	$.get('api/sequence/' + file + '/meta', function (data) {
		DoModalDialog({
			id: 'SequenceViewer',
			title: 'Sequence Info',
			class: 'modal-lg modal-dialog-scrollable',
			body: '<pre>' + syntaxHighlight(JSON.stringify(data, null, 2)) + '</pre>',
			keyboard: true,
			backdrop: true,
			buttons: {
				Close: function () {
					CloseModalDialog('SequenceViewer');
				}
			}
		});
	});
}

function UpdateFileCount ($dir) {
	$('#fileCount_' + $dir)[0].innerText = $('#tbl' + $dir + ' tbody tr')
		.not('.unselectableRow')
		.not('.filtered').length;
	if ($('#tbl' + $dir + ' tbody tr.filtered').length > 0) {
		//is filtered
		$('#div' + $dir + ' .fileCountlabelHeading')[0].innerHTML = innerHtml =
			'<span class="filtered">Filtered items:<span>';
		$('#fileCount_' + $dir)
			.removeClass('text-bg-secondary')
			.addClass('text-bg-success');
	} else {
		//not filtered
		$('#div' + $dir + ' .fileCountlabelHeading')[0].innerHTML =
			'<span class="">Items:<span>';
		$('#fileCount_' + $dir)
			.removeClass('text-bg-success')
			.addClass('text-bg-secondary');
	}
}

function FileManagerFilterToggled () {
	value = settings.fileManagerTableFilter == '1' ? true : false;
	var $t = $('#fileManager').find('table');
	if (!value) {
		//logic for hidden filter
		$('.tablesorter-filter-row').addClass('hideme');
		$('table').trigger('filterReset');

		if ($t.length) {
			var loopSize = $t.length;
			for (let i = 0; i < loopSize; i += 1) {
				var tableName = $t[i].id;
				var fileType = tableName.substring(3);
				if ($t[i].config) {
					$($t)[i].config.widgetOptions.filter_hideFilters = true; //set current instance
					eval(
						'tablesorterOptions_' + fileType
					).widgetOptions.filter_hideFilters = true; //set config option for when tabs regenerate
				}
			}
		}
	} else {
		//logic for filter shown
		$('.tablesorter-filter-row').removeClass('hideme');
		if ($t.length) {
			var loopSize = $t.length;
			for (let i = 0; i < loopSize; i += 1) {
				var tableName = $t[i].id;
				var fileType = tableName.substring(3);
				if ($t[i].config) {
					$($t)[i].config.widgetOptions.filter_hideFilters = false; //set current instance
					eval(
						'tablesorterOptions_' + fileType
					).widgetOptions.filter_hideFilters = false; //set config option for when tabs regenerate
				}
			}
		}
	}
}

function GetVideoInfo (file) {
	$('#fileText').html('Getting Video Info.');

	$.get('api/media/' + file + '/meta', function (data) {
		DoModalDialog({
			id: 'VideoViewer',
			title: 'Video Info',
			class: 'modal-lg modal-dialog-scrollable',
			body: '<pre>' + syntaxHighlight(JSON.stringify(data, null, 2)) + '</pre>',
			keyboard: true,
			backdrop: true,
			buttons: {
				Close: function () {
					CloseModalDialog('VideoViewer');
				}
			}
		});
	});
}
function mp3GainProgressDialogDone () {
	$('#mp3GainProgressCloseButton').prop('disabled', false);
	EnableModalDialogCloseButton('mp3GainProgress');
}
function ButtonHandler (table, button) {
	var selectedCount = $('#tbl' + table + ' tr.selectedEntry').length;
	var filename = '';
	var filenames = [];
	if (selectedCount == 1) {
		filename = $('#tbl' + table + ' tr.selectedEntry')
			.find('td:first')
			.text();
	}

	if (button == 'play' || button == 'playHere') {
		if (selectedCount == 1) {
			PlayPlaylist(filename, button == 'play' ? 1 : 0);
		} else {
			DialogError(
				'Error',
				'Error, unable to play multiple sequences at the same time.'
			);
		}
	} else if (button == 'download') {
		var files = [];
		$('#tbl' + table + ' tr.selectedEntry').each(function () {
			files.push($(this).find('td:first').text());
		});
		DownloadFiles(table, files);
	} else if (button == 'rename') {
		if (selectedCount == 1) {
			RenameFile(table, filename);
		} else {
			DialogError(
				'Error',
				'Error, unable to rename multiple files at the same time.'
			);
		}
	} else if (button == 'copyFile') {
		if (selectedCount == 1) {
			CopyFile(table, filename);
		} else {
			DialogError(
				'Error',
				'Error, unable to copy multiple files at the same time.'
			);
		}
	} else if (button == 'delete') {
		$('#tbl' + table + ' tr.selectedEntry').each(function () {
			DeleteFile(table, $(this), $(this).find('td:first').text());
		});
	} else if (button == 'editScript') {
		if (selectedCount == 1) {
			EditScript(filename);
		} else {
			DialogError(
				'Error',
				'Error, unable to edit multiple files at the same time.'
			);
		}
	} else if (button == 'playInBrowser') {
		if (selectedCount == 1) {
			PlayFileInBrowser(table, filename);
		} else {
			DialogError(
				'Error',
				'Error, unable to play multiple files at the same time.'
			);
		}
	} else if (button == 'runScript') {
		if (selectedCount == 1) {
			RunScript(filename);
		} else {
			DialogError(
				'Error',
				'Error, unable to run multiple files at the same time.'
			);
		}
	} else if (button == 'videoInfo') {
		if (selectedCount == 1) {
			GetVideoInfo(filename);
		} else {
			DialogError(
				'Error',
				'Error, unable to get info for multiple files at the same time.'
			);
		}
	} else if (button == 'viewFile') {
		if (selectedCount == 1) {
			ViewFile(table, filename);
		} else {
			DialogError(
				'Error',
				'Error, unable to view multiple files at the same time.'
			);
		}
	} else if (button == 'tailFile') {
		if (selectedCount == 1) {
			TailFile(table, filename, 50);
		} else {
			DialogError(
				'Error',
				'Error, unable to view multiple files at the same time.'
			);
		}
	} else if (button == 'viewImage') {
		if (selectedCount == 1) {
			ViewImage(filename);
		} else {
			DialogError(
				'Error',
				'Error, unable to view multiple files at the same time.'
			);
		}
	} else if (button == 'mp3gain') {
		var files = [];
		$('#tbl' + table + ' tr.selectedEntry').each(function () {
			files.push($(this).find('td:first').text());
		});
		var postData = JSON.stringify(files);
		DisplayProgressDialog('mp3GainProgress', 'MP3Gain');
		StreamURL(
			'run_mp3gain.php',
			'mp3GainProgressText',
			'mp3GainProgressDialogDone',
			'mp3GainProgressDialogDone',
			'POST',
			postData,
			'application/json'
		);
	} else if (button == 'addToPlaylist') {
		var files = [];
		$('#tbl' + table + ' tr.selectedEntry').each(function () {
			files.push($(this).find('td:first').text());
		});

		AddFilesToPlaylist(table, files);
	} else if (button == 'sequenceInfo') {
		if (selectedCount == 1) {
			GetSequenceInfo(filename);
		} else {
			DialogError(
				'Error',
				'Error, unable to get info for multiple files at the same time.'
			);
		}
	} else if (button == 'fileInfo') {
		if (selectedCount == 1) {
			eval('FileInfo' + table + '("' + filename.replace('"', '\\"') + '");');
		} else {
			DialogError(
				'Error',
				'Error, unable to get info for multiple files at the same time.'
			);
		}
	} else {
		eval(table + button + 'Pressed("' + filename.replace('"', '\\"') + '");');
	}
}

function ClearSelections (table) {
	$('#tbl' + table + ' tr').removeClass('selectedEntry');
	DisableButtonClass('single' + table + 'Button');
	DisableButtonClass('multi' + table + 'Button');
}

function HandleMouseClick (event, row, table) {
	HandleTableRowMouseClick(event, row);

	var selectedCount = $('#tbl' + table + ' tr.selectedEntry').length;

	DisableButtonClass('single' + table + 'Button');
	DisableButtonClass('multi' + table + 'Button');

	if (selectedCount > 1) {
		EnableButtonClass('multi' + table + 'Button');
	} else if (selectedCount > 0) {
		EnableButtonClass('single' + table + 'Button');
	}
}

function pageSpecific_PageLoad_PostDOMLoad_ActionsSetup () {
	//setup the pageLoad actions unique for the file manager page
	$('#tblSequences').on('mousedown', 'tbody tr', function (event, ui) {
		HandleMouseClick(event, $(this), 'Sequences');
	});

	$('#tblMusic').on('mousedown', 'tbody tr', function (event, ui) {
		HandleMouseClick(event, $(this), 'Music');
	});

	$('#tblVideos').on('mousedown', 'tbody tr', function (event, ui) {
		HandleMouseClick(event, $(this), 'Videos');
	});

	$('#tblImages').on('mousedown', 'tbody tr', function (event, ui) {
		HandleMouseClick(event, $(this), 'Images');
	});

	$('#tblEffects').on('mousedown', 'tbody tr', function (event, ui) {
		HandleMouseClick(event, $(this), 'Effects');
	});

	$('#tblScripts').on('mousedown', 'tbody tr', function (event, ui) {
		HandleMouseClick(event, $(this), 'Scripts');
	});

	$('#tblLogs').on('mousedown', 'tbody tr', function (event, ui) {
		HandleMouseClick(event, $(this), 'Logs');
	});

	$('#tblUploads').on('mousedown', 'tbody tr', function (event, ui) {
		HandleMouseClick(event, $(this), 'Uploads');
	});

	$('#tblCrashes').on('mousedown', 'tbody tr', function (event, ui) {
		HandleMouseClick(event, $(this), 'Crashes');
	});

	$('#tblBackups').on('mousedown', 'tbody tr', function (event, ui) {
		HandleMouseClick(event, $(this), 'Backups');
	});

	const pond = FilePond.create(document.querySelector('#filepondInput'), {
		labelIdle: `<b style="font-size: 1.3em;">Drag & Drop or Select Files to upload</b><br><br><span class="btn btn-dark filepond--label-action" style="text-decoration:none;">Select Files</span><br>`,
		server: 'api/file/upload',
		credits: false,
		chunkUploads: true,
		chunkSize: 1024 * 1024 * 64,
		chunkForce: true,
		maxParallelUploads: 3,
		labelTapToUndo: 'Tap to Close'
	});

	pond.on('processfile', (error, file) => {
		console.log('Process file: ' + file.filename);
		moveFile(file.filename, function () {
			GetAllFiles();
		});
	});

	$('#fileManager').tabs({
		activate: function (event, ui) {
			var $t = ui.newPanel.find('table');
			var $tableName = $t[0].id;
			var fileType = $tableName.substring(3);
			eval('tablesorterOptions_' + fileType).widgetOptions.filter_hideFilters =
				settings.fileManagerTableFilter == '1' ? false : true;

			if ($t.length && $t.find('tbody').length) {
				$($t[0]).trigger('destroy');
				switch ($tableName) {
					case 'tblSequences':
						$($t[0]).tablesorter(tablesorterOptions_Sequences);
						break;
					case 'tblMusic':
						$($t[0]).tablesorter(tablesorterOptions_Music);
						break;
					case 'tblVideos':
						$($t[0]).tablesorter(tablesorterOptions_Videos);
						break;
					case 'tblImages':
						$($t[0]).tablesorter(tablesorterOptions_Images);
						break;
					case 'tblEffects':
						$($t[0]).tablesorter(tablesorterOptions_Effects);
						break;
					case 'tblScripts':
						$($t[0]).tablesorter(tablesorterOptions_Scripts);
						break;
					case 'tblLogs':
						$($t[0]).tablesorter(tablesorterOptions_Logs);
						break;
					case 'tblUploads':
						$($t[0]).tablesorter(tablesorterOptions_Uploads);
						break;
					case 'tblCrashes':
						$($t[0]).tablesorter(tablesorterOptions_Crashes);
						break;
					case 'tblBackups':
						$($t[0]).tablesorter(tablesorterOptions_Backups);
						break;
					default:
						var opts = eval('tablesorterOptions_' + $tableName.substring(3));
						$($t[0]).tablesorter(opts);
						break;
				}
				$($t[0]).trigger('applyWidgets');
			}
		}
	});
}

function AddFilesToPlaylist (type, files) {
	GetPlaylistArray();

	var plOptions = '';
	for (var i = 0; i < playListArray.length; i++) {
		plOptions +=
			"<option value='" +
			playListArray[i].name +
			"'>" +
			playListArray[i].name +
			'</option>';
	}

	var sequenceFiles = {};
	var mediaFiles = {};

	if (type == 'Sequences') {
		$('#tblMusic tr').each(function () {
			mediaFiles[$(this).find('td:first').text()] = 1;
		});
		$('#tblVideos tr').each(function () {
			mediaFiles[$(this).find('td:first').text()] = 1;
		});
	} else if (type == 'Music' || type == 'Videos') {
		$('#tblSequences tr').each(function () {
			sequenceFiles[$(this).find('td:first').text()] = 1;
		});
	}

	var etype = '';
	var tbody = '';
	var duration = 0.0;
	var mediaFile = '';

	for (var i = 0; i < files.length; i++) {
		duration = 0.0;
		mediaFile = '';

		if (type == 'Sequences') {
			etype = 'sequence';

			var seqInfo = Get('api/sequence/' + files[i] + '/meta', false);
			if (seqInfo.hasOwnProperty('NumFrames')) {
				duration += (1.0 * seqInfo.NumFrames * seqInfo.StepTime) / 1000;
			}

			if (
				seqInfo.hasOwnProperty('variableHeaders') &&
				seqInfo.variableHeaders.hasOwnProperty('mf')
			) {
				var mf = seqInfo.variableHeaders.mf.split(/[\\/]/).pop();
				if (mediaFiles.hasOwnProperty(mf)) {
					mediaFile = mf;
				} else {
					mf = mf.replace(/\.[^/.]+$/, '');
					if (mediaFiles.hasOwnProperty(mf + '.mp3')) {
						mediaFile = mf + '.mp3';
					} else if (mediaFiles.hasOwnProperty(mf + '.MP3')) {
						mediaFile = mf + '.MP3';
					} else if (mediaFiles.hasOwnProperty(mf + '.mp4')) {
						mediaFile = mf + '.mp4';
					} else if (mediaFiles.hasOwnProperty(mf + '.MP4')) {
						mediaFile = mf + '.MP4';
					}
				}

				if (mediaFile != '') etype = 'both';
			}
		} else if (type == 'Music' || type == 'Videos') {
			etype = 'media';

			var mediaInfo = Get('api/media/' + files[i] + '/duration', false);
			if (mediaInfo.hasOwnProperty(files[i])) {
				duration = mediaInfo[files[i]].duration;
			}

			var sf = files[i].replace(/\.[^/.]+$/, '.fseq');
			if (sequenceFiles.hasOwnProperty(sf)) {
				etype = 'both';
				mediaFile = files[i];
				files[i] = sf;
			}
		} else if (type == 'Scripts') {
			etype = 'command';
		}

		var fileStr = files[i];
		if (mediaFile != '') fileStr += ' (' + mediaFile + ')';

		tbody +=
			"<tr class='fppTableRow'><td class='file' file='" +
			files[i] +
			"' media='" +
			mediaFile +
			"' duration='" +
			duration +
			"'>" +
			fileStr +
			"</td><td class='type' etype='" +
			etype +
			"'>" +
			PlaylistEntryTypeToString(etype) +
			(etype == 'command' ? ' (Run Script)' : '') +
			'</td><td>' +
			SecondsToHuman(duration, true) +
			'</td></tr>';
	}

	var options = {
		id: 'bulkAdd',
		title: 'Bulk Add',
		body: $('#bulkAddTemplate').html().replaceAll('Template', ''),
		class: 'modal-dialog-scrollable',
		keyboard: true,
		backdrop: true,
		buttons: {
			Add: {
				id: 'bulkAddAddButton',
				click: function () {
					BulkAddPlaylist();
					CloseModalDialog('bulkAdd');
				}
			},
			Cancel: {
				id: 'bulkAddCancelButton',
				click: function () {
					CloseModalDialog('bulkAdd');
				}
			}
		}
	};

	DoModalDialog(options);

	$('#bulkAddPlaylist').html(plOptions);
	$('#bulkAddPlaylistSection').val('mainPlaylist');
	$('#bulkAddType').html(type);
	$('#bulkAddList').html(tbody);
}

function BulkAddPlaylist () {
	var playlistName = $('#bulkAddPlaylist').val();
	var pl = Get('api/playlist/' + playlistName, false);
	var files = 'Playlist: ' + playlistName + '\n';
	$('#bulkAddList')
		.find('tr')
		.each(function () {
			var file = $(this).find('td.file').attr('file');
			var duration = parseFloat($(this).find('td.file').attr('duration'));

			var e = {};
			e.type = $(this).find('td.type').attr('etype');
			e.enabled = 1;
			e.playOnce = 0;
			e.duration = duration;

			if (e.type == 'both') {
				e.sequenceName = file;
				e.mediaName = $(this).find('td.file').attr('media');
			} else if (e.type == 'sequence') {
				e.sequenceName = file;
			} else if (e.type == 'media') {
				e.mediaName = file;
			} else if (e.type == 'command') {
				e.command = 'Run Script';
				e.args = [file, '', ''];
			}

			pl[$('#bulkAddPlaylistSection').val()].push(e);

			pl.playlistInfo.total_duration += duration;
			pl.playlistInfo.total_items += 1;
		});

	var result = Post('api/playlist/' + playlistName, false, JSON.stringify(pl));
	if (result.hasOwnProperty('Status') && result.Status == 'Error') {
		$.jGrowl('Error Saving Playlist: ' + result.Message, {
			themeState: 'danger'
		});
	} else {
		$.jGrowl('Playlist updated', { themeState: 'success' });
	}
	CloseModalDialog('bulkAdd');
}

function RunScriptDone () {
	$('#runScriptCloseButton').prop('disabled', false);
	EnableModalDialogCloseButton('runScriptDialog');
}

function RunScript (scriptName) {
	var options = {
		id: 'runScriptDialog',
		title: 'Run Script',
		body: "<textarea style='width: 99%; height: 500px;' disabled id='runScriptText'></textarea>",
		noClose: true,
		keyboard: false,
		backdrop: 'static',
		footer: '',
		buttons: {
			Close: {
				id: 'runScriptCloseButton',
				click: function () {
					CloseModalDialog('runScriptDialog');
				},
				disabled: true,
				class: 'btn-success'
			}
		}
	};

	$('#runScriptCloseButton').prop('disabled', true);
	DoModalDialog(options);

	StreamURL(
		'runEventScript.php?scriptName=' + scriptName + '&nohtml=1',
		'runScriptText',
		'RunScriptDone'
	);
}

function EditScript (scriptName) {
	var options = {
		id: 'scriptEditorDialog',
		title: 'Script Editor : ' + scriptName,
		body: "<div id='fileEditText' class='fileText'>Loading...</div>",
		footer: '',
		class: 'modal-dialog-scrollable',
		keyboard: true,
		backdrop: true,
		buttons: {
			Save: {
				id: 'fileViewerCloseButton',
				class: 'btn-success',
				click: function () {
					SaveScript($('#scriptText').data('scriptName'));
				}
			},
			Cancel: {
				click: function () {
					AbortScriptChange();
				}
			}
		}
	};
	DoModalDialog(options);
	$.get('api/Scripts/' + scriptName, function (text) {
		var ext = scriptName.split('.').pop();
		if (ext != 'html') {
			var html =
				"<textarea style='width: 100%' rows='25' id='scriptText'>" +
				text +
				'</textarea></center></div></fieldset>';
			$('#fileEditText').html(html);
			$('#scriptText').data('scriptName', scriptName);
		}
	});
}

function SaveScript (scriptName) {
	var contents = $('#scriptText').val();
	var url = 'api/scripts/' + scriptName;
	var postData = JSON.stringify(contents);

	$.post(url, postData)
		.done(function (data) {
			if (data.status == 'OK') {
				CloseModalDialog('scriptEditorDialog');
				$.jGrowl('Script saved.', { themeState: 'success' });
			} else {
				DialogError('Save Failed', 'Save Failed: ' + data.status);
			}
		})
		.fail(function () {
			DialogError('Save Failed', 'Save Failed!');
		});
}

function AbortScriptChange () {
	CloseModalDialog('scriptEditorDialog');
}

var tablesorterOptions_Common = {
	// *** APPEARANCE ***
	// Add a theme - 'blackice', 'blue', 'dark', 'default', 'dropbox',
	// 'green', 'grey' or 'ice' stylesheets have all been loaded
	// to use 'bootstrap' or 'jui', you'll need to include "uitheme"
	// in the widgets option - To modify the class names, extend from
	// themes variable. Look for "$.extend($.tablesorter.themes.jui"
	// at the bottom of this window
	// this option only adds a table class name "tablesorter-{theme}"
	theme: 'jui',

	// fix the column widths
	widthFixed: false,

	// Show an indeterminate timer icon in the header when the table
	// is sorted or filtered
	showProcessing: false,

	// header layout template (HTML ok); {content} = innerHTML,
	// {icon} = <i/> (class from cssIcon)
	headerTemplate: '{content}{icon}',

	// return the modified template string
	onRenderTemplate: null, // function(index, template){ return template; },

	// called after each header cell is rendered, use index to target the column
	// customize header HTML
	onRenderHeader: function (index) {
		// the span wrapper is added by default
		//$(this).find('div.tablesorter-header-inner').addClass('roundedCorners');
	},

	// *** FUNCTIONALITY ***
	// prevent text selection in header
	cancelSelection: true,

	// add tabindex to header for keyboard accessibility
	tabIndex: true,

	// other options: "ddmmyyyy" & "yyyymmdd"
	dateFormat: 'mmddyyyy',

	// The key used to select more than one column for multi-column
	// sorting.
	sortMultiSortKey: 'shiftKey',

	// key used to remove sorting on a column
	sortResetKey: 'ctrlKey',

	// false for German "1.234.567,89" or French "1 234 567,89"
	usNumberFormat: true,

	// If true, parsing of all table cell data will be delayed
	// until the user initializes a sort
	delayInit: true,

	// if true, server-side sorting should be performed because
	// client-side sorting will be disabled, but the ui and events
	// will still be used.
	serverSideSorting: false,

	// default setting to trigger a resort after an "update",
	// "addRows", "updateCell", etc has completed
	resort: true,

	// *** SORT OPTIONS ***
	// These are detected by default,
	// but you can change or disable them
	// these can also be set using data-attributes or class names
	headers: {
		//  set "sorter : false" (no quotes) to disable the column
		0: { sorter: 'text', sortInitialOrder: 'asc' },
		1: { sorter: 'metric' },
		2: { sorter: 'text' }
	},

	// ignore case while sorting
	ignoreCase: true,

	// forces the user to have this/these column(s) sorted first
	sortForce: null,
	// initial sort order of the columns, example sortList: [[0,0],[1,0]],
	// [[columnIndex, sortDirection], ... ]
	// sortList: [ [0,0],[1,0],[2,0] ],
	// default sort that is added to the end of the users sort
	// selection.
	sortAppend: null,

	// when sorting two rows with exactly the same content,
	// the original sort order is maintained
	sortStable: false,

	// starting sort direction "asc" or "desc"
	sortInitialOrder: 'asc',

	// Replace equivalent character (accented characters) to allow
	// for alphanumeric sorting
	sortLocaleCompare: false,

	// third click on the header will reset column to default - unsorted
	sortReset: false,

	// restart sort to "sortInitialOrder" when clicking on previously
	// unsorted columns
	sortRestart: false,

	// sort empty cell to bottom, top, none, zero, emptyMax, emptyMin
	emptyTo: 'bottom',

	// sort strings in numerical column as max, min, top, bottom, zero
	stringTo: 'max',

	// extract text from the table
	textExtraction: {
		0: function (node, table) {
			// this is how it is done by default
			return (
				$(node).attr(table.config.textAttribute) ||
				node.textContent ||
				node.innerText ||
				$(node).text() ||
				''
			);
		},
		1: function (node) {
			return $(node).text();
		}
	},

	// data-attribute that contains alternate cell text
	// (used in default textExtraction function)
	textAttribute: 'data-text',

	// use custom text sorter
	// function(a,b){ return a.sort(b); } // basic sort
	textSorter: null,

	// choose overall numeric sorter
	// function(a, b, direction, maxColumnValue)
	numberSorter: null,

	// *** WIDGETS ***
	// apply widgets on tablesorter initialization
	initWidgets: true,

	// table class name template to match to include a widget
	widgetClass: 'widget-{name}',

	// include zebra and any other widgets, options:
	// 'columns', 'filter', 'stickyHeaders' & 'resizable'
	// 'uitheme' is another widget, but requires loading
	// a different skin and a jQuery UI theme.
	widgets: ['columns', 'uitheme', 'cssStickyHeaders', 'filter'],

	widgetOptions: {
		//cssStickyHeader config
		cssStickyHeaders_attachTo: null,
		cssStickyHeaders_addCaption: false,
		cssStickyHeaders_includeCaption: false,
		cssStickyHeaders_filteredToTop: true,
		cssStickyHeaders_offset: 0,

		// zebra widget: adding zebra striping, using content and
		// default styles - the ui css removes the background
		// from default even and odd class names included for this
		// demo to allow switching themes
		// [ "even", "odd" ]
		zebra: ['ui-widget-content even', 'ui-state-default odd'],

		// columns widget: change the default column class names
		// primary is the 1st column sorted, secondary is the 2nd, etc
		columns: ['primary', 'secondary', 'tertiary'],

		// columns widget: If true, the class names from the columns
		// option will also be added to the table tfoot.
		columns_tfoot: true,

		// columns widget: If true, the class names from the columns
		// option will also be added to the table thead.
		columns_thead: true,

		// filter widget: If there are child rows in the table (rows with
		// class name from "cssChildRow" option) and this option is true
		// and a match is found anywhere in the child row, then it will make
		// that row visible; default is false
		filter_childRows: false,

		// filter widget: If true, a filter will be added to the top of
		// each table column.
		filter_columnFilters: true,

		// filter widget: css class name added to the filter cell
		// (string or array)
		filter_cellFilter: '',

		// filter widget: css class name added to the filter row & each
		// input in the row (tablesorter-filter is ALWAYS added)
		filter_cssFilter: '',

		// filter widget: add a default column filter type
		// "~{query}" to make fuzzy searches default;
		// "{q1} AND {q2}" to make all searches use a logical AND.
		filter_defaultFilter: {},

		// filter widget: filters to exclude, per column
		filter_excludeFilter: {},

		// filter widget: jQuery selector string (or jQuery object)
		// of external filters
		filter_external: '',

		// filter widget: class added to filtered rows;
		// needed by pager plugin
		filter_filteredRow: 'filtered',

		// filter widget: add custom filter elements to the filter row
		filter_formatter: null,

		// filter widget: Customize the filter widget by adding a select
		// dropdown with content, custom options or custom filter functions
		// see http://goo.gl/HQQLW for more details
		filter_functions: null,

		// filter widget: hide filter row when table is empty
		filter_hideEmpty: true,

		// filter widget: Set this option to true to hide the filter row
		// initially. The rows is revealed by hovering over the filter
		// row or giving any filter input/select focus.
		filter_hideFilters: false,

		// filter widget: Set this option to false to keep the searches
		// case sensitive
		filter_ignoreCase: true,

		// filter widget: if true, search column content while the user
		// types (with a delay)
		filter_liveSearch: true,

		// filter widget: a header with a select dropdown & this class name
		// will only show available (visible) options within the drop down
		filter_onlyAvail: 'filter-onlyAvail',

		// filter widget: default placeholder text
		// (overridden by any header "data-placeholder" setting)
		filter_placeholder: { search: '', select: '' },

		// filter widget: jQuery selector string of an element used to
		// reset the filters.
		filter_reset: null,

		// filter widget: Use the $.tablesorter.storage utility to save
		// the most recent filters
		filter_saveFilters: false,

		// filter widget: Delay in milliseconds before the filter widget
		// starts searching; This option prevents searching for every character
		// while typing and should make searching large tables faster.
		filter_searchDelay: 300,

		// filter widget: allow searching through already filtered rows in
		// special circumstances; will speed up searching in large tables if true
		filter_searchFiltered: true,

		// filter widget: include a function to return an array of values to be
		// added to the column filter select
		filter_selectSource: null,

		// filter widget: Set this option to true if filtering is performed on
		// the server-side.
		filter_serversideFiltering: false,

		// filter widget: Set this option to true to use the filter to find
		// text from the start of the column. So typing in "a" will find
		// "albert" but not "frank", both have a's; default is false
		filter_startsWith: false,

		// filter widget: If true, ALL filter searches will only use parsed
		// data. To only use parsed data in specific columns, set this option
		// to false and add class name "filter-parsed" to the header
		filter_useParsedData: false,

		// filter widget: data attribute in the header cell that contains
		// the default filter value
		filter_defaultAttrib: 'data-value',

		// filter widget: filter_selectSource array text left of the separator
		// is added to the option value, right into the option text
		filter_selectSourceSeparator: '|',

		// Resizable widget: If this option is set to false, resized column
		// widths will not be saved. Previous saved values will be restored
		// on page reload
		resizable: true,

		// Resizable widget: If this option is set to true, a resizing anchor
		// will be included in the last column of the table
		resizable_addLastColumn: false,

		// Resizable widget: Set this option to the starting & reset header widths
		resizable_widths: [],

		// Resizable widget: Set this option to throttle the resizable events
		// set to true (5ms) or any number 0-10 range
		resizable_throttle: false,

		// saveSort widget: If this option is set to false, new sorts will
		// not be saved. Any previous saved sort will be restored on page
		// reload.
		saveSort: true,

		// stickyHeaders widget: extra class name added to the sticky header row
		stickyHeaders: '',

		// jQuery selector or object to attach sticky header to
		stickyHeaders_attachTo: null,

		// jQuery selector or object to monitor horizontal scroll position
		// (defaults: xScroll > attachTo > window)
		stickyHeaders_xScroll: null,

		// jQuery selector or object to monitor vertical scroll position
		// (defaults: yScroll > attachTo > window)
		stickyHeaders_yScroll: null,

		// number or jquery selector targeting the position:fixed element
		stickyHeaders_offset: 0,

		// scroll table top into view after filtering
		stickyHeaders_filteredToTop: true,

		// added to table ID, if it exists
		stickyHeaders_cloneId: '-sticky',

		// trigger "resize" event on headers
		stickyHeaders_addResizeEvent: true,

		// if false and a caption exist, it won't be included in the
		// sticky header
		stickyHeaders_includeCaption: true,

		// The zIndex of the stickyHeaders, allows the user to adjust this
		// to their needs
		stickyHeaders_zIndex: 2
	},

	// *** CALLBACKS ***
	// function called after tablesorter has completed initialization
	initialized: null, // function (table) {},

	// *** extra css class names
	tableClass: '',
	cssAsc: '',
	cssDesc: '',
	cssNone: '',
	cssHeader: '',
	cssHeaderRow: '',
	// processing icon applied to header during sort/filter
	cssProcessing: '',

	// class name indiciating that a row is to be attached to the its parent
	cssChildRow: 'tablesorter-childRow',
	// if this class does not exist, the {icon} will not be added from
	// the headerTemplate
	cssIcon: 'tablesorter-icon',
	// class name added to the icon when there is no column sort
	cssIconNone: '',
	// class name added to the icon when the column has an ascending sort
	cssIconAsc: '',
	// class name added to the icon when the column has a descending sort
	cssIconDesc: '',
	// don't sort tbody with this class name
	// (only one class name allowed here!)
	cssInfoBlock: 'tablesorter-infoOnly',
	// header row to ignore; cells within this row will not be added
	// to table.config.$headers
	cssIgnoreRow: 'tablesorter-ignoreRow',

	// *** SELECTORS ***
	// jQuery selectors used to find the header cells.
	selectorHeaders: '> thead th, > thead td',

	// jQuery selector of content within selectorHeaders
	// that is clickable to trigger a sort.
	selectorSort: 'th, td',

	// rows with this class name will be removed automatically
	// before updating the table cache - used by "update",
	// "addRows" and "appendCache"
	selectorRemove: '.remove-me',

	// *** DEBUGING ***
	// send messages to console
	debug: false
};

// Extend the themes to change any of the default class names
// this example modifies the jQuery UI theme class names
$.extend($.tablesorter.themes.jui, {
	/* change default jQuery uitheme icons - find the full list of icons
here: http://jqueryui.com/themeroller/
(hover over them for their name)
*/
	// table classes
	table: 'ui-widget ui-widget-content ui-corner-all',
	caption: 'ui-widget-content',
	// *** header class names ***
	// header classes
	header: 'ui-widget-header ui-corner-all ui-state-default',
	sortNone: '',
	sortAsc: '',
	sortDesc: '',
	// applied when column is sorted
	active: 'ui-state-active',
	// hover class
	hover: 'ui-state-hover',
	// *** icon class names ***
	// icon class added to the <i> in the header
	icons: 'ui-icon',
	// class name added to icon when column is not sorted
	iconSortNone: 'ui-icon-carat-2-n-s',
	// class name added to icon when column has ascending sort
	iconSortAsc: 'ui-icon-carat-1-n',
	// class name added to icon when column has descending sort
	iconSortDesc: 'ui-icon-carat-1-s',
	filterRow: '',
	footerRow: '',
	footerCells: '',
	// even row zebra striping
	even: 'ui-widget-content',
	// odd row zebra striping
	odd: 'ui-state-default'
});

var tablesorterOptions_Override_Sequences = {
	theme: 'fpp',
	headers: {
		0: { sorter: 'text', sortInitialOrder: 'asc' },
		1: { sorter: 'metric' },
		2: { sorter: 'text' }
	},
	widgetOptions: {
		cssStickyHeaders_attachTo: '#divSeqData'
	}
};

var tablesorterOptions_Override_Music = {
	theme: 'fpp',
	headers: {
		0: { sorter: 'text', sortInitialOrder: 'asc' },
		1: { sorter: 'text' },
		2: { sorter: 'text' }
	},
	widgetOptions: {
		cssStickyHeaders_attachTo: '#divMusicData'
	}
};

var tablesorterOptions_Override_Videos = {
	theme: 'fpp',
	headers: {
		0: { sorter: 'text', sortInitialOrder: 'asc' },
		1: { sorter: 'text' },
		2: { sorter: 'text' }
	},
	widgetOptions: {
		cssStickyHeaders_attachTo: '#divVideoData'
	}
};

var tablesorterOptions_Override_Images = {
	theme: 'fpp',
	headers: {
		0: { sorter: 'text', sortInitialOrder: 'asc' },
		1: { sorter: 'metric' },
		2: { sorter: 'text' },
		3: { sorter: false }
	},
	widgetOptions: {
		cssStickyHeaders_attachTo: '#divImagesData',
		filter_excludeFilter: { 3: 'hideme' }
	}
};

var tablesorterOptions_Override_Effects = {
	theme: 'fpp',
	headers: {
		0: { sorter: 'text', sortInitialOrder: 'asc' },
		1: { sorter: 'text' },
		2: { sorter: 'text' }
	},
	widgetOptions: {
		cssStickyHeaders_attachTo: '#divEffectsData'
	}
};

var tablesorterOptions_Override_Scripts = {
	theme: 'fpp',
	headers: {
		0: { sorter: 'text', sortInitialOrder: 'asc' },
		1: { sorter: 'metric' },
		2: { sorter: 'text' }
	},
	widgetOptions: {
		cssStickyHeaders_attachTo: '#divScriptsData'
	}
};

var tablesorterOptions_Override_Logs = {
	theme: 'fpp',
	headers: {
		0: { sorter: 'text', sortInitialOrder: 'asc' },
		1: { sorter: 'metric' },
		2: { sorter: 'text' }
	},
	widgetOptions: {
		cssStickyHeaders_attachTo: '#divLogsData'
	}
};

var tablesorterOptions_Override_Uploads = {
	theme: 'fpp',
	headers: {
		0: { sorter: 'text', sortInitialOrder: 'asc' },
		1: { sorter: 'metric' },
		2: { sorter: 'text' }
	},
	widgetOptions: {
		cssStickyHeaders_attachTo: '#divUploadsData'
	}
};

var tablesorterOptions_Override_Crashes = {
	theme: 'fpp',
	headers: {
		0: { sorter: 'text', sortInitialOrder: 'asc' },
		1: { sorter: 'metric' },
		2: { sorter: 'text' }
	},
	widgetOptions: {
		cssStickyHeaders_attachTo: '#divCrashesData'
	}
};

var tablesorterOptions_Override_Backups = {
	theme: 'fpp',
	headers: {
		0: { sorter: 'text', sortInitialOrder: 'asc' },
		1: { sorter: 'metric' },
		2: { sorter: 'text' }
	},
	widgetOptions: {
		cssStickyHeaders_attachTo: '#divBackupsData'
	}
};
//create config options from common and table specific override settings as well as UI setting for filter on/off

var tablesorterOptions_Sequences = $.extend(
	true,
	{},
	tablesorterOptions_Common,
	tablesorterOptions_Override_Sequences,
	{
		widgetOptions: {
			filter_hideFilters: settings.fileManagerTableFilter == '1' ? false : true
		}
	}
);
var tablesorterOptions_Music = $.extend(
	true,
	{},
	tablesorterOptions_Common,
	tablesorterOptions_Override_Music,
	{
		widgetOptions: {
			filter_hideFilters: settings.fileManagerTableFilter == '1' ? false : true
		}
	}
);
var tablesorterOptions_Videos = $.extend(
	true,
	{},
	tablesorterOptions_Common,
	tablesorterOptions_Override_Videos,
	{
		widgetOptions: {
			filter_hideFilters: settings.fileManagerTableFilter == '1' ? false : true
		}
	}
);
var tablesorterOptions_Images = $.extend(
	true,
	{},
	tablesorterOptions_Common,
	tablesorterOptions_Override_Images,
	{
		widgetOptions: {
			filter_hideFilters: settings.fileManagerTableFilter == '1' ? false : true
		}
	}
);
var tablesorterOptions_Effects = $.extend(
	true,
	{},
	tablesorterOptions_Common,
	tablesorterOptions_Override_Effects,
	{
		widgetOptions: {
			filter_hideFilters: settings.fileManagerTableFilter == '1' ? false : true
		}
	}
);
var tablesorterOptions_Scripts = $.extend(
	true,
	{},
	tablesorterOptions_Common,
	tablesorterOptions_Override_Scripts,
	{
		widgetOptions: {
			filter_hideFilters: settings.fileManagerTableFilter == '1' ? false : true
		}
	}
);
var tablesorterOptions_Logs = $.extend(
	true,
	{},
	tablesorterOptions_Common,
	tablesorterOptions_Override_Logs,
	{
		widgetOptions: {
			filter_hideFilters: settings.fileManagerTableFilter == '1' ? false : true
		}
	}
);
var tablesorterOptions_Uploads = $.extend(
	true,
	{},
	tablesorterOptions_Common,
	tablesorterOptions_Override_Uploads,
	{
		widgetOptions: {
			filter_hideFilters: settings.fileManagerTableFilter == '1' ? false : true
		}
	}
);
var tablesorterOptions_Crashes = $.extend(
	true,
	{},
	tablesorterOptions_Common,
	tablesorterOptions_Override_Crashes,
	{
		widgetOptions: {
			filter_hideFilters: settings.fileManagerTableFilter == '1' ? false : true
		}
	}
);
var tablesorterOptions_Backups = $.extend(
	true,
	{},
	tablesorterOptions_Common,
	tablesorterOptions_Override_Backups,
	{
		widgetOptions: {
			filter_hideFilters: settings.fileManagerTableFilter == '1' ? false : true
		}
	}
);

function SetupTableSorter (tableName) {
	var fileType = tableName.substring(3);
	if ($('#' + tableName).find('tbody').length > 0) {
		$('#' + tableName)
			.tablesorter(eval('tablesorterOptions_' + fileType))
			.bind('filterEnd', function (event, config) {
				if (event.type === 'filterEnd') {
					UpdateFileCount(fileType);
				}
			});
		$('#' + tableName)[0].config.widgetOptions.filter_hideFilters =
			settings.fileManagerTableFilter == '1' ? false : true;
		$('#' + tableName).trigger('applyWidgets');
	}
}

function AddStickyHeaderWidget () {
	var $t = $('#fileManager').find('table');

	if ($t.length) {
		var loopSize = $t.length;
		for (let i = 0; i < loopSize; i += 1) {
			var tableName = $t[i].id;
			if ($t[i].config) {
				$($t[i]).trigger('applyWidgetId', 'cssStickyHeaders');
			}
		}
	}
}
