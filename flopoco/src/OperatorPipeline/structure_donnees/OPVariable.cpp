#include "OPVariable.hpp"

namespace OperatorPipeline
{

OPVariable::OPVariable(std::string value, type_enum type, std::vector<int>* type_param, OPExpression* expression)
{
	value_ = value;
	type_ = type;
	type_param_ = type_param;
	expression_ = expression;
}

OPVariable::~OPVariable()
{
	delete expression_;
	delete type_param_;
}

}
