#!/usr/bin/bash
#
# Scan for available wireless networks on each wifi device and list them.
#
# We run BOTH scan tools for every adapter and show their results separately:
#   - `iw` (nl80211) is the modern path and the one FPP uses at runtime.
#   - `iwlist` (legacy WEXT) still works on some out-of-tree Realtek USB
#     drivers (e.g. RTL8812BU / RTL8822BU on 88x2bu) whose nl80211 scan is
#     broken and returns nothing.
# Showing them side by side is what tells a support person whether an adapter's
# nl80211 scan is the thing that's broken.
#
# Scan output is kept in shell variables (no temp files), so concurrent
# invocations can never interfere with each other.
#
# --summary: per device, its mode, channel and link quality, then per-channel
# network counts and signal, with no names or BSSIDs (a BSSID can be looked up
# in public Wi-Fi location databases).  Safe for crash reports.  iw scan only.

SUMMARY=0
[ "$1" = "--summary" ] && SUMMARY=1

# Parse raw `iw dev ... scan` output into numbered per-network lines.
parse_iw() {
    awk '
        function emit() {
            if (bss == "") return
            if (signal_dbm == "") pct = 0
            else {
                pct = int(2 * (signal_dbm + 100))
                if (pct < 0) pct = 0
                if (pct > 100) pct = 100
            }
            enc = secure ? "(secure)" : "(open)  "
            con = associated ? " - Connected" : ""
            freq_disp = (freq != "") ? sprintf("%.3f GHz", freq/1000) : ""
            printf("%5d : %s  %s %s %s (Signal strength: %d%%)%s\n",
                   ++n, ssid, freq_disp, bss, enc, pct, con)
        }
        /^BSS / {
            emit()
            bss=""; ssid=""; freq=""; signal_dbm=""; secure=0; associated=0
            if (match($0, /[0-9a-fA-F:]{17}/)) {
                bss = toupper(substr($0, RSTART, RLENGTH))
            }
            if ($0 ~ /associated/) associated = 1
        }
        /^\tfreq:/       { freq = $2 }
        /^\tsignal:/     { signal_dbm = $2 + 0 }
        /^\tSSID:/       { sub(/^\tSSID: ?/, ""); ssid = $0 }
        /^\tRSN:/        { secure = 1 }
        /^\tWPA:/        { secure = 1 }
        /capability:.*Privacy/ { secure = 1 }
        END { emit() }
    '
}

