#ifndef __POSITNUMBER_HPP
#define __POSITNUMBER_HPP

#include <gmpxx.h>
#include <mpfr.h>

namespace flopoco{
	/**
	 * An abstraction of Posit numbers. Defines an
	 * abstraction on which arithmetic operations can easily be applied
	 * but at the same times can easily be converted to VHDL signals.
	 * Used for TestBench generation.
	 */
	class PositNumber 
	{
	public:

		/** Several possible special values */
		typedef enum {
			NaR,     /**< Capturates positive and negatives overflow and similar values*/
			Zero     /**< Zero*/
		} SpecialValue;

		/**
		 * Constructs a new PositNumber.
		 * @param width the total bit width of the type
		 * @param eS the exponent shit size
		 */
		PositNumber(int width, int eS);

		/**
		 * Constructs a new PositNumber.
		 * @param width the total bit width of the type
		 * @param eS the exponent shit size
		 * @param v a special value
		 */
		PositNumber(int width, int eS, SpecialValue v);

		/**
		 * Constructs a new PositNumber.
		 * @param width the total bit width of the type
		 * @param eS the exponent shit size
		 * @param m a mpfr_t value 
		 */
		PositNumber(int width, int eS, mpfr_t m);

		/**
		 * Constructs a new PositNumber.
		 * @param width the total bit width of the type
		 * @param eS the exponent shit size
		 * @param m a mpz_class corresponding to the bit representation of the
		 * posit to create
		 */
		PositNumber(int width, int eS, mpz_class signalValue);

		/**
		 * Converts the currently stored PositNumber to an mpfr_t
		 * @param[out] m a preinitialized mpfr_t where to store the floating point
		 */
		void getMPFR(mpfr_t m);

		/**
		 * Stores an mpfr_t as a PositNumber.
		 * @param m the mpfr_t to store.
		 */
		PositNumber &operator=(mpfr_t m);

		/**
		 * stores a double as a PositNumber
		 * @param d the value to convert
		 */
		PositNumber &operator=(double d);

		/**
		 * Assignes a signal value. Converts the signal value to the
		 * relevant PositNumber fields.
		 * @param s the signal value to assign.
		 */
		PositNumber &operator=(mpz_class s);

		/**
		 * Retrieved the VHDL signal representation of this floating point.
		 * @return a VHDL signal stored as mpz_class.
		 */
		mpz_class getSignalValue();

		/**
		 * Affectation operator
		 */
		PositNumber &operator=(PositNumber posit);

		/**
		 * Returns width and eS.
		 * @param[out] width signal width
		 * @param[out] eS the exponent shift field size 
		 */
		void getPrecision(int &width, int &eS);

	private:
		/** The width of the type*/
		int width_;

		/** The width of the exponent shift field size */
		int eS_;

		/** The value of the sign field */
		mpz_class sign_;

		/** The range running length */
		int64_t rangeRL_;
		
		/** The value of the exponent field */
		int32_t exponentShift_;

		/** The value of the mantissa field  */
		mpz_class mantissa_;

		mpz_class constructRange(int rangeRL);

		// Compute the two complement of the value val on width_ - 1 bits
		mpz_class twoComplement(mpz_class val);

	};

}

#endif

