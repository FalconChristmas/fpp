<h3>FPP Backups</h3>
<p>This page creates, downloads and restores backups of the FPP configuration, and can also copy raw files or OS images to other FPPs. Backups are JSON snapshots stored in two places at once: <code>config/backups → /home/fpp/media/config/backups</code> (on the media/flash drive that travels with your sequences) and, optionally, a second copy on an additional USB device you nominate. <b>What a backup contains:</b> everything needed to restore this player, including passwords, the WiFi passphrase and access tokens, in plain text. That is what makes it able to bring a box back. Treat the file as you would the passwords themselves: do not attach it to a forum post or a bug report. <i>Retained: this warning is the most important original note and is kept verbatim in meaning.</i></p>

<h4>Backup Configuration</h4>
<ul>
    <li><b>Areas to back up</b> — Checkboxes for groups such as <b>Settings</b>, <b>Network</b>, <b>Channel Outputs</b>, <b>Playlists</b>, <b>Sequences</b>, <b>Universes</b>, <b>Schedule</b>, and <b>All</b>. Select which settings you want to backup, then hit download. A copy is also saved in <code>/media/config</code> (the USB stick with your sequences). <i>Retained: original Backup sentence kept.</i></li>
    <li><b>Protect sensitive data</b> (default checked) — Removes passwords/keys before writing the file. Uncheck only when you intend an exact clone on a box you control — then the file contains credentials in plain text and you assume responsibility for it.</li>
    <li><b>Download Backup</b> — Creates the JSON and triggers a browser download. The same file is also left in <code>config/backups</code> for later restore without re-uploading. Filename encodes area and date.</li>
</ul>

<h4>Alternative Storage copy</h4>
<p>Below the backup options, the page shows <b>Specify an additional storage device where configuration backups will be copied to. Backups will first be saved to the config directory … and then copied to the alternative location. Each copied backup carries this player's credentials in plain text, and a USB stick is not encrypted.</b> This is the “copy to another drive” feature. Pick a mounted USB path from the dropdown (populated from connected storage devices). When set, every new backup is automatically duplicated there after being saved locally. The field’s help icon repeats the plain-text warning verbatim.</p>

<h4>Download Existing Backups</h4>
<ul>
    <li>Table of backups already on the local <code>config/backups</code> directory: <b>Filename</b>, <b>Size</b>, <b>Date</b>, with <b>Download</b> and <b>Delete</b> per row. Use to grab an older backup without recreating it.</li>
    <li><b>Copy Backups to USB</b> — Appears when an alternative location is configured. Copies all existing local backups to that USB path in one go (streamed with progress), without creating a new backup.</li>
</ul>

<h4>Restore Configuration</h4>
<ul>
    <li><b>Restore area</b> (dropdown) — Choose a single area to restore (e.g. Channel Outputs) or <b>All</b>. Restoration works the same way as backup: choose an area to restore, choose the JSON file. You may use the full/All backup and restore only one area from it — for example restore the schedule only out of a full backup. <i>Retained: original Restoration sentence kept.</i></li>
    <li><b>Restore file</b> (file picker) — Select the JSON you previously downloaded or that already lives in <code>config/backups</code>. The file can be a full backup even when you are restoring a single area — only that area is applied; the rest is ignored.</li>
    <li><b>Keep Existing Master/Slave Settings</b> (default checked) — When checked, the restore does not overwrite this system’s Master/Slave (Player/Remote) role. Leave checked when cloning a config to another box so you do not accidentally turn a Player into a Remote or vice versa. <i>Retained: original note kept; the file still uses the historic label even though the modern mode is Player/Remote.</i></li>
    <li><b>Keep Network Settings</b> (default checked) — When checked, the restore keeps the current network (IPs, gateway, DNS, hostname). Leave checked when cloning so the target does not steal the donor’s address. <i>Retained: original note kept.</i></li>
    <li><b>Restore</b> (button) — Applies the chosen area from the chosen file, merges it into the live config, and shows success/failure toasts. After restore, reboot or restart FPPD if the affected subsystem requires it (the page will suggest it).</li>
</ul>

<h4>Restore Existing Backups (table of on-device backups)</h4>
<ul>
    <li>Lists backups already in <code>config/backups</code> (and alternative location if set). Each row offers <b>Apply</b> (restore that area from that file without re-uploading) and <b>Delete</b>. The same Master/Slave and Network keep-checkboxes apply.</li>
</ul>

<h4>File Copy Backup/Restore (bottom section)</h4>
<ul>
    <li><b>Direction</b> — <b>FROM FPP</b> (backup files to this browser/computer) vs <b>TO FPP</b> (restore files to this FPP) — the title text swaps between “FPP File Copy Backup” and “FPP File Copy Restore” accordingly.</li>
    <li><b>File groups</b> — Checkboxes for raw file areas: Sequences, Media, Effects, Scripts, etc. Copies the actual files, not JSON config.</li>
    <li><b>Copy dialog</b> (<code>#copyPopup</code> title “FPP Backup/Restore”) — Streams the copy with progress. For restores, files are placed in their proper directories on the target.</li>
    <li><b>Do You Want To Copy Existing Backups to USB?</b> (<code>#dialog_copyToUsb</code>) — Confirmation prompt shown when the alternative location is first configured, offering to retroactively copy all existing local backups there.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Always keep <b>Protect sensitive data</b> checked unless you are doing an immediate clone on a private network.</li>
    <li>Use the <b>Alternative Storage</b> copy to keep a second physical copy on a USB stick that stays with the show — but remember it is plain text and not encrypted.</li>
    <li>When cloning to another FPP, leave both <b>Keep Existing Master/Slave</b> and <b>Keep Network Settings</b> checked so the target keeps its own role and address.</li>
</ul>
