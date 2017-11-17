#!/bin/bash
#
# Install console warning
#

#######################################
# Print notice during login regarding console access
cat <<-EOF >> /etc/motd
[0;31m
                   _______  ___
                  / __/ _ \/ _ \\
                 / _// ___/ ___/ [0m FalconPiPlayer[0;31m
                /_/ /_/  /_/
[1m
This FPP console is for advanced users, debugging, and developers.  If
you aren't one of those, you're probably looking for the web-based GUI.

You can access the UI by typing "http://fpp/" into a web browser.[0m
	EOF
