#!/bin/bash
#############################################################################
# pipewire_diagnostics.sh
#
# Health checks for FPP's PipeWire / WirePlumber / GStreamer media stack,
# written for the Troubleshooting page (www/troubleshoot-commands.json) and
# for the support bundle that page generates.
#
# Every check prints one of:
#     [PASS]  the thing is as it should be
#     [WARN]  works, but something will bite later or is only probably fine
#     [FAIL]  broken - this is why audio/video is not working
#     [INFO]  context, no verdict
#     [SKIP]  not applicable / not configured on this device
#
# so a user can paste the output into a support thread and a developer can
# read the verdict column without knowing the machine.  The raw-dump commands
# in the Media Backend group stay as they were; these answer questions
# instead of printing state.
#
# Usage:  pipewire_diagnostics.sh <section>
#   services     daemons, sockets, restart loops, realtime scheduling
#   config       conf.d integrity, JSON validity, deployed-vs-cached drift
#   graph        what the generated conf declares vs what the graph has
#   alsa         cards, contention, channel counts, dummy device
#   gstreamer    element availability, plugin/library version mismatch
#   network      AES67, Opus RTP, RTSP outputs, PTP, multicast
#   video        capture devices, video sources, video routing
#   performance  quantum, sample rate, xruns, throttling
#   smoke        silent end-to-end pipeline negotiation test
#   all          every section
#############################################################################

PATH=/bin:/usr/bin:/sbin:/usr/sbin:${PATH}
export PATH

BINDIR=$(cd "$(dirname "$0")" && pwd)
if [ -f "${BINDIR}/common" ]; then
    . "${BINDIR}/common"
fi
MEDIADIR="${MEDIADIR:-/home/fpp/media}"
CFGDIR="${CFGDIR:-${MEDIADIR}/config}"
SETTINGSFILE="${SETTINGSFILE:-${MEDIADIR}/settings}"

PW_RUNTIME="/run/pipewire-fpp"
PW_CONFD="/etc/pipewire/pipewire.conf.d"
WP_CONFD="/etc/wireplumber/wireplumber.conf.d"
PW_SERVICES="fpp-pipewire fpp-wireplumber fpp-pipewire-pulse"

# AES67's fixed media clock (AES67::AUDIO_RATE in src/mediaoutput/AES67Manager.h).
AES67_RATE=48000
# RTSPOutput::DEFAULT_PORT in src/mediaoutput/RTSPOutputManager.h.
RTSP_DEFAULT_PORT=8554

export PIPEWIRE_RUNTIME_DIR="${PW_RUNTIME}"
export XDG_RUNTIME_DIR="${PW_RUNTIME}"
export PULSE_RUNTIME_PATH="${PW_RUNTIME}/pulse"
export PIPEWIRE_CONFIG_DIR="/etc/pipewire"

TMPDIR_DIAG=$(mktemp -d /tmp/fpp-pwdiag.XXXXXX 2>/dev/null) || TMPDIR_DIAG="/tmp"
trap 'rm -rf "${TMPDIR_DIAG}" 2>/dev/null' EXIT

pass() { echo "[PASS] $*"; }
warn() { echo "[WARN] $*"; }
fail() { echo "[FAIL] $*"; }
info() { echo "[INFO] $*"; }
skip() { echo "[SKIP] $*"; }
note() { echo "       $*"; }
hdr()  { echo; echo "--- $* ---"; }

have() { command -v "$1" >/dev/null 2>&1; }

# True when $1 is this script or something it started.  Several checks here
# open a card to probe it (aplay --dump-hw-params), so without this the run
# finds its own children holding /dev/snd and reports them as foreign
# processes stealing the device (issue #2934).
#
# The variable names are deliberately private: POSIX sh has no locals, and the
# device contention check calls this from inside a `while read -r p` loop.
# Walking the tree in the caller's own "p" left it pointing at pid 1 by the
# time the function returned, so every foreign holder read back as "systemd"
# and was filed under "only the PipeWire stack has sound devices open" -- the
# check could never report contention at all (issue #2934).
own_process() {
    _op_pid="$1"
    _op_depth=0
    while [ -n "${_op_pid}" ] && [ "${_op_pid}" -gt 1 ] 2>/dev/null && [ "${_op_depth}" -lt 20 ]; do
        [ "${_op_pid}" = "$$" ] && return 0
        _op_pid=$(awk '{print $4}' "/proc/${_op_pid}/stat" 2>/dev/null)
        _op_depth=$((_op_depth+1))
    done
    return 1
}

# `aplay --dump-hw-params` output for one device.  Every probe goes through
# here because of the timeout: --dump-hw-params prints the parameters and then
# carries on to *play* the file, so on a card that accepts the raw stream --
# the Pi's onboard headphone jack does -- an unguarded probe never returns.  It
# plays /dev/zero forever, holding one of the card's PCM subdevices, and the
# web request that started it hangs until Apache's ProxyTimeout (20 minutes).
# Eight such leaks exhaust the eight bcm2835 headphone subdevices, after which
# PipeWire cannot open the card at all and its fpp_alsa_* node retries
# "Device or resource busy" for ever (issue #2934).
hw_params_probe() {
    timeout 3 aplay -D "$1" --dump-hw-params /dev/zero 2>&1
}

# True in the simple backend.  Several features -- input mixing, AES67, Opus
# RTP, video output groups -- exist only in the advanced one, and their JSON
# survives a switch to simple mode.  Any check that reads such a file has to
# ask this first or it reports the previous mode's configuration as broken.
simple_mode() {
    # Unset counts as simple: that is the documented default (settings.json)
    # and what the code falls back to -- FPPINIT_Audio.cpp seeds the variable
    # with "pipewire-simple" before getRawSetting() overwrites it.  A device
    # that has never had the setting written is therefore running simple mode,
    # and treating it as advanced made every advanced-only check read the
    # leftover JSON and report the previous mode's config as broken.
    b=$(setting MediaBackend)
    [ -z "${b}" ] || [ "${b}" = "pipewire-simple" ]
}

# A setting out of $SETTINGSFILE, quotes stripped, empty if unset.
setting() {
    [ -f "${SETTINGSFILE}" ] || return 0
    sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*//p" "${SETTINGSFILE}" 2>/dev/null |
        head -1 | sed -e 's/^"//' -e 's/"$//'
}

# One key out of PipeWire's settings metadata, empty if unreadable.  Cached
# because the AES67 rate check and the performance section both want it and
# pw-metadata costs a round trip to the daemon.
PW_META_FILE="${TMPDIR_DIAG}/pw-metadata.txt"
PW_META_STATE=""
pw_setting() {
    if [ -z "${PW_META_STATE}" ]; then
        PW_META_STATE="done"
        have pw-metadata &&
            timeout 5 pw-metadata -n settings > "${PW_META_FILE}" 2>/dev/null
    fi
    [ -s "${PW_META_FILE}" ] || return 0
    sed -n "s/.*key:'$1' value:'\([^']*\)'.*/\1/p" "${PW_META_FILE}" 2>/dev/null | head -1
}

# The default-device metadata, which is a different metadata object from the
# settings one above ("-n default" vs "-n settings") and holds the default sink
# and source rather than the clock.
PW_DEFAULT_FILE="${TMPDIR_DIAG}/pw-default.txt"
PW_DEFAULT_STATE=""
pw_default() {
    if [ -z "${PW_DEFAULT_STATE}" ]; then
        PW_DEFAULT_STATE="done"
        have pw-metadata &&
            timeout 5 pw-metadata -n default > "${PW_DEFAULT_FILE}" 2>/dev/null
    fi
    [ -s "${PW_DEFAULT_FILE}" ] || return 0
    sed -n "s/.*key:'$1' value:'\([^']*\)'.*/\1/p" "${PW_DEFAULT_FILE}" 2>/dev/null | head -1
}

# pw-dump once per run; every graph check reads the cached copy.  Without the
# timeout a down daemon hangs the whole page rather than failing a check.
PW_DUMP_FILE="${TMPDIR_DIAG}/pw-dump.json"
PW_DUMP_STATE=""
pw_dump() {
    if [ -z "${PW_DUMP_STATE}" ]; then
        PW_DUMP_STATE="empty"
        if have pw-dump; then
            if timeout 8 pw-dump > "${PW_DUMP_FILE}" 2>/dev/null &&
               [ -s "${PW_DUMP_FILE}" ]; then
                PW_DUMP_STATE="ok"
            fi
        fi
    fi
    [ "${PW_DUMP_STATE}" = "ok" ]
}

# The ALSA card behind a node name, or empty when it is not a card node.
# Two spellings reach here: FPP's own "fpp_alsa_<cardId>" adapters, and
# WirePlumber's "alsa_output.platform-<addr>.hdmi.*" for the nodes it creates
# itself.  The second is the one nobody can read -- 107c706400 is a device
# tree address, so the message has to resolve it or the reader cannot tell
# which physical socket is being talked about.
card_for_node() {
    case "$1" in
        fpp_alsa_*)
            # FPP lowercases the card id when it builds the node name, but ALSA
            # card ids are case sensitive: "hw:s3" is "No such device" where
            # "hw:S3" is the Sound Blaster.  Recover the real spelling.
            want="${1#fpp_alsa_}"
            real=$(awk -F'[][]' '/^[[:space:]]*[0-9]+[[:space:]]*\[/ {gsub(/ /, "", $2); print $2}' \
                   /proc/asound/cards 2>/dev/null |
                   awk -v w="${want}" 'tolower($0) == tolower(w) {print; exit}')
            echo "${real:-${want}}"
            ;;
        alsa_output.platform-*|alsa_input.platform-*)
            addr=$(echo "$1" | sed -n 's/^alsa_\(output\|input\)\.platform-\([^.]*\)\..*/\2/p')
            [ -z "${addr}" ] && return 0
            for d in /sys/class/sound/card*; do
                [ -e "${d}/device" ] || continue
                case "$(readlink -f "${d}/device" 2>/dev/null)" in
                    *"${addr}"*) cat "${d}/id" 2>/dev/null; return 0 ;;
                esac
            done
            ;;
    esac
}

# One line describing what a card is physically doing right now: whether a
# display is attached (HDMI carries no audio without one) and whether the
# device opens.  This is what turns "a node is missing" into something the
# person standing next to the machine can act on.
card_physical_state() {
    cardId="$1"
    [ -n "${cardId}" ] || return 0
    cardNum=$(awk -v id="${cardId}" '$2 == "[" id' /proc/asound/cards 2>/dev/null | awk '{print $1}' | head -1)
    [ -z "${cardNum}" ] &&
        cardNum=$(grep -n "\[${cardId}[[:space:]]*\]" /proc/asound/cards 2>/dev/null |
                  head -1 | sed 's/.*^//' | awk '{print $1}')
    eld=""
    if [ -n "${cardNum}" ]; then
        for e in /proc/asound/card${cardNum}/eld*; do
            [ -f "${e}" ] || continue
            mon=$(sed -n 's/^monitor_name[[:space:]]*//p' "${e}" 2>/dev/null | head -1)
            if [ -n "${mon}" ]; then
                eld="a display is attached (${mon})"
            else
                eld="no display is attached - an HDMI socket with nothing in it"
            fi
            break
        done
    fi
    # The output is the verdict, not the exit status: the probe is killed by
    # its own timeout on a card that accepts the stream, so the status says
    # nothing.  Same test the ALSA Hardware section uses.
    probe=$(hw_params_probe "hw:${cardId}")
    if echo "${probe}" | grep -q "HW Params"; then
        opens="the device opens right now"
    elif echo "${probe}" | grep -qi "busy"; then
        opens="the device is open and in use by something else"
    else
        opens="the device will not open"
    fi
    if [ -n "${eld}" ]; then
        echo "${cardId}: ${eld}, and ${opens}"
    else
        echo "${cardId}: ${opens}"
    fi
}

# Node names actually present in the running graph, one per line.
graph_node_names() {
    pw_dump || return 1
    if have jq; then
        jq -r '.[] | select(.type == "PipeWire:Interface:Node")
               | .info.props["node.name"] // empty' "${PW_DUMP_FILE}" 2>/dev/null | sort -u
    else
        grep -o '"node\.name"[^,]*' "${PW_DUMP_FILE}" 2>/dev/null |
            sed -e 's/.*:[[:space:]]*"//' -e 's/"$//' | sort -u
    fi
}

# The FPP-generated confs declare every node they create (node.name) and
# every node they expect to link to (node.target).  Both sets are worth
# having on their own, so pull them out of the conf text rather than the
# graph - they are the intent, and the graph is the outcome.
declared_node_names() {
    cat "${PW_CONFD}"/9[0-9]-fpp-*.conf 2>/dev/null |
        sed -n 's/.*node\.name[[:space:]]*=[[:space:]]*"\([^"]*\)".*/\1/p' | sort -u
}
declared_node_targets() {
    cat "${PW_CONFD}"/9[0-9]-fpp-*.conf 2>/dev/null |
        sed -n 's/.*node\.target[[:space:]]*=[[:space:]]*"\([^"]*\)".*/\1/p' | sort -u
}

# jq's `//` treats `false` as absent, so `.enabled // true` reports an
# explicitly disabled entry as enabled.  Everything here means "enabled
# unless it says otherwise", which is this instead.
JQ_ENABLED='def enabled: (.enabled == null) or (.enabled == true);'

json_ok() {
    if have jq; then
        jq -e . "$1" >/dev/null 2>&1
    elif have php; then
        php -r 'exit(json_decode(file_get_contents($argv[1])) === null ? 1 : 0);' "$1" >/dev/null 2>&1
    else
        return 2
    fi
}

# Tally the verdicts a finished section printed.  Several checks run inside
# a `... | while read` pipeline, whose subshell would lose any counter kept
# in a variable, so the output itself is what gets counted.
summary() {
    _p=$(grep -c "^\[PASS\]" "$1" 2>/dev/null)
    _w=$(grep -c "^\[WARN\]" "$1" 2>/dev/null)
    _f=$(grep -c "^\[FAIL\]" "$1" 2>/dev/null)
    echo
    echo "==================================================================="
    echo "Result: ${_p:-0} passed, ${_w:-0} warnings, ${_f:-0} failures"
    if [ "${_f:-0}" -gt 0 ]; then
        echo "Failures above are the place to start; each names the file,"
        echo "node or service involved."
    fi
    echo "==================================================================="
}

