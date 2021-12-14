#ifndef OPERATORPIPELINE_OPERATION_HPP_
#define OPERATORPIPELINE_OPERATION_HPP_


#include "OPExpression.hpp"

namespace OperatorPipeline
{

enum operator_enum
{
    OP_PLUS,
    OP_MOINS,
    OP_MUL,
    OP_DIV
};
	
class OPOperation : public OPExpression
{
public:
    operator_enum get_operator(){
        return operator_;
    }

    OPExpression* op_left(){
        return op_left_;
    }

    OPExpression* op_right(){
        return op_right_;
    }

	OPOperation(OPExpression* op_left, OPExpression* op_right, operator_enum op);
	~OPOperation();

    void SetOpLeft(OPExpression* op_left)
	{
		op_left_ = op_left;
	}
	
private:
    OPExpression* op_left_;
	OPExpression* op_right_;
	operator_enum operator_;
};

}

#endif
