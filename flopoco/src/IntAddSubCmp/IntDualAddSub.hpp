#ifndef IntDualAddSub_HPP
#define IntDualAddSub_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <gmpxx.h>
#include "Operator.hpp"


namespace flopoco{

	/** The IntDualAddSub class computes both X-Y and Y-X, or both X-Y and X+Y
	 */
	class IntDualAddSub : public Operator
	{
	public:
		static const int SUBSUB = 0;
		static const int ADDSUB = 1;
		/**
		 * The IntDualAddSub constructor
		 * @param[in] target the target device
		 * @param[in] wIn    the with of the inputs and output
		 * @param[in] opType:  if 1, compute X-Y and X+Y; if 0, compute X-Y and Y-X
		 * @param[in] inputDelays the delays for each input
		 **/
		IntDualAddSub(Operator* parentOp, Target* target, int wIn, int opType);
		/*IntDualAddSub(Target* target, int wIn);
		  void cmn(Target* target, int wIn, map<string, double> inputDelays);*/

		/**
		 *  Destructor
		 */
		~IntDualAddSub();

		void emulate(TestCase* tc);
		void buildStandardTestCases(TestCaseList* tcl);
		static TestList unitTest(int index);
		
		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		static void registerFactory();



	protected:
		int wIn_;                         /**< the width for X, Y and the results */
		int opType_;					  /**< the operation type. if 0, op type is x-y y-x; if 1 op_type is x-y x+y */
		string son;			   	  /**< second output name; can be yMx or xPy */

	private:
		int bufferedInputs;               /**< variable denoting an initial buffering of the inputs */
		int nbOfChunks;                   /**< the number of chunks that the addition will be split in */
		int chunkSize_;                   /**< the suggested chunk size so that the addition can take place at the objective frequency*/
		int *cSize;                       /**< array containing the chunk sizes for all nbOfChunks*/

	};
}
#endif
