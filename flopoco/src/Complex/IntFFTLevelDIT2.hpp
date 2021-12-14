#ifndef IntFFTLevelDIT2_HPP
#define IntFFTLevelDIT2_HPP
#include <vector>
#include <sstream>

#include "../utils.hpp"

#include "../Operator.hpp"
#include "./IntFFTButterfly.hpp"

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
	class IntFFTLevelDIT2 : public Operator
	{
	public:
		IntFFTLevelDIT2(Target* target, int wI, int wF, int k, int n, bool signedOperator = true);
		~IntFFTLevelDIT2();

		void emulate(TestCase * tc);

		//user defined class-specific functions
		/**
		 * @brief Compute the number formed as the bitreversal of x,
		 * represented on log2(n) bits
		 * @param n the size of the FFT (in number of points)
		 * @param x the number to be reversed
		 * @return the reversed number
		 */
		int bitReverse(int x, int n);

		/**
		 * @brief Compute the real or imaginary parts of the twiddle factor, or
		 * the sum, or the difference of the two. The values of the
		 * exponent and the size of the FFT are taken from the class
		 * variables.
		 * @param constantType decide what to compute (real/imaginary
		 * parts, or the sum/difference of the two)
		 * @param twiddleExponent the exponent of the twiddle factor
		 * @return the real/imaginary part of the twiddle factor, or
		 * the sum/difference of the two, as a number
		 */
		mpz_class getTwiddleConstant(int constantType, int twiddleExponent);

		//user-defined class specific variables
		int wI;						/**< Number of bits in the integer part of the fixed-point numbers in the input*/
		int wF;						/**< Number of bits in the fractional part of the fixed-point numbers in the input*/
		int w;						/**< Total number of bits of the fixed-point numbers in the input*/
		int k;						/**< The current level in the FFT, that is being implemented through this operator*/
		int n;						/**< The size of the FFT (as number of inputs to the FFT)*/
		bool signedOperator;		/**< The operator uses signed/unsigned numbers*/

		int nrGroups;				/**< The number of groups of inputs that use the same twiddle factor*/
		int distance;				/**< The distance (in terms of indexes) between two members of a butterfly operation*/

	};
}
#endif
