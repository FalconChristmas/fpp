<h3>Proxied Hosts</h3>
<p>This page lets you reach FPPs that are not directly addressable from your browser (for example a Remote behind a show-network router) by proxying their web interface through another FPP you can reach. Entries here are a convenience; the actual proxy is per-host and works once saved — you do not need to be on this page to use it, only to maintain the list.</p>

<h4>Proxied Hosts (Manually Maintained)</h4>
<ul>
    <li><b>Table</b> — Shows each manually configured proxy: <b>Host Name</b> (the name you will browse to, e.g. <code>remote-fpp</code>), <b>Host Address</b> (IP or resolvable name of the real target), and <b>Actions</b> (Edit/Delete). The table is the authoritative list for hosts you explicitly want proxied; it is stored on the FPP and survives reboots.</li>
    <li><b>Add / Edit dialog</b> — Fields: <b>Host Name</b> (short name you will type in the browser, no spaces) and <b>Host Address</b> (IP address or DNS name of the actual target). Both are required and validated before save.</li>
    <li><b>Delete</b> — Removes the entry. Proxied requests to that name will then fail until the host is directly reachable.</li>
    <li><b>How proxying works</b> — When you visit a manually proxied name, this FPP forwards HTTP to the stored Host Address and returns the response, so the remote’s UI appears as if you were on its own network. The entry does not create DNS records — it only affects requests that arrive at this FPP via the proxy name.</li>
</ul>

<h4>DHCP Proxied Hosts (Auto Populated)</h4>
<ul>
    <li><b>Table</b> — Auto-discovered hosts learned from DHCP leases on this FPP’s own DHCP server (the “Tethering” / “DHCP Server” interface in Network Settings). Each row shows the lease hostname and the leased IP, refreshed from the lease file. This is read-only; you cannot Add/Edit here — entries appear and disappear as devices obtain and release leases.</li>
    <li><b>When it appears</b> — Only when this FPP is acting as the network’s DHCP server (e.g. show-LAN master). If you are not serving DHCP, this table stays empty, which is normal.</li>
</ul>

<h4>Tips</h4>
<ul>
    <li>Use <b>Manually Maintained</b> for a Remote that is always behind the same router with a stable address, and <b>DHCP Proxied Hosts</b> when remotes come and go on a show network that this FPP itself numbers.</li>
    <li>Keep Host Names short, lower-case and unique across both tables so they do not collide with real DNS names on your main network.</li>
    <li>If a proxied host stops loading, verify the stored Host Address still pings from this FPP and that the upstream router’s DHCP lease for it has not changed.</li>
</ul>
