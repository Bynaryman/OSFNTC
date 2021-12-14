#ifndef FixFFTFullyPA_HPP
#define FixFFTFullyPA_HPP
#include <vector>
#include <sstream>

#include "../Operator.hpp"
#include "FixComplexKCM.hpp" 
#include "FixComplexR2Butterfly.hpp" 
	//#include "../ComplexOperators/IntComplexAdder.hpp"

//#include "ComplexTwiddleMult.hpp"  //A sub module to multiply complex input by complex twidlle factor

	//#include "../ComplexOperators/IntTwiddleMultiplierAlternative.hpp"

	//#define TWIDDLEIM 			0
	//#define TWIDDLERE 			1

namespace flopoco{

	/**
	 * @brief Operator representing level k of a Decimation In Time Radix 2
	 * Fast Fourier Transform, having n inputs (complex numbers), that
	 * are represented as fixed point numbers, with wI and wF bits for
	 * the integer, respectively fractionary parts.
	 * NOTE: the size n of the FFT must be a power of 2
	 */
	class FixFFTFullyPA : public Operator
	{
	public:
		FixFFTFullyPA(
					OperatorPtr parentOp,
					Target* target, 
					int msbin, 
					int lsbin, 
					int msbout,
					int lsbout,
					int N,
					int radix = 2,
					bool signedIn = true,
					bool decimation = true,	// for DIT FFT scheme, else DIF
					bool revbitorder = true //make a reversed-bit order output
				);

		~FixFFTFullyPA();

		int guardbits = 0;

		int bitReverse(int x, int n);
		void emulate(TestCase * tc);		
		void emulate2(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);
		unsigned int bitrev(int indexvar, int n);
		mpz_class getTwiddleConstant(int constantType, int twiddleExponent);
		int computeGuardBits(int N);


		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		static void registerFactory();

	private:
		int msbin;
		int lsbin;
		int msbout;
		int lsbout;
		int N;
		int radix;
		bool signedIn;
		bool decimation;
		bool revbitorder;


	};
}
#endif
