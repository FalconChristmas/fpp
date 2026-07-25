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

#include "Commands.h"

// Base class for FPP-internal commands whose whole job is evaluating *local*
// state (a GPIO pin, a local Variable, a local sensor reading) and reacting to
// it. Multisync broadcasts a command's args verbatim to other instances, which
// then execute it independently - coherent for a plain action, but for a
// local-state check "multisync the check" silently becomes "every instance
// re-evaluates the same check against its own, possibly different, local state"
// rather than propagating the result of one evaluation. The way to actually
// multisync the *outcome* of a check is to put a Multisync-enabled command
// inside the check's Then/Else branch instead.
//
// This is surfaced by overriding the existing getDescription() virtual (which
// makes the UI hide the Multisync option), NOT by adding a virtual to Command -
// see the ABI RULE in Commands.h. Only FPP's own commands subclass this;
// external plugins subclass Command directly, so nothing here affects the
// Command vtable they were compiled against.
class LocalOnlyCommand : public Command {
public:
    using Command::Command;

    Json::Value getDescription() override {
        Json::Value cmd = Command::getDescription();
        cmd["disallowMultisync"] = true;
        return cmd;
    }
};
