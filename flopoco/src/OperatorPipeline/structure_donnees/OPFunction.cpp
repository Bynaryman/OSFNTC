#include "OPFunction.hpp"

namespace OperatorPipeline
{

OPFunction::OPFunction(function_enum function, OPExpression *operande)
{
	operande_ = operande;
	function_ = function;
}

OPFunction::~OPFunction()
{
	delete operande_;
}

}
