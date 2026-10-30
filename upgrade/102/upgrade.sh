#!/bin/bash
#####################################

# Installed the networkd-dispatcher routable.d/ntpd hook that forced an NTP
# sync when the network came up (GitHub issue #2535). The switch to chrony
# removed that hook from the tree; upgrade 116 installs the chrony one in its
# place and removes the ntpd one, and 159 checks it again. Copying the old
# hook here failed, and a failing step stops the upgrade below 102 for good.
exit 0
