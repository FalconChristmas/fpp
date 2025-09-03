(function ($) {
	if (!$.jGrowl) {
		$.jGrowl = function (message, options) {
			// Notifications disabled
			return null;
		};

		$.jGrowl.defaults = {
			closerTemplate: '' // or null, depending on your code
			// Add other expected defaults here if needed
		};
	}
})(jQuery);
