#ifndef IntTwiddleMultiplierAlternative_HPP
#define IntTwiddleMultiplierAlternative_HPP
#include <vector>
#include <sstream>

#include "../Operator.hpp"
#include "../ConstMult/FixRealKCM.hpp"
#include "../ConstMult/IntConstMult.hpp"
#include "../IntAdder.hpp"

#define TWIDDLES 			0		/**< Constant representing the choice for sine(w) (denoted as s).*/
#define TWIDDLEC 			1		/**< Constant representing the choice for cosine(w) (denoted as c).*/
#define TWIDDLECP 			2		/**< Constant representing the choice for (1-cosine(w))/sine(w) (denoted as c', hence the name).*/

namespace flopoco{

	/**
	 * @brief Multiplier by a constant twiddle factor, using a method that is
	 * proven to better maintain the accuracy (the shift and scale
	 * operation becomes just a shift with the w angle, corresponding
	 * to the twiddle factor). The method replaces the cosine(w) factor
	 * in the computations with (cosine(w)-1)/sine(w).
	 */
	class IntTwiddleMultiplierAlternative : public Operator
	{
	public:
		IntTwiddleMultiplierAlternative(Target* target, int wI, int wF, int twiddleExponent, int n, bool signedOperator = true, int multiplierMode = 0);
		~IntTwiddleMultiplierAlternative();

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

		/**
		 * @brief Compute the real or imaginary parts of the twiddle factor, or
		 * the sum, or the difference of the two. The values of the
		 * exponent and the size of the FFT are taken from the class
		 * variables.
		 * @param constantType decide what to compute (real/imaginary
		 * parts, or the sum/difference of the two)
		 * @return the real/imaginary part of the twiddle factor, or
		 * the sum/difference of the two, as a string
		 */
		std::string getTwiddleConstantString(int constantType);

		//user-defined class specific variables
		int wI;						/**< Number of bits in the integer part of the fixed-point numbers in the input*/
		int wF;						/**< Number of bits in the fractional part of the fixed-point numbers in the input*/
		int w;						/**< Total number of bits of the fixed-point numbers in the input*/
		int twiddleExponent;		/**< The exponent in the twiddle factor: w^twiddleExp_N*/
		int n;						/**< The size of the FFT (as number of inputs to the FFT)*/
		bool signedOperator;		/**< The FFT inputs are signed/ unsigned numbers*/
		int multiplierMode;			/**< The type of multiplier to be used inside the twiddle factor: 0=KCM, 1=Shift-And-Add*/

	};
}
#endif
