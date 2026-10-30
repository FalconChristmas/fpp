/**
 * Bootstrap Table Configuration and Initialization
 * For FPP File Manager (PoC - Proof of Concept)
 *
 * This file provides Bootstrap Table setup for file manager tables
 * Bootstrap Table is a modern, actively-maintained alternative to jQuery Tablesorter
 *
 * v1.0 - April 2026
 */

// Bootstrap Table options for Sequences table
const bootstrapTableOptions_Sequences = {
	// *** APPEARANCE ***
	// Bootstrap 5 theme
	theme: 'bootstrapTable',

	// Basic table functionality
	striped: true,
	bordered: false,
	hover: true,
	condensed: false,

	// Sorting configuration
	sortName: 'name',
	sortOrder: 'asc',
	sortable: true,
	multiSort: false,

	// Pagination
	pagination: false,
	pageSize: 50,
	pageList: [10, 25, 50, 100],

	// Toolbar
	showHeader: true,
	showFooter: false,
	showColumns: false,
	showColumnsPagination: false,
	showExport: false,

	// Fixed header with scrollable body
	height: 300,

	// Row selection
	selectItemName: 'btSelectItem',
	singleSelect: false,
	checkboxHeader: true,
	maintainSelected: false,

	// Column definitions with data-field names matching table headers
	columns: [
		{
			field: 'filename',
			title: 'File',
			sortable: true,
			searchable: true,
			filterControl: 'input'
		},
		{
			field: 'size',
			title: 'Size',
			sortable: true,
			searchable: true,
			align: 'right',
			sorter: 'metricSorter',
			filterControl: 'input'
		},
		{
			field: 'fps',
			title: 'FPS',
			sortable: true,
			searchable: true,
			align: 'right',
			sorter: 'numericSorter',
			filterControl: 'input'
		},
		{
			field: 'dateModified',
			title: 'Date Modified',
			sortable: true,
			searchable: true,
			sorter: 'dateSorter',
			filterControl: 'input'
		}
	],

	// Callbacks
	onClickRow: function (row, $element, field) {
		// Handle row click - select/deselect
		$($element).toggleClass('table-active');
	},

	onDblClickRow: function (row, $element, field) {
		// Handle double-click - could open file or edit
	},

	// Filter control
	filterControl: true,
	filterShowClear: true,
	filter_hideFilters: false,

	// Classes
	classes: 'table table-hover table-sm',

	// Locale
	locale: 'en-US',

	// Event handlers for Bootstrap Table
	onPostBody: function () {
		UpdateFileCount('Sequences');
	},
	onPostFilter: function () {
		UpdateFileCount('Sequences');
	}
};

// Bootstrap Table options for Music table
const bootstrapTableOptions_Music = {
	theme: 'bootstrapTable',
	striped: true,
	bordered: false,
	hover: true,
	sortName: 'name',
	sortOrder: 'asc',
	sortable: true,
	multiSort: false,
	pagination: false,
	showHeader: true,
	showFooter: false,
	showColumns: false,
	selectItemName: 'btSelectItem',
	singleSelect: false,
	checkboxHeader: true,
	// Fixed header with scrollable body
	height: 300,
	// Filter control
	filterControl: true,
	filterShowClear: true,
	filter_hideFilters: false,
	columns: [
		{
			field: 'filename',
			title: 'File',
			sortable: true,
			searchable: true,
			filterControl: 'input'
		},
		{
			field: 'duration',
			title: 'Duration',
			sortable: true,
			searchable: true,
			align: 'right',
			filterControl: 'input'
		},
		{
			field: 'dateModified',
			title: 'Date Modified',
			sortable: true,
			searchable: true,
			sorter: 'dateSorter',
			filterControl: 'input'
		}
	],
	classes: 'table table-hover table-sm',
	locale: 'en-US',

	// Event handlers for Bootstrap Table
	onPostBody: function () {
		UpdateFileCount('Music');
	},
	onPostFilter: function () {
		UpdateFileCount('Music');
	}
};

