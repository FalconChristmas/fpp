#!/bin/bash
#####################################
# Upgrade 124: REMOVED - it made things worse, see upgrade/125
#
# This upgrade used to build the PipeWire GStreamer plugin from upstream 1.6.0
# to pick up the mode=provide buffer-lifecycle fix.  It must not run: a 1.6.0
# plugin is compiled against 1.6.0's in-tree libpipewire headers but at runtime
# resolves libpipewire-0.3.so.0 to the distro's 1.4.2 library, and the
# resulting mismatch breaks audio playback outright -- the pipewiresink stream
# node never leaves "suspended", GStreamer never reports a position, and the
# playlist runs silently with seconds_elapsed pinned at 0.  Verified by A/B on
# an FPP dev Pi: 1.6.0 = silence, restore 1.4.2 = audio back immediately.
#
# Swapping just the plugin is only safe when it matches the libpipewire the
# process actually loads.  build_pipewire_gst_plugin.sh now refuses to install
# a mismatched build unless forced, and is manual-only again.
#
# upgrade/125 repairs devices that already ran this.
#####################################

echo "Upgrade 124 was removed - see upgrade/125"

exit 0
