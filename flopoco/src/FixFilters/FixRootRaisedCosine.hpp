#ifndef FIXROOTRAISEDCOSINE_HPP
#define FIXROOTRAISEDCOSINE_HPP

#include "Operator.hpp"
#include "FixFIR.hpp"
#include "utils.hpp"

#include <iostream>
#include <sstream>
#include <vector>

#include <string.h>


namespace flopoco{

	class FixRootRaisedCosine : public FixFIR
	{
	public:

		FixRootRaisedCosine(OperatorPtr parentOp, Target* target, int lsbIn, int lsbOut, int N, double alpha_);

		virtual ~FixRootRaisedCosine();

		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		static void registerFactory();
		static TestList unitTest(int index);

	private: 
		int N; /* FixFIR::n = 2*N+1 */
		double alpha;
	};

}


#endif
