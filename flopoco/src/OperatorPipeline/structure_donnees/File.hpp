#ifndef FILE_HPP_
#define FILE_HPP_

#include "OPExpression.hpp"
#include "OPVariable.hpp"

#include <vector>
#include <map>
#include <string>

namespace OperatorPipeline
{

struct File
{
	std::map<std::string, OPVariable**> variables;
	std::vector<OPVariable*> returns;
	std::vector<OPExpression*> expressions;
};

}

#endif

