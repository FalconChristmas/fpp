<h3>Output Processors</h3>
<p>The Output Processors page allows adding different types of processing to the channel data prior to the channels being sent to the outputs.  These include:</p>
<br>

<h4>Channel Remaps</h4>
<p>Allows output channels to be remapped onto other channels before being sent out to controllers.  This can be use for the following:</p>
<ul>
<li>Copying 1 channel of data to another to work around a bad controller port</li>
<li>Copying multiple channels of data to replace a whole controller with another without having to reconfigure the replacement controller with the original controller's start channel configuration.</li>
<li>Duplicating channel data across two or more controllers for instance two identical matrices or pixel trees.</li>
</ul>

<h4>Brightness</h4>
<p>Allows adjusting the brightness and gamma of a range of channels</p>

<h4>Hold Value</h4>
<p>Holds the channel value at the last non-zero value when a zero is received.</p>

<h4>Set Value</h4>
<p>Sets the exact value on a range of channels.  This is useful if some channels are not working properly and need to always be set off (possibly a pixel not working correctly) or on (always turn on a tune to sign). </p>

<h4>Override Zero</h4>
<p>Sets the exact value on a range of channels, but only when the source value is zero. </p>

<h4>Reorder Colors</h4>
<p>Change the order of colors for a range of nodes</p>


<h4>Three to Four</h4>
<p>Expands RGB colors to four channel RGBW.  There are three algorithms: "No White" will set the white channel as off.   "R=G=B" will
    set the white channel if all three other channels are equal.   "Advanced" uses some complex algorithms to try and determine when the white channel
should be used to fill in some of the color space of the desired colors.</p>
