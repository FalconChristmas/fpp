#pragma once
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

// Questions about the PipeWire graph FPP generates, answered from the conf
// files rather than from the running daemon.  Shared by the network audio
// senders (AES67, Opus RTP), which have to know what the graph will link
// before they create the node it would link to.

#include <string>

// Will anything in FPP's generated PipeWire graph feed this node?
//
// A network sender is a PipeWire *sink*: its pipewiresrc is created with
// node.autoconnect=false, so the only thing that ever links into it is an
// Audio Output Group member carrying node.target = "<nodeName>".  Those group
// members are the whole of the answer, and they live in the confs the group
// pages generate -- 97 for output groups (which is also where Simple mode's
// synthetic group lands) and 96 for input groups, both read by PipeWire at
// daemon startup.
//
// Asking the *running* graph instead cannot work, and not for want of a
// parser: the node does not exist until the pipeline that creates it starts,
// so there is nothing to look for until after the decision has been made.  The
// generated conf is what PipeWire will link when the node does appear, which
// makes it the only thing that can answer this in advance.
//
// A missing 97 conf means the group pages have never generated one, and this
// then cannot tell "nothing feeds it" from "FPP does not manage this graph" --
// so it says fed and lets the pipeline try, which is what every release before
// this one did unconditionally.
bool PipeWireGraphFeedsNode(const std::string& nodeName);
