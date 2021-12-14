#ifndef ATAN2TABLE_HPP
#define ATAN2TABLE_HPP

#include <iostream>
#include <math.h>
#include <cstdlib>

#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "../Table.hpp"
#include "../Tools/Point.hpp"
#include "../Tools/Plane.hpp"
#include "../utils.hpp"

namespace flopoco{

	class Atan2Table : public Table
	{
	public:
		/**
		 * Constructor for the Atan2Table class
		 * @param target		the target FPGA
		 * @param wIn			the width of the signal addressing the table
		 * @param wOut			the size of the output of the table
		 * @param archType		the constants stored in the table depend on the architecture in which
		 * 							the table will be used (see FixAtan2 for an explanation of
		 * 							the possible values of this parameter)
		 * @param sizeA			the size of the constant A, stored in the table (if not specified, will be determined internally)
		 * @param sizeB			the size of the constant B, (same as A)
		 * @param sizeC			the size of the constant C, (same as A, B)
		 */
		Atan2Table(Target* target, int wIn, int wOut, int archType,
						int sizeA = -1, int sizeB = -1, int sizeC = -1,
						int sizeD = -1, int sizeE = -1, int sizeF = -1,
						map<string, double> inputDelays = emptyDelayMap);

		~Atan2Table();

		mpz_class function(int input);

		/**
		 * Computed the values stored in the table, using the equation of the plane
		 * @param x				the upper bits of x, at a given resolution
		 * 						(k out of a total of n), used to compute the function
		 * @param y				the upper bits of y, at a given resolution
		 * 						(k out of a total of n), used to compute the function
		 * @param[out] fa		-(A/C), where the plane's equation is Ax+By+Cz+D=0
		 * @param[out] fb		-(B/C), where the plane's equation is Ax+By+Cz+D=0
		 * @param[out] fc		-(D/C), where the plane's equation is Ax+By+Cz+D=0
		 * 							named this way as to keep some sort of coherency with the other cases
		 */
		void generatePlaneParameters(int x, int y, mpfr_t &fa, mpfr_t &fb, mpfr_t &fc);

		/**
		 * Computed the values stored in the table, using an order 1 Taylor approximating polynomial
		 * @param x				the upper bits of x, at a given resolution
		 * 						(k out of a total of n), used to compute the function
		 * @param y				the upper bits of y, at a given resolution
		 * 						(k out of a total of n), used to compute the function
		 * @param[out] fa		A, where the polynomial is Ax+By+C=0
		 * @param[out] fb		B, where the polynomial is Ax+By+C=0
		 * @param[out] fc		C, where the polynomial is Ax+By+C=0
		 * 							named this way as to keep some sort of coherency with the other cases
		 */
		void generateTaylorOrder1Parameters(int x, int y, mpfr_t &fa, mpfr_t &fb, mpfr_t &fc);

		/**
		 * Computed the values stored in the table, using an order 2 Taylor approximating polynomial
		 * @param x				the upper bits of x, at a given resolution
		 * 						(k out of a total of n), used to compute the function
		 * @param y				the upper bits of y, at a given resolution
		 * 						(k out of a total of n), used to compute the function
		 * @param[out] fa		A, where the polynomial is Ax+By+C+Dx^2+Ey^2+Fxy=0
		 * @param[out] fb		B, where the polynomial is Ax+By+C+Dx^2+Ey^2+Fxy=0
		 * @param[out] fc		C, where the polynomial is Ax+By+C+Dx^2+Ey^2+Fxy=0
		 * @param[out] fd		D, where the polynomial is Ax+By+C+Dx^2+Ey^2+Fxy=0
		 * @param[out] fe		E, where the polynomial is Ax+By+C+Dx^2+Ey^2+Fxy=0
		 * @param[out] ff		F, where the polynomial is Ax+By+C+Dx^2+Ey^2+Fxy=0
		 * 							named this way as to keep some sort of coherency with the other cases
		 */
		void generateTaylorOrder2Parameters(int x, int y, mpfr_t &fa, mpfr_t &fb, mpfr_t &fc, mpfr_t &fd, mpfr_t &fe, mpfr_t &ff);

		int wIn;				/**< the width of the signal addressing the table */
		int wOut;				/**< the size of the output of the table */
		int archType;			/**< the constants stored in the table depend on the architecture
										in which the table will be used (see FixAtan2 for an explanation of
 										the possible values of this parameter) */

	private:
		int msbA;				/**< the msb of the constant A, stored in the table (if not specified, will be determined internally) */
		int msbB;				/**< the msb of the constant B, (same as A) */
		int msbC;				/**< the msb of the constant C, (same as A, B) */
		int msbD;				/**< the msb of the constant D, (same as A, B etc.) */
		int msbE;				/**< the msb of the constant E, (same as A, B etc.) */
		int msbF;				/**< the msb of the constant F, (same as A, B etc.) */
	};

}
#endif //Atan2Table_HPP

