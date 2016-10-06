# colpick - A jQuery Color Picker

colpick is a simple Photoshop-style color picker jQuery plugin. Its interface is familiar for most users and it's also very lightweight loading less than 30 KB to the browser.


* **No images!** Just a JS and a CSS file
* Very intuitive Photoshop-like interface
* Light and dark easy-to-customize CSS3 skins
* Only **29 KB total**, even less if minified and gziped
* Works and looks nice **even on IE7**
* Extremely easy to implement

## Installation
Add **colpick** to your project using your the tool of you choice or download the files.

### Bower
```bash
bower install jquery-colpick --save
```

### npm
```bash
npm install jquery-colpick --save
```

## Usage
Include `colpick.js` and `colpick.css` to into your application. Make sure you have included jQuery (v1.7.0+) as well.
```html
<script src="js/colpick.js" type="text/javascript"></script>
<link rel="stylesheet" href="css/colpick.css" type="text/css"/>
```

### Browserify/Node
Require the module in your application. Don't forget to include the css file as well.
```js
require('jquery-colpick');
```

Now you may call the colpick method on any jQuery object to create a color picker. By default you get a dropdown color picker.
```
<button id="colorpicker">Show Color Picker</button>
$('#colorpicker').colpick();
```

For more examples see [Usage examples](example/index.html).


