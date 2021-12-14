#ifndef OperatorContext_hpp
#define OperatorContext_hpp

#include "Target.hpp"
#include "Operator.hpp"

namespace flopoco{

	class Operator;
 	typedef Operator* OperatorPtr;

	class OperatorContext
	{
	public:
		OperatorContext(OperatorPtr parentOp, Target* target, double frequency);

		Target* getTarget();
		OperatorPtr getParentOp();
		double getFrequency();
		
	private:
		OperatorPtr parentOp;
		Target* target;
		double frequency;
		
	};
}
		
#endif
