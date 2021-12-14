#ifndef OPERATORPIPELINE_FUNCTION_HPP_
#define OPERATORPIPELINE_FUNCTION_HPP_

#include "OPExpression.hpp"

#include <string>

namespace OperatorPipeline
{

typedef enum function_enum
{
	LOG,
	EXP
} function_enum;

class OPFunction : public OPExpression
{
public:
    function_enum function(){
        return function_;
    }

    OPExpression* operande(){
        return operande_;
    }

	OPFunction(function_enum function, OPExpression *operande);
	~OPFunction();
	
private:
	OPExpression *operande_;
	function_enum function_;
};

}

#endif
