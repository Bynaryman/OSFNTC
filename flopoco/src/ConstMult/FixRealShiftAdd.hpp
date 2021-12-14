#ifndef FIXREALSHIFTADD_HPP
#define FIXREALSHIFTADD_HPP

#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)

#include "FixRealConstMult.hpp"
#include "../Table.hpp"
#include "../BitHeap/BitHeap.hpp"

#include "pagsuite/types.h"
#include "pagsuite/adder_graph.h"

namespace flopoco{
	class FixRealShiftAdd : public FixRealConstMult
	{
	public:

		/**
		 * @brief Standalone version of KCM. Input size will be msbIn-lsbIn+1
		 * @param target : target on which we want the KCM to run
		 * @param signedIn : true if input are 2'complement fixed point numbers
		 * @param msbin : power of two associated with input msb. For unsigned 
		 * 				  input, msb weight will be 2^msb, for signed input, it
		 * 				  will be -2^msb
		 * 	@param lsbIn :  Weight of the least significant bit of the input
		 * 	@param lsbOut : Weight of the least significant bit of the output
		 * 	@param constant : string that describes the constant in sollya syntax
		 * 	@param targetUlpError :  error bound on result. Difference
		 * 							between result and real value should be
		 * 							less than targetUlpError * 2^lsbOut.
		 * 							Value has to be in ]0.5 ; 1] (if 0.5 wanted,
		 * 							please consider to create a one bit more
		 * 							precise KCM with a targetUlpError of 1 and
		 * 							truncate the result
		 */
		FixRealShiftAdd(
							 OperatorPtr thisOp,
							 Target* target, 
							 bool signedIn, 
							 int msbIn, 
							 int lsbIn, 
							 int lsbOut, 
							 string constant, 
							 double targetUlpError = 1.0
							 );


		// Overloading the virtual functions of Operator


		static OperatorPtr parseArguments(OperatorPtr parentOp, Target* target, vector<string>& args			);

		static void registerFactory();
		


		/** The heap of weighted bits that will be used to do the additions */
		BitHeap*	bitHeap;    	

		/** The input signal. */
		string inputSignalName;
		
		int numberOfTables;
		vector<int> m; /**< MSB of chunk i; m[0] == msbIn */
		vector<int> l; /**< LSB of chunk i; l[numberOfTables-1] = lsbIn, or maybe >= lsbIn if not all the input bits are used due to a small constant */
		vector<int> tableOutputSign; /**< +1: table is always positive. -1: table output is always negative. 0: table output is signed */

	protected:
		bool computeAdderGraph(PAGSuite::adder_graph_t &adderGraph, string &adderGraphStr, PAGSuite::int_t coefficient);
	};
}

#else
namespace flopoco{
	class FixRealShiftAdd	{
	public:
		static void registerFactory();
	};
}
#endif //defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)

#endif