// Bootstrap Table options for Videos table
const bootstrapTableOptions_Videos = {
	theme: 'bootstrapTable',
	striped: true,
	bordered: false,
	hover: true,
	sortName: 'name',
	sortOrder: 'asc',
	sortable: true,
	multiSort: false,
	pagination: false,
	showHeader: true,
	showFooter: false,
	showColumns: false,
	selectItemName: 'btSelectItem',
	singleSelect: false,
	checkboxHeader: true,
	// Fixed header with scrollable body
	height: 300,
	// Filter control
	filterControl: true,
	filterShowClear: true,
	filter_hideFilters: false,
	columns: [
		{
			field: 'filename',
			title: 'File',
			sortable: true,
			searchable: true,
			filterControl: 'input'
		},
		{
			field: 'duration',
			title: 'Duration',
			sortable: true,
			searchable: true,
			align: 'right',
			filterControl: 'input'
		},
		{
			field: 'dateModified',
			title: 'Date Modified',
			sortable: true,
			searchable: true,
			sorter: 'dateSorter',
			filterControl: 'input'
		}
	],
	classes: 'table table-hover table-sm',
	locale: 'en-US',

	// Event handlers for Bootstrap Table
	onPostBody: function () {
		UpdateFileCount('Videos');
	},
	onPostFilter: function () {
		UpdateFileCount('Videos');
	}
};

// Bootstrap Table options for Images table
const bootstrapTableOptions_Images = {
	theme: 'bootstrapTable',
	striped: true,
	bordered: false,
	hover: true,
	sortName: 'name',
	sortOrder: 'asc',
	sortable: true,
	multiSort: false,
	pagination: false,
	showHeader: true,
	showFooter: false,
	showColumns: false,
	selectItemName: 'btSelectItem',
	singleSelect: false,
	checkboxHeader: true,
	// Fixed header with scrollable body
	height: 300,
	// Filter control
	filterControl: true,
	filterShowClear: true,
	filter_hideFilters: false,
	columns: [
		{
			field: 'filename',
			title: 'File',
			sortable: true,
			searchable: true,
			filterControl: 'input'
		},
		{
			field: 'size',
			title: 'Size',
			sortable: true,
			searchable: true,
			align: 'right',
			sorter: 'metricSorter',
			filterControl: 'input'
		},
		{
			field: 'thumbnail',
			title: 'Thumbnail',
			sortable: false,
			searchable: false,
			filterControl: false
		},
		{
			field: 'dateModified',
			title: 'Date Modified',
			sortable: true,
			searchable: true,
			sorter: 'dateSorter',
			filterControl: 'input'
		}
	],
	classes: 'table table-hover table-sm',
	locale: 'en-US',

	// Event handlers for Bootstrap Table
	onPostBody: function () {
		UpdateFileCount('Images');
	},
	onPostFilter: function () {
		UpdateFileCount('Images');
	}
};

// Bootstrap Table options for Effects table
const bootstrapTableOptions_Effects = {
	theme: 'bootstrapTable',
	striped: true,
	bordered: false,
	hover: true,
	sortName: 'name',
	sortOrder: 'asc',
	sortable: true,
	multiSort: false,
	pagination: false,
	showHeader: true,
	showFooter: false,
	showColumns: false,
	selectItemName: 'btSelectItem',
	singleSelect: false,
	checkboxHeader: true,
	// Fixed header with scrollable body
	height: 300,
	// Filter control
	filterControl: true,
	filterShowClear: true,
	filter_hideFilters: false,
	columns: [
		{
			field: 'filename',
			title: 'File',
			sortable: true,
			searchable: true,
			filterControl: 'input'
		},
		{
			field: 'duration',
			title: 'Duration',
			sortable: true,
			searchable: true,
			align: 'right',
			filterControl: 'input'
		},
		{
			field: 'dateModified',
			title: 'Date Modified',
			sortable: true,
			searchable: true,
			sorter: 'dateSorter',
			filterControl: 'input'
		}
	],
	classes: 'table table-hover table-sm',
	locale: 'en-US',

	// Event handlers for Bootstrap Table
	onPostBody: function () {
		UpdateFileCount('Effects');
	},
	onPostFilter: function () {
		UpdateFileCount('Effects');
	}
};

