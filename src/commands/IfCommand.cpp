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

#include "fpp-pch.h"

#include "Condition.h"
#include "../Timers.h"
#include "../common.h"
#include "../log.h"

#include "IfCommand.h"

#include <mutex>

IfCommand::IfCommand() :
    LocalOnlyCommand("If") {
    // One condition-tree editor, not two competing ways to say the same
    // thing - a single Check/Is/Value field pair alongside a separate,
    // Advanced-only "Rules" box that could add more conditions was
    // confusing (which one is actually being used?). The "conditionlist"
    // widget already supports exactly "one condition" through "several
    // conditions with AND/OR" in one place (fpp.js
    // RenderConditionListRows only shows the AND/OR toggle once there are
    // 2+ rows, so the single-condition case still looks simple) - it's
    // always visible now, not gated behind UI Level.
    args.push_back(CommandArg("conditions", "conditionlist", "Check")
                       .setHelp("What to test. With more than one condition, choose whether ALL or ANY of them must be true."));
    args.push_back(CommandArg("thenCommands", "commandlist", "Then Run")
                       .setSection("When Check is True")
                       .setHelp("Commands run when the check above is True."));
    args.push_back(CommandArg("thenMode", "string", "Then Run")
                       .setSection("When Check is True")
                       .setContentList({ "Sequential", "Parallel" })
                       .setDefaultValue("Sequential")
                       .setToggleStyle()
                       .setToggleLabel("Order")
                       .setHelp("Sequential waits for each command to finish before starting the next. "
                                "Parallel fires them all together without waiting."));
    args.push_back(CommandArg("elseCommands", "commandlist", "Otherwise Run")
                       .setDefaultValue("")
                       .setSection("When Check is False")
                       .setHelp("Commands run when the check above is False. Leave empty to do nothing."));
    args.push_back(CommandArg("elseMode", "string", "Otherwise Run")
                       .setSection("When Check is False")
                       .setContentList({ "Sequential", "Parallel" })
                       .setDefaultValue("Sequential")
                       .setToggleStyle()
                       .setToggleLabel("Order")
                       .setHelp("Sequential waits for each command to finish before starting the next. "
                                "Parallel fires them all together without waiting."));
}

namespace
{

    struct CommandListEntry {
        std::string command;
        std::vector<std::string> args;
    };

    std::vector<CommandListEntry> ParseCommandList(const std::string& json) {
        std::vector<CommandListEntry> result;
        if (json.empty()) {
            return result;
        }
        Json::Value root;
        if (!LoadJsonFromString(json, root)) {
            return result; // LoadJsonFromString() already logged the parse error
        }
        if (!root.isArray()) {
            LogWarn(VB_COMMAND, "If: command list JSON was valid but not an array, treating as empty: %s\n",
                    TruncateForLog(json).c_str());
            return result;
        }
        for (auto& entry : root) {
            if (!entry.isObject()) {
                LogWarn(VB_COMMAND, "If: command list entry was not a JSON object, skipping: %s\n",
                        TruncateForLog(SaveJsonToString(entry)).c_str());
                continue;
            }
            CommandListEntry e;
            e.command = entry.get("command", "").asString();
            if (e.command.empty()) {
                continue;
            }
            if (entry.isMember("args") && entry["args"].isArray()) {
                for (auto& a : entry["args"]) {
                    e.args.push_back(a.asString());
                }
            }
            result.push_back(e);
        }
        return result;
    }

    std::string DescribeEntry(const CommandListEntry& entry) {
        std::string argsStr;
        for (auto& a : entry.args) {
            if (!argsStr.empty()) {
                argsStr += ", ";
            }
            argsStr += "\"" + TruncateForLog(a) + "\"";
        }
        return entry.command + "(" + argsStr + ")";
    }

    // A Sequential Then/Else list that isn't finished yet - the next entry
    // in "remaining" starts as soon as "inFlight" (the currently-running
    // entry's Result) reports isDone(). Never blocks the thread that called
    // If::run(): AdvancePendingChains() (below) is what actually advances
    // this, driven by a self-re-arming one-shot Timers timer that exists
    // only while chains are pending (same pattern as
    // RecurringTasks::armPollTimer()). This replaces the old sleep-loop -
    // busy-waiting was fine from an HTTP handler thread, but If is also
    // reached synchronously from Sequence::SendSequenceData() (the
    // per-frame playback path, via a frame-triggered Command Preset) and
    // from GPIO edge handlers, where a 1-second stall is a real problem.
    struct PendingChain {
        std::vector<CommandListEntry> remaining;
        std::unique_ptr<Command::Result> inFlight;
        long long startTimeMS = 0;
    };