# Run a section, echo it, then summarise what it said.
run_section() {
    out="${TMPDIR_DIAG}/section-$1.txt"
    "section_$1" > "${out}" 2>&1
    cat "${out}"
    summary "${out}"
}

#############################################################################
section_services() {
    echo "=== PipeWire Service & Runtime Health ==="

    hdr "systemd units"
    for svc in ${PW_SERVICES}; do
        active=$(systemctl is-active "${svc}.service" 2>/dev/null)
        enabled=$(systemctl is-enabled "${svc}.service" 2>/dev/null)
        case "${active}" in
            active)
                pass "${svc}: active (${enabled:-unknown at boot})"
                ;;
            activating)
                warn "${svc}: still activating - it has not finished starting"
                note "If it never leaves this state the daemon is exiting before it is ready."
                ;;
            inactive)
                fail "${svc}: not running (${enabled:-unknown at boot})"
                ;;
            *)
                fail "${svc}: ${active:-unknown}"
                ;;
        esac

        # A service that restarts repeatedly looks 'active' at any given
        # moment but is really crash-looping; NRestarts is the only place
        # that shows up.
        restarts=$(systemctl show "${svc}.service" -p NRestarts --value 2>/dev/null)
        if [ -n "${restarts}" ] && [ "${restarts}" -gt 0 ] 2>/dev/null; then
            if [ "${restarts}" -ge 5 ]; then
                fail "${svc}: has restarted ${restarts} times - it is crash-looping"
                note "Usually the configured audio device is missing or another"
                note "process holds it.  See the ALSA section and the service logs."
            else
                warn "${svc}: has restarted ${restarts} time(s) since boot"
            fi
        fi

        # How long the current instance has been up, so a restart 4 seconds
        # ago is distinguishable from a daemon that has been stable all show.
        started=$(systemctl show "${svc}.service" -p ExecMainStartTimestamp --value 2>/dev/null)
        [ -n "${started}" ] && note "${svc}: current instance started ${started}"
    done

    hdr "Runtime directory and sockets"
    if [ -d "${PW_RUNTIME}" ]; then
        pass "${PW_RUNTIME} exists"
        note "$(ls -ld "${PW_RUNTIME}" 2>/dev/null)"
        owner=$(stat -c '%U:%G' "${PW_RUNTIME}" 2>/dev/null)
        mode=$(stat -c '%a' "${PW_RUNTIME}" 2>/dev/null)
        # fppd runs as a non-root user and reaches the daemon through this
        # directory; root-only permissions here are silent no-audio.
        case "${owner}" in
            *:audio) pass "Runtime directory group is 'audio' (${owner}, mode ${mode})" ;;
            *)       warn "Runtime directory is ${owner} mode ${mode}, expected group 'audio'" ;;
        esac
    else
        fail "${PW_RUNTIME} does not exist - PipeWire has never started successfully"
    fi

    for sock in "${PW_RUNTIME}/pipewire-0" "${PW_RUNTIME}/pulse/native"; do
        if [ -S "${sock}" ]; then
            pass "Socket present: ${sock}"
        else
            fail "Socket missing: ${sock}"
            case "${sock}" in
                */pulse/native)
                    note "pactl and the mixer pages talk to this; volume control will fail." ;;
                *)
                    note "Nothing can connect to PipeWire without this socket." ;;
            esac
        fi
    done

    hdr "Daemon reachable"
    if have pw-cli; then
        if timeout 5 pw-cli info 0 >"${TMPDIR_DIAG}/core.txt" 2>&1; then
            pass "pw-cli connected to the daemon"
            grep -E "core\.(name|version)|cookie|clock" "${TMPDIR_DIAG}/core.txt" 2>/dev/null |
                sed 's/^/       /' | head -10
        else
            fail "pw-cli could not connect to the daemon"
            note "$(head -2 "${TMPDIR_DIAG}/core.txt" 2>/dev/null)"
        fi
    else
        fail "pw-cli is not installed (package pipewire-bin)"
    fi

    if have wpctl; then
        if timeout 5 wpctl status >/dev/null 2>&1; then
            pass "WirePlumber is managing the graph (wpctl responded)"
        else
            fail "wpctl could not query the session manager"
            note "PipeWire may be up while WirePlumber is not; without it no"
            note "device is ever turned into a usable sink."
        fi
    fi

    hdr "Versions"
    for tool in pipewire wireplumber; do
        if have "${tool}"; then
            info "${tool}: $(${tool} --version 2>&1 | head -2 | tr '\n' ' ')"
        else
            fail "${tool} binary not found"
        fi
    done
    # A daemon and a client library from different releases talk a protocol
    # that mostly works, which is worse than one that does not.
    libver=$(ls /usr/lib/*/libpipewire-0.3.so.* 2>/dev/null | head -1)
    [ -n "${libver}" ] && info "libpipewire: ${libver}"

    hdr "Realtime scheduling"
    pwpid=$(systemctl show fpp-pipewire.service -p MainPID --value 2>/dev/null)
    if [ -n "${pwpid}" ] && [ "${pwpid}" -gt 0 ] 2>/dev/null; then
        if have chrt; then
            prio=$(chrt -p "${pwpid}" 2>/dev/null | tr '\n' ' ')
            if echo "${prio}" | grep -qi "FIFO\|RR"; then
                pass "PipeWire has realtime scheduling: ${prio}"
            else
                warn "PipeWire is not running with realtime priority: ${prio}"
                note "Expect dropouts under load.  Check the module-rt block in"
                note "${PW_CONFD}/90-fpp.conf and LimitRTPRIO in the unit file."
            fi
        fi
        rtprio=$(grep -i "^Max realtime priority" "/proc/${pwpid}/limits" 2>/dev/null | awk '{print $4}')
        if [ -n "${rtprio}" ]; then
            if [ "${rtprio}" -ge 80 ] 2>/dev/null; then
                pass "RLIMIT_RTPRIO is ${rtprio} (unit grants realtime headroom)"
            else
                warn "RLIMIT_RTPRIO is ${rtprio}; the unit asks for 95"
            fi
        fi
    else
        skip "PipeWire has no main PID - cannot check scheduling"
    fi

    hdr "Recent service errors"
    # Scope is the whole check.  The journal for these units survives reboots
    # and goes back as far as it was configured to, so an unbounded grep
    # reports failures from configurations that no longer exist: on the
    # device this was written against, 55818 matching lines across weeks
    # against 16 for the running instance.  The old -n 400 cap did not bound
    # it either -- it only hid how far back the matches went.
    #
    # The boundary is the later of two events, because either one invalidates
    # what came before it: the services starting (a restart replaces the
    # process that logged) and the newest generated conf being written (a
    # regenerated conf.d replaces the graph the messages were describing).
    #
    # Separately, a headless FPP always logs a few connect failures for things
    # it deliberately does not have -- a session bus, a portal, jackdbus, a
    # camera at /dev/video0.  Those are structural and permanent, so they are
    # counted and named rather than listed as faults to act on.
    if have journalctl; then
        # Earliest of the three, so every unit's current instance is covered.
        boundary=""
        for u in ${PW_SERVICES}; do
            t=$(systemctl show -p ActiveEnterTimestamp --value "${u}" 2>/dev/null)
            [ -z "${t}" ] && continue
            e=$(date -d "${t}" +%s 2>/dev/null) || continue
            [ -z "${e}" ] && continue
            if [ -z "${boundary}" ] || [ "${e}" -lt "${boundary}" ] 2>/dev/null; then
                boundary="${e}"
            fi
        done
        boundaryWhy="the services started"
        for d in "${PW_CONFD}" "${WP_CONFD}"; do
            [ -d "${d}" ] || continue
            for f in "${d}"/*.conf; do
                [ -f "${f}" ] || continue
                m=$(stat -c %Y "${f}" 2>/dev/null) || continue
                if [ -n "${m}" ] && { [ -z "${boundary}" ] || [ "${m}" -gt "${boundary}" ] 2>/dev/null; }; then
                    boundary="${m}"
                    boundaryWhy="the configuration was last generated"
                fi
            done
        done

        # Structural on a headless player, not faults.  Kept out of the verdict
        # but counted, so a genuinely new one is not hidden by being filtered.
        benign="session bus|autolaunch a dbus-daemon|mod\.portal|jackdbus"
        benign="${benign}|spa\.v4l2: Cannot open|no such file or directory: /proc"

        if [ -n "${boundary}" ]; then
            since="--since @${boundary}"
            info "Looking at the journal since $(date -d "@${boundary}" '+%Y-%m-%d %H:%M:%S' 2>/dev/null) (${boundaryWhy})"
        else
            since="-n 400"
            info "Service start time unknown - falling back to the last 400 journal lines"
        fi

        allErrs=$(journalctl -u fpp-pipewire -u fpp-wireplumber -u fpp-pipewire-pulse \
                      --no-pager ${since} 2>/dev/null |
                  grep -iE "error|fail|cannot|denied|refused|no such|timeout")
        errs=$(echo "${allErrs}" | grep -viE "${benign}")
        benignN=$(echo "${allErrs}" | grep -icE "${benign}")

        if [ -n "${errs}" ]; then
            errN=$(echo "${errs}" | grep -c .)
            if [ "${errN}" -gt 15 ] 2>/dev/null; then
                warn "${errN} error(s) in the service journal (most recent 15):"
            else
                warn "${errN} error(s) in the service journal:"
            fi
            echo "${errs}" | tail -15 | sed 's/^/       /'
        else
            pass "No service errors since ${boundaryWhy}"
        fi
        if [ "${benignN:-0}" -gt 0 ] 2>/dev/null; then
            info "${benignN} more are the usual headless messages (no session bus,"
            note "no portal, no jackdbus, no camera) and need no action."
        fi
    else
        skip "journalctl not available"
    fi

}

#############################################################################
section_config() {
    echo "=== PipeWire Configuration Integrity ==="

    hdr "Deployed configuration files"
    if [ -d "${PW_CONFD}" ]; then
        ls -l "${PW_CONFD}" 2>/dev/null | sed 's/^/       /'
    else
        fail "${PW_CONFD} does not exist - no FPP PipeWire config is deployed"
    fi
    if [ -d "${WP_CONFD}" ]; then
        ls -l "${WP_CONFD}" 2>/dev/null | sed 's/^/       /'
    else
        fail "${WP_CONFD} does not exist - no FPP WirePlumber config is deployed"
    fi

    hdr "Required configuration present"
    # 90 and the wireplumber drop-ins ship with FPP and are always expected;
    # 95/96/97 are generated, so their absence only matters once something is
    # actually configured to need them.
    for f in "${PW_CONFD}/90-fpp.conf" \
             "${WP_CONFD}/10-fpp-alsa.conf" \
             "${WP_CONFD}/15-fpp-usb-audio-headroom.conf" \
             "${WP_CONFD}/60-fpp-block-combine-fallback.conf"; do
        if [ -f "${f}" ]; then
            pass "Present: ${f}"
        else
            fail "Missing: ${f}"
            note "Re-run: sudo /opt/fpp/scripts/install_pipewire.sh"
        fi
    done

    lua="/usr/share/wireplumber/scripts/linking/fpp-block-combine-fallback.lua"
    if [ -f "${lua}" ]; then
        if cmp -s "${lua}" "/opt/fpp/etc/wireplumber/scripts/linking/fpp-block-combine-fallback.lua"; then
            pass "Combine-fallback blocking script is deployed and current"
        else
            warn "${lua} differs from the copy shipped in /opt/fpp"
            note "An outdated copy lets WirePlumber link combine-stream outputs"
            note "to the default sink, which sounds like audio on the wrong card."
        fi
    else
        fail "Missing: ${lua}"
        note "Without it, group outputs can be linked to the wrong sink."
    fi

    hdr "Generated configuration"
    for f in "${PW_CONFD}/95-fpp-alsa-sink.conf" \
             "${PW_CONFD}/96-fpp-input-groups.conf" \
             "${PW_CONFD}/97-fpp-audio-groups.conf"; do
        if [ -f "${f}" ]; then
            info "Present: ${f} ($(stat -c '%s bytes, modified %y' "${f}" 2>/dev/null | cut -d. -f1))"
        else
            info "Not generated: ${f}"
        fi
    done

    if [ -f "${CFGDIR}/pipewire-audio-groups.json" ] && [ ! -f "${PW_CONFD}/97-fpp-audio-groups.conf" ]; then
        fail "Audio groups are configured but 97-fpp-audio-groups.conf was never generated"
        note "Open the Audio Output Groups page and press Save & Apply."
    fi
    if simple_mode && [ -f "${CFGDIR}/pipewire-input-groups.json" ]; then
        skip "Input groups configured for advanced mode - not generated in simple mode"
    elif [ -f "${CFGDIR}/pipewire-input-groups.json" ] && [ ! -f "${PW_CONFD}/96-fpp-input-groups.conf" ]; then
        fail "Input groups are configured but 96-fpp-input-groups.conf was never generated"
        note "Open the Input Mixing page and press Save & Apply."
    fi

    hdr "Configuration syntax (SPA-JSON parse)"
    if have spa-json-dump; then
        for f in "${PW_CONFD}"/*.conf "${WP_CONFD}"/*.conf; do
            [ -f "${f}" ] || continue
            if spa-json-dump "${f}" >/dev/null 2>"${TMPDIR_DIAG}/spa.err"; then
                pass "Parses: $(basename "${f}")"
            else
                fail "Will not parse: ${f}"
                note "$(head -2 "${TMPDIR_DIAG}/spa.err" 2>/dev/null)"
                note "PipeWire ignores the whole file, so everything it defines"
                note "silently disappears from the graph."
            fi
        done
    else
        skip "spa-json-dump not available - cannot syntax check the conf files"
    fi

    hdr "Deployed vs cached configuration"
    # The pages write a cached copy under media/config and install the same
    # bytes into /etc.  A difference means something was regenerated but
    # never applied, so the running graph is not the configured one.
    check_drift() {
        cached="$1"; deployed="$2"; what="$3"
        if [ ! -f "${cached}" ]; then
            skip "${what}: no cached copy (${cached})"
        elif [ ! -f "${deployed}" ]; then
            fail "${what}: cached config exists but nothing is deployed to ${deployed}"
        elif cmp -s "${cached}" "${deployed}"; then
            pass "${what}: deployed config matches the saved configuration"
        else
            fail "${what}: ${deployed} differs from ${cached}"
            note "The graph is running older config than the pages show."
            note "Press Save & Apply on the relevant page, then restart PipeWire."
        fi
    }
    check_drift "${CFGDIR}/pipewire-audio-groups.conf" \
                "${PW_CONFD}/97-fpp-audio-groups.conf" "Audio output groups"
    check_drift "${CFGDIR}/pipewire-input-groups.conf" \
                "${PW_CONFD}/96-fpp-input-groups.conf" "Input groups"

    # A conf newer than the daemon's start means it was written after the
    # daemon read it; the daemon is running the previous revision.
    pwstart=$(systemctl show fpp-pipewire.service -p ExecMainStartTimestampMonotonic --value 2>/dev/null)
    if [ -n "${pwstart}" ] && [ "${pwstart}" -gt 0 ] 2>/dev/null; then
        boot=$(awk '{print int($1)}' /proc/uptime 2>/dev/null)
        startedAgo=$(( boot - pwstart/1000000 ))
        stale=""
        for f in "${PW_CONFD}"/*.conf; do
            [ -f "${f}" ] || continue
            age=$(( $(date +%s) - $(stat -c %Y "${f}" 2>/dev/null || echo 0) ))
            [ "${age}" -lt "${startedAgo}" ] 2>/dev/null && stale="${stale} $(basename "${f}")"
        done
        if [ -n "${stale}" ]; then
            warn "Config changed after PipeWire started:${stale}"
            note "Restart the media stack so the daemon picks it up:"
            note "  sudo systemctl restart fpp-pipewire fpp-wireplumber fpp-pipewire-pulse"
        else
            pass "No configuration file is newer than the running daemon"
        fi
    fi

    hdr "Configuration JSON validity"
    found=0
    for f in "${CFGDIR}"/pipewire-*.json "${CFGDIR}"/gst-pipewire-quirks.json; do
        [ -f "${f}" ] || continue
        case "${f}" in *.bak*) continue ;; esac
        found=1
        json_ok "${f}"
        case "$?" in
            0) pass "Valid JSON: $(basename "${f}")" ;;
            2) skip "No jq or php available to validate $(basename "${f}")" ;;
            *) fail "Corrupt JSON: ${f}"
               note "FPP falls back to defaults for anything it cannot parse,"
               note "so this file's settings are being ignored entirely." ;;
        esac
    done
    [ "${found}" -eq 0 ] && info "No PipeWire configuration files in ${CFGDIR}"

    hdr "Media backend settings"
    backend=$(setting MediaBackend)
    case "${backend}" in
        pipewire|pipewire-simple)
            pass "MediaBackend = ${backend}" ;;
        "")
            info "MediaBackend is not set; FPP uses its default, pipewire-simple" ;;
        alsa)
            fail "MediaBackend = alsa, which has been retired"
            note "It is migrated to pipewire-simple on the next boot." ;;
        *)
            warn "MediaBackend = ${backend} (unrecognised)" ;;
    esac
    for k in AudioOutput PipeWireSinkName PipeWireVideoSinkName; do
        v=$(setting "${k}")
        info "${k} = ${v:-<unset>}"
    done

    # A sink name setting that no longer matches any node is the classic
    # after-a-rename failure: playback goes to a node that does not exist and
    # nothing is heard, with no error anywhere.
    # Two settings are deliberately not in this list.  PipeWireVideoSinkName
    # names fpp_video_bus, which the video pipeline creates at runtime rather
    # than declaring in a conf, so its absence here means nothing (the Video
    # section checks it instead).  PipeWirePrimaryOutput is left over from the
    # removed "Primary Audio Output" setting and nothing reads it any more.
    hdr "Settings point at nodes that exist"
    pw_dump
    declared=$(declared_node_names)
    for k in PipeWireSinkName \
             PipeWireSinkName_2 PipeWireSinkName_3 PipeWireSinkName_4 PipeWireSinkName_5; do
        v=$(setting "${k}")
        [ -z "${v}" ] && continue
        if echo "${declared}" | grep -qx "${v}"; then
            pass "${k}=${v} is declared by the generated config"
        elif graph_node_names 2>/dev/null | grep -qx "${v}"; then
            pass "${k}=${v} exists in the running graph"
        else
            fail "${k}=${v} matches no node in the config or the graph"
            note "Playback aimed at this sink is discarded.  Re-pick the output"
            note "on the Audio settings page, or re-apply the group that owned it."
        fi
    done

    # Three places name a rate and the last file loaded wins: 90-fpp.conf ships
    # a default, 95-fpp-alsa-sink.conf is generated from the FPP audio setting
    # and overrides it, and the daemon reports what it actually settled on.
    hdr "Sample rate consistency"
    baseRate=$(sed -n 's/.*default\.clock\.rate[[:space:]]*=[[:space:]]*\([0-9]*\).*/\1/p' \
               "${PW_CONFD}/90-fpp.conf" 2>/dev/null | head -1)
    sinkRate=$(sed -n 's/.*default\.clock\.rate[[:space:]]*=[[:space:]]*\([0-9]*\).*/\1/p' \
               "${PW_CONFD}/95-fpp-alsa-sink.conf" 2>/dev/null | head -1)
    liveRate=""
    if have pw-metadata; then
        liveRate=$(timeout 5 pw-metadata -n settings 2>/dev/null |
                   sed -n "s/.*key:.clock\.rate. value:.\([0-9]*\).*/\1/p" | head -1)
    fi
    info "90-fpp.conf base rate:            ${baseRate:-<unset>}"
    info "95-fpp-alsa-sink.conf rate:       ${sinkRate:-<not generated>}"
    info "Running graph clock rate:         ${liveRate:-<unavailable>}"
    wantRate="${sinkRate:-${baseRate}}"
    if [ -n "${wantRate}" ] && [ -n "${liveRate}" ]; then
        if [ "${wantRate}" = "${liveRate}" ]; then
            pass "Running clock rate (${liveRate}) matches the configured rate"
        else
            warn "Graph is running at ${liveRate} but is configured for ${wantRate}"
            note "Everything is being resampled.  Restart the media stack so the"
            note "daemon re-clocks:"
            note "  sudo systemctl restart fpp-pipewire fpp-wireplumber fpp-pipewire-pulse"
        fi
    fi
    # The two rates in that file are deliberately different things and are
    # expected to disagree: the "# configured rate:" tag records what the
    # AudioFormat setting *asked for*, while default.clock.rate records what the
    # selected card's PCM actually clocks at.  A fixed-bit-clock cape refines the
    # request upward -- an AM62x PCM5102A clocks no lower than 88200, so a 44100
    # request lands on 88200 -- and the generator keeps both on purpose so a
    # later boot can tell "the setting changed" from "the hardware refined it"
    # (see alsaSinkConfRateTag() in src/boot/FPPINIT_Audio.cpp).  Reporting the
    # difference as a fault sent the reader looking for a problem that is the
    # design working.  Whether the graph is actually resampling is the check
    # above, against the running clock.
    genRate=$(sed -n 's/^# configured rate: *\([0-9]*\).*/\1/p' \
              "${PW_CONFD}/95-fpp-alsa-sink.conf" 2>/dev/null | head -1)
    if [ -n "${genRate}" ] && [ -n "${sinkRate}" ] && [ "${genRate}" != "${sinkRate}" ]; then
        info "Audio Sample Rate asked for ${genRate}; the card clocks at ${sinkRate},"
        note "which is what the graph is pinned to.  Expected - not a mismatch."
    fi

    hdr "ALSA compatibility shim"
    # ALSA resolves ~/.asoundrc from HOME, and everything in FPP's audio path
    # runs as root -- fppd.service sets no User=, and fpp-pipewire,
    # fpp-wireplumber and fpp-pipewire-pulse all set User=root.  So
    # /root/.asoundrc is the only one of these files that decides whether FPP's
    # own playback goes through PipeWire, and it is the only one FPP writes
    # (setupAudio() in src/boot/FPPINIT_Audio.cpp, kept card-aligned by
    # www/common.php).  Grade that one; anything in a user home is somebody
    # else's file.
    if [ -f /root/.asoundrc ]; then
        if grep -q "pipewire" /root/.asoundrc 2>/dev/null; then
            pass "/root/.asoundrc routes ALSA clients into PipeWire"
        else
            warn "/root/.asoundrc does not reference pipewire"
            note "fppd and the PipeWire services all run as root, so this is the file"
            note "that matters.  Regenerate it with:"
            note "  sudo /opt/fpp/src/fppinit bootPre"
        fi
    else
        warn "/root/.asoundrc missing - FPP generates this at boot"
        note "Without it an ALSA client opens the card directly instead of PipeWire."
        note "Regenerate it with:"
        note "  sudo /opt/fpp/src/fppinit bootPre"
    fi
    # FPP has never written this one; when it exists it came from the base OS
    # image or the user.  It only applies to an ALSA tool run interactively as
    # the fpp user, which is not a playback path, so it is not a fault.
    userRc="${FPPHOME:-/home/fpp}/.asoundrc"
    if [ -f "${userRc}" ] && ! grep -q "pipewire" "${userRc}" 2>/dev/null; then
        info "${userRc} does not reference pipewire"
        note "FPP does not use this file - playback runs as root against /root/.asoundrc."
        note "Only an ALSA tool run interactively as the fpp user would bypass PipeWire."
    fi
    if [ -f /etc/pipewire/client.conf ]; then
        pass "/etc/pipewire/client.conf present"
    else
        warn "/etc/pipewire/client.conf missing - clients fall back to /usr/share defaults"
    fi

}