// Bootstrap Table options for Scripts table
const bootstrapTableOptions_Scripts = {
	theme: 'bootstrapTable',
	striped: true,
	bordered: false,
	hover: true,
	sortName: 'name',
	sortOrder: 'asc',
	sortable: true,
	multiSort: false,
	pagination: false,
	showHeader: true,
	showFooter: false,
	showColumns: false,
	selectItemName: 'btSelectItem',
	singleSelect: false,
	checkboxHeader: true,
	// Fixed header with scrollable body
	height: 300,
	// Filter control
	filterControl: true,
	filterShowClear: true,
	filter_hideFilters: false,
	columns: [
		{
			field: 'filename',
			title: 'File',
			sortable: true,
			searchable: true,
			filterControl: 'input'
		},
		{
			field: 'size',
			title: 'Size',
			sortable: true,
			searchable: true,
			align: 'right',
			sorter: 'metricSorter',
			filterControl: 'input'
		},
		{
			field: 'dateModified',
			title: 'Date Modified',
			sortable: true,
			searchable: true,
			sorter: 'dateSorter',
			filterControl: 'input'
		}
	],
	classes: 'table table-hover table-sm',
	locale: 'en-US',

	// Event handlers for Bootstrap Table
	onPostBody: function () {
		UpdateFileCount('Scripts');
	},
	onPostFilter: function () {
		UpdateFileCount('Scripts');
	}
};

// Bootstrap Table options for Logs table
const bootstrapTableOptions_Logs = {
	theme: 'bootstrapTable',
	striped: true,
	bordered: false,
	hover: true,
	sortName: 'name',
	sortOrder: 'asc',
	sortable: true,
	multiSort: false,
	pagination: false,
	showHeader: true,
	showFooter: false,
	showColumns: false,
	selectItemName: 'btSelectItem',
	singleSelect: false,
	checkboxHeader: true,
	// Fixed header with scrollable body
	height: 300,
	// Filter control
	filterControl: true,
	filterShowClear: true,
	filter_hideFilters: false,
	columns: [
		{
			field: 'filename',
			title: 'File',
			sortable: true,
			searchable: true,
			filterControl: 'input'
		},
		{
			field: 'size',
			title: 'Size',
			sortable: true,
			searchable: true,
			align: 'right',
			sorter: 'metricSorter',
			filterControl: 'input'
		},
		{
			field: 'dateModified',
			title: 'Date Modified',
			sortable: true,
			searchable: true,
			sorter: 'dateSorter',
			filterControl: 'input'
		}
	],
	classes: 'table table-hover table-sm',
	locale: 'en-US',

	// Event handlers for Bootstrap Table
	onPostBody: function () {
		UpdateFileCount('Logs');
	},
	onPostFilter: function () {
		UpdateFileCount('Logs');
	}
};

// Bootstrap Table options for Uploads table
const bootstrapTableOptions_Uploads = {
	theme: 'bootstrapTable',
	striped: true,
	bordered: false,
	hover: true,
	sortName: 'name',
	sortOrder: 'asc',
	sortable: true,
	multiSort: false,
	pagination: false,
	showHeader: true,
	showFooter: false,
	showColumns: false,
	selectItemName: 'btSelectItem',
	singleSelect: false,
	checkboxHeader: true,
	// Fixed header with scrollable body
	height: 300,
	// Filter control
	filterControl: true,
	filterShowClear: true,
	filter_hideFilters: false,
	columns: [
		{
			field: 'filename',
			title: 'File',
			sortable: true,
			searchable: true,
			filterControl: 'input'
		},
		{
			field: 'size',
			title: 'Size',
			sortable: true,
			searchable: true,
			align: 'right',
			sorter: 'metricSorter',
			filterControl: 'input'
		},
		{
			field: 'dateModified',
			title: 'Date Modified',
			sortable: true,
			searchable: true,
			sorter: 'dateSorter',
			filterControl: 'input'
		}
	],
	classes: 'table table-hover table-sm',
	locale: 'en-US',

	// Event handlers for Bootstrap Table
	onPostBody: function () {
		UpdateFileCount('Uploads');
	},
	onPostFilter: function () {
		UpdateFileCount('Uploads');
	}
};

