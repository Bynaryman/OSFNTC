#ifndef FIXATAN2_HPP
#define FIXATAN2_HPP
#include <iomanip>
#include <vector>
#include <sstream>
#include <math.h>
#include <gmp.h>
#include <gmpxx.h>

#include "../utils.hpp"
#include "../Operator.hpp"

#define PLANE_BASED				0
#define TAYLOR_ORDER1_BASED		1
#define TAYLOR_ORDER2_BASED		2
#define SINCOS_TABLE_BASED		3
#define POLYEVAL_BASED			4


namespace flopoco {

	class FixAtan2 : public Operator {

	public:
		/**
		 * The FixAtan2 generic constructor computes atan(y/x), faithful to outLSB.
		 * 		the inputs x and y are assumed to be x in [0, 1) an y in [0, 1)
		 * @param[in] target            target device
		 * @param[in] wIn               input width
		 * @param[in] wOut              output width
		 * @param[in] architectureType	type of architecture
		 * 									0 = based on the plane's equation
		 * 									1 = based on an order 1 Taylor approximating polynomial
		 * 									2 = based on an order 2 Taylor approximating polynomial
		 * 									3 = based on rotating using a table (for sine and cosine) and then using the Taylor series for (1/x)
		 **/

		FixAtan2(Target* target, int wIn, int wOut, map<string, double> inputDelays = emptyDelayMap);


		/**
		 *  Destructor
		 */
		~FixAtan2();

		/** The code generation for first/quadrant range reduction.
		 */
		void buildQuadrantRangeReduction();

		/** The code generation for second/scaling range reduction.
		 */
		void buildScalingRangeReduction();

		/** The code generation for (last) quadrant reconstruction */
		void buildQuadrantReconstruction();

		/**
		 * The emulate function from Operator
		 */
		void emulate(TestCase* tc);
		/**
		 * The buildStandardTestCases function from Operator
		 */
		void buildStandardTestCases(TestCaseList * tcl) ;
		
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();

	protected:

		int wIn;                     					/**< input width */
		int wOut;                    					/**< output width */

		bool negateByComplement=false; /**< An architecture parameter: we negate negative values to obtain the first octant */
		mpfr_t constPi;
	};

}
#endif
