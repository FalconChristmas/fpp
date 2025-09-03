(function ($) {
	if (!$.jGrowl) {
		$.jGrowl = function (message, options) {
			// Notifications disabled
			console.log('Notification suppressed:', message);
			return null;
		};

		$.jGrowl.defaults = {
			closerTemplate: '' // or null, depending on your code
			// Add other expected defaults here if needed
		};
	}
})(jQuery);
