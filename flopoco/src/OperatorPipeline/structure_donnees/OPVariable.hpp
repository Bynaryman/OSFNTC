#ifndef OPERATORPIPELINE_VARIABLE_HPP_
#define OPERATORPIPELINE_VARIABLE_HPP_

#include "OPExpression.hpp"

#include <vector>
#include <string>
#include <iostream>

namespace OperatorPipeline
{

typedef enum type_enum
{
	UNDEFINED,
	UINT,
	SINT,
	FLOAT
} type_enum;

class OPVariable : public OPExpression
{
public:

    type_enum GetType(){
        return type_;
    }

    std::string GetValue(){
        return value_;
    }

    OPExpression* GetExpression(){
        return expression_;
    }

    std::vector<int> type_param(){
        return *type_param_;
    }
	
	OPVariable(std::string value, type_enum type, std::vector<int>* type_param, OPExpression* expression);
	
	~OPVariable();

    void SetValue(std::string value)
	{
		value_ = value;
	}
	
private:
	std::string value_;
	type_enum type_;
	std::vector<int>* type_param_;
    OPExpression* expression_;
};

}

#endif