    // Same rationale as RecurringTasks::PENDING_RESULT_MAX_MS - don't let a
    // single hung command (e.g. a URL fetch with no server-side timeout)
    // keep a chain alive forever; drop it and move on, same as a normal
    // Sequential wait giving up after its own bounded poll used to.
    constexpr long long PENDING_CHAIN_MAX_MS = 60000;

    // How often to re-check an in-flight entry's Result while chains are
    // pending - roughly the old worst-case advance latency, when the fppd
    // main loop called a tick every iteration at up to 50ms sleeps.
    constexpr long long PENDING_POLL_MS = 50;

    std::mutex g_pendingLock;
    std::vector<PendingChain> g_pendingChains;

    void AdvancePendingChains();

    // One-shot rather than periodic for the same reason as
    // RecurringTasks::armPollTimer(): a periodic timer stopping itself from
    // inside its own callback would delete the std::function currently
    // executing. Every push into g_pendingChains is followed by an arm on
    // the same thread (RunCommandList runs on arbitrary threads - HTTP
    // handlers, GPIO edges, the frame path - and Timers::addTimer is
    // thread-safe), so an advance can never be missed; at worst an arm
    // races an in-progress advance and the extra fire finds nothing to do.
    void ArmChainPollTimer() {
        Timers::INSTANCE.addTimer("IfCommand-pendingChains", GetTimeMS() + PENDING_POLL_MS, []() {
            AdvancePendingChains();
        });
    }

    // Starts chain.remaining.front() (removing it from the queue) as the new
    // inFlight entry, or clears inFlight if the chain is exhausted - the
    // caller (either RunCommandList kicking a chain off, or tick() advancing
    // one) is responsible for then deciding whether to keep the chain around.
    void StartNextEntry(PendingChain& chain) {
        if (chain.remaining.empty()) {
            chain.inFlight.reset();
            return;
        }
        CommandListEntry entry = std::move(chain.remaining.front());
        chain.remaining.erase(chain.remaining.begin());
        // Deliberately not gated on WillLog(): a line at Debug or below is
        // retained in the crash ring even when the configured level discards it
        // (see CrashLogRingWillCapture), so gating would drop the If tracing
        // out of crash dumps - which is exactly where it earns its keep.
        LogDebug(VB_COMMAND, "If: running command %s\n", DescribeEntry(entry).c_str());
        chain.inFlight = CommandManager::INSTANCE.run(entry.command, entry.args);
        chain.startTimeMS = GetTimeMS();
    }

    // Sequential: fires the first entry, then queues the rest as a
    // PendingChain that IfCommand::tick() advances asynchronously - never
    // waits on this thread. Parallel: the exact loop shape TriggerPreset
    // already uses (fire each entry, never wait on isDone() between them) -
    // unchanged, was never the source of the stall risk.
    void RunCommandList(std::vector<CommandListEntry> list, bool sequential) {
        if (list.empty()) {
            return;
        }
        LogDebug(VB_COMMAND, "If: running %zu command(s) %s\n", list.size(), sequential ? "sequentially" : "in parallel");
        if (!sequential) {
            for (auto& entry : list) {
                // Ungated for the same reason as StartNextEntry() above.
                LogDebug(VB_COMMAND, "If: running command %s\n", DescribeEntry(entry).c_str());
                CommandManager::INSTANCE.run(entry.command, entry.args);
            }
            return;
        }
        PendingChain chain;
        chain.remaining = std::move(list);
        StartNextEntry(chain);
        if (!chain.inFlight) {
            return; // shouldn't happen (list wasn't empty), but nothing to track either way
        }
        if (chain.inFlight->isDone()) {
            // Finished synchronously (most commands do) - just keep going on
            // this thread rather than parking a chain for tick() to find
            // already-done on its very next pass.
            while (chain.inFlight->isDone() && !chain.remaining.empty()) {
                StartNextEntry(chain);
            }
            if (chain.inFlight && chain.inFlight->isDone()) {
                return; // fully drained synchronously
            }
        }
        {
            std::unique_lock<std::mutex> l(g_pendingLock);
            g_pendingChains.push_back(std::move(chain));
        }
        ArmChainPollTimer();
    }