// Bootstrap Table options for Crashes table
const bootstrapTableOptions_Crashes = {
	theme: 'bootstrapTable',
	striped: true,
	bordered: false,
	hover: true,
	sortName: 'name',
	sortOrder: 'asc',
	sortable: true,
	multiSort: false,
	pagination: false,
	showHeader: true,
	showFooter: false,
	showColumns: false,
	selectItemName: 'btSelectItem',
	singleSelect: false,
	checkboxHeader: true,
	// Fixed header with scrollable body
	height: 300,
	// Filter control
	filterControl: true,
	filterShowClear: true,
	filter_hideFilters: false,
	columns: [
		{
			field: 'filename',
			title: 'File',
			sortable: true,
			searchable: true,
			filterControl: 'input'
		},
		{
			field: 'size',
			title: 'Size',
			sortable: true,
			searchable: true,
			align: 'right',
			sorter: 'metricSorter',
			filterControl: 'input'
		},
		{
			field: 'dateModified',
			title: 'Date Modified',
			sortable: true,
			searchable: true,
			sorter: 'dateSorter',
			filterControl: 'input'
		}
	],
	classes: 'table table-hover table-sm',
	locale: 'en-US',

	// Event handlers for Bootstrap Table
	onPostBody: function () {
		UpdateFileCount('Crashes');
	},
	onPostFilter: function () {
		UpdateFileCount('Crashes');
	}
};

// Bootstrap Table options for Backups table
const bootstrapTableOptions_Backups = {
	theme: 'bootstrapTable',
	striped: true,
	bordered: false,
	hover: true,
	sortName: 'name',
	sortOrder: 'asc',
	sortable: true,
	multiSort: false,
	pagination: false,
	showHeader: true,
	showFooter: false,
	showColumns: false,
	selectItemName: 'btSelectItem',
	singleSelect: false,
	checkboxHeader: true,
	// Fixed header with scrollable body
	height: 300,
	// Filter control
	filterControl: true,
	filterShowClear: true,
	filter_hideFilters: false,
	columns: [
		{
			field: 'filename',
			title: 'File',
			sortable: true,
			searchable: true,
			filterControl: 'input'
		},
		{
			field: 'size',
			title: 'Size',
			sortable: true,
			searchable: true,
			align: 'right',
			sorter: 'metricSorter',
			filterControl: 'input'
		},
		{
			field: 'dateModified',
			title: 'Date Modified',
			sortable: true,
			searchable: true,
			sorter: 'dateSorter',
			filterControl: 'input'
		}
	],
	classes: 'table table-hover table-sm',
	locale: 'en-US',

	// Event handlers for Bootstrap Table
	onPostBody: function () {
		UpdateFileCount('Backups');
	},
	onPostFilter: function () {
		UpdateFileCount('Backups');
	}
};

/**
 * Custom sorters for Bootstrap Table
 * Registered on window so BT can resolve string references via calculateObjectValue
 */

// Metric sorter (for file sizes like "3.05kB", "1.5 MB")
window.metricSorter = function (a, b) {
	const parseMetric = val => {
		if (!val) return 0;
		const str = String(val).trim();
		const parts = str.match(/^([\d.]+)\s*([KMGT]?)i?B?$/i);
		if (!parts) return 0;
		const number = parseFloat(parts[1]);
		const unit = (parts[2] || '').toUpperCase();
		const multipliers = { K: 1024, M: 1024 ** 2, G: 1024 ** 3, T: 1024 ** 4 };
		return number * (multipliers[unit] || 1);
	};
	return parseMetric(a) - parseMetric(b);
};

// Numeric sorter (for plain numbers like the sequence FPS column). Cells may
// contain a placeholder (e.g. "…") while the value is loading; those sort as 0.
window.numericSorter = function (a, b) {
	const parseNum = val => {
		const num = parseFloat(String(val).replace(/[^\d.-]/g, ''));
		return isNaN(num) ? 0 : num;
	};
	return parseNum(a) - parseNum(b);
};

