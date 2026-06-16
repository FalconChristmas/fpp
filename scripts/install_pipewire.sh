#!/bin/bash
#####################################
# install_pipewire.sh
#
# Standalone script to install/repair the GStreamer + PipeWire audio
# stack on existing FPP systems. Idempotent — safe to run multiple
# times. Performs the same steps as upgrade/105/upgrade.sh but can
# be invoked manually at any time.
#
# Installs:
#   - PipeWire (audio graph, combine-streams, filter-chains)
#   - WirePlumber (session manager, ALSA monitor, link policy)
#   - pipewire-pulse (PulseAudio compatibility)
#   - GStreamer 1.x (media playback, AES67 RTP, HDMI video)
#   - linuxptp (PTP clock for AES67)
#   - pulseaudio-utils (pactl for diagnostics)
#
# Usage:  sudo /opt/fpp/scripts/install_pipewire.sh
#####################################

BINDIR=$(cd $(dirname $0) && pwd)
. ${BINDIR}/common
. ${BINDIR}/functions

echo "FPP - Installing/repairing GStreamer + PipeWire audio stack"
echo "==========================================================="

FPPPLATFORM=$(cat /etc/fpp/platform 2>/dev/null)

if [ "${FPPPLATFORM}" == "MacOS" ]; then
    echo "Not supported on macOS."
    exit 0
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: This script must be run as root (sudo)."
    exit 1
fi

# --- 1. Ensure packages are installed ---
echo ""
echo "Step 1: Checking required packages..."

# Core PipeWire + WirePlumber stack
REQUIRED_PKGS="
    pipewire
    pipewire-bin
    pipewire-alsa
    pipewire-pulse
    pipewire-jack
    pipewire-audio-client-libraries
    wireplumber
    libpipewire-0.3-dev
    pulseaudio-utils
    linuxptp
"

# GStreamer core + plugins needed by FPP
REQUIRED_PKGS="${REQUIRED_PKGS}
    gstreamer1.0-tools
    gstreamer1.0-plugins-base
    gstreamer1.0-plugins-good
    gstreamer1.0-plugins-bad
    gstreamer1.0-plugins-ugly
    gstreamer1.0-pipewire
    gstreamer1.0-libav
    libgstreamer1.0-dev
    libgstreamer-plugins-base1.0-dev
"

# Optional packages (best-effort, don't fail if unavailable)
OPTIONAL_PKGS="
    gstreamer1.0-gl
    gstreamer1.0-x
    libgstreamer-plugins-bad1.0-0
    yt-dlp
"

PKGS_NEEDED=""
for pkg in ${REQUIRED_PKGS}; do
    dpkg -l ${pkg} 2>/dev/null | grep -q '^ii' || PKGS_NEEDED="${PKGS_NEEDED} ${pkg}"
done

if [ -n "${PKGS_NEEDED}" ]; then
    echo "  Installing required packages:${PKGS_NEEDED}"
    apt-get update -q
    apt-get -y -q -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" install ${PKGS_NEEDED}
    if [ $? -ne 0 ]; then
        echo "  WARNING: Some required packages failed to install."
    fi
else
    echo "  All required packages already installed."
fi

OPTS_NEEDED=""
for pkg in ${OPTIONAL_PKGS}; do
    dpkg -l ${pkg} 2>/dev/null | grep -q '^ii' || OPTS_NEEDED="${OPTS_NEEDED} ${pkg}"
done
if [ -n "${OPTS_NEEDED}" ]; then
    echo "  Installing optional packages (best-effort):${OPTS_NEEDED}"
    apt-get -y -q install ${OPTS_NEEDED} 2>/dev/null || true
fi

# --- 2. PipeWire base configuration ---
echo ""
echo "Step 2: Setting up PipeWire base configuration..."
mkdir -p /etc/pipewire /etc/pipewire/pipewire.conf.d

for conf in pipewire.conf pipewire-pulse.conf client.conf; do
    if [ ! -f "/etc/pipewire/${conf}" ] && [ -f "/usr/share/pipewire/${conf}" ]; then
        cp "/usr/share/pipewire/${conf}" "/etc/pipewire/${conf}"
        echo "    Copied ${conf}"
    else
        echo "    ${conf} already exists."
    fi
done

# --- 3. FPP PipeWire config overlay ---
echo ""
echo "Step 3: Deploying FPP PipeWire configuration overlay..."
if [ -d /opt/fpp/etc/pipewire/pipewire.conf.d ]; then
    cp -a /opt/fpp/etc/pipewire/pipewire.conf.d/. /etc/pipewire/pipewire.conf.d/
    echo "    Deployed FPP PipeWire config (48kHz, quantum 256, RT priority)"
else
    echo "    WARNING: /opt/fpp/etc/pipewire/pipewire.conf.d not found in repo!"
fi

# --- 4. WirePlumber configuration ---
echo ""
echo "Step 4: Setting up WirePlumber configuration..."
mkdir -p /etc/wireplumber/wireplumber.conf.d

if [ -d /opt/fpp/etc/wireplumber/wireplumber.conf.d ]; then
    cp -a /opt/fpp/etc/wireplumber/wireplumber.conf.d/. /etc/wireplumber/wireplumber.conf.d/
    echo "    Deployed FPP WirePlumber config (systemwide session, no ALSA reservation)"
else
    echo "    WARNING: /opt/fpp/etc/wireplumber/wireplumber.conf.d not found in repo!"
fi

# FPP WirePlumber Lua hooks (static, e.g. the combine-stream default-target
# fallback blocker referenced by 60-fpp-block-combine-fallback.conf).
if [ -d /opt/fpp/etc/wireplumber/scripts ]; then
    mkdir -p /usr/share/wireplumber/scripts
    cp -a /opt/fpp/etc/wireplumber/scripts/. /usr/share/wireplumber/scripts/
    echo "    Deployed FPP WirePlumber Lua hooks"
