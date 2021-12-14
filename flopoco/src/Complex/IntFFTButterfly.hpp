#ifndef IntFFTButterfly_HPP
#define IntFFTButterfly_HPP
#include <vector>
#include <sstream>

#include "../Operator.hpp"
#include "../ComplexOperators/IntComplexAdder.hpp"
#include "../ComplexOperators/IntTwiddleMultiplier.hpp"
#include "../ComplexOperators/IntTwiddleMultiplierAlternative.hpp"

#define TWIDDLEIM 			0
#define TWIDDLERE 			1

namespace flopoco{

	/**
	 * @brief Operator representing level k of a Decimation In Time Radix 2
	 * Fast Fourier Transform, having n inputs (complex numbers), that
	 * are represented as fixed point numbers, with wI and wF bits for
	 * the integer, respectively fractionary parts.
	 * NOTE: the size n of the FFT must be a power of 2
	 */
	class IntFFTButterfly : public Operator
	{
	public:
		IntFFTButterfly(Target* target, int wI, int wF, int twiddleExponent, int n, bool signedOperator = true);
		~IntFFTButterfly();

		void emulate(TestCase * tc);

		//user defined class-specific functions
		/**
		 * @brief Compute the real or imaginary parts of the twiddle factor, or
		 * the sum, or the difference of the two. The values of the
		 * exponent and the size of the FFT are taken from the class
		 * variables.
		 * @param constantType decide what to compute (real/imaginary
		 * parts, or the sum/difference of the two)
		 * @return the real/imaginary part of the twiddle factor, or
		 * the sum/difference of the two, as a number
		 */
		mpz_class getTwiddleConstant(int constantType);

		//user-defined class specific variables
		int wI;						/**< Number of bits in the integer part of the fixed-point numbers in the input*/
		int wF;						/**< Number of bits in the fractional part of the fixed-point numbers in the input*/
		int w;						/**< Total number of bits of the fixed-point numbers in the input*/
		int twiddleExponent;		/**< The exponent in the twiddle factor: w^twiddleExp_N*/
		int n;						/**< The size of the FFT (as number of inputs to the FFT)*/
		bool signedOperator;		/**< The operator uses signed/unsigned numbers*/

	};
}
#endif
