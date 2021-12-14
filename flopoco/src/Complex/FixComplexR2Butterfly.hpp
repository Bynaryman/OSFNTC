#ifndef FixComplexR2Butterfly_HPP
#define FixComplexR2Butterfly_HPP
#include <vector>
#include <sstream>

#include "../Operator.hpp"
#include "FixComplexKCM.hpp" 
#include "FixComplexAdder.hpp" 
	//#include "../ComplexOperators/IntComplexAdder.hpp"

//#include "ComplexTwiddleMult.hpp"  //A sub module to multiply complex input by complex twidlle factor

	//#include "../ComplexOperators/IntTwiddleMultiplierAlternative.hpp"

	//#define TWIDDLEIM 			0
	//#define TWIDDLERE 			1

namespace flopoco{

	/**
	 * @brief Operator representing level k of a decimation In Time Radix 2
	 * Fast Fourier Transform, having n inputs (complex numbers), that
	 * are represented as fixed point numbers, with wI and wF bits for
	 * the integer, respectively fractionary parts.
	 * NOTE: the size n of the FFT must be a power of 2
	 */
	class FixComplexR2Butterfly : public Operator
	{
	public:
		FixComplexR2Butterfly(
					OperatorPtr parentOp,
					Target* target, 
					int msbin, 
					int lsbin, 
					int msbout,
					int lsbout,
					string Twiddle_re,
					string Twiddle_im,
					bool signedIn = true,
					bool bypassmult = false,
					bool decimation = true,	// for DIT FFT scheme, else DIF
					bool extrabit = true,
					bool laststage = false
				);

		~FixComplexR2Butterfly();

		void emulate(TestCase * tc);


		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		static void registerFactory();

	private:
		int msbin;
		int lsbin;
		int msbout;
		int lsbout;

		string Twiddle_re;
		string Twiddle_im;
		bool signedIn;
		bool bypassmult;
		bool decimation;
		bool extrabit;
		bool laststage;

	};
}
#endif