#############################################################################
section_graph() {
    echo "=== PipeWire Graph vs Configuration ==="
    echo
    echo "The generated conf files name every node FPP creates (node.name) and"
    echo "every node it expects to link to (node.target).  These checks compare"
    echo "that intent against the graph the daemon actually built."

    if ! pw_dump; then
        fail "Could not read the graph (pw-dump failed or returned nothing)"
        note "Run the Service & Runtime Health check first - nothing below can"
        note "be answered while the daemon is unreachable."
        return
    fi

    graph_node_names > "${TMPDIR_DIAG}/graph-nodes.txt"
    declared_node_names > "${TMPDIR_DIAG}/declared-nodes.txt"
    declared_node_targets > "${TMPDIR_DIAG}/declared-targets.txt"

    nodeCount=$(wc -l < "${TMPDIR_DIAG}/graph-nodes.txt")
    info "Nodes in the running graph: ${nodeCount}"
    info "Nodes declared by the FPP config: $(wc -l < "${TMPDIR_DIAG}/declared-nodes.txt")"
    info "Link targets named by the FPP config: $(wc -l < "${TMPDIR_DIAG}/declared-targets.txt")"

    if [ "${nodeCount}" -eq 0 ]; then
        fail "The graph has no nodes at all"
        note "PipeWire is running but WirePlumber has not enumerated any device."
        return
    fi

    hdr "Declared nodes that never appeared"
    missing=0
    explained=0
    while read -r n; do
        [ -z "${n}" ] && continue
        if ! grep -qx "${n}" "${TMPDIR_DIAG}/graph-nodes.txt"; then
            fail "Declared but not in the graph: ${n}"
            c=$(card_for_node "${n}")
            if [ -n "${c}" ]; then
                st=$(card_physical_state "${c}")
                if [ -n "${st}" ]; then
                    note "${st}"
                    explained=$((explained+1))
                fi
                # The adapters in the generated conf are static context.objects,
                # created once when the daemon reads the file.  A card that was
                # not ready at that moment -- an HDMI display still negotiating
                # is the usual one -- is skipped for the life of the daemon, so
                # a device that opens now and has no node is a start-order
                # problem, not a broken card.
                case "${st}" in
                    *"opens right now"*)
                        note "It opens now but has no node, so it was not ready when"
                        note "PipeWire read the config.  The generated adapters are"
                        note "created once at start and are not retried, so this"
                        note "clears with: sudo systemctl restart ${PW_SERVICES}" ;;
                esac
            fi
            missing=$((missing+1))
        fi
    done < "${TMPDIR_DIAG}/declared-nodes.txt"
    if [ "${missing}" -eq 0 ]; then
        pass "Every node the config declares exists in the graph"
    else
        note "${missing} node(s) the config asks for were never created."
        if [ "${explained}" -lt "${missing}" ] 2>/dev/null; then
            note "For an fpp_alsa_* node this means the card would not open, so"
            note "PipeWire skipped it (the generated adapters carry nofail, which"
            note "is what stops one bad card taking the whole daemon down) -- the"
            note "ALSA Hardware section names the device and the reason."
            note "For anything else, check the service log for the matching"
            note "module-filter-chain / module-combine-stream line."
        fi
    fi

    hdr "Link targets that cannot resolve"
    # A node.target naming something neither FPP creates nor the graph has is
    # a dead link: the module loads, the node exists, and audio goes nowhere.
    unresolved=0
    while read -r t; do
        [ -z "${t}" ] && continue
        if grep -qx "${t}" "${TMPDIR_DIAG}/graph-nodes.txt"; then
            continue
        fi
        if grep -qx "${t}" "${TMPDIR_DIAG}/declared-nodes.txt"; then
            warn "Target ${t} is declared by FPP but is not in the graph"
            unresolved=$((unresolved+1))
            continue
        fi
        fail "Target ${t} exists nowhere - the link that needs it cannot be made"
        c=$(card_for_node "${t}")
        if [ -n "${c}" ]; then
            st=$(card_physical_state "${c}")
            [ -n "${st}" ] && note "${st}"
            case "${st}" in
                *"no display is attached"*)
                    note "Nothing is plugged into that socket, so the card carries no"
                    note "audio and WirePlumber creates no node for it.  Expected while"
                    note "the socket is empty; remove the member from the group if it"
                    note "is not going to be used." ;;
                *"will not open"*)
                    note "Check the ALSA Hardware section for why the device refuses." ;;
                *"opens right now"*)
                    note "The card opens, so the node should exist - restart the"
                    note "PipeWire services to have it picked up." ;;
            esac
        else
            note "Usually the sound card behind it is unplugged, renamed, or held"
            note "by another process.  Check the ALSA Hardware section."
        fi
        unresolved=$((unresolved+1))
    done < "${TMPDIR_DIAG}/declared-targets.txt"
    [ "${unresolved}" -eq 0 ] && pass "Every configured link target resolves to a real node"

    hdr "Duplicate node names"
    # Two nodes under one name is not cosmetic when both are ALSA adapters on
    # the same PCM: the loser's snd_pcm_open() returns EBUSY, its node goes to
    # error, and a stream that landed on it neither starts nor ends -- the
    # client blocks in poll() forever.  FPP generates a WirePlumber rule to
    # stop the *monitor* creating such a twin (50-fpp-suppress-alsa-dupes.conf,
    # see ensureWirePlumberAlsaDupeSuppression in src/boot/FPPINIT_Audio.cpp),
    # but that cannot catch two static declarations of the same name in two
    # generated files -- which is what switching backend mode can leave behind.
    # node.target naming such a pair is ambiguous on top of that.
    if have jq && pw_dump; then
        dupes=$(jq -r '.[] | select(.type == "PipeWire:Interface:Node")
                       | .info.props["node.name"] // empty' "${PW_DUMP_FILE}" 2>/dev/null |
                sort | uniq -d)
        if [ -z "${dupes}" ]; then
            pass "Every node in the graph has a unique name"
        else
            echo "${dupes}" | while read -r d; do
                [ -z "${d}" ] && continue
                nd=$(jq -r --arg d "${d}" '[.[] | select(.type == "PipeWire:Interface:Node")
                          | select(.info.props["node.name"] == $d)] | length' \
                     "${PW_DUMP_FILE}" 2>/dev/null)
                # Only ALSA sinks contending for one PCM carry the EBUSY hazard.
                # Other stacks reuse a name legitimately: a Pi's bcm2835-isp
                # publishes four v4l2_input nodes under one platform device, and
                # calling that a fault sent people looking for an audio problem
                # that does not exist (issue #2934).
                alsaDupes=$(jq -r --arg d "${d}" '[.[] | select(.type == "PipeWire:Interface:Node")
                              | select(.info.props["node.name"] == $d)
                              | select(.info.props["api.alsa.path"] != null)] | length' \
                            "${PW_DUMP_FILE}" 2>/dev/null)
                if [ "${alsaDupes:-0}" -lt 2 ] 2>/dev/null; then
                    echo "[INFO] Node name '${d}' is used by ${nd} nodes (not ALSA sinks - normal for"
                    echo "       device stacks that publish several nodes per device)"
                    continue
                fi
                echo "[FAIL] Node name '${d}' exists ${nd} times in the graph"
                jq -r --arg d "${d}" '.[] | select(.type == "PipeWire:Interface:Node")
                       | select(.info.props["node.name"] == $d)
                       | "       id=\(.id) desc=\(.info.props["node.description"] // "-") dev=\(.info.props["api.alsa.path"] // "-") rate=\(.info.props["audio.rate"] // "-")"' \
                    "${PW_DUMP_FILE}" 2>/dev/null
                decl=$(grep -n "node\.name[[:space:]]*=[[:space:]]*\"${d}\"" \
                       "${PW_CONFD}"/*.conf 2>/dev/null)
                if [ -n "${decl}" ]; then
                    echo "       Declared at:"
                    echo "${decl}" | sed 's/^/         /' | sed 's/:[[:space:]]*node\.name.*//'
                fi
                echo "       Two adapters on one PCM: the second to open gets EBUSY and"
                echo "       hangs whatever plays to it.  Remove the duplicate declaration,"
                echo "       or regenerate the audio configuration from the FPP UI."
            done
        fi
    else
        skip "jq or pw-dump not available - cannot check for duplicate node names"
    fi

    hdr "Configured audio output groups"
    # pipewire-simple and pipewire read different group files; the unused one
    # lingers from whenever that mode was last selected, so comparing it
    # against the graph would report groups that nothing is meant to create.
    if simple_mode; then
        groupFiles="${CFGDIR}/pipewire-audio-groups-simple.json"
    else
        groupFiles="${CFGDIR}/pipewire-audio-groups.json"
    fi
    for gj in ${groupFiles}; do
        [ -f "${gj}" ] || { skip "No group configuration at ${gj}"; continue; }
        have jq || { skip "jq not available - cannot read $(basename "${gj}")"; break; }
        info "From $(basename "${gj}"):"
        jq -r "${JQ_ENABLED}"' .groups[]? | [(.name // "?"), (enabled|tostring), ((.channels // 0)|tostring), ((.members|length)|tostring)] | join("\u001f")' \
            "${gj}" 2>/dev/null |
        while IFS=$(printf '\037') read -r gname genabled gchan gmem; do
            # KEEP IN SYNC with the node.name emitted by
            # GeneratePipeWireGroupsConfig() in www/api/controllers/pipewire.php
            slug=$(echo "${gname}" | tr 'A-Z' 'a-z' | sed 's/[^a-zA-Z0-9_]/_/g')
            nodename="fpp_group_${slug}"
            if [ "${genabled}" != "true" ]; then
                skip "Group '${gname}' is disabled"
            elif grep -qx "${nodename}" "${TMPDIR_DIAG}/graph-nodes.txt"; then
                echo "[PASS] Group '${gname}' -> ${nodename} is in the graph (${gchan}ch, ${gmem} member(s))"
            else
                echo "[FAIL] Group '${gname}' -> ${nodename} is NOT in the graph"
                echo "       Nothing sent to this group will be heard.  Re-apply the"
                echo "       group on the Audio Output Groups page."
            fi
        done
    done

    hdr "Configured input groups (mix buses)"
    # Input mixing exists only in the advanced backend (MediaBackend's children
    # list PipeWireInputMixing under "pipewire", not "pipewire-simple"), and
    # simple mode does not generate 96-fpp-input-groups.conf at all.  The JSON
    # survives the mode switch though, so reading it regardless reports every
    # group that mode used to have as missing from the graph.  Same stale-file
    # trap the output-group check above already avoids.
    igj="${CFGDIR}/pipewire-input-groups.json"
    if simple_mode; then
        skip "Input mixing is an advanced-mode feature - not used in simple mode"
    elif [ -f "${igj}" ] && have jq; then
        jq -r "${JQ_ENABLED}"' .inputGroups[]? | [(.name // "?"), (enabled|tostring), ((.members|length)|tostring)] | join("\u001f")' \
            "${igj}" 2>/dev/null |
        while IFS=$(printf '\037') read -r igname igenabled igmem; do
            # KEEP IN SYNC with InputGroupNodeName() in pipewire.php
            slug=$(echo "${igname}" | tr 'A-Z' 'a-z' | sed 's/[^a-zA-Z0-9_]/_/g')
            nodename="fpp_input_${slug}"
            if [ "${igenabled}" != "true" ]; then
                skip "Input group '${igname}' is disabled"
            elif grep -qx "${nodename}" "${TMPDIR_DIAG}/graph-nodes.txt"; then
                echo "[PASS] Input group '${igname}' -> ${nodename} is in the graph (${igmem} source(s))"
            else
                echo "[FAIL] Input group '${igname}' -> ${nodename} is NOT in the graph"
            fi
        done
    else
        skip "No input groups configured (or jq unavailable)"
    fi

    hdr "Node states"
    if have jq; then
        jq -r '.[] | select(.type == "PipeWire:Interface:Node")
               | [(.info.props["node.name"] // "?"), (.info.state // "?"),
                  (.info.props["media.class"] // "-")] | join("\u001f")' \
            "${PW_DUMP_FILE}" 2>/dev/null > "${TMPDIR_DIAG}/states.txt"

        errNodes=$(awk -F'\037' '$2 == "error"' "${TMPDIR_DIAG}/states.txt" | tr '\037' ' ')
        if [ -n "${errNodes}" ]; then
            fail "Nodes in the error state:"
            echo "${errNodes}" | sed 's/^/       /'
            note "An errored node has usually failed to open its device."
        else
            pass "No node is in the error state"
        fi

        # FPP's own sinks are meant to be sitting idle, ready to be linked.
        # Suspended is what a sink looks like when nothing has ever driven it.
        susp=$(awk -F'\037' '$1 ~ /^fpp_/ && $2 == "suspended"' "${TMPDIR_DIAG}/states.txt" | tr '\037' ' ')
        if [ -n "${susp}" ]; then
            info "FPP nodes currently suspended (normal when idle):"
            echo "${susp}" | sed 's/^/       /'
        fi

        running=$(awk -F'\037' '$2 == "running"' "${TMPDIR_DIAG}/states.txt" | wc -l)
        info "Nodes currently running: ${running}"

        hdr "Sinks and sources in the graph"
        awk -F'\037' '$3 ~ /Audio\/(Sink|Source)/ || $3 ~ /Video\// {printf "       %-52s %-10s %s\n", $1, $2, $3}' \
            "${TMPDIR_DIAG}/states.txt"

        hdr "Links"
        linkCount=$(jq -r '[.[] | select(.type == "PipeWire:Interface:Link")] | length' \
                    "${PW_DUMP_FILE}" 2>/dev/null)
        info "Links in the graph: ${linkCount:-0}"
        brokenLinks=$(jq -r '.[] | select(.type == "PipeWire:Interface:Link")
                            | select(.info.state == "error" or .info.state == "unlinked")
                            | "\(.id) state=\(.info.state) \(.info.error // "")"' \
                      "${PW_DUMP_FILE}" 2>/dev/null)
        if [ -n "${brokenLinks}" ]; then
            fail "Links that failed to establish:"
            echo "${brokenLinks}" | sed 's/^/       /'
        else
            pass "No failed links in the graph"
        fi

        # An FPP group sink with nothing linked out of it is configured but
        # inert: it will accept audio and drop it on the floor.
        hdr "FPP sinks with no outgoing link"
        # A group sink is a module-combine-stream, and a combine-stream never
        # carries links on the sink node itself: for each member it creates a
        # separate stream node called output.<sink>_<member> and the links hang
        # off that.  Looking only at the sink's own id therefore reported every
        # group in the graph as feeding nothing, however healthy -- the chain
        # here is fpp_group_default -> output.fpp_group_default_fpp_fx_g1_s3 ->
        # fpp_fx_g1_s3 -> fpp_fx_g1_s3_out -> fpp_alsa_s3, fully linked.
        # A sink is orphaned only when neither it nor any of its member streams
        # has an outgoing link.
        jq -r '
          [ .[] | select(.type == "PipeWire:Interface:Link") | .info["output-node-id"] ] as $outs
          | [ .[] | select(.type == "PipeWire:Interface:Node") ] as $nodes
          | $nodes[]
          | select((.info.props["node.name"] // "") | startswith("fpp_group_") or startswith("fpp_input_"))
          | . as $g | ($g.info.props["node.name"]) as $gn
          | ( [$g.id]
              + [ $nodes[] | select((.info.props["node.name"] // "")
                                    | startswith("output." + $gn + "_")) | .id ] ) as $ids
          | select( [ $ids[] | IN($outs[]) ] | any | not )
          | $gn' \
            "${PW_DUMP_FILE}" 2>/dev/null > "${TMPDIR_DIAG}/orphans.txt"
        if [ -s "${TMPDIR_DIAG}/orphans.txt" ]; then
            warn "These FPP sinks feed nothing (fine if unused, silent if not):"
            sed 's/^/       /' "${TMPDIR_DIAG}/orphans.txt"
        else
            pass "Every FPP group sink has at least one outgoing link"
        fi
    else
        skip "jq not installed - node state and link checks were skipped"
    fi

    hdr "Default devices"
    if have wpctl; then
        timeout 5 wpctl status 2>/dev/null | grep -A2 -i "default" | sed 's/^/       /' | head -10
        defsink=$(timeout 5 wpctl status 2>/dev/null | grep -E "^\s+\*" | head -3)
        [ -n "${defsink}" ] && echo "${defsink}" | sed 's/^/       /'
    fi
    # WirePlumber keeps two of these: default.audio.sink is what the graph
    # settled on, default.configured.audio.sink is the sticky preference a
    # "set default" wrote.  Only the second survives the node it names being
    # deleted, which is what switching backend mode does to every group sink of
    # the mode being left.  The graph falls back and audio keeps working, so
    # nothing complains -- until a node of that name exists again and silently
    # takes the default back.
    cfgSink=$(pw_default default.configured.audio.sink |
              sed -n 's/.*"name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')
    liveSink=$(pw_default default.audio.sink |
               sed -n 's/.*"name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')
    if [ -n "${cfgSink}" ]; then
        if grep -qx "${cfgSink}" "${TMPDIR_DIAG}/graph-nodes.txt" 2>/dev/null; then
            pass "The configured default sink (${cfgSink}) exists"
        else
            warn "The configured default sink '${cfgSink}' does not exist in the graph"
            note "Left behind by a node that has since gone -- a backend mode"
            note "switch or a removed group.  The graph is using ${liveSink:-a fallback}"
            note "instead, so audio works, but a future node of that name would"
            note "silently take the default back.  Clear it with:"
            note "  wpctl set-default <id of the sink you want>"
        fi
    fi

}

#############################################################################
section_alsa() {
    echo "=== ALSA Hardware & Device Contention ==="

    hdr "Registered cards"
    if [ -f /proc/asound/cards ]; then
        sed 's/^/       /' /proc/asound/cards
        cardCount=$(grep -c "^ *[0-9]" /proc/asound/cards 2>/dev/null)
        if [ "${cardCount}" -gt 0 ] 2>/dev/null; then
            pass "${cardCount} sound card(s) registered with ALSA"
        else
            fail "No sound cards are registered with ALSA"
            note "PipeWire has nothing to open; every output will be silent."
        fi
    else
        fail "/proc/asound/cards does not exist - no ALSA support in this kernel"
    fi

    if lsmod 2>/dev/null | grep -q "^snd_dummy"; then
        info "snd-dummy is loaded (FPP loads it when no real card is present,"
        info "so playback has somewhere to go).  Audio through it is discarded."
    fi

    hdr "Cards referenced by the audio group configuration"
    # A group member names an ALSA card id.  If that card is gone - unplugged
    # USB adapter, HDMI display asleep - the member is configured against
    # nothing and its share of the group is silent.
    if [ "$(setting MediaBackend)" = "pipewire-simple" ]; then
        gj="${CFGDIR}/pipewire-audio-groups-simple.json"
    else
        gj="${CFGDIR}/pipewire-audio-groups.json"
    fi
    [ -f "${gj}" ] || gj="${CFGDIR}/pipewire-audio-groups-simple.json"
    if [ -f "${gj}" ] && have jq; then
        jq -r '.groups[]? | .name as $g | .members[]? | [($g // "?"), (.cardId // ""), (.cardName // "")] | join("\u001f")' \
            "${gj}" 2>/dev/null | sort -u |
        while IFS=$(printf '\037') read -r gname cardId cardName; do
            [ -z "${cardId}" ] && continue
            case "${cardId}" in aes67*|opus*|rtsp*|fpp_*) 
                echo "[SKIP] '${gname}' member ${cardId} is a virtual output, not a card"
                continue ;;
            esac
            if ! grep -qE "^ *[0-9]+ \[${cardId}[ ]*\]" /proc/asound/cards 2>/dev/null; then
                echo "[FAIL] '${gname}' member card ${cardId} (${cardName}) is NOT present"
                echo "       This member of the group is silent.  Reconnect the device,"
                echo "       or remove the member from the group."
                continue
            fi
            # Being registered is not the same as being usable.  An HDMI audio
            # device stays in /proc/asound/cards while its monitor is powered
            # off, and a card another process holds is registered too -- so ask
            # the device itself whether it will open.
            probe=$(hw_params_probe "hw:${cardId}")
            if echo "${probe}" | grep -q "HW Params"; then
                echo "[PASS] '${gname}' member card ${cardId} (${cardName}) is present and opens"
            elif echo "${probe}" | grep -qi "busy"; then
                # Busy is what a working card looks like while PipeWire has it.
                echo "[PASS] '${gname}' member card ${cardId} (${cardName}) is present and in use"
            else
                why=$(echo "${probe}" | grep -i "open error" | sed 's/.*open error: //' | head -1)
                echo "[FAIL] '${gname}' member card ${cardId} (${cardName}) is registered but will not open"
                echo "       ALSA says: ${why:-unknown error}"
                echo "       It is listed in /proc/asound/cards, so it looks present"
                echo "       everywhere else, but nothing can play to it."
                case "${why}" in
                    *524*|*"not supported"*)
                        echo "       Error 524 on an HDMI device means the display supplied no"
                        echo "       audio capability (ELD).  The usual cause is a monitor that"
                        echo "       is switched off or in standby: the cable still asserts"
                        echo "       hotplug, so the connector reads as connected, but there is"
                        echo "       no audio sink behind it.  Power the display on, or remove"
                        echo "       this member from the group." ;;
                esac
            fi
        done
    else
        skip "No audio group configuration to cross-check (or jq unavailable)"
    fi

    hdr "Playback devices"
    if have aplay; then
        aplay -l 2>&1 | sed 's/^/       /'
    fi

    hdr "Device contention"
    # PipeWire opens a card exclusively.  Anything else holding it - a stray
    # aplay, an old fppd, mpg123 - makes PipeWire's node fail to start with
    # nothing obvious in the UI to say why.
    if have fuser; then
        holders=$(fuser -v /dev/snd/* 2>&1 | grep -v "^ *USER" | grep -v "^$")
        if [ -n "${holders}" ]; then
            info "Processes holding ALSA devices:"
            echo "${holders}" | sed 's/^/       /'
            # Only the pcm nodes.  Holding a control node is not contention --
            # anything reading a mixer has one open, and systemd itself shows up
            # against /dev/snd/seq -- while a pcm node is the thing that makes
            # PipeWire's open fail.  Own children are skipped because the probes
            # in these very checks open a pcm to read its parameters.
            others=$(fuser /dev/snd/pcm* 2>/dev/null | tr ' ' '\n' | grep -E "^[0-9]+$" |
                     while read -r pid; do
                         [ "${pid}" = "1" ] && continue
                         own_process "${pid}" && continue
                         cmd=$(ps -o comm= -p "${pid}" 2>/dev/null)
                         case "${cmd}" in
                             pipewire|wireplumber|pipewire-pulse|systemd|"") ;;
                             *) echo "${pid} ${cmd} -- $(ps -o args= -p "${pid}" 2>/dev/null | cut -c1-70)" ;;
                         esac
                     done)
            if [ -n "${others}" ]; then
                warn "Non-PipeWire processes have a sound device open:"
                echo "${others}" | sed 's/^/       /'
                note "PipeWire cannot open a card another process holds."
                # A leftover --dump-hw-params probe is this script's own doing,
                # from a release that ran it without a timeout: it plays
                # /dev/zero for ever and never lets go of the subdevice.  Say so
                # outright, because nothing about the process name suggests the
                # audio system broke itself (issue #2934).
                stale=$(echo "${others}" | grep -c -e "dump-hw-params" 2>/dev/null)
                if [ "${stale:-0}" -gt 0 ] 2>/dev/null; then
                    warn "${stale} of those are stale card probes left by an earlier"
                    note "diagnostics run (aplay --dump-hw-params never exits on a card"
                    note "that accepts the stream).  They are safe to kill:"
                    note "    sudo pkill -f 'aplay -D hw:.*--dump-hw-params'"
                    note "    sudo systemctl restart ${PW_SERVICES}"
                fi
            else
                pass "Only the PipeWire stack has sound devices open"
            fi
        else
            info "No process currently has an ALSA device open"
        fi
    else
        skip "fuser not available - cannot check device contention"
    fi

    hdr "Per-device stream status"
    # What each open PCM actually negotiated.  A card running at a rate or
    # format other than the one configured is where resampling and pitch
    # complaints come from.
    anyOpen=0
    for st in /proc/asound/card*/pcm*/sub*/status; do
        [ -f "${st}" ] || continue
        state=$(head -1 "${st}" 2>/dev/null)
        [ "${state}" = "closed" ] && continue
        anyOpen=1
        echo "       ${st}: ${state}"
        grep -E "^(rate|format|channels|buffer_size|period_size)" "${st}" 2>/dev/null |
            sed 's/^/         /'
    done
    if [ "${anyOpen}" -eq 0 ]; then
        info "No PCM device is currently open"
        note "PipeWire opens a card only while something is playing through it,"
        note "so this is expected when the player is idle."
    fi

    hdr "Hardware channel counts vs group configuration"
    # Asking a stereo card for 8 channels makes the node fail to negotiate,
    # which shows up as a group that exists but produces nothing.
    if [ -f "${gj}" ] && have jq && have aplay; then
        jq -r '.groups[]? | .name as $g | .members[]? | [($g // "?"), (.cardId // ""), ((.channels // 0)|tostring)] | join("\u001f")' \
            "${gj}" 2>/dev/null |
        while IFS=$(printf '\037') read -r gname cardId chans; do
            [ -z "${cardId}" ] && continue
            cardNum=$(sed -n "s/^ *\([0-9]*\) \[${cardId}[ ]*\].*/\1/p" /proc/asound/cards 2>/dev/null | head -1)
            [ -z "${cardNum}" ] && continue
            # Only what the device advertises.  This used to prefer
            # /proc/asound/cardN/pcm0p/sub0/hw_params, but that file holds the
            # parameters the *current* opener negotiated, not the card's
            # capability -- and one of this script's own aplay probes is often
            # that opener.  aplay with no format given defaults to mono, so the
            # check read back "1" and failed a perfectly good stereo card
            # (issue #2934: every Pi with an onboard headphone jack).
            maxCh=$(hw_params_probe "hw:${cardNum},0" |
                    sed -n 's/^CHANNELS: *//p' | head -1 |
                    tr -cs '0-9' ' ' | awk '{print $NF}')
            if [ -z "${maxCh}" ]; then
                # Busy or refusing to open: the ALSA Hardware checks above cover
                # that, and guessing a channel count here would be worse.
                continue
            fi
            if [ -n "${maxCh}" ] && [ "${chans}" -gt "${maxCh}" ] 2>/dev/null; then
                echo "[FAIL] '${gname}' asks card ${cardId} for ${chans} channels; it supports ${maxCh}"
                echo "       The node will not negotiate and the member stays silent."
            elif [ -n "${maxCh}" ]; then
                echo "[PASS] '${gname}' card ${cardId}: ${chans} channel(s) within the card's ${maxCh}"
            fi
        done
    else
        skip "Cannot compare channel counts (needs jq and aplay)"
    fi

    hdr "USB audio headroom rule"
    # 15-fpp-usb-audio-headroom.conf exists because USB cards drift against
    # the graph clock; if the rule is not being applied the symptom is
    # crackling rather than silence, which users report as 'bad audio'.
    if pw_dump && have jq; then
        usb=$(jq -r '.[] | select(.type == "PipeWire:Interface:Node")
                    | select((.info.props["node.name"] // "") | startswith("alsa_"))
                    | select((.info.props["node.name"] // "") | contains("usb"))
                    | "\(.info.props["node.name"]) headroom=\(.info.props["api.alsa.headroom"] // "unset")"' \
              "${PW_DUMP_FILE}" 2>/dev/null)
        if [ -n "${usb}" ]; then
            echo "${usb}" | sed 's/^/       /'
            if echo "${usb}" | grep -q "headroom=unset"; then
                warn "A USB audio node has no api.alsa.headroom set"
                note "Expect crackling when it shares a clock with another card."
                note "Check ${WP_CONFD}/15-fpp-usb-audio-headroom.conf is deployed."
            else
                pass "USB audio nodes have the FPP headroom applied"
            fi
        else
            skip "No USB audio nodes in the graph"
        fi
    else
        skip "Graph unavailable - cannot check USB headroom"
    fi

    hdr "Muted or zeroed playback controls"
    # Hardware mute is invisible from PipeWire's side: the graph looks
    # perfect and the card produces nothing.  Only playback controls matter;
    # capture controls are routinely off.  The full mixer dump lives in the
    # Audio group's "Mixer Devices" command.
    if have amixer; then
        anyMuted=0
        for id in $(sed -n 's/^ *\([0-9]*\) \[.*/\1/p' /proc/asound/cards 2>/dev/null); do
            cname=$(sed -n "s/^ *${id} \[\([^ ]*\).*/\1/p" /proc/asound/cards 2>/dev/null | head -1)
            ctl=""
            amixer -c "${id}" 2>/dev/null |
            while IFS= read -r line; do
                case "${line}" in
                    "Simple mixer control "*)
                        ctl="${line#Simple mixer control }"
                        # Only the controls in the card's main playback path
                        # can silence it.  Line/Mic/CD are input passthroughs
                        # and are muted on almost every card by design.
                        case "${ctl}" in
                            "'Master'"*|"'PCM'"*|"'Speaker'"*|"'Headphone'"*|"'Digital'"*|"'Front'"*) ;;
                            *) ctl="" ;;
                        esac ;;
                    *Playback*"[off]"*)
                        [ -z "${ctl}" ] && continue
                        echo "[WARN] card ${id} (${cname}): ${ctl} playback is muted"
                        echo "       This card is silent no matter what the graph does."
                        echo "       Unmute it:  amixer -c ${id} set ${ctl%%,*} unmute" ;;
                    *Playback*"[0%]"*)
                        [ -z "${ctl}" ] && continue
                        echo "[WARN] card ${id} (${cname}): ${ctl} playback is at 0%"
                        echo "       Raise it:  amixer -c ${id} set ${ctl%%,*} 80%" ;;
                esac
            done
        done | awk '!seen[$0]++' > "${TMPDIR_DIAG}/muted.txt"
        if [ -s "${TMPDIR_DIAG}/muted.txt" ]; then
            cat "${TMPDIR_DIAG}/muted.txt"
        else
            pass "No playback control is muted or at zero on any card"
        fi
    else
        skip "amixer not available"
    fi

}

#############################################################################
section_gstreamer() {
    echo "=== GStreamer Elements & Plugins ==="

    if ! have gst-inspect-1.0; then
        fail "gst-inspect-1.0 is not installed - GStreamer is missing entirely"
        note "Run: sudo /opt/fpp/scripts/install_pipewire.sh"
        return
    fi

    hdr "Version"
    info "$(gst-inspect-1.0 --version 2>&1 | head -2 | tr '\n' ' ')"

    hdr "Core elements (media playback)"
    for e in pipewiresink pipewiresrc audioconvert audioresample audiorate \
             volume appsink decodebin uridecodebin playbin queue tee \
             capsfilter identity; do
        if gst-inspect-1.0 "${e}" >/dev/null 2>&1; then
            pass "${e}"
        else
            fail "${e} is missing - media playback will not work"
        fi
    done

    hdr "Video elements"
    for e in videoconvert videoscale videocrop videorate v4l2src kmssink \
             glimagesink autovideosink; do
        if gst-inspect-1.0 "${e}" >/dev/null 2>&1; then
            pass "${e}"
        else
            warn "${e} is missing - some video features will be unavailable"
        fi
    done

    hdr "Network audio/video elements (AES67, Opus RTP, RTSP)"
    for e in opusenc opusdec rtpopuspay rtpopusdepay rtpL24pay rtpL16pay \
             rtph264pay rtph265pay rtpjpegpay rtpjitterbuffer udpsink udpsrc \
             interaudiosink interaudiosrc pitch; do
        if gst-inspect-1.0 "${e}" >/dev/null 2>&1; then
            pass "${e}"
        else
            warn "${e} is missing - the feature that uses it will fail to start"
        fi
    done

    hdr "RTSP server library"
    # FPP links libgstrtspserver directly rather than shelling out, so a
    # missing library is a link failure at fppd start, not an element lookup.
    if have ldconfig && ldconfig -p 2>/dev/null | grep -q "libgstrtspserver"; then
        pass "libgstrtspserver is installed"
        ldconfig -p 2>/dev/null | grep "libgstrtspserver" | sed 's/^/       /'
    else
        rtspLib=$(ls /usr/lib/*/libgstrtspserver-1.0.so* 2>/dev/null | head -1)
        if [ -n "${rtspLib}" ]; then
            pass "libgstrtspserver present: ${rtspLib}"
        else
            fail "libgstrtspserver is not installed - RTSP output cannot run"
            note "Install: sudo apt-get install libgstrtspserver-1.0-0"
        fi
    fi

    hdr "PipeWire GStreamer plugin vs libpipewire"
    # The plugin and the library have to come from the same release.  A
    # mismatch leaves pipewiresink permanently suspended: every playlist
    # plays through, silently, with nothing logged.  This is the single
    # most confusing failure in the stack, so check it explicitly.
    plugVer=$(gst-inspect-1.0 pipewiresink 2>/dev/null | awk '/^ *Version/ {print $NF; exit}')
    libSo=$(ls /usr/lib/*/libpipewire-0.3.so.0.* 2>/dev/null | head -1)
    libVer=$(basename "${libSo}" 2>/dev/null | sed 's/libpipewire-0\.3\.so\.0\.//')
    daemonVer=$(pipewire --version 2>/dev/null | sed -n 's/.*Compiled with libpipewire *//p' | head -1)
    [ -z "${daemonVer}" ] && daemonVer=$(pipewire --version 2>/dev/null | awk '/pipewire/ {print $NF; exit}')
    info "pipewiresink plugin version: ${plugVer:-unknown}"
    info "libpipewire shared object:   ${libSo:-unknown}"
    info "pipewire daemon version:     ${daemonVer:-unknown}"
    if [ -n "${plugVer}" ] && [ -n "${daemonVer}" ]; then
        pMaj=$(echo "${plugVer}" | cut -d. -f1-2)
        dMaj=$(echo "${daemonVer}" | cut -d. -f1-2)
        if [ "${pMaj}" = "${dMaj}" ]; then
            pass "Plugin and daemon are from the same PipeWire release (${pMaj})"
        else
            fail "Plugin is ${plugVer} but the daemon is ${daemonVer}"
            note "A mismatched pipewiresink leaves the node suspended and every"
            note "playlist plays silently with nothing logged.  Reinstall the"
            note "distro plugin: sudo apt-get install --reinstall gstreamer1.0-pipewire"
        fi
    fi

    hdr "Locally built plugins shadowing the distro ones"
    # build_pipewire_gst_plugin.sh installs into /usr/local; a stale build
    # left there overrides the packaged plugin for every process.
    localPlugins=$(ls /usr/local/lib/*/gstreamer-1.0/libgstpipewire.so \
                      /usr/local/lib/gstreamer-1.0/libgstpipewire.so 2>/dev/null)
    if [ -n "${localPlugins}" ]; then
        warn "A locally built PipeWire GStreamer plugin is installed:"
        echo "${localPlugins}" | sed 's/^/       /'
        note "It takes precedence over the packaged one.  If audio is silent,"
        note "this is the first thing to remove."
    else
        pass "No locally built plugin is shadowing the packaged one"
    fi
    if [ -n "${GST_PLUGIN_PATH}" ]; then
        warn "GST_PLUGIN_PATH is set in this environment: ${GST_PLUGIN_PATH}"
    fi

    hdr "Broken / blacklisted plugins"
    # gst-inspect -b always prints a header and a total, so count the total
    # rather than testing whether it printed anything.
    blOut=$(gst-inspect-1.0 -b 2>/dev/null)
    blCount=$(echo "${blOut}" | sed -n 's/^Total count: *\([0-9]*\) blacklisted.*/\1/p' | head -1)
    # Plugins FPP never loads.  Debian ships them, their optional runtime is
    # not installed, so they blacklist on a perfectly healthy player and there
    # is nothing to fix -- libgstonnx wants ONNX Runtime for ML inference and
    # nothing in FPP references it (issue #2934).  Named individually rather
    # than counted away, so a blacklisted plugin FPP *does* use still warns.
    blIgnore="libgstonnx"
    blFiles=$(echo "${blOut}" | sed -n 's/^ *\(libgst[A-Za-z0-9_.-]*\.so\).*/\1/p')
    blReal=$(echo "${blFiles}" | grep -vE "^(${blIgnore})\.so$" | grep -v '^$')
    blBenign=$(echo "${blFiles}" | grep -cE "^(${blIgnore})\.so$")
    if [ -n "${blReal}" ]; then
        warn "GStreamer has blacklisted plugin file(s) FPP may need:"
        echo "${blReal}" | sed 's/^/       /'
        note "A blacklisted plugin failed to load, so every element it provides"
        note "is missing.  Try: rm -rf /root/.cache/gstreamer-1.0 and retest."
    elif [ "${blCount:-0}" -gt 0 ] 2>/dev/null; then
        pass "No blacklisted GStreamer plugins that FPP uses"
        info "${blCount} blacklisted file(s), all optional plugins FPP never loads:"
        echo "${blFiles}" | sed 's/^/       /' | head -10
    else
        pass "No blacklisted GStreamer plugins"
    fi

    hdr "Plugin registry"
    for reg in /root/.cache/gstreamer-1.0 "${FPPHOME:-/home/fpp}/.cache/gstreamer-1.0"; do
        [ -d "${reg}" ] && info "Registry cache: $(ls -l "${reg}" 2>/dev/null | tail -n +2 | tr '\n' ' ')"
    done

}

#############################################################################
section_network() {
    echo "=== Network Audio & Video Outputs ==="

    hdr "AES67"
    # AES67Manager::Init() returns early unless MediaBackend is "pipewire"
    # (AES67Manager.cpp:102-110), so in simple mode none of this is running
    # however the JSON is left configured.
    aesj="${CFGDIR}/pipewire-aes67-instances.json"
    if simple_mode; then
        skip "AES67 is an advanced-mode feature - not started in simple mode"
    elif [ -f "${aesj}" ] && have jq; then
        n=$(jq -r "${JQ_ENABLED}"' [.instances[]? | select(enabled)] | length' "${aesj}" 2>/dev/null)
        info "Enabled AES67 streams: ${n:-0}"
        jq -r "${JQ_ENABLED}"' .instances[]? | "       \(.name // "?")  ip=\(.multicastIP // "?")  ch=\(.channels // "?")  iface=\(if (.interface // "") == "" then "default route" else .interface end)  enabled=\(enabled)  sap=\((.sapEnabled != false))"' \
            "${aesj}" 2>/dev/null
        if [ "${n:-0}" -gt 0 ] 2>/dev/null; then
            # An AES67 sender is a PipeWire sink with autoconnect off; only an
            # output-group member carrying node.target links into it.  A stream
            # nothing feeds transmits silence, which looks identical on the wire.
            targets=$(declared_node_targets)
            jq -r "${JQ_ENABLED}"' .instances[]? | select(enabled) | .name' "${aesj}" 2>/dev/null |
            while read -r sname; do
                slug=$(echo "${sname}" | tr 'A-Z' 'a-z' | sed 's/[^a-zA-Z0-9_]/_/g')
                if echo "${targets}" | grep -q "aes67.*${slug}"; then
                    echo "[PASS] AES67 stream '${sname}' is fed by an output group"
                else
                    echo "[WARN] Nothing in the audio group config feeds AES67 stream '${sname}'"
                    echo "       It will transmit silence.  Add it as a member of an"
                    echo "       Audio Output Group on the Audio Output Groups page."
                fi
            done
        fi

        for f in "${PW_CONFD}/96-fpp-aes67-rtp.conf" "${PW_CONFD}/96-fpp-aes67-sap.conf"; do
            [ -f "${f}" ] && info "Present: ${f}"
        done

        hdr "PTP clock (AES67)"
        ptpOn=$(jq -r '.ptpEnabled // false' "${aesj}" 2>/dev/null)
        if [ "${ptpOn}" = "true" ]; then
            if [ -x /usr/sbin/ptp4l ]; then
                pass "ptp4l is installed"
            else
                fail "PTP is enabled but ptp4l is not installed"
                note "Install: sudo apt-get install linuxptp"
            fi
            if pgrep -x ptp4l >/dev/null 2>&1; then
                pass "ptp4l is running (PID $(pgrep -x ptp4l | tr '\n' ' '))"
            else
                fail "PTP is enabled but ptp4l is not running"
                note "Receivers will drift out of sync.  Check fppd.log for the"
                note "AES67Manager PTP lines."
            fi
            ptpIf=$(jq -r '.ptpInterface // ""' "${aesj}" 2>/dev/null)
            if [ -n "${ptpIf}" ] && [ "${ptpIf}" != "null" ]; then
                if ip link show "${ptpIf}" >/dev/null 2>&1; then
                    pass "PTP interface ${ptpIf} exists"
                    if have ethtool && ethtool -T "${ptpIf}" 2>/dev/null | grep -q "hardware-transmit"; then
                        pass "${ptpIf} supports hardware timestamping"
                    else
                        info "${ptpIf} has software timestamping only (workable, less precise)"
                    fi
                else
                    fail "PTP interface ${ptpIf} does not exist on this device"
                fi
            fi
        else
            skip "PTP is not enabled"
        fi

        hdr "Graph clock rate (AES67)"
        # AES67 is a 48 kHz interoperability profile: the payload format is
        # fixed, so a sender never emits anything else.  A graph running at
        # some other rate therefore does not fail -- PipeWire silently puts an
        # adaptive resampler in front of the sender and it transmits valid
        # 48 kHz audio.  That is worth naming rather than leaving to be
        # inferred from a rate printed in another section: it costs CPU, adds
        # latency, and undoes some of the point of a clock-locked stream.
        aesN=$(jq -r "${JQ_ENABLED}"' [.instances[]? | select(enabled)] | length' "${aesj}" 2>/dev/null)
        if [ "${aesN:-0}" -eq 0 ] 2>/dev/null; then
            skip "No AES67 streams enabled"
        else
            gRate=$(pw_setting clock.rate)
            gAllowed=$(pw_setting clock.allowed-rates)
            if [ -z "${gRate}" ]; then
                skip "Graph clock rate is not readable (pw-metadata unavailable)"
            elif [ "${gRate}" = "${AES67_RATE}" ]; then
                pass "Graph clock is ${gRate} Hz, matching AES67"
            else
                warn "Graph clock is ${gRate} Hz, but AES67 is ${AES67_RATE} Hz only"
                note "The stream is still valid: PipeWire resamples into the sender."
                note "It costs CPU and latency on a stream that exists to be clock-locked."
                case "${gAllowed}" in
                    *"${AES67_RATE}"*)
                        note "${AES67_RATE} is in clock.allowed-rates, so the graph can switch to it." ;;
                    "") ;;
                    *)  note "${AES67_RATE} is NOT in clock.allowed-rates (${gAllowed}), so the"
                        note "graph can never switch and the resampler is permanent." ;;
                esac
                # Which file wins is not the obvious one: conf.d is read in
                # name order, so a higher-numbered generated file overrides the
                # rate 90-fpp.conf asks for.  Printing the chain saves an
                # editing session spent on a file that is being overridden.
                rateFiles=$(grep -l "default\.clock\.rate" "${PW_CONFD}"/*.conf 2>/dev/null | sort)
                if [ -n "${rateFiles}" ]; then
                    note "default.clock.rate is set by, in the order conf.d reads them:"
                    for rf in ${rateFiles}; do
                        rv=$(sed -n 's/.*default\.clock\.rate[^0-9]*\([0-9][0-9]*\).*/\1/p' "${rf}" 2>/dev/null | head -1)
                        echo "         $(basename "${rf}") = ${rv:-?}"
                    done
                    note "The last one listed wins."
                fi
                note "To change it: Audio Sample Rate on the Audio settings page, or"
                note "the per-card rate in PipeWire Audio Groups in Advanced mode."
            fi
        fi

        hdr "Multicast routing (AES67 / SAP)"
        # With one interface up the default route is the only place multicast
        # can go and no explicit route is needed.  With two it becomes a
        # coin flip, and AES67 leaving over Wi-Fi is a real failure mode.
        #
        # A stream that names an interface is exempt from all of that: the
        # sender is built with multicast-iface=<iface> (IP_MULTICAST_IF), which
        # the kernel honours ahead of the routing table.  Only streams that
        # leave the interface blank follow the default route, so the route is
        # only worth warning about when such a stream exists.
        upIfaces=$(ip -o link show up 2>/dev/null |
                   awk -F": " '{print $2}' | grep -vE "^(lo|docker|veth|br-)" | tr '\n' ' ')
        ifCount=$(echo "${upIfaces}" | wc -w)
        info "Interfaces up: ${upIfaces:-none}"

        pinnedN=$(jq -r "${JQ_ENABLED}"' [.instances[]? | select(enabled)
                        | select((.interface // "") != "")] | length' "${aesj}" 2>/dev/null)
        unpinnedN=$(jq -r "${JQ_ENABLED}"' [.instances[]? | select(enabled)
                        | select((.interface // "") == "")] | length' "${aesj}" 2>/dev/null)

        if [ "${pinnedN:-0}" -gt 0 ] 2>/dev/null; then
            info "Streams pinned to an interface (routing table does not apply):"
            jq -r "${JQ_ENABLED}"' .instances[]? | select(enabled)
                   | select((.interface // "") != "")
                   | "\(.name // "?")\u001f\(.interface)"' "${aesj}" 2>/dev/null |
            while IFS=$(printf '\037') read -r pname pif; do
                if ip link show "${pif}" >/dev/null 2>&1; then
                    echo "       ${pname} -> ${pif}"
                else
                    echo "[FAIL] AES67 stream '${pname}' is pinned to ${pif}, which does not exist"
                    echo "       The sender cannot bind its socket and will not transmit."
                fi
            done
        fi

        # Suggest a real wired interface rather than a hardcoded eth0.
        suggestIf=""
        for i in ${upIfaces}; do
            [ -d "/sys/class/net/${i}/wireless" ] && continue
            suggestIf="${i}"
            break
        done
        [ -z "${suggestIf}" ] && suggestIf=$(echo "${upIfaces}" | awk '{print $1}')
        [ -z "${suggestIf}" ] && suggestIf="eth0"

        if ip route show 2>/dev/null | grep -q "^224.0.0.0/4"; then
            pass "An explicit route for 224.0.0.0/4 exists"
            ip route show 2>/dev/null | grep "^224.0.0.0/4" | sed 's/^/       /'
        elif [ "${unpinnedN:-0}" -eq 0 ] 2>/dev/null && [ "${pinnedN:-0}" -gt 0 ] 2>/dev/null; then
            pass "No explicit multicast route, but every enabled AES67 stream names its own interface"
            note "The route table is not consulted for these streams."
        elif [ "${ifCount}" -gt 1 ] 2>/dev/null; then
            warn "${unpinnedN:-0} AES67 stream(s) name no interface, and ${ifCount} interfaces are up"
            jq -r "${JQ_ENABLED}"' .instances[]? | select(enabled)
                   | select((.interface // "") == "") | "       \(.name // "?")"' "${aesj}" 2>/dev/null
            note "Those streams follow the default route and may leave over the"
            note "wrong interface.  Set the interface on the stream (Audio Output"
            note "Groups -> AES67), or pin the route for everything:"
            note "  sudo ip route add 224.0.0.0/4 dev ${suggestIf}"
        else
            pass "Multicast follows the only interface that is up"
        fi

        # The SAP announcer is a separate socket from the senders: it takes
        # IP_MULTICAST_IF from the global PTP interface, never from the
        # per-stream one.  Pinned senders with no PTP interface set therefore
        # transmit correctly while their announcements go out the default route.
        sapOn=$(jq -r "${JQ_ENABLED}"' [.instances[]? | select(enabled)
                      | select((.sapEnabled != false))] | length' "${aesj}" 2>/dev/null)
        sapIf=$(jq -r '.ptpInterface // ""' "${aesj}" 2>/dev/null)
        [ "${sapIf}" = "null" ] && sapIf=""
        if [ "${sapOn:-0}" -gt 0 ] 2>/dev/null && [ "${ifCount}" -gt 1 ] 2>/dev/null &&
           [ -z "${sapIf}" ] && ! ip route show 2>/dev/null | grep -q "^224.0.0.0/4"; then
            warn "SAP announcements are not pinned to an interface"
            note "The announcer uses the PTP interface, which is unset, so SAP"
            note "follows the default route even where the audio streams do not."
            note "Receivers may not discover streams that are transmitting fine."
            note "Set the PTP interface, or add the 224.0.0.0/4 route above."
        elif [ "${sapOn:-0}" -gt 0 ] 2>/dev/null && [ -n "${sapIf}" ]; then
            info "SAP announcements go out ${sapIf} (the PTP interface)"
        fi
        info "Multicast group memberships:"
        (netstat -gn 2>/dev/null || ip maddr show 2>/dev/null) | sed 's/^/       /' | head -25
    else
        skip "No AES67 configuration on this device"
    fi

    hdr "Opus RTP"
    # Same gate as AES67 (OpusRTPManager.cpp:66-74).
    opusj="${CFGDIR}/pipewire-opus-rtp-instances.json"
    if simple_mode; then
        skip "Opus RTP is an advanced-mode feature - not started in simple mode"
    elif [ -f "${opusj}" ] && have jq; then
        jq -r "${JQ_ENABLED}"' .instances[]? | "       \(.name // "?")  mode=\(.mode // "send")  dest=\(.destIP // .destination // "?"):\(.port // "?")  iface=\(.interface // "default")  enabled=\(enabled)"' \
            "${opusj}" 2>/dev/null
        n=$(jq -r "${JQ_ENABLED}"' [.instances[]? | select(enabled)] | length' "${opusj}" 2>/dev/null)
        if [ "${n:-0}" -gt 0 ] 2>/dev/null; then
            if gst-inspect-1.0 opusenc >/dev/null 2>&1 && gst-inspect-1.0 rtpopuspay >/dev/null 2>&1; then
                pass "Opus RTP elements (opusenc, rtpopuspay) are available"
            else
                fail "Opus RTP is configured but opusenc/rtpopuspay are missing"
                note "Install: sudo apt-get install gstreamer1.0-plugins-base"
            fi
            # A sender bound to an interface that has since been renamed or
            # unplugged fails at socket-bind time, well before any packet.
            jq -r "${JQ_ENABLED}"' .instances[]? | select(enabled)
                   | "\(.name // "?")\u001f\(.interface // "")"' "${opusj}" 2>/dev/null |
            while IFS=$(printf '\037') read -r oname oif; do
                [ -z "${oif}" ] && continue
                if ip link show "${oif}" >/dev/null 2>&1; then
                    echo "[PASS] Opus RTP '${oname}' sends via ${oif}, which exists"
                else
                    echo "[FAIL] Opus RTP '${oname}' is bound to ${oif}, which does not exist"
                    echo "       The sender cannot bind its socket and will not transmit."
                fi
            done
        else
            skip "No Opus RTP streams enabled"
        fi
    else
        skip "No Opus RTP configuration on this device"
    fi

    hdr "RTSP outputs"
    # The enable flag and port live in pipewire-rtsp-outputs.json, but the
    # mounts do not: an RTSP stream is a *member of a Video Output Group*, so
    # RTSPOutputManager::LoadMounts() reads pipewire-video-consumers.json and
    # keeps the entries with type "rtsp".  Reading .streams[] out of the
    # outputs file (as this check used to) looks for a key that never exists,
    # so every device looked like it had no streams.
    rtspj="${CFGDIR}/pipewire-rtsp-outputs.json"
    vconsj="${CFGDIR}/pipewire-video-consumers.json"
    if [ -f "${rtspj}" ] && have jq; then
        enabled=$(jq -r '.enabled // false' "${rtspj}" 2>/dev/null)
        port=$(jq -r ".port // ${RTSP_DEFAULT_PORT}" "${rtspj}" 2>/dev/null)
        info "RTSP output enabled=${enabled} port=${port}"

        # LoadConfig() clamps an out-of-range port back to the default, so the
        # listener is not where the file says it is.
        if [ "${port}" -lt 1024 ] 2>/dev/null || [ "${port}" -gt 65535 ] 2>/dev/null; then
            warn "Port ${port} is out of range; fppd falls back to ${RTSP_DEFAULT_PORT}"
            port=${RTSP_DEFAULT_PORT}
        fi

        # A mount fppd will actually serve needs a video source: ResolveSourceNode()
        # takes sourceNode or streamSlots, and a member with neither is logged
        # and skipped -- which can empty the mount list even with members present.
        rtspN=0
        rtspSourced=0
        if [ -f "${vconsj}" ]; then
            rtspN=$(jq -r '[.[]? | select(.type == "rtsp")] | length' "${vconsj}" 2>/dev/null)
            rtspSourced=$(jq -r '[.[]? | select(.type == "rtsp")
                                 | select((.sourceNode // "") != ""
                                          or ((.streamSlots // []) | length) > 0)] | length' \
                          "${vconsj}" 2>/dev/null)
            if [ "${rtspN:-0}" -gt 0 ] 2>/dev/null; then
                jq -r '.[]? | select(.type == "rtsp")
                       | "       \(.name // "?")  mount=\(.mountPoint // "(auto)")  source=\(if (.sourceNode // "") != "" then .sourceNode elif ((.streamSlots // []) | length) > 0 then "slots " + (.streamSlots | join(",")) else "NONE" end)  audio=\(.audioEnabled // false)"' \
                    "${vconsj}" 2>/dev/null
            fi
        fi

        if [ "${enabled}" != "true" ]; then
            skip "RTSP output is disabled"
        elif [ "${rtspN:-0}" -eq 0 ] 2>/dev/null && simple_mode; then
            # An RTSP mount is a member of a Video Output Group, and video
            # output groups are an advanced-mode feature, so in simple mode
            # there is no way to add one and nothing to report -- the enabled
            # flag is just left over from the mode that could use it.
            skip "RTSP output is enabled but its mounts are an advanced-mode feature"
        elif [ "${rtspN:-0}" -eq 0 ] 2>/dev/null; then
            # Not a fault, and the commonest reason for a silent port: fppd
            # returns from ApplyConfig() before StartServer() when the mount
            # list is empty, logging "enabled but no mounts configured".
            warn "RTSP output is enabled but no Video Output Group member serves it"
            note "The server is only started once at least one RTSP mount exists,"
            note "so nothing listening on port ${port} is expected here, not a fault."
            note "Add an RTSP member to a Video Output Group to publish a stream."
        elif [ "${rtspSourced:-0}" -eq 0 ] 2>/dev/null; then
            fail "${rtspN} RTSP mount(s) exist but none names a video source"
            note "fppd skips a mount with no sourceNode and no streamSlots, which"
            note "leaves nothing to serve, so the server never starts."
            note "Set the video source on the group those members belong to."
        elif have ss && ss -lnt 2>/dev/null | grep -q ":${port} "; then
            pass "Something is listening on RTSP port ${port}"
            ss -lntp 2>/dev/null | grep ":${port} " | sed 's/^/       /'
        elif ! pgrep -x fppd >/dev/null 2>&1; then
            fail "Nothing is listening on RTSP port ${port} because fppd is not running"
            note "fppd hosts the RTSP server itself."
        else
            fail "fppd is running with ${rtspSourced} servable mount(s) but port ${port} is not open"
            note "The server could not bind - another service most likely has the"
            note "port.  Check fppd.log for RTSPOutputManager lines."
        fi
    else
        skip "No RTSP output configuration on this device"
    fi

    hdr "Ports in use by the media stack"
    if have ss; then
        ss -lnup 2>/dev/null | grep -iE "fppd|pipewire" | sed 's/^/       /' | head -20
        ss -lntp 2>/dev/null | grep -iE "fppd|pipewire" | sed 's/^/       /' | head -20
    fi

}

#############################################################################
section_video() {
    echo "=== Video Input & Output ==="

    hdr "Capture devices"
    # A Pi exposes a dozen /dev/video* nodes for its own ISP and codec
    # blocks.  Listing those buries the one device the user actually
    # plugged in, so only real capture sources are reported.
    real=""
    for d in /dev/video*; do
        [ -c "${d}" ] || continue
        name=$(v4l2-ctl -d "${d}" --info 2>/dev/null | sed -n 's/^[[:space:]]*Card type[[:space:]]*:[[:space:]]*//p' | head -1)
        case "${name}" in
            pispbe*|rpi-hevc*|rpivid*|bcm2835-codec*|*ISP*) continue ;;
        esac
        # Output-only and metadata nodes cannot be a video input source.
        v4l2-ctl -d "${d}" --list-formats 2>/dev/null | grep -q "\[0\]" || continue
        info "${d}  ${name:-unknown}"
        real="${real} ${d}"
    done
    if [ -n "${real}" ]; then
        pass "Capture device(s) available:${real}"
    else
        info "No capture devices (normal unless a camera or capture card is attached)"
        if ls /dev/video* >/dev/null 2>&1; then
            note "There are /dev/video* nodes, but they are all internal ISP or"
            note "codec devices, not capture sources."
        fi
    fi

    hdr "Configured video input sources"
    vsj="${CFGDIR}/pipewire-video-input-sources.json"
    if [ -f "${vsj}" ] && have jq; then
        jq -r "${JQ_ENABLED}"' (.videoInputSources // .sources // [])[]?
               | [(.name // "?"), (.type // "?"),
                  (.device // .uri // .url // ""), (enabled|tostring),
                  (.pipeWireNodeName // "")] | join("\u001f")' \
            "${vsj}" 2>/dev/null > "${TMPDIR_DIAG}/vsources.txt"
        if [ -s "${TMPDIR_DIAG}/vsources.txt" ]; then
            while IFS=$(printf '\037') read -r sname stype starget senabled snode; do
                if [ "${senabled}" != "true" ]; then
                    echo "[SKIP] Source '${sname}' (${stype}) is disabled"
                elif [ "${stype}" = "v4l2src" ]; then
                    if [ -c "${starget}" ]; then
                        echo "[PASS] Source '${sname}' -> ${starget} exists"
                    else
                        echo "[FAIL] Source '${sname}' -> ${starget} does not exist"
                        echo "       The capture device is unplugged, or the kernel"
                        echo "       renumbered it after a reboot.  Re-pick the device"
                        echo "       on the Video Inputs page."
                    fi
                else
                    echo "[INFO] Source '${sname}' type=${stype} target=${starget:-<none>}"
                fi
            done < "${TMPDIR_DIAG}/vsources.txt"
        else
            skip "No video input sources are configured"
        fi
    else
        skip "No video input sources configured"
    fi

    hdr "Video output consumers"
    # A consumer names the DRM connector it draws on and the source node it
    # draws from.  An unplugged display or a renamed source leaves the
    # consumer configured and producing nothing.
    vcj="${CFGDIR}/pipewire-video-consumers.json"
    if [ -f "${vcj}" ] && have jq; then
        srcNodes=$(jq -r '(.videoInputSources // .sources // [])[]? | .pipeWireNodeName // empty' \
                   "${vsj}" 2>/dev/null)
        jq -r '(if type == "array" then .[] else (.consumers // [])[] end)?
               | [(.name // .groupName // "?"), (.type // "?"), (.connector // ""),
                  (.cardPath // ""), (.sourceNode // "")] | join("\u001f")' \
            "${vcj}" 2>/dev/null > "${TMPDIR_DIAG}/vconsumers.txt"
        while IFS=$(printf '\037') read -r cname ctype cconn ccard csrc; do
            [ -z "${cname}" ] && continue
            if [ -n "${ccard}" ] && [ ! -e "${ccard}" ]; then
                echo "[FAIL] Consumer '${cname}': DRM device ${ccard} does not exist"
                echo "       The card numbering changed, usually after a kernel update."
                echo "       Re-pick the display on the Video Output Groups page."
            fi
            if [ -n "${cconn}" ]; then
                st=$(cat /sys/class/drm/card*-${cconn}/status 2>/dev/null | head -1)
                case "${st}" in
                    connected)
                        echo "[PASS] Consumer '${cname}' -> ${cconn} is connected" ;;
                    disconnected)
                        echo "[WARN] Consumer '${cname}' -> ${cconn} has nothing plugged in"
                        echo "       Video is rendered to a display that is not there." ;;
                    "")
                        echo "[FAIL] Consumer '${cname}' -> connector ${cconn} does not exist"
                        echo "       Re-pick the display on the Video Output Groups page." ;;
                    *)
                        echo "[INFO] Consumer '${cname}' -> ${cconn} status ${st}" ;;
                esac
            fi
            if [ -n "${csrc}" ]; then
                if echo "${srcNodes}" | grep -qx "${csrc}"; then
                    echo "[PASS] Consumer '${cname}' draws from ${csrc}, which is configured"
                else
                    echo "[FAIL] Consumer '${cname}' draws from ${csrc}, which no source provides"
                    echo "       The video input source was renamed or deleted; this"
                    echo "       output will stay black."
                fi
            fi
        done < "${TMPDIR_DIAG}/vconsumers.txt"
    else
        skip "No video output consumers configured"
    fi

    hdr "Video output routing"
    vsink=$(setting PipeWireVideoSinkName)
    if [ -n "${vsink}" ]; then
        info "PipeWireVideoSinkName = ${vsink}"
        if pw_dump && graph_node_names | grep -qx "${vsink}"; then
            pass "Video sink ${vsink} exists in the graph"
        elif declared_node_names | grep -qx "${vsink}"; then
            warn "Video sink ${vsink} is declared in the config but not in the graph"
        else
            info "Video sink ${vsink} is created on demand when video plays"
        fi
    else
        skip "Video routing through PipeWire is not configured"
    fi

    for f in "${CFGDIR}/pipewire-video-groups.json" \
             "${CFGDIR}/pipewire-video-groups-simple.json" \
             "${CFGDIR}/pipewire-video-consumers.json" \
             "${CFGDIR}/pipewire-stream-slots.json"; do
        [ -f "${f}" ] || continue
        if json_ok "${f}"; then
            pass "Valid: $(basename "${f}")"
        else
            fail "Corrupt JSON: ${f}"
        fi
    done

    hdr "Display / DRM"
    if [ -d /sys/class/drm ]; then
        for c in /sys/class/drm/card*/status; do
            [ -f "${c}" ] || continue
            echo "       $(dirname "${c}" | xargs basename): $(cat "${c}" 2>/dev/null)"
        done
    else
        info "No DRM devices (headless build)"
    fi

}

#############################################################################
section_performance() {
    echo "=== Latency, Clocking & Dropouts ==="

    hdr "Graph settings"
    if have pw-metadata; then
        timeout 5 pw-metadata -n settings 2>/dev/null | sed 's/^/       /' | head -25
    else
        skip "pw-metadata not available"
    fi

    hdr "Xruns (audio dropouts)"
    # An xrun is the graph missing its deadline: the audible symptom is a
    # click or a gap.  A non-zero and rising count is the difference between
    # 'audio is broken' and 'audio is fine but the Pi is overloaded'.
    #
    # pw-top's counts live in the ERR column, and are cumulative for the life
    # of the node -- a node that glitched once while its links were being
    # built carries that 1 forever.  So the total is reported per node, and
    # the wording says to re-run rather than implying it is happening now.
    # The column is located by header name: reading a fixed field number got
    # WAIT ("852.3us" -> 8523) and reported gibberish totals in the thousands.
    #
    # -n 2 because pw-top's first pass is a priming snapshot that reports all
    # zeros; only the last snapshot holds real measurements.
    if have pw-top; then
        if timeout 10 pw-top -b -n 2 > "${TMPDIR_DIAG}/pwtop.txt" 2>/dev/null &&
           [ -s "${TMPDIR_DIAG}/pwtop.txt" ]; then
            # Display the final snapshot only; two stacked copies of the graph
            # read as duplicate nodes.
            awk '/^S +ID/ {buf = $0 "\n"; next} {buf = buf $0 "\n"} END {printf "%s", buf}' \
                "${TMPDIR_DIAG}/pwtop.txt" 2>/dev/null | head -40 | sed 's/^/       /'

            # Keyed by node ID, not by name: a name repeats (every bare
            # "gst-launch-1.0" client carries the same one), and keying on it
            # silently dropped every node but the last of each name -- both from
            # the per-node list and from the total.
            #
            # Split, because the two halves do not mean the same thing.  An xrun
            # on an ALSA adapter (fpp_alsa_* / fpp_alsain_*) is the card missing
            # its deadline: audible, and worth chasing.  An xrun on a client node
            # means that client was late handing over a buffer, which the sink's
            # own buffer usually absorbs inaudibly -- and those nodes are created
            # per playback (see StreamSlotManager: a slot holds a GStreamerOutput
            # only while it plays), so their counts belong to whichever instance
            # happens to be alive right now.  Comparing them between runs reads
            # as a trend that is really just a fresh node paying its start-up
            # cost again, which is exactly how a looping file looks.
            splitErrs=$(awk '
                /^S +ID/ { errcol = 0
                           for (i = 1; i <= NF; i++) if ($i == "ERR") errcol = i
                           delete seen; delete name; next }
                errcol && $errcol ~ /^[0-9]+$/ && $errcol + 0 > 0 {
                           seen[$2] = $errcol + 0; name[$2] = $NF }
                END { for (k in seen)
                          printf "%s %s %s\n",
                                 (name[k] ~ /^fpp_alsa/ ? "dev" : "cli"),
                                 seen[k], name[k] }
            ' "${TMPDIR_DIAG}/pwtop.txt" 2>/dev/null)
            devErrs=$(echo "${splitErrs}" | awk '$1 == "dev" {print $2, $3}' | sort -rn)
            cliErrs=$(echo "${splitErrs}" | awk '$1 == "cli" {print $2, $3}' | sort -rn)
            hasErrCol=$(awk '/^S +ID/ {for (i = 1; i <= NF; i++) if ($i == "ERR") {print "yes"; exit}}' \
                        "${TMPDIR_DIAG}/pwtop.txt" 2>/dev/null)

            if [ -z "${hasErrCol}" ]; then
                skip "pw-top output has no ERR column - cannot count xruns"
            elif [ -z "${devErrs}" ] && [ -z "${cliErrs}" ]; then
                pass "No xruns reported"
            else
                if [ -n "${devErrs}" ]; then
                    xr=$(echo "${devErrs}" | awk '{s += $1} END {print s+0}')
                    warn "${xr} xrun(s) on the audio device since the graph started"
                    echo "${devErrs}" | awk '{printf "       %s  %s\n", $1, $2}'
                    note "These nodes live until the PipeWire services restart, so"
                    note "this total is comparable between runs: re-run while audio"
                    note "is playing and see whether it has moved."
                    note "If it is climbing: raise AudioPeriodSize (Audio settings)"
                    note "or default.clock.quantum in ${PW_CONFD}/90-fpp.conf, or"
                    note "reduce what else is running.  Note that both are counts of"
                    note "frames, so a cape that refines the rate upward (a PCM5102A"
                    note "asked for 44100 lands on 88200) gets half the wall-clock"
                    note "margin from the same number."
                else
                    pass "No xruns on the audio device - nothing audible was dropped"
                fi
                if [ -n "${cliErrs}" ]; then
                    xc=$(echo "${cliErrs}" | awk '{s += $1} END {print s+0}')
                    info "${xc} xrun(s) on client nodes (context, not a fault)"
                    echo "${cliErrs}" | awk '{printf "       %s  %s\n", $1, $2}'
                    note "Playback nodes (fppd_stream_N) and short-lived client"
                    note "pipelines are rebuilt per track, and a few xruns while a"
                    note "node's links are being built are normal.  Each new"
                    note "instance starts its own count, so these numbers are not"
                    note "a trend and do not accumulate across tracks."
                fi
            fi
        else
            skip "pw-top produced no output (daemon not reachable)"
        fi
    else
        skip "pw-top not available"
    fi

    hdr "CPU and thermal"
    info "Load average: $(cat /proc/loadavg 2>/dev/null)"
    if have vcgencmd; then
        thr=$(vcgencmd get_throttled 2>/dev/null)
        info "Throttling: ${thr}"
        case "${thr}" in
            *=0x0) pass "No throttling or under-voltage recorded" ;;
            "")    ;;
            *)     warn "The Pi has thrown a throttling/under-voltage flag: ${thr}"
                   note "Under-voltage causes USB audio dropouts and card resets."
                   note "Use a supply that can hold 5V under load." ;;
        esac
        info "SoC temperature: $(vcgencmd measure_temp 2>/dev/null)"
    fi
    gov=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null)
    if [ -n "${gov}" ]; then
        if [ "${gov}" = "performance" ]; then
            pass "CPU governor: performance"
        else
            info "CPU governor: ${gov} (ondemand is fine for most shows)"
        fi
    fi

    hdr "Media stack process usage"
    ps -eo pid,pri,ni,pcpu,pmem,rss,comm 2>/dev/null |
        grep -E "PID|pipewire|wireplumber|fppd" | sed 's/^/       /'

    hdr "fppd"
    if pgrep -x fppd >/dev/null 2>&1; then
        pass "fppd is running (PID $(pgrep -x fppd | tr '\n' ' '))"
    else
        warn "fppd is not running - nothing is producing audio"
    fi

}

#############################################################################
section_smoke() {
    echo "=== End-to-end Pipeline Test ==="
    echo
    echo "These run real GStreamer pipelines at zero volume.  They prove the"
    echo "path FPP uses can be built and started; they make no sound."

    if ! have gst-launch-1.0; then
        fail "gst-launch-1.0 is not installed - cannot run the pipeline test"
        return
    fi

    hdr "Silent playback through pipewiresink"
    if timeout 20 gst-launch-1.0 -q audiotestsrc num-buffers=20 volume=0 \
            ! audioconvert ! audioresample ! pipewiresink \
            > "${TMPDIR_DIAG}/smoke.txt" 2>&1; then
        pass "A pipeline reached the default PipeWire sink and ran to completion"
    else
        fail "The pipeline could not play to PipeWire"
        sed 's/^/       /' "${TMPDIR_DIAG}/smoke.txt" 2>/dev/null | head -12
        note "This is the same path media playback uses; if it fails here,"
        note "no playlist will produce audio either."
    fi

    hdr "Silent playback into the configured sink"
    target=$(setting PipeWireSinkName)
    if [ -n "${target}" ]; then
        if timeout 20 gst-launch-1.0 -q audiotestsrc num-buffers=20 volume=0 \
                ! audioconvert ! audioresample \
                ! pipewiresink "target-object=${target}" \
                > "${TMPDIR_DIAG}/smoke2.txt" 2>&1; then
            pass "A pipeline reached ${target} and ran to completion"
        else
            fail "Could not play into the configured sink ${target}"
            sed 's/^/       /' "${TMPDIR_DIAG}/smoke2.txt" 2>/dev/null | head -12
            note "The sink FPP is configured to use cannot be reached, even"
            note "though a default-sink pipeline may work."
        fi
    else
        skip "PipeWireSinkName is not set - nothing specific to test"
    fi

    hdr "Capture from PipeWire"
    # The authoritative test is pw-record: it proves the daemon will hand a
    # client real capture data.  A standalone "gst-launch pipewiresrc" probe
    # used to stand in for this and was the wrong question twice over.  It
    # reported a bare warning with no output (it times out rather than
    # failing, so there was nothing to print), and its stated consequence was
    # wrong: FPP's senders do not capture this way.  AES67Manager and
    # OpusRTPManager build a pipewiresrc that is the *destination* -- it
    # registers a node under a known name and the audio group's filter chain
    # links into it via node.target -- deliberately NOT target-object (see the
    # comment at AES67Manager.cpp:2078).  A probe that cannot resolve a target
    # therefore says nothing about whether those senders work.
    capNode=$(pw_setting default.audio.source | sed -n 's/.*"name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')
    if [ -z "${capNode}" ] && have jq && pw_dump; then
        capNode=$(jq -r '[.[] | select(.type=="PipeWire:Interface:Node")
                          | .info.props | select(.["media.class"] == "Audio/Source")
                          | .["node.name"]] | first // empty' \
                  "${PW_DUMP_FILE}" 2>/dev/null)
    fi

    if ! have pw-record; then
        skip "pw-record is not available - cannot test capture"
    elif [ -z "${capNode}" ]; then
        skip "No audio source node in the graph - nothing to capture from"
    else
        rm -f "${TMPDIR_DIAG}/capture.wav"
        timeout 6 pw-record --target "${capNode}" "${TMPDIR_DIAG}/capture.wav" \
            > "${TMPDIR_DIAG}/smoke3.txt" 2>&1
        # pw-record is killed by the timeout, so its exit status is always
        # failure; whether bytes landed in the file is the actual answer.
        capBytes=0
        if [ -f "${TMPDIR_DIAG}/capture.wav" ]; then
            capBytes=$(wc -c < "${TMPDIR_DIAG}/capture.wav" 2>/dev/null)
        fi
        if [ "${capBytes:-0}" -gt 1024 ] 2>/dev/null; then
            pass "Captured ${capBytes} bytes from ${capNode}"
        else
            warn "Could not capture from ${capNode}"
            sed 's/^/       /' "${TMPDIR_DIAG}/smoke3.txt" 2>/dev/null | head -8
            note "Audio input groups read through this path.  Check that the"
            note "device is not held open by another program, and that it"
            note "appears under Audio Input in the PipeWire settings."
        fi
    fi

    hdr "Decoder availability"
    # Not having a decoder for the format a user's media is in is one of the
    # most common 'it plays on my PC' reports.
    for f in mp3 aac flac ogg opus wav; do
        case "${f}" in
            mp3)  el=mpg123audiodec ; alt=avdec_mp3 ;;
            aac)  el=faad           ; alt=avdec_aac ;;
            flac) el=flacdec        ; alt=avdec_flac ;;
            ogg)  el=vorbisdec      ; alt=avdec_vorbis ;;
            opus) el=opusdec        ; alt=avdec_opus ;;
            wav)  el=wavparse       ; alt=wavparse ;;
        esac
        if gst-inspect-1.0 "${el}" >/dev/null 2>&1; then
            pass "${f}: ${el}"
        elif gst-inspect-1.0 "${alt}" >/dev/null 2>&1; then
            pass "${f}: ${alt} (via libav)"
        else
            warn "${f}: no decoder found - files in this format will not play"
        fi
    done

}

#############################################################################
case "${1:-all}" in
    services|config|graph|alsa|gstreamer|network|video|performance|smoke)
        run_section "$1"
        ;;
    all)
        for s in services config graph alsa gstreamer network video performance; do
            run_section "${s}"
            echo
        done
        ;;
    *)
        echo "Usage: $0 {services|config|graph|alsa|gstreamer|network|video|performance|smoke|all}"
        exit 1
        ;;
esac
exit 0
