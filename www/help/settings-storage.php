<h3>Storage</h3>
<p>Where your media lives and how to move FPP onto another drive or fix storage. Changes here can erase data — read the warnings in each dialog before confirming.</p>

<h4>SD Card Actions (when booted from SD/eMMC and unpartitioned space exists)</h4>
<ul>
    <li><b>Grow Filesystem</b> (button) — Expands the current filesystem to use the whole card. Requires reboot to take effect (confirmation dialog + progress dialog via <code>growsd.php</code>).</li>
    <li><b>New Partition</b> (button, Advanced) — Creates a new partition in the unused space of the card. After reboot you can format it and select it as the storage location.</li>
</ul>

<h4>Flash FPP to Another Device (every flashable target not currently booted/mounted)</h4>
<ul>
    <li><b>Create vs Copy</b> — <b>Create</b> writes a clean FPP install (media/sequences/settings left behind). <b>Copy</b> clones this device’s media, sequences and settings too. Both generate fresh SSH host keys for the copy so two players can safely coexist on the same network. <b>Everything on the target device is erased.</b></li>
    <li><b>Create (BTRFS)</b> (button, Advanced, eMMC, BeagleBone Black only, not PocketBeagle2) — Same as Create but compresses the root filesystem with BTRFS to save eMMC space at some CPU cost.</li>
    <li>Each target row shows <b>name</b> and <b>/dev/&lt;device&gt;</b> with its own Create/Copy buttons (invokes <code>flash-storage.php</code> and sets <i>Reboot Required</i> on success).</li>
</ul>

<h4>Storage Device</h4>
<ul>
    <li><b>Storage Device</b> (dropdown) — Partition where <code>mediaDirectory</code> lives. Built from detected partitions (<code>sd[a-z][0-9]</code>, <code>mmcblk*p*</code>, <code>nvme*n*p*</code>). Labels show size, free space, and tags: <b>(current storage device)</b>, <b>(boot device)</b>, <b>(USB)</b>. Changing it triggers a chooser: don’t format, or format as <b>ext4 (most stable)</b>, <b>exFAT (experimental)</b>, or <b>FAT (unsupported, slow)</b> via <code>formatstorage.php</code>, then optionally copies all files (<code>copystorage.php</code>). <i>Do not pick a “Not Mounted” entry without formatting first.</i></li>
    <li><b>Pi 4/5 USB storage warning</b> (callout) — USB/SATA on Pi 4/5 can cause network lag, packet drops, audio clicks and higher boot time; many capes/hats and advanced features are not tested with it, so you assume higher upgrade risk. Pi 5 limits USB to 600 mA without a 5A/27W supply — SSDs may brown out. Flashing USB adds <code>usb_max_current_enabled=1</code> for 1.6 A on the copy.</li>
    <li><b>Generic USB warning</b> (other platforms) — Recommends USB 3.0 SSDs over thumb drives and good cooling for the Pi’s USB hub.</li>
</ul>

<h4>Mounted USB Device Actions</h4>
<ul>
    <li>Listed from <b>GetAvailableBackupsDevices</b>. Each mounted usable device shows name, vendor, model, size and mount point, plus <b>Force Unmount</b> (POST <code>api/backups/devices/unmount/{dev}/{mount}</code>) and, when open files exist, an <b>Open files</b> panel from <code>lsof</code>. When none are usable, “No Mounted USB Detected.” is shown. The yellow callout warns that forcing a unmount while files are open can corrupt or lose data.</li>
</ul>