    void AdvancePendingChains() {
        std::vector<PendingChain> toAdvance;
        {
            std::unique_lock<std::mutex> l(g_pendingLock);
            if (g_pendingChains.empty()) {
                return;
            }
            toAdvance = std::move(g_pendingChains);
            g_pendingChains.clear();
        }
        std::vector<PendingChain> stillPending;
        for (auto& chain : toAdvance) {
            bool timedOut = (GetTimeMS() - chain.startTimeMS) > PENDING_CHAIN_MAX_MS;
            if (timedOut && chain.inFlight && !chain.inFlight->isDone()) {
                LogWarn(VB_COMMAND, "If: a Sequential command did not finish within %lldms, abandoning the rest of its chain\n",
                        PENDING_CHAIN_MAX_MS);
                continue; // drop the whole chain, same as RecurringTasks giving up on a hung task
            }
            while (chain.inFlight && chain.inFlight->isDone() && !chain.remaining.empty()) {
                StartNextEntry(chain);
            }
            if (chain.inFlight && !chain.inFlight->isDone()) {
                stillPending.push_back(std::move(chain));
            }
            // else: inFlight finished and remaining is empty - chain is done, drop it.
        }
        if (!stillPending.empty()) {
            {
                std::unique_lock<std::mutex> l(g_pendingLock);
                for (auto& chain : stillPending) {
                    g_pendingChains.push_back(std::move(chain));
                }
            }
            ArmChainPollTimer();
        }
    }

} // namespace

std::unique_ptr<Command::Result> IfCommand::run(const std::vector<std::string>& args) {
    if (args.size() < 5) {
        return std::make_unique<Command::ErrorResult>(
            "If requires conditions, thenCommands, thenMode, elseCommands, elseMode");
    }
    const std::string& conditions = args[0];
    const std::string& thenCommandsJson = args[1];
    bool thenSequential = args[2] != "Parallel";
    const std::string& elseCommandsJson = args[3];
    bool elseSequential = args[4] != "Parallel";

    LogExcess(VB_COMMAND, "If: Check = %s\n", TruncateForLog(conditions, 2000).c_str());

    if (conditions.empty()) {
        return std::make_unique<Command::ErrorResult>("If: no condition configured under Check");
    }
    Json::Value conditionsJson;
    std::unique_ptr<ConditionNode> condition;
    if (LoadJsonFromString(conditions, conditionsJson)) {
        condition = ConditionNode::FromJSON(conditionsJson);
    }
    if (!condition) {
        return std::make_unique<Command::ErrorResult>("If: could not parse the condition(s) under Check");
    }

    bool matched = condition->evaluate();
    // Two tiers, matching the rest of the Command facility: Debug carries the
    // decision - the causal link between this If and the commands that follow,
    // which each log themselves at Info from CommandManager::run() - while the
    // command-list JSON goes to Excessive. That JSON is up to 2000 chars
    // restating, less legibly, what those next lines already say, and Excessive
    // is the one level the crash ring does not capture, so it stays out of the
    // 256 slots the ring has to work with.
    LogDebug(VB_COMMAND, "If: overall result = %s -> running %s\n",
             matched ? "true" : "false", matched ? "Then Run" : "Otherwise Run");
    LogExcess(VB_COMMAND, "If: %s command list = %s\n", matched ? "Then Run" : "Otherwise Run",
              TruncateForLog(matched ? thenCommandsJson : elseCommandsJson, 2000).c_str());
    auto list = ParseCommandList(matched ? thenCommandsJson : elseCommandsJson);
    bool sequential = matched ? thenSequential : elseSequential;

    if (!list.empty()) {
        RunCommandList(std::move(list), sequential);
    } else {
        LogDebug(VB_COMMAND, "If: %s has no commands configured, nothing to run\n",
                 matched ? "Then Run" : "Otherwise Run");
    }

    return std::make_unique<Command::Result>(matched ? "true" : "false");
}