##Options##
<table>
  <thead>
    <tr>
      <th>Option</th>
      <th>Type</th>
      <th>Description</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>flat</td><td>boolean</td>
      <td>Flat mode just displays the color picker in place, always visible, with no show/hide behavior. Use it with colpickShow() and colpickHide() to define your own show/hide behavior or if you want the picker to be always visible. Default: false</td>
    </tr>
    <tr>
      <td>layout</td><td>string</td>
      <td>Name of the color picker layout to use. Posible values are 'full' (RGB, HEX, and HSB fields), 'rgbhex' (no HSB fields) and hex (no HSB, no RGB). You can see the full layout at the top of the page. rgbhex and hex layouts are shown in the examples below. Default: 'full'</td>
    </tr>
    <tr>
      <td>submit</td><td>boolean</td>
      <td>If false the picker has no submit or OK button and no previous color viewer. If false use onChange function to get the picked color. Default: true</td>
    </tr>
    <tr>
      <td>colorScheme</td><td>string</td>
      <td>The color scheme to use for the picker, 'light' or 'dark'. Default: 'light'</td>
    </tr>
    <tr>
      <td>submitText</td><td>string</td>
      <td>The text to show on the submit button if active. Default: 'OK'</td>
    </tr>
    <tr>
      <td>color</td><td>string or object</td>
      <td>Default color. Hex string (eg. 'ff0000') or object for RGB (eg. {r:255, g:0, b:0}) and HSB (eg. {h:0, s:100, b:100}). String 'auto' to read color from the element's value attribute. Default: 'auto'</td>
    </tr>
    <tr>
      <td>showEvent</td><td>string</td>
      <td>Event that shows the color picker. Default: 'click'</td>
    </tr>
    <tr>
      <td>livePreview</td><td>boolean</td>
      <td>If true color values change live while the user moves a selector or types in a field. Turn it off to improve speed. Default: true</td>
    </tr>
    <tr>
      <td>polyfill</td><td>boolean or function</td>
      <td>If true, the color picker is only activated when no native browser behavior is available. Use a function (should receive a colorpicker DOM object) to determine the option dynamically (e.g. by user-agent). Default: false</td>
    </tr>
    <tr>
      <td>appendTo</td><td>DOM element</td>
      <td>
        Append the picker to the specified DOM element.<br>
        Defaults:
        <ul>
          <li>flat: true -> this (the element itself)</li>
          <li>flat: false -> document.body</li>
      </td>
    </tr>
    <tr>
      <td>style</td><td>object</td>
      <td>Set additional styles to the picker. Will accept any object that could be passed to <a href="http://api.jquery.com/css/#css-properties">jQuery's .css method</a>. Default: null</td>
    </tr>
    <tr>
      <td>onBeforeShow</td><td>function</td>
      <td>Callback function triggered before the color picker is shown. Use it to define custom behavior. Should receive a colorpicker DOM object.</td>
    </tr>
    <tr>
      <td>onShow</td><td>function</td>
      <td>Callback function triggered when the color picker is shown. Use it to define custom behavior. Should receive a colorpicker DOM object.</td>
    </tr>
    <tr>
      <td>onHide</td><td>function</td>
      <td>Callback function triggered when the color picker is hidden. Use it to define custom behavior. Should receive a colorpicker DOM object.</td>
    </tr>
    <tr>
      <td>onChange</td><td>function</td>
      <td>Callback function triggered when the color is changed. This is the function that allows you to get the color picked by the user whenever it changes, whithout the user pressing the OK button. Should receive:
        <ul>
          <li>HSB object (eg. {h:0, s:100, b:100})</li>
          <li>HEX string (with no #)</li>
          <li>RGB object(eg. {r:255, g:0, b:0})</li>
          <li>el element, the parent element on which colorpicker() was called. Use it to modify this parent element on change (see first example below).
          <li>bySetColor flag: if true, the onChange callback was fired by the colpickSetColor function and not by the user changing the color directly in the picker. There are some cases in which you'll want a different behaviour in this case (see last example).</li>
        </ul>
      </td>
    </tr>
    <tr>
      <td>onSubmit</td><td>function</td>
      <td>Callback function triggered when the color is chosen by the user, using the OK button. Should receive exactly the same as onChange. It's never fired if using submit:0 option.</td>
    </tr>
  </tbody>
</table>


##jQuery.fn Functions##
<table>
  <thead>
    <tr>
      <th>Function</th>
      <th>Description</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>colpick(options)</td>
      <td>The main function used to create a color picker.</td>
    </tr>
    <tr>
      <td>colpickHide()</td>
      <td>Hides the color picker. Accepts no arguments. Use it to hide the picker with an external trigger.</td>
    </tr>
    <tr>
      <td>colpickShow()</td>
      <td>Shows the color picker. Accepts no arguments. Use it to show the picker with an external trigger.</td>
    </tr>
    <tr>
      <td>colpickSetColor(col,setCurrent)</td>
      <td>Use it to set the picker color. The onChange callback is fired with bySetColor set to true. Parameters:
        <ul>
          <li>col: a hex string (eg. 'd78b5a') or object for RGB (eg. {r:255, g:0, b:0}) and HSB (eg. {h:150, s:50, b:50})</li>
          <li>setCurrent: If true the color picker's current color (the one to the right in layouts with submit button) is set in addition to the new color, which is always set.</li>
        </ul>
      </td>
    </tr>
  </tbody>
</table>
    
    
##jQuery Functions##
<table>
  <thead>
    <tr>
      <th>Function</th>
      <th>Description</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>$.colpick.rgbToHex(rgb)</td>
      <td>Receives an object like {r:255, g:0, b:0} and returns the corresponding HEX string (with no #).</td>
    </tr>
    <tr>
      <td>$.colpick.rgbToHsb(rgb)</td>
      <td>Receives an object like {r:255, g:0, b:0} and returns the corresponding HSB object (eg. {h:0, s:100, b:100}). HSB values are not rounded. H is in the [0,360] range, while S and B are in the [0,100] range.</td>
    </tr>
    <tr>
      <td>$.colpick.hsbToHex(hsb)</td>
      <td>Receives an object like {h:0, s:100, b:100} and returns the corresponding HEX string (with no #).</td>
    </tr>
    <tr>
      <td>$.colpick.hsbToRgb(hsb)</td>
      <td>Receives an object like {h:0, s:100, b:100} and returns the corresponding RGB object (eg. {r:255, g:0, b:0}). RGB values are whole numbers in the [0,255] range.</td>
    </tr>
    <tr>
      <td>$.colpick.hexToHsb(hex)</td>
      <td>Receives a hex string with no # and returns the corresponding HSB object (eg. {h:0, s:100, b:100}). HSB values are not rounded. H is in the [0,360] range, while S and B are in the [0,100] range.</td>
    </tr>
    <tr>
      <td>$.colpick.hexToRgb(hex)</td>
      <td>Receives a hex string with no # and returns the corresponding RGB object (eg. {r:255, g:0, b:0}). RGB values are whole numbers in the [0,255] range.</td>
    </tr>
  </tbody>
</table>
	
  
##Layouts##
<table>
  <thead>
    <tr>
      <th>Layout</th>
      <th>Image</th>
    </tr>
  </thead>
  <tbody>
      <tr>
        <td>full:</td>
        <td><img src="http://colpick.com/images/colpick_full.jpg" alt="colpick full layout"/></td>
      </tr>
      <tr>
        <td>rgbhex:</td>
        <td><img src="http://colpick.com/images/colpick_rgbhex.jpg" alt="colpick rgbhex layout"/></td>
      </tr>
      <tr>
        <td>hex:</td>
        <td><img src="http://colpick.com/images/colpick_hex.jpg" alt="colpick hex layout"/></td>
      </tr>
  </tbody>
</table>


##Changes to josedvq's version##
* **Polyfill:** New option 'polyfill' to work with native color input fields
* **Auto color:** Get the default color from an element's 'value' attribute using jQuery function .val()
* **Custom parent:**  New option 'appendTo' to specify which element to append the picker to ([PR #44](https://github.com/josedvq/colpick-jQuery-Color-Picker/pull/44))
* **CSS styles:** New option 'styles' to specify additional styles to be set on the picker ([PR #44](https://github.com/josedvq/colpick-jQuery-Color-Picker/pull/44))
* **UMD compatibility:** Uses an UMD style closure to be loadable with AMD loaders (require.js) or CommonJS
* **3 character hex support:** Added support for entering three character hex codes as specificied in the CSS 2.1 spec ([PR #43](https://github.com/josedvq/colpick-jQuery-Color-Picker/pull/43))
* **Fixed Issues:** [#16](https://github.com/josedvq/colpick-jQuery-Color-Picker/issues/16), [#17](https://github.com/josedvq/colpick-jQuery-Color-Picker/issues/17), [PR#58](https://github.com/josedvq/colpick-jQuery-Color-Picker/pull/48)


Dual licensed under the MIT and GPLv2 licenses.

Fork of josedvq's [colpick](https://github.com/josedvq/colpick-jQuery-Color-Picker)  
Based on Stefan Petre's [color picker](http://www.eyecon.ro/colorpicker/)
