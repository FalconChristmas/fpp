<h3>Plugins</h3>
<p>Plugins add features to FPP. They are written and published by other people, and the FPP project does
    not test, vet or guarantee their quality or safety. <b>Every plugin runs as root: once installed it has
    full control of this player</b>. It can read and change every setting, including the privacy settings,
    reach anything on the network this player is connected to, and do anything at all, whether or not its
    privacy disclosure mentions it. This is inherently dangerous. Install a plugin at your own risk, and only if
    you trust the person who wrote it. Plugins marked <b>Official</b> are maintained by the FPP project, and
    still run as root with the same access.</p>

<h4>The six privacy lights</h4>
<p>Each plugin's author fills in a short privacy disclosure, and FPP turns it into six coloured lights. They
    are shown as small dots on the plugin's card, and in full when you click the card or press
    <b>Install</b>.</p>
<ul>
    <li><b>Sends data</b> - Does it send anything off this player, and to whom?</li>
    <li><b>Collects data</b> - Does it keep information about you, your household or your visitors?</li>
    <li><b>Camera &amp; mic</b> - Does it use a camera, microphone or similar sensor, and does it record?</li>
    <li><b>Remote access</b> - Can it be reached from outside this player, or from the internet?</li>
    <li><b>System changes</b> - Does it change the player itself, beyond its own files?</li>
    <li><b>Can it be checked?</b> - Is all of its code somewhere anyone can look at it?</li>
</ul>
<p><b>Green</b> means the author says there is nothing to report under that heading. <b>Amber</b> means
    there is something you should know; tap the chip to read it. <b>Red</b> means read the line before you
    install. A line above the lights sums up the worst finding, and the install button says what you are
    agreeing to, for example <i>Install anyway</i> or <i>Install, opens FPP to internet</i>.</p>
<p>Remember that the lights come from what the author wrote. FPP does not check the plugin's code against
    its disclosure, and a green light is the author's word, not a test result. The lights describe what the
    author says the plugin does; they do not limit what it can do.</p>
<p>If a plugin keeps information about your visitors or passers-by - phone numbers, messages, votes,
    camera images - the headline reads <i>Handles other people's data</i>. Whether a plugin keeps such
    information or only passes it on to a service, what happens to it is your responsibility, not the
    author's or FPP's.</p>

<h4>Plugins with no privacy disclosure</h4>
<p>If the author has not written a disclosure, every light reads <i>not disclosed</i> and the install button
    says <i>Install, no disclosure</i>. Nothing is known about what the plugin does with data; treat it as
    unknown rather than safe. Every plugin will be required to have a disclosure from 1 January 2027, and from
    that date a plugin without one is shown in red.</p>

<h4>Checking a plugin without installing</h4>
<p>Click a card to see its lights and the author's disclosure without installing anything. Use it to see
    what an installed plugin says about itself, or to compare plugins before choosing one. In Developer UI
    mode the disclosure as the author wrote it is also shown under <i>Full disclosure</i>. For a plugin that
    is not installed, this screen shows everything the install screen would - the root warning, the
    disclosure, and the disclosure of any other plugin it depends on and would install with it - and its
    <b>Install</b> button installs straight away; what is on the screen is what you are accepting.</p>

<h4>Updates and reinstalls</h4>
<p>When a plugin update changes its disclosure, FPP shows you the new one before updating and asks you
    to accept it. <b>Update All</b> asks the same question for each such plugin before any update runs;
    the others go straight through. Cancel skips that plugin and leaves it at its current version. A change
    only to the author's summary sentence, with the rest of the disclosure the same, is updated without
    asking; a change to anything else, the author's notes included, asks.</p>
<p><b>Reinstall</b> asks the same question, because a reinstall fetches the plugin's current code: accept
    and reinstall, uninstall instead, or cancel and leave the plugin as it is. After an FPP OS upgrade the
    plugin no longer works as it is, so until it has been reinstalled the choice is only accept or uninstall.
    <b>Reinstall All</b> asks about each changed plugin in turn before anything is removed, with those two
    choices. When more than one plugin needs an answer, <b>Reinstall All</b> and <b>Update All</b> first show
    a screen saying which plugins will be asked about, then one screen per plugin.
    A plugin installed before FPP kept a record of accepted disclosures is asked about once. A plugin that
    cannot be checked, for example because the player is offline, is left as it is. If only the installed
    copy's origin has gone (its branch was renamed or the repository moved) but the plugin list still points
    at it, the check says so: <b>Update</b> cannot run for that plugin, and <b>Reinstall</b> clones it afresh.</p>

<h4>Other buttons on a card</h4>
<ul>
    <li><b>Open</b> - Opens the plugin's own page, if it has one.</li>
    <li><b>Reinstall</b> - Removes and reinstalls the plugin. Try this after an FPP upgrade, or when a plugin
        has stopped working.</li>
    <li><b>Uninstall</b> - Removes the plugin. Its privacy disclosure says what it changed outside its own
        files under <b>System changes</b>, and what it stored and where under <b>Collects data</b>; whether
        the uninstall undoes those is up to the plugin, so check the disclosure's notes and the listed
        locations afterwards.</li>
</ul>
