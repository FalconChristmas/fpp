/*
 *   ExpressionProcessor - wrapper around tinyexpr
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _EXPRESSIONPROCESSOR_H
#define _EXPRESSIONPROCESSOR_H

#include <string>

class ExpressionProcessorData;

class ExpressionProcessor {
public:
    class ExpressionVariable {
    public:
        ExpressionVariable(const std::string &n);
        ~ExpressionVariable();
        const std::string &getName() const { return name;};
        
        void setValue(const std::string &s);
        const std::string &getValue() const { return sValue;};
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
    void bindVariable(ExpressionVariable *var);
    bool compile(const std::string &s);
    
    std::string evaluate(const std::string &type);
    
private:
    ExpressionProcessorData *data;
};



#endif
