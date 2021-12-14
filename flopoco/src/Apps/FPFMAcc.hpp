
#ifndef FPFMACC_HPP
#define FPFMACC_HPP
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

	/** The FPFMAcc class.  */
	class FPFMAcc : public Operator
	{
	public:

		/**
		 * @brief			The FPFMAcc constructor
		 * @param[in]		target   the target device
		 * @param[in]		wE       the width of the exponent for the inputs X and Y
		 * @param[in]		wF      the width of the fraction for the input X
		 **/
		FPFMAcc(Target* target, int wE, int wF, int adderLatency = -1);

		/**
		 * @brief			FPFMAcc destructor
		 */
		~FPFMAcc();

	protected:

		int wE;     /**< The width of the exponent for the inputs X and Y*/
		int wF;     /**< The width of the fraction for the input X */

	private:

//		FPTruncMult* fpTruncMultiplier; /**< instance of a FPMult */
//		FPAdd*     longAcc;      /**< instance of a FPAdd */

	};
}
#endif
