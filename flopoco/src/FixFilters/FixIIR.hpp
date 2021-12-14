#ifndef FIXIIR_HPP
#define FIXIIR_HPP

#include "Operator.hpp"
#include "utils.hpp"
#include "BitHeap/BitHeap.hpp"

namespace flopoco{

	class FixIIR : public Operator {

	public:
		/** @brief Constructor ; you must use bitheap in case of negative coefficient*/
		FixIIR(OperatorPtr parentOp, Target* target, int lsbIn, int lsbOut, vector<string> coeffb, vector<string> coeffa, double H=0.0, double Heps=0.0, bool buildWorstCaseTestBench=false);

		/** @brief Destructor */
		~FixIIR();

		// Below all the functions needed to test the operator
		/**
		 * @brief the emulate function is used to simulate in software the operator
		 * in order to compare this result with those outputed by the vhdl opertator
		 */
		void emulate(TestCase * tc);

		/** @brief function used to create Standard testCase defined by the developper */
		void buildStandardTestCases(TestCaseList* tcl);

		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		static TestList unitTest(int index);
		static void registerFactory();

	private:
		void computeImpulseResponse(); // evaluates the filter on an impulsion

		
	private:
		int lsbIn;					/**< weight of the LSB in the input, considered as a signed number in (-1,1) */
		int msbOut;					/**< weight of the MSB in the result */
		int lsbOut;					/**< weight of the LSB in the result */
		vector<string> coeffb;			/**< the b_i coefficients as strings */
		vector<string> coeffa;			/**< the a_i coefficients as strings */
		double H;						/**< Worst case peak gain of this filter */
		double Heps;						/**< Worst case peak gain of the error filter */
		bool buildWorstCaseTestBench; /**< if true, build the worst-case test bench */
		uint32_t n;							/**< number of taps on the numerator */
		uint32_t m;							/**< number of taps on the denominator */
		int g;							/**< number of guard bits used for the IIR -- more are used inside the SOPC */

	private:
		int hugePrec;

		mpfr_t* coeffb_mp;			/**< the coefficients as MPFR numbers */
		mpfr_t* coeffa_mp;			/**< the coefficients as MPFR numbers */
		mpfr_t* xHistory; // history of x used by emulate
		mpfr_t* yHistory; // history of y (result) used by emulate
		double* coeffb_d;           /**< version of coeffb as C-style arrays of double, because WCPG needs it this way */
		double* coeffa_d;           /**< version of coeffa as C-style arrays of double, because WCPG needs it this way */

		uint64_t currentIndex;       // used for round-robin access to the history 

		vector<double> ui;  // inputs in the trace of simulation in double precision
		vector<double> yi;  // outputs in the trace of simulation in double precision
		uint64_t vanishingK;
		double miny, maxy;
	};

}

#endif
