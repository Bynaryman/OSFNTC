#ifndef FixSOPC_HPP
#define FixSOPC_HPP

#include "Operator.hpp"
#include "utils.hpp"

#include "BitHeap/BitHeap.hpp"

/*  All flopoco operators and utility functions are declared within
  the flopoco namespace.
	You have to use flopoco:: or using namespace flopoco in order to access these
  functions.
*/

namespace flopoco{

	class FixSOPC : public Operator {
	public:

		/**
		 * @brief simplest constructor for inputs in the fixed-point format (0, lsbIn), computing msbOut out of the coeffs, computing the internal format.
		 * This constructor is all we need for a FIR
		 */
		FixSOPC(OperatorPtr parentOp_, Target* target, int lsbIn, int lsbOut, vector<string> coeff);

		/**
		 * @brief simple constructor for inputs in the fixed-point format (0, lsbIn) with msbOut provided.
		 */
		FixSOPC(OperatorPtr parentOp_, Target* target, int lsbIn, int msbOut, int lsbOut, vector<string> coeff);




		/**
		 * @brief Generic constructor for inputs in various formats and/or for splitting a SOPC into several ones, etc.
		 * msbOut must be provided.
		 * @param g
		 *			If g=-1, the number of needed guard bits will be computed for a faithful result, and a final round bit added in position lsbOut-1.
		 *			If g=0, the architecture will have no guard bit, no final round bit will be added. The architecture will not be faithful.
		 *			If g>0, the provided number of guard bits will be used and a final round bit added in position lsbOut-1.
		 */
		FixSOPC(OperatorPtr parentOp_, Target* target, vector<int> msbIn, vector<int> lsbIn, int msbOut, int lsbOut, vector<string> coeff_, int g=-1, double targetError = 0.0);




				/**
		 * @brief Generic constructor for inputs in various formats and/or for splitting a SOPC into several ones, etc.
		 * msbOut must be provided.
		 * This version deduces msbIn from maxX. It leads to finer optimization in FixIIR
		 * @param g
		 *			If g=-1, the number of needed guard bits will be computed for a faithful result, and a final round bit added in position lsbOut-1.
		 *			If g=0, the architecture will have no guard bit, no final round bit will be added. The architecture will not be faithful.
		 *			If g>0, the provided number of guard bits will be used and a final round bit added in position lsbOut-1.
		 */
		FixSOPC(OperatorPtr parentOp_, Target* target, vector<double> maxX, vector<int> lsbIn, int msbOut, int lsbOut, vector<string> coeff_, int g=-1, double targetError = 0.0);


		
		/** @brief destructor */
		~FixSOPC();

		/** @brief The method that does most of operator construction for the two constructors */
		void initialize();

		/** @brief Overloading the method of Operator */
		void emulate(TestCase * tc);

		/** @brief Overloading the method of Operator */
		void buildStandardTestCases(TestCaseList* tcl);

		/** @brief This method does most of the work for emulate(), because we want to call it also from the emulate() of FixFIR */
		pair<mpz_class,mpz_class> computeSOPCForEmulate(vector<mpz_class> x);

		// User-interface stuff
		/** Factory method - these are for internal use, by default FixSOPC should not be listed in Interfaced.txt */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		static OperatorPtr parseArgumentsFull(OperatorPtr parentOp, Target *target , vector<string> &args);
		static TestList unitTest(int index);
		static void registerFactory();

	protected:
		int n;							        /**< number of products, also size of the vectors coeff, msbIn and lsbIn */
		vector<double> maxAbsX;			/**< max absolute value of each input */
		vector<int> msbIn;			    /**< MSB weights of the inputs (redundant with maxX) */
		vector<int> lsbIn;			    /**< LSB weights of the inputs */
	public: // readable by FIR etc
		int msbOut;							    /**< MSB weight of the output, may be computed out of the constants (depending on the constructor used) */
		int lsbOut;							    /**< LSB weight of the output */
	protected:
		vector<string> coeff;			  /**< the coefficients as strings */
		mpfr_t mpcoeff[10000];			/**< the coefficients as MPFR numbers -- 10000 should be enough for anybody */
		int g;                      /**< Number of guard bits; the internal format will have LSB at lsbOut-g  */
		double targetError;				/**< the target error, in absolute value */


	private:
		bool computeMSBOut;     /** <*/
		bool computeGuardBits;     /** <*/
		bool addFinalRoundBit;     /** <*/
		BitHeap* bitHeap;    			 /**< The heap of weighted bits that will be used to do the additions */
	};


}

#endif
