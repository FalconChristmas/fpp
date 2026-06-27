// Global storage for file data to calculate total sizes
var fileData = {};

function GetFiles (dir, extraParams) {
	$.ajax({
		dataType: 'json',
		url: 'api/files/' + dir + (extraParams ? '?' + extraParams : ''),
		success: function (data) {
			let i = 0;

			// Store file data globally
			fileData[dir] = data.files;

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
							"<tr class='fileDetails fileIsDirectory' id='fileDetail_" +
							i +
							"'><td class ='filenameColumn fileName'>" +
							f.name.replace(/&/g, '&amp;').replace(/</g, '&lt;') +
							"</td><td class='fileExtraInfo'>" +
							detail +
							"</td><td class ='fileTime'>" +
							f.mtime +
							'</td><td>Subdir</td></tr>';
					}
				} else {
					var extraClass = 'fileDetails';
					if (f.sizeBytes == 0) {
						extraClass += ' fileIsDirectory';
					}
					tableRow =
						"<tr class='" +
						extraClass +
						"' id='fileDetail_" +
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

function formatBytes (bytes) {
	if (bytes === 0) return '0 B';
	const k = 1024;
	const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
	const i = Math.floor(Math.log(bytes) / Math.log(k));
	return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function UpdateFileCount ($dir) {
	var fileCountEl = $('#fileCount_' + $dir)[0];
	if (!fileCountEl) {
		return; // Element doesn't exist, skip update
	}

	var $table = $('#tbl' + $dir);
	var visibleRows;

	// Use different logic for Bootstrap Table vs Tablesorter
	// Bootstrap Table hides rows with display:none when filtering
	if ($table.closest('.bootstrap-table').length) {
		// For Bootstrap Table: count all rows that aren't hidden
		visibleRows = $table.find('tbody tr:visible').not('.unselectableRow');
	} else {
		// For Tablesorter: count rows without .filtered class
		visibleRows = $table
			.find('tbody tr')
			.not('.unselectableRow')
			.not('.filtered');
	}

	$('#fileCount_' + $dir)[0].innerText = visibleRows.length;

	// Calculate total file size
	let totalSize = 0;
	if (fileData[$dir]) {
		// Get visible file names from the table
		const visibleFileNames = new Set();
		visibleRows.each(function () {
			const fileName = $(this).find('.fileName').text();
			visibleFileNames.add(fileName);
		});

		// Sum up sizes for visible files only
		fileData[$dir].forEach(function (f) {
			if (visibleFileNames.has(f.name)) {
				totalSize += parseInt(f.sizeBytes) || 0;
			}
		});
	}

	// Update the file size badge
	if ($('#fileSize_' + $dir).length > 0) {
		$('#fileSize_' + $dir)[0].innerText = formatBytes(totalSize);
	}

	// Check if filtered by comparing total rows to visible rows
	const totalRows = $table.find('tbody tr').not('.unselectableRow').length;
	if (visibleRows.length < totalRows) {
		// is filtered
		var headingEl = $('#div' + $dir + ' .fileCountlabelHeading')[0];
		if (headingEl) {
			headingEl.innerHTML = '<span class="filtered">Filtered items:<span>';
		}
		$('#fileCount_' + $dir)
			.removeClass('text-bg-secondary')
			.addClass('text-bg-success');
		$('#fileSize_' + $dir)
			.removeClass('text-bg-secondary')
			.addClass('text-bg-success');
	} else {
		//not filtered
		var headingEl = $('#div' + $dir + ' .fileCountlabelHeading')[0];
		if (headingEl) {
			headingEl.innerHTML = '<span class="">Items:<span>';
		}
		$('#fileCount_' + $dir)
			.removeClass('text-bg-success')
			.addClass('text-bg-secondary');
		$('#fileSize_' + $dir)
			.removeClass('text-bg-success')
			.addClass('text-bg-secondary');
	}
}

function FileManagerFilterToggled () {
	var value = settings.fileManagerTableFilter == '1';
	var $t = $('#fileManager').find('table');

	if ($t.length) {
		var loopSize = $t.length;
		for (let i = 0; i < loopSize; i += 1) {
			var tableName = $t[i].id;
			if (!tableName) continue;
			var $bt = $('#' + tableName);
			if ($bt.closest('.bootstrap-table').length) {
				var currentVisible =
					$bt.bootstrapTable('getOptions').filterControlVisible;
				if (currentVisible !== value) {
					$bt.bootstrapTable('toggleFilterControl');
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
	} else if (button == 'deleteConfig') {
		// Developer-mode Config tab: confirm before deleting since these are
		// FPP's internal configuration files.
		var rows = $('#tbl' + table + ' tr.selectedEntry');
		if (rows.length == 0) {
			return;
		}
		var files = [];
		rows.each(function () {
			files.push($(this).find('td:first').text());
		});
		var plural = files.length > 1 ? 's' : '';
		var listHtml =
			'<ul>' +
			files
				.map(function (f) {
					return (
						'<li>' + f.replace(/&/g, '&amp;').replace(/</g, '&lt;') + '</li>'
					);
				})
				.join('') +
			'</ul>';
		DisplayConfirmationDialog(
			'confirmDeleteConfig',
			'Delete Configuration File' + plural,
			'Are you sure you want to delete the following configuration file' +
				plural +
				'? This cannot be undone and may break your system.' +
				listHtml,
			function () {
				rows.each(function () {
					DeleteFile(table, $(this), $(this).find('td:first').text());
				});
			}
		);
	} else if (button == 'editScript') {
		if (selectedCount == 1) {
			EditScript(filename);
		} else {
			DialogError(
				'Error',
				'Error, unable to edit multiple files at the same time.'
			);
		}
	} else if (button == 'editConfig') {
		if (selectedCount == 1) {
			EditConfigFile(filename);
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
			TailFile(table, filename, 250);
		} else {
			DialogError(
				'Error',
				'Error, unable to view multiple files at the same time.'
			);
		}
	} else if (button == 'tailFollow') {
		if (selectedCount == 1) {
			TailFollowFile(table, filename);
		} else {
			DialogError(
				'Error',
				'Error, unable to tail follow multiple files at the same time.'
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
	var dirCount = $('#tbl' + table + ' tr.selectedEntry.fileIsDirectory').length;

	DisableButtonClass('single' + table + 'Button');
	DisableButtonClass('multi' + table + 'Button');

	if (selectedCount > 1) {
		EnableButtonClass('multi' + table + 'Button');
	} else if (selectedCount > 0) {
		EnableButtonClass('single' + table + 'Button');
	}

	if (dirCount > 0) {
		DisableButtonClass('noDirButton');
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

	// Developer-mode only Config tab (only present when uiLevel >= 3)
	$('#tblConfig').on('mousedown', 'tbody tr', function (event, ui) {
		HandleMouseClick(event, $(this), 'Config');
	});
	// there is a bug/issue with FilePond and Safari that causes it to stop when uploading large files, stopping after the first
	// chunk. This is a workaround to increase the chunk size to 512MB so at least files up to 512MB can be uploaded.
	var isSafari = window.safari !== undefined;
	var maxChunkSize = isSafari ? 1024 * 1024 * 512 : 1024 * 1024 * 64;
	const pond = FilePond.create(document.querySelector('#filepondInput'), {
		labelIdle: `<b style="font-size: 1.3em;">Drag & Drop or Select Files to upload</b><br><br><span class="btn btn-dark filepond--label-action" style="text-decoration:none;">Select Files</span><br>`,
		server: 'api/file/upload',
		credits: false,
		chunkUploads: true,
		chunkSize: maxChunkSize,
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
			if (!$t.length || !$t[0].id) return;
			var $tableName = $t[0].id;

			if ($t.length && $t.find('tbody').length) {
				// Clean up previous Bootstrap Table instance
				DestroyBootstrapTable($tableName);
				// Initialize Bootstrap Table for all file manager tables
				InitializeBootstrapTable($tableName);
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

		let trElem = document.createElement('tr');
		trElem.className = 'fppTableRow';

		let tdFile = document.createElement('td');
		tdFile.className = 'file';
		tdFile.setAttribute('file', files[i]);
		tdFile.setAttribute('media', mediaFile);
		tdFile.setAttribute('duration', duration);
		tdFile.textContent = fileStr;

		let tdType = document.createElement('td');
		tdType.className = 'type';
		tdType.setAttribute('etype', etype);
		tdType.textContent =
			PlaylistEntryTypeToString(etype) +
			(etype == 'command' ? ' (Run Script)' : '');

		let tdDuration = document.createElement('td');
		tdDuration.textContent = SecondsToHuman(duration, true);

		trElem.appendChild(tdFile);
		trElem.appendChild(tdType);
		trElem.appendChild(tdDuration);

		tbody += trElem.outerHTML;
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

	// Mark playlist as non-empty since we're adding items to it
	pl.empty = false;

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

// Developer-mode only: edit a file in the config directory. Uses the generic
// file API (api/file/Config/...) for both reading and saving so no script-only
// endpoints are involved.
function EditConfigFile (fileName) {
	var options = {
		id: 'configEditorDialog',
		title: 'Config Editor : ' + fileName,
		body: "<div id='fileEditText' class='fileText'>Loading...</div>",
		footer: '',
		class: 'modal-dialog-scrollable',
		keyboard: true,
		backdrop: true,
		buttons: {
			Save: {
				id: 'configEditorSaveButton',
				class: 'btn-success',
				click: function () {
					SaveConfigFile($('#configText').data('fileName'));
				}
			},
			Cancel: {
				click: function () {
					CloseModalDialog('configEditorDialog');
				}
			}
		}
	};
	DoModalDialog(options);

	var url =
		'api/file/Config/' + encodeURIComponent(fileName).replaceAll('%2F', '/');
	$.ajax({
		url: url,
		dataType: 'text',
		success: function (text) {
			$('#fileEditText').html(
				"<textarea style='width: 100%' rows='25' id='configText'></textarea>"
			);
			$('#configText').val(text).data('fileName', fileName);
		},
		error: function () {
			$('#fileEditText').html('Error loading file.');
		}
	});
}

function SaveConfigFile (fileName) {
	var contents = $('#configText').val();
	var url =
		'api/file/Config/' + encodeURIComponent(fileName).replaceAll('%2F', '/');

	$.ajax({
		url: url,
		type: 'POST',
		data: contents,
		contentType: 'text/plain',
		processData: false
	})
		.done(function (data) {
			if (data && data.status == 'OK') {
				CloseModalDialog('configEditorDialog');
				$.jGrowl('Config file saved.', { themeState: 'success' });
				GetFiles('Config', 'maxdepth=1');
			} else {
				DialogError(
					'Save Failed',
					'Save Failed: ' + (data && data.status ? data.status : 'unknown error')
				);
			}
		})
		.fail(function () {
			DialogError('Save Failed', 'Save Failed!');
		});
}

function SetupTableSorter (tableName) {
	if ($('#' + tableName).find('tbody').length > 0) {
		// Only initialize if the table's tab is currently visible.
		// Bootstrap Table cannot calculate column widths on hidden elements.
		// The tab activate handler will initialize when the tab becomes visible.
		if ($('#' + tableName).is(':visible')) {
			DestroyBootstrapTable(tableName);
			InitializeBootstrapTable(tableName);
		}
	}
}
