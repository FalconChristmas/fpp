$(function() {

	var $menu = $('#main-menu');

	// add the HTML structure
	$('div.right-column').prepend('\
  <div id="themes">\
   <h2>Switch theme (class)</h2>\
   <p>\
    <select id="themes-classes">\
\
\
	<!-- include new themes by adding a new option below -->\
\
\
     <option value="sm-blue" data-page-bg="#fbf3e8" data-init-options="{\n\
			subMenusSubOffsetX: 1,\n\
			subMenusSubOffsetY: -8\n\
		}" data-init-options-vertical="{\n\
			mainMenuSubOffsetX: 1,\n\
			mainMenuSubOffsetY: -8,\n\
			subMenusSubOffsetX: 1,\n\
			subMenusSubOffsetY: -8\n\
		}">sm-blue</option>\
     <option value="sm-clean" data-page-bg="#fcfcfc" data-init-options="{\n\
			mainMenuSubOffsetX: -1,\n\
			mainMenuSubOffsetY: 4,\n\
			subMenusSubOffsetX: 6,\n\
			subMenusSubOffsetY: -6\n\
		}" data-init-options-vertical="{\n\
			mainMenuSubOffsetX: 6,\n\
			mainMenuSubOffsetY: -6,\n\
			subMenusSubOffsetX: 6,\n\
			subMenusSubOffsetY: -6\n\
		}">sm-clean</option>\
     <option value="sm-mint" data-page-bg="#fff" data-init-options="{\n\
			subMenusSubOffsetX: 6,\n\
			subMenusSubOffsetY: -8\n\
		}" data-init-options-vertical="{\n\
			mainMenuSubOffsetX: 6,\n\
			mainMenuSubOffsetY: -7,\n\
			subMenusSubOffsetX: 6,\n\
			subMenusSubOffsetY: -8\n\
		}">sm-mint</option>\
     <option value="sm-simple" data-page-bg="#f6f6f6" data-init-options="{\n\
			mainMenuSubOffsetX: -1,\n\
			subMenusSubOffsetX: 10,\n\
			subMenusSubOffsetY: 0\n\
		}" data-init-options-vertical="{\n\
			mainMenuSubOffsetX: 10,\n\
			mainMenuSubOffsetY: 0,\n\
			subMenusSubOffsetX: 10,\n\
			subMenusSubOffsetY: 0\n\
		}">sm-simple</option>\
    </select>\
    <input id="themes-horizontal" name="themes-orientation" value="horizontal" type="radio" checked="checked" /><label for="themes-horizontal">Horizontal main menu</label><br />\
    <input id="themes-vertical" name="themes-orientation" value="vertical" type="radio" /><label for="themes-vertical">Vertical main menu</label><br />\
   </p>\
  </div>\
\
  <div id="source">\
   <h2>Source code</h2>\
   <h3>CSS:</h3>\
   <pre class="sh_html sh_sourceCode">&lt;!-- SmartMenus core CSS (required) --&gt;\n\
&lt;link href="../css/sm-core-css.css" rel="stylesheet" type="text/css" /&gt;\n\
\n\
&lt;!-- "<span class="themes-code-class">sm-blue</span>" menu theme (optional, you can use your own CSS, too) --&gt;\n\
&lt;link href="../css/<span class="themes-code-class">sm-blue</span>/<span class="themes-code-class">sm-blue</span>.css" rel="stylesheet" type="text/css" /&gt;\n' + (window.addonCSS ? window.addonCSS : '') + '\
\n\
&lt;!-- #main-menu config - instance specific stuff not covered in the theme --&gt;\n\
&lt;style type="text/css"&gt;\n\
	#main-menu {\n\
		position:relative;\n\
		z-index:9999;\n\
		width:<span class="themes-code-main-width">auto</span>;\n\
	}\n\
	#main-menu ul {\n\
		width:12em; /* fixed width only please - you can use the "subMenusMinWidth"/"subMenusMaxWidth" script options to override this if you like */\n\
	}\n\
&lt;/style&gt;</pre>\
   <h3>JavaScript:</h3>\
   <pre class="sh_html sh_sourceCode">&lt;!-- jQuery --&gt;\n\
&lt;script type="text/javascript" src="../libs/jquery-loader.js"&gt;&lt;/script&gt;\n\
\n\
&lt;!-- SmartMenus jQuery plugin --&gt;\n\
&lt;script type="text/javascript" src="../jquery.smartmenus.js"&gt;&lt;/script&gt;\n' + (window.addonScriptSrc ? $.map(window.addonScriptSrc, function(arr) {
	return '\n&lt;!-- ' + arr[0] + ' --&gt;\n&lt;script type="text/javascript" src="' + arr[1] + '"&gt;&lt;/script&gt;\n';
}).join('') : '') + '\
\n\
&lt;!-- SmartMenus jQuery init --&gt;\n\
&lt;script type="text/javascript"&gt;\n\
	$(function() {\n\
		$(\'#main-menu\').smartmenus(<span class="themes-code-init-options">{\n\
			subMenusSubOffsetX: 1,\n\
			subMenusSubOffsetY: -8\n\
		}</span>);\n' + (window.addonScriptInit ? window.addonScriptInit : '') + '\
	});\n\
&lt;/script&gt;</pre>\
   <h3>HTML:</h3>\
   <pre class="sh_html sh_sourceCode">' + (window.addonHTMLBefore ? window.addonHTMLBefore : '') + '&lt;ul id="main-menu" class="<span class="themes-code-main-class">' + $menu[0].className + '</span>"&gt; ...' + (window.addonHTMLAfter ? window.addonHTMLAfter : '') + '</pre>\
  </div>\
');

	// load additional themes
	$('#themes-classes option').not(':first').each(function() {
		var className = $(this).attr('value');
		$('<link href="../css/' + className + '/' + className + '.css" rel="stylesheet" type="text/css" />').appendTo('head');
	});

	// hook theme switcher
	$('#themes-classes, #themes-horizontal, #themes-vertical').change(function() {
		var $select = $('#themes-classes'),
			className = $select.val(),
			vertical = $('#themes-vertical')[0].checked,
			$optionElm = $select.children().eq($select[0].selectedIndex),
			initOptions = $optionElm.data('init-options' + (vertical ? '-vertical' : '')),
			mainMenuClass = 'sm ' + ($menu.hasClass('sm-rtl') ? 'sm-rtl ' : '') + (vertical ? 'sm-vertical ' + className + ' ' + className + '-vertical' : className),
			mainMenuWidth = vertical ? '12em' : 'auto';

		// switch #main-menu theme
		$menu.smartmenus('destroy').css('width', mainMenuWidth).smartmenus(eval('(' + initOptions + ')'))[0].className = mainMenuClass;
		$('html,body').css('background', $optionElm.data('page-bg'));

		// update code samples
		$('span.themes-code-main-width').text(mainMenuWidth);
		$('span.themes-code-class span').text(className);
		$('span.themes-code-main-class span').text(mainMenuClass);
		$('span.themes-code-init-options').text(initOptions);

		// call any addon init code
		if (window.addonScriptInit) {
			try { eval(window.addonScriptInit); } catch(e) {};
		}
	});

	// init SHJS syntax highlighter
	$('<link href="../libs/demo-assets/shjs/shjs.css" rel="stylesheet" type="text/css" />').appendTo('head');
	sh_highlightDocument();

});

// load SHJS syntax highlighter synchronously
document.write('<scr' + 'ipt type="text/javascript" src="../libs/demo-assets/shjs/shjs.js"></scr' + 'ipt>');