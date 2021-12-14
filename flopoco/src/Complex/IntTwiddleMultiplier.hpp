#ifndef IntTwiddleMultiplier_HPP
#define IntTwiddleMultiplier_HPP
#include <vector>
#include <sstream>

#include "../Operator.hpp"
#include "../ConstMult/FixRealKCM.hpp"
#include "../ConstMult/IntConstMult.hpp"
#include "../IntAdder.hpp"

#define TWIDDLEIM 			0
#define TWIDDLERE 			1
#define TWIDDLERESUBIM 		2
#define TWIDDLENEGREADDIM 	3

namespace flopoco{

	/**
	 * @brief Multiplier by a constant twiddle factor
	 * Depending on the value of hasLessMultiplications, the multiplication
	 * (a+jb)*(c+jd) can be either
	 * 		Re(z)=a*c+b*d
	 * 		Im(z)=-a*d+b*c, with 4 multiplications by a constant and 3 additions
	 * or
	 * 		m1=(a+b)*c
	 * 		m2=(-d+c)*b		-> d+c can be precomputed
	 * 		m3=(-d-c)*a		-> d-c can be precomputed
	 * 		Re(z)=m1-m2
	 * 		Im(z)=m1+m3, with 3 multiplications by a constant and 3 additions
	 * where
	 * 		c and d are constants
	 * 		consequntly, multiplications by c and d are multiplications
	 * 		by constants
	 * 		d+c and d-c are constants also
	 */
	class IntTwiddleMultiplier : public Operator
	{
	public:
		IntTwiddleMultiplier(Target* target, int wI, int wF, int twiddleExponent, int n, bool signedOperator = true, bool reducedMultiplications = false, int multiplierMode = 0);
		~IntTwiddleMultiplier();

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

		/**
		 * @brief Compute the greatest common divisor of two numbers
		 * @param x the first number
		 * @param y the second number
		 * @return the gcd
		 */
		int getGCD(int x, int y);

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
