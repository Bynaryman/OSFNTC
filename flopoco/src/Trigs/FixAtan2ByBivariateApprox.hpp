#ifndef FIXATAN2BYBIVARIATEAPPROX_HPP
#define FIXATAN2BYBIVARIATEAPPROX_HPP
#include <iomanip>
#include <vector>
#include <sstream>
#include <math.h>
#include <gmp.h>
#include <gmpxx.h>

#include "../utils.hpp"
#include "../Operator.hpp"
#include "../BitHeap/BitHeap.hpp"
#include "../IntMult/IntMultiplier.hpp"
#include "../ShiftersEtc/LZOC.hpp"
#include "../ShiftersEtc/Shifters.hpp"

#include "../Tools/Point.hpp"
#include "../Tools/Plane.hpp"
#include "FixAtan2.hpp"

#include "./Atan2Table.hpp"

#include "../FixFunctions/BipartiteTable.hpp"

#define PLANE_BASED				0
#define TAYLOR_ORDER1_BASED		1
#define TAYLOR_ORDER2_BASED		2
#define SINCOS_TABLE_BASED		3
#define POLYEVAL_BASED			4


namespace flopoco {

	class FixAtan2ByBivariateApprox : public FixAtan2 {

	public:
		/**
		 * The FixAtan2ByBivariateApprox generic constructor computes atan(y/x), faithful to outLSB.
		 * 		the inputs x and y are assumed to be x in [0, 1) an y in [0, 1)
		 * @param[in] target            target device
		 * @param[in] wIn               input width
		 * @param[in] wOut              output width
		 * @param[in] architectureType	type of architecture
		 * 									0 = based on the plane's equation
		 * 									1 = based on an order 1 Taylor approximating polynomial
		 * 									2 = based on an order 2 Taylor approximating polynomial
		 * 									3 = based on rotating using a table (for sine and cosine) and then using the Taylor series for (1/x)
		 **/
		FixAtan2ByBivariateApprox(Target* target, int wIn, int wOut, int architectureType = 0,   map<string, double> inputDelays = emptyDelayMap);


		/**
		 *  Destructor
		 */
		~FixAtan2ByBivariateApprox();

		/**
		 * Generates a component, and produces VHDL code for the instance inside an operator.
		 * The parent operator uses the std_logic_vector type.
		 */
		static FixAtan2ByBivariateApprox* newComponentAndInstance(Operator* op,
													int wIn,
													int wOut
												);

		/**
		 * Generates a component, and produces VHDL code for the instance inside an operator.
		 * The parent operator uses the signed/unsigned types.
		 */
		static FixAtan2ByBivariateApprox* newComponentAndInstanceNumericStd(Operator* op,
																int wIn,
																int wOut
															);


		void generateTaylorOrder2Parameters(int x, int y, mpfr_t &fa, mpfr_t &fb, mpfr_t &fc, mpfr_t &fd, mpfr_t &fe, mpfr_t &ff);

	private:

		/**
		 * Check the precision of the given type of architecture
		 * @param archType the possible types of architectures
		 * 			0 = the one based on the equation of the plane
		 * 			1 = the Taylor of order 1 approximating polynomial
		 * 			2 = the Taylor of order 2 approximating polynomial
		 */
		int checkArchitecture(int archType);

		int architectureType;							/**< the possible types of architectures
		  														0 = the one based on the equation of the plane
		  														1 = the Taylor of order 1 approximating polynomial
		  														2 = the Taylor of order 2 approximating polynomial */
		BitHeap* bitHeap;								/**< the bitheap used for the computations */
		int g;											/**< the number of guard bits */
		double maxValA, maxValB, maxValC;				/**< the maximum values of the parameters A, B and C, as determined by checkArchitecture()*/
		double maxValD, maxValE, maxValF;				/**< the maximum values of the parameters D, E and F, as determined by checkArchitecture()*/
		int kSize;


		double tempCriticalPath, tempCriticalPath2;		/**< temporarily store the critical path*/
	};

}
#endif
