/*
  Conversion from  FloPoCo format to IEEE-like compact floating-point format

  This file is part of the FloPoCo project developed by the Arenaire
  team at Ecole Normale Superieure de Lyon

  Author : Fabrizio Ferrandi ferrandi@elet.polimi.it

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, 2009. All right reserved.

*/

#ifndef OUTPUTIEEE_HPP
#define OUTPUTIEEE_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "Operator.hpp"
#include "TestBenches/FPNumber.hpp"

namespace flopoco{

	/** The OutputIEEE class */
	class OutputIEEE : public Operator
	{
	public:
		/**
		 * @brief The OutputIEEE constructor
		 * @param[in]		target		the target device
		 * @param[in]		wEI			the the with of the exponent for the input floating point number encoded according FloPoCo format
		 * @param[in]		wFI			the the with of the fraction for the input floating point number encoded according FloPoCo format
		 * @param[in]		wEO			the the with of the exponent for the output floating point number encoded according IEEE format
		 * @param[in]		wFO			the the with of the fraction for the output floating point number encoded according IEEE format
		 * @param[in]		onlyPositiveZeroes      when true normalize +0 and -0 to +0
		 */
		OutputIEEE(OperatorPtr parentOp, Target* target, int wEI, int wFI, int wEO, int wFO, bool onlyPositiveZeroes=false);

		/**
		 * @brief OutputIEEE destructor
		 */
		~OutputIEEE();



		/**
		 * @brief Emulate the operator. This function overload the method from Operator.
		 * @param tc a TestCase partially filled with input values
		 */
		void emulate(TestCase * tc);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();


	private:
		/** The width of the exponent for the input X */
		int wEI;
		/** The width of the fraction for the input X */
		int wFI;
		/** The width of the exponent for the output R */
		int wEO;
		/** The width of the fraction for the output R */
		int wFO;
		/** when true normalize +0 and -0 to +0 */
		bool onlyPositiveZeroes;
		/** used only when wEI>wEO: minimal exponent representable in output format, biased with input bias */
		int underflowThreshold;
		/** used only when wEI>wEO: maximal exponent representable in output format, biased with input bias */
		int overflowThreshold;
	}
		;
}
#endif //OUTPUTIEEE_HPP