// Date sorter (for dates like "04/01/26  09:42 PM")
window.dateSorter = function (a, b) {
	const parseDate = dateStr => {
		if (!dateStr) return 0;
		// Normalize double spaces to single
		const str = String(dateStr).trim().replace(/\s+/g, ' ');
		const date = new Date(str);
		return isNaN(date.getTime()) ? 0 : date.getTime();
	};
	return parseDate(a) - parseDate(b);
};

/**
 * Initialize a Bootstrap Table
 * Called when a file manager table needs to be set up
 *
 * @param {string} tableName - The ID of the table (e.g., 'tblSequences')
 */
function InitializeBootstrapTable (tableName) {
	const fileType = tableName.substring(3); // Remove 'tbl' prefix
	const optionsKey = 'bootstrapTableOptions_' + fileType;

	const $table = $('#' + tableName);

	// Skip if already initialized (Bootstrap Table wraps the table in a .bootstrap-table div)
	if (
		$table.closest('.bootstrap-table').length ||
		$table.data('bootstrap.table')
	) {
		return;
	}

	// Check if options exist for this table type
	if (typeof window[optionsKey] === 'undefined') {
		console.warn('Bootstrap Table options not found for: ' + tableName);
		return;
	}

	// Apply filter visibility from settings
	var opts = window[optionsKey];
	if (
		typeof settings !== 'undefined' &&
		typeof settings.fileManagerTableFilter !== 'undefined'
	) {
		opts.filterControlVisible = settings.fileManagerTableFilter == '1';
	}

	// Initialize Bootstrap Table
	$table.bootstrapTable(opts);

	// Let Bootstrap Table manage scrolling - disable outer container scroll
	$table.closest('.fileManagerDivData').addClass('bt-managed');
}

/**
 * Get selected items from a Bootstrap Table
 *
 * @param {string} tableName - The ID of the table
 * @returns {array} Array of selected row objects
 */
function GetBootstrapTableSelectedItems (tableName) {
	const $table = $('#' + tableName);
	if (!$table.length) return [];
	return $table.bootstrapTable('getSelections');
}

/**
 * Clear all selections in a Bootstrap Table
 *
 * @param {string} tableName - The ID of the table
 */
function ClearBootstrapTableSelection (tableName) {
	const $table = $('#' + tableName);
	if (!$table.length) return;
	$table.bootstrapTable('uncheckAll');
}

/**
 * Refresh a Bootstrap Table with new data
 *
 * @param {string} tableName - The ID of the table
 * @param {array} data - Array of data objects to display
 */
function RefreshBootstrapTable (tableName, data) {
	const $table = $('#' + tableName);
	if (!$table.length) return;

	// Load the new data
	$table.bootstrapTable('load', data);

	// Optionally restore selections
	$table.bootstrapTable('checkAll');
}

/**
 * Destroy a Bootstrap Table and clean up
 *
 * @param {string} tableName - The ID of the table
 */
function DestroyBootstrapTable (tableName) {
	const $table = $('#' + tableName);
	if (!$table.length) return;
	if (
		!$table.closest('.bootstrap-table').length &&
		!$table.data('bootstrap.table')
	)
		return;
	$table.bootstrapTable('destroy');
}

// Export all Bootstrap Table options to window for global access
window.bootstrapTableOptions_Sequences = bootstrapTableOptions_Sequences;
window.bootstrapTableOptions_Music = bootstrapTableOptions_Music;
window.bootstrapTableOptions_Videos = bootstrapTableOptions_Videos;
window.bootstrapTableOptions_Images = bootstrapTableOptions_Images;
window.bootstrapTableOptions_Effects = bootstrapTableOptions_Effects;
window.bootstrapTableOptions_Scripts = bootstrapTableOptions_Scripts;
window.bootstrapTableOptions_Logs = bootstrapTableOptions_Logs;
window.bootstrapTableOptions_Uploads = bootstrapTableOptions_Uploads;
window.bootstrapTableOptions_Crashes = bootstrapTableOptions_Crashes;
window.bootstrapTableOptions_Backups = bootstrapTableOptions_Backups;
