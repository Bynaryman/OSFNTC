#ifndef IntFFT_HPP
#define IntFFT_HPP
#include <vector>
#include <sstream>

#include "../utils.hpp"

#include "../Operator.hpp"
#include "./IntFFTLevelDIT2.hpp"


namespace flopoco{

	/**
	 * @brief Fast Fourier Transform. The radix can be customized (for now the
	 * only option is radix 2, but more to follow).
	 */
	class IntFFT : public Operator
	{
	public:
		IntFFT(Target* target, int wI, int wF, int n, bool signedOperator = true);
		~IntFFT();

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
		int n;						/**< The size of the FFT (as number of inputs to the FFT)*/
		bool signedOperator;		/**< The operator uses signed/unsigned numbers*/

		int stages;					/**< The number of stages in the FFT*/
	};
}
#endif
