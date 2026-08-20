#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2026 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "LocalOnlyCommand.h"

// An If evaluates local state, so it is a LocalOnlyCommand: multisyncing it
// would broadcast the raw condition check to other instances, which each
// independently re-evaluate it against their own local state
// (GPIO/Variable/Sensor/etc.) - not what "Multisync" means for any other
// command. Put a Multisync-enabled command inside Then/Else instead to
// propagate the *result* of a check. (LocalOnlyCommand emits the
// "disallowMultisync" UI flag; see its header for why that route, not a
// Command virtual.)
class IfCommand : public LocalOnlyCommand {
public:
    IfCommand();
    virtual ~IfCommand() {}
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
    // In-progress Sequential Then/Else command chains are advanced by a
    // self-re-arming Timers timer that exists only while chains are pending
    // - see AdvancePendingChains() in IfCommand.cpp for why Sequential
    // never waits on the calling thread: If is reached synchronously from
    // frame-triggered Command Presets and GPIO edge handlers, threads a
    // lighting player can't afford to stall.
};
