#ifndef FPJACOBI_HPP
#define FPJACOBI_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <cstdlib>

#include "../Operator.hpp"
#include "../FPAddSinglePath.hpp"
#include "../FPMult.hpp"

namespace flopoco{

	/** The FPJacobi class.  */
	class FPJacobi : public Operator
	{
	public:

		/**
		 * @brief			The FPJacobi constructor
		 * @param[in]		target   the target device
		 * @param[in]		wE       the width of the exponent for the inputs X and Y
		 * @param[in]		wF      the width of the fraction for the input X
		 **/
		FPJacobi(Target* target, int wE, int wF, int l1, int l2, int l3, int version);

		/**
		 * @brief			FPJacobi destructor
		 */
		~FPJacobi();

	protected:

		int wE;     /**< The width of the exponent for the inputs X and Y*/
		int wF;     /**< The width of the fraction for the input X */

	private:

//		FPTruncMult* fpTruncMultiplier; /**< instance of a FPMult */
//		FPAdd*     longAcc;      /**< instance of a FPAdd */

	};
}
#endif
