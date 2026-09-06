/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include "PipeWireGraphConfig.h"

#include "common.h"

// See the header for why this reads the generated conf rather than the graph.
bool PipeWireGraphFeedsNode(const std::string& nodeName) {
    static const char* confs[] = {
        "/etc/pipewire/pipewire.conf.d/97-fpp-audio-groups.conf",
        "/etc/pipewire/pipewire.conf.d/96-fpp-input-groups.conf"
    };

    if (GetFileContents(confs[0]).empty()) {
        return true;
    }

    const std::string needle = "node.target";
    for (const char* conf : confs) {
        const std::string contents = GetFileContents(conf);
        for (std::size_t t = contents.find(needle); t != std::string::npos;
             t = contents.find(needle, t + 1)) {
            // node.target = "<value>" -- take what is between the next two
            // quotes and compare whole, so aes67_stream_1_send does not match
            // a hypothetical aes67_stream_10_send.
            std::size_t a = contents.find('"', t + needle.size());
            if (a == std::string::npos) {
                continue;
            }
            std::size_t b = contents.find('"', a + 1);
            if (b == std::string::npos) {
                continue;
            }
            if (contents.compare(a + 1, b - a - 1, nodeName) == 0) {
                return true;
            }
        }
    }
    return false;
}