# Parse raw `iwlist ... scan` (WEXT) output into the same numbered line shape.
parse_iwlist() {
    awk '
        function emit() {
            if (bss == "") return
            enc = secure ? "(secure)" : "(open)  "
            printf("%5d : %s  %s %s %s (Signal strength: %d%%)\n",
                   ++n, ssid, freq_disp, bss, enc, pct)
        }
        /Cell [0-9]+ - Address:/ {
            emit()
            bss=""; ssid=""; freq_disp=""; pct=0; secure=0
            if (match($0, /[0-9a-fA-F:]{17}/)) {
                bss = toupper(substr($0, RSTART, RLENGTH))
            }
        }
        /ESSID:/ { if (match($0, /ESSID:"[^"]*"/)) ssid = substr($0, RSTART+7, RLENGTH-8) }
        /Frequency:[0-9.]+ GHz/ {
            if (match($0, /Frequency:[0-9.]+/))
                freq_disp = sprintf("%.3f GHz", substr($0, RSTART+10, RLENGTH-10))
        }
        /Signal level=.*dBm/ {
            if (match($0, /Signal level=-?[0-9]+/)) {
                dbm = substr($0, RSTART+13, RLENGTH-13) + 0
                pct = int(2 * (dbm + 100))
                if (pct < 0) pct = 0
                if (pct > 100) pct = 100
            }
        }
        /Encryption key:on/ { secure = 1 }
        /IE:.*WPA/          { secure = 1 }
        END { emit() }
    '
}

# Summarise raw `iw dev ... scan` output per channel: networks (BSSs), distinct
# network names, how many are strong enough to interfere (-70 dBm or better)
# and the strongest signal.  Names are counted, never printed.
summarise_iw() {
    awk '
        /^BSS / { n++ }
        /^\tfreq:/   { freq[n] = int($2) }
        /^\tsignal:/ { sig[n] = $2 + 0 }
        /^\tSSID:/   { sub(/^\tSSID: ?/, ""); ssid[n] = $0 }
        END {
            for (i = 1; i <= n; i++) {
                f = freq[i]
                if (f == 2484)      { band = "2.4"; ch = 14 }
                else if (f < 3000)  { band = "2.4"; ch = (f - 2407) / 5 }
                else if (f < 5000)  { band = "4.9"; ch = (f - 4000) / 5 }
                else if (f < 5925)  { band = "5";   ch = (f - 5000) / 5 }
                else if (f == 5935) { band = "6";   ch = 2 }
                else                { band = "6";   ch = (f - 5950) / 5 }
                k = sprintf("%08d\t%-14s", f, band " GHz ch " ch)
                bss[k]++
                # Hidden networks have no name to count
                if (ssid[i] != "" && !((k, ssid[i]) in seen)) { seen[k, ssid[i]] = 1; names[k]++ }
                if (i in sig) {
                    if (!(k in best) || sig[i] > best[k]) best[k] = sig[i]
                    if (sig[i] >= -70) strong[k]++
                }
            }
            for (k in bss)
                printf("%s %6d %6d %9d %12s\n", k, bss[k], names[k], strong[k],
                       (k in best) ? best[k] " dBm" : "n/a")
        }
    ' | sort | cut -f2-
}

# Print the per-channel table for one device's raw `iw dev ... scan` output.
show_channel_table() {
    local dev="$1" raw="$2"
    printf -- "----- networks per channel (iw dev %s scan) -----\n" "$dev"
    if grep -q '^BSS ' <<< "$raw"; then
        printf "%-14s %6s %6s %9s %12s\n" "channel" "BSSs" "names" ">=-70dBm" "strongest"
        summarise_iw <<< "$raw"
    else
        # Nothing parsed, so this is the scan's error text, not a network list
        printf "%s\n" "$raw"
    fi
    printf "\n"
}

# Print one scan tool's section: its parsed network list, or the raw output if
# nothing parsed (so a scan error or empty result stays visible).
show() {
    local label="$1" marker="$2" formatter="$3" raw="$4"
    printf -- "----- %s -----\n" "$label"
    if grep -q "$marker" <<< "$raw"; then
        "$formatter" <<< "$raw"
    else
        printf "%s\n" "$raw"
    fi
    printf "\n"
}

# Is this AP-mode interface currently beaconing to any associated stations?
# Active scanning stops beaconing, which would drop every one of them - on
# typical single-radio FPP hardware there's no way to scan while beaconing.
ap_client_count() {
    iw dev "$1" station dump 2>/dev/null | grep -c '^Station '
}

# Is this managed-mode interface currently associated to an AP?
# Echoes the SSID (may be empty) and returns non-zero if not connected.
#
# Asks the kernel over nl80211 rather than wpa_cli. Only "associated, and to
# what" is needed here, which iw answers directly, and it stays correct however
# the link came up -- wpa_cli additionally needs wpa_supplicant's control
# socket in /run/wpa_supplicant, which is a runtime directory owned by the
# generic wpa_supplicant.service that FPP does not use.
station_connected_ssid() {
    local out ssid
    out=$(iw dev "$1" link 2>/dev/null)
    grep -q '^Connected to' <<< "$out" || return 1
    ssid=$(sed -n 's/^[[:space:]]*SSID: //p' <<< "$out" | head -1)
    printf '%s' "$ssid"
}

while read -r wifi_device || [[ -n $wifi_device ]]; do
    [ -z "$wifi_device" ] && continue

    UPORDOWN=$(cat "/sys/class/net/$wifi_device/operstate" 2>/dev/null || echo down)
    ip link set "$wifi_device" up 2>/dev/null

    iftype=$(iw dev "$wifi_device" info 2>/dev/null | awk '$1=="type"{print $2}')

    printf "Wifi Device: %s\n" "$wifi_device"

    if [ "$iftype" = "AP" ] && [ "$(ap_client_count "$wifi_device")" -gt 0 ]; then
        # Hotspot with client(s) attached - scanning would drop beaconing and
        # disconnect all of them. No safe way to scan on a single radio, so skip.
        printf "%s: hotspot has %s client(s) connected - skipping disruptive scan\n\n" \
            "$wifi_device" "$(ap_client_count "$wifi_device")"
        [ "$UPORDOWN" = "down" ] && ip link set "$wifi_device" down 2>/dev/null
        continue
    fi

    if [ "$SUMMARY" -eq 1 ]; then
        # Mode, channel and TX power, and link quality if associated.  The addr
        # and ssid lines, and the "Connected to <BSSID>" line, are left out.
        iw dev "$wifi_device" info 2>/dev/null | grep -E '^[[:space:]]*(type|channel|txpower) '
        link=$(iw dev "$wifi_device" link 2>/dev/null)
        if grep -q '^Connected to' <<< "$link"; then
            printf "\tconnected\n"
            grep -E '^[[:space:]]*(freq|signal|rx bitrate|tx bitrate):' <<< "$link"
        else
            printf "\tnot connected\n"
        fi

        iw_scan=$(iw dev "$wifi_device" scan 2>&1)
        [ "$UPORDOWN" = "down" ] && ip link set "$wifi_device" down 2>/dev/null

        show_channel_table "$wifi_device" "$iw_scan"
        continue
    fi

    ssid=""
    connected=0
    if [ "$iftype" = "managed" ] && ssid=$(station_connected_ssid "$wifi_device"); then
        connected=1
    fi

    if [ "$connected" -eq 1 ]; then
        # Connected station. Scanning from here is routine - the station tells
        # its AP to buffer traffic before it leaves the channel, the same way
        # wpa_supplicant's own background/roam scans do - so run the normal
        # scans and just prefer the low-priority hint where the driver has it.
        printf "%s: connected to '%s'\n\n" "$wifi_device" "$ssid"
        if iw_scan=$(iw dev "$wifi_device" scan low-priority 2>&1); then
            show "iw dev $wifi_device scan (nl80211, low-priority)" '^BSS ' parse_iw "$iw_scan"
        else
            # Most in-kernel drivers (brcmfmac on the Pi's onboard radio
            # included) never advertise NL80211_FEATURE_LOW_PRIORITY_SCAN.
            printf -- "----- iw dev %s scan (nl80211, low-priority) -----\n%s\n\n" "$wifi_device" "$iw_scan"
        fi
    fi

    iw_scan=$(iw dev "$wifi_device" scan 2>&1)
    iwlist_scan=$(iwlist "$wifi_device" scan 2>&1)

    [ "$UPORDOWN" = "down" ] && ip link set "$wifi_device" down 2>/dev/null

    show "iw dev $wifi_device scan (nl80211)" '^BSS ' parse_iw "$iw_scan"
    show "iwlist $wifi_device scan (WEXT)" 'Cell ' parse_iwlist "$iwlist_scan"
done < <(iw dev | awk '$1=="Interface"{print $2}')
