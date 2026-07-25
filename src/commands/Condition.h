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

#include "fpp-json-fwd.h"
#include <memory>
#include <string>
#include <vector>

// Shared condition tree backing the "If" command's Check field (a
// conditionlist widget - one or more conditions, AND'd or OR'd together).
// Every leaf is a direct, synchronous call into an existing subsystem
// accessor - no I/O, no blocking, safe to call from any thread including
// (eventually, Phase 3) the player loop.
class ConditionNode {
public:
    virtual ~ConditionNode() = default;
    virtual bool evaluate() const = 0;

    // Compound tree: {"op":"AND"|"OR","not":bool,"conditions":[...]} for a
    // group, or {"source":...,"name":...,"comparator":...,"value":...,"not":bool}
    // for a leaf. Returns nullptr (never throws) on malformed JSON - callers
    // should treat that as "condition false".
    static std::unique_ptr<ConditionNode> FromJSON(const Json::Value& j);

    // Reads one leaf's current raw value (before any comparator/Value is
    // applied) - the exact same lookup a leaf's evaluate() uses internally,
    // exposed for the If editor's "Show Current Value" preview button so
    // picking the right Value to compare against isn't guesswork. found is
    // set false if source/name doesn't currently resolve to anything (e.g.
    // an MQTT topic never seen, or an unset Variable).
    static std::string PreviewSourceValue(const std::string& source, const std::string& name, bool& found);

    // Full ad-hoc-leaf evaluation for the Check editor's consolidated
    // eye-preview modal: given the same Source/Name/Comparator/Value/Not a
    // saved leaf would have, returns the LHS/RHS values it currently
    // resolves to and the comparator's result - reuses the exact same
    // evaluation path a real saved leaf's evaluate() does, so this preview
    // can never drift from what actually happens at runtime.
    static void PreviewLeafResult(const std::string& source, const std::string& name, const std::string& comparatorStr,
                                   const std::string& value, bool negate, bool& lhsFound, std::string& lhsValue,
                                   std::string& rhsValue, bool& result);

    // How a Name/Value field's raw text will actually be treated at
    // evaluate() time - same priority order as the private
    // EvaluateNameOrExpression() in Condition.cpp: an exact match against a
    // real variable name always wins first, before any '='/%%/formula
    // detection ever runs. Exposed for GET /api/variables?validateExpression
    // (Variables.cpp) so the Check editor's valid/invalid icon can tell "this
    // matches a real variable" and "this compiles as a formula" apart from
    // "this is just inert literal text" (Condition.cpp's own documented,
    // non-error passthrough for unmatched plain text) - the raw
    // ExpressionProcessor::compile() the icon used before always reported
    // "valid" for the literal case too, so a typo'd variable name (e.g.
    // missing/extra characters) looked identical to a real match.
    enum class ExpressionKind { Variable,
                                 Formula,
                                 Literal };
    static ExpressionKind ClassifyNameOrExpression(const std::string& text, bool& compileOk);
};