fi

# Remove old WirePlumber 0.4 Lua configs (not supported by WirePlumber 0.5+)
if [ -d /etc/wireplumber/main.lua.d ]; then
    echo "    Removing old WirePlumber 0.4 Lua configs..."
    rm -rf /etc/wireplumber/main.lua.d
fi

# --- 5. systemd service files ---
echo ""
echo "Step 5: Installing systemd service files..."
for svc in fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service; do
    if [ -f "/opt/fpp/etc/systemd/${svc}" ]; then
        cp "/opt/fpp/etc/systemd/${svc}" /lib/systemd/system/
        echo "    Installed ${svc}"
    else
        echo "    WARNING: /opt/fpp/etc/systemd/${svc} not found!"
    fi
done

# Also refresh fppd.service (picks up /run/fppd/fpp-audio.env EnvironmentFile)
if [ -f "/opt/fpp/etc/systemd/fppd.service" ]; then
    cp /opt/fpp/etc/systemd/fppd.service /lib/systemd/system/
    echo "    Updated fppd.service"
fi

systemctl daemon-reload
# The PipeWire services are intentionally left DISABLED: FPPINIT's setupAudio
# starts them on demand (from its audio thread) once it has written/validated the
# audio config, so PipeWire reads the correct graph on its first and only start
# instead of coming up empty at sound.target and needing a restart. Disable here
# to undo any enablement from a prior install.
systemctl disable fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service 2>/dev/null || true
echo "    Services installed (left disabled; started on demand by setupAudio)."

# --- 6. Mask user-session PipeWire services ---
echo ""
echo "Step 6: Masking user-session PipeWire services..."
mkdir -p /home/fpp/.config/systemd/user
for svc in pipewire.socket pipewire.service pipewire-pulse.service pipewire-pulse.socket wireplumber.service; do
    ln -sf /dev/null "/home/fpp/.config/systemd/user/${svc}"
done
chown -R fpp:fpp /home/fpp/.config
echo "    User-session PipeWire services masked."

# --- 7. Create runtime directory ---
echo ""
echo "Step 7: Ensuring PipeWire runtime directory exists..."
mkdir -p /run/pipewire-fpp/pulse
chmod 755 /run/pipewire-fpp
chmod 755 /run/pipewire-fpp/pulse
echo "    /run/pipewire-fpp ready."

# --- 8. Restart PipeWire services ---
echo ""
echo "Step 8: (Re)starting PipeWire services..."
systemctl stop fpp-pipewire-pulse.service 2>/dev/null || true
systemctl stop fpp-wireplumber.service 2>/dev/null || true
systemctl stop fpp-pipewire.service 2>/dev/null || true
sleep 1

systemctl start fpp-pipewire.service
sleep 2
systemctl start fpp-wireplumber.service
sleep 3
systemctl start fpp-pipewire-pulse.service
sleep 1

# --- 9. Build PipeWire GStreamer plugin from source (video mode=provide fix) ---
echo ""
echo "Step 9: Checking PipeWire GStreamer plugin for mode=provide support..."
PLUGIN_HAS_PROVIDE=$(PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp \
    gst-inspect-1.0 pipewiresink 2>/dev/null | grep -c "provide" || true)
if [ "${PLUGIN_HAS_PROVIDE}" -gt 0 ]; then
    echo "    GStreamer pipewiresink already supports mode=provide — skipping build."
else
    echo "    Stock plugin lacks mode=provide fix.  Building from source..."
    if [ -x /opt/fpp/scripts/build_pipewire_gst_plugin.sh ]; then
        /opt/fpp/scripts/build_pipewire_gst_plugin.sh 1.6.0 || {
            echo "    WARNING: Plugin build failed. Video Input Sources (mode=provide) may crash."
            echo "    Run manually: sudo /opt/fpp/scripts/build_pipewire_gst_plugin.sh 1.6.0"
        }
    else
        echo "    WARNING: build_pipewire_gst_plugin.sh not found."
    fi
fi

# --- 10. Verify ---
echo ""
echo "Step 10: Verifying status..."

ALL_OK=true
for svc in fpp-pipewire fpp-wireplumber fpp-pipewire-pulse; do
    STATUS=$(systemctl is-active ${svc}.service 2>/dev/null)
    if [ "${STATUS}" == "active" ]; then
        echo "    ${svc}: active"
    else
        echo "    ${svc}: ${STATUS} (PROBLEM)"
        ALL_OK=false
    fi
done

echo ""
echo "  Checking GStreamer elements..."
GST_OK=true
for element in pipewiresink pipewiresrc audioconvert audioresample volume appsink decodebin; do
    if gst-inspect-1.0 ${element} &>/dev/null; then
        echo "    ${element}: OK"
    else
        echo "    ${element}: MISSING"
        GST_OK=false
    fi
done

echo ""
if ${ALL_OK} && ${GST_OK}; then
    echo "Installation complete — GStreamer + PipeWire audio stack is ready."
    echo ""
    echo "To verify audio sinks, run:"
    echo "  PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp wpctl status"
    echo ""
    echo "If the MediaBackend setting is not yet set to 'pipewire', update it via"
    echo "the FPP web UI under Status/Control > FPP Settings > Audio."
else
    if ! ${ALL_OK}; then
        echo "WARNING: Some PipeWire services are not running."
        echo "  Check logs: journalctl -u fpp-pipewire -u fpp-wireplumber -u fpp-pipewire-pulse --no-pager -n 50"
    fi
    if ! ${GST_OK}; then
        echo "WARNING: Some GStreamer elements are missing. Media playback or AES67 may not work."
    fi
fi
