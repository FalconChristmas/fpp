#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <string>

class ExpressionProcessorData;

class ExpressionProcessor {
public:
    class ExpressionVariable {
    public:
        ExpressionVariable(const std::string& n);
        ~ExpressionVariable();
        const std::string& getName() const { return name; };

        void setValue(const std::string& s);
        const std::string& getValue() const { return sValue; };

    private:
        std::string name;
        std::string sValue;
        double dValue;

        friend class ExpressionProcessorData;
    };

    ExpressionProcessor();
    ~ExpressionProcessor();

    // the ExpressionVariable MUST be locked in memory
    // as the address of the values may be used directly
    void bindVariable(ExpressionVariable* var);
    bool compile(const std::string& s);

    std::string evaluate(const std::string& type);

private:
    ExpressionProcessorData* data;
};
