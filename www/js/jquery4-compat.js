/**
 * jQuery 4.0 Compatibility Shim
 *
 * Restores APIs removed in jQuery 4.0 that are still used by third-party
 * plugins (tablesorter, jquery.form, jgrowl, touch-punch, etc.).
 *
 * Load this immediately after jquery-latest.min.js.
 */
(function ($) {
	'use strict';

	// $.trim() - removed in jQuery 4.0
	if (!$.trim) {
		$.trim = function (str) {
			return str == null ? '' : (str + '').trim();
		};
	}

	// $.isFunction() - removed in jQuery 4.0
	if (!$.isFunction) {
		$.isFunction = function (obj) {
			return typeof obj === 'function';
		};
	}

	// $.isArray() - removed in jQuery 4.0
	if (!$.isArray) {
		$.isArray = Array.isArray;
	}

	// $.type() - removed in jQuery 4.0
	if (!$.type) {
		var class2type = {};
		'Boolean Number String Function Array Date RegExp Object Error Symbol'
			.split(' ')
			.forEach(function (name) {
				class2type['[object ' + name + ']'] = name.toLowerCase();
			});
		$.type = function (obj) {
			if (obj == null) {
				return obj + '';
			}
			return typeof obj === 'object' || typeof obj === 'function'
				? class2type[Object.prototype.toString.call(obj)] || 'object'
				: typeof obj;
		};
	}

	// $.isNumeric() - removed in jQuery 4.0
	if (!$.isNumeric) {
		$.isNumeric = function (obj) {
			var type = typeof obj;
			return (
				(type === 'number' || type === 'string') &&
				!isNaN(obj - parseFloat(obj))
			);
		};
	}

	// $.isWindow() - removed in jQuery 4.0
	if (!$.isWindow) {
		$.isWindow = function (obj) {
			return obj != null && obj === obj.window;
		};
	}

	// $.proxy() - removed in jQuery 4.0
	if (!$.proxy) {
		$.proxy = function (fn, context) {
			if (typeof context === 'string') {
				var tmp = fn[context];
				context = fn;
				fn = tmp;
			}
			if (typeof fn !== 'function') {
				return undefined;
			}
			var args = Array.prototype.slice.call(arguments, 2);
			var proxy = function () {
				return fn.apply(
					context || this,
					args.concat(Array.prototype.slice.call(arguments))
				);
			};
			return proxy;
		};
	}

	// $.parseJSON() - removed in jQuery 4.0
	if (!$.parseJSON) {
		$.parseJSON = JSON.parse;
	}

	// $.camelCase() - removed in jQuery 4.0
	if (!$.camelCase) {
		$.camelCase = function (str) {
			return str.replace(/-([\da-z])/gi, function (all, letter) {
				return letter.toUpperCase();
			});
		};
	}

	// $.now() - removed in jQuery 4.0
	if (!$.now) {
		$.now = Date.now;
	}

	// $.fn.bind() / $.fn.unbind() - removed in jQuery 4.0
	// Used by tablesorter, jgrowl, floatThead, jcanvas, msgBox, etc.
	if (!$.fn.bind) {
		$.fn.bind = function (types, data, fn) {
			return this.on(types, null, data, fn);
		};
	}
	if (!$.fn.unbind) {
		$.fn.unbind = function (types, fn) {
			return this.off(types, null, fn);
		};
	}

	// $.fn.delegate() / $.fn.undelegate() - removed in jQuery 4.0
	if (!$.fn.delegate) {
		$.fn.delegate = function (selector, types, data, fn) {
			return this.on(types, selector, data, fn);
		};
	}
	if (!$.fn.undelegate) {
		$.fn.undelegate = function (selector, types, fn) {
			return arguments.length === 1
				? this.off(selector, '**')
				: this.off(types, selector || '**', fn);
		};
	}

	// Array methods on jQuery objects - removed in jQuery 4.0
	// jQuery objects no longer inherit from Array, but plugins like
	// tablesorter rely on .sort(), .splice(), and .push() being available.
	var arrayMethods = ['push', 'sort', 'splice'];
	arrayMethods.forEach(function (method) {
		if (!$.fn[method]) {
			$.fn[method] = Array.prototype[method];
		}
	});
})(jQuery);
