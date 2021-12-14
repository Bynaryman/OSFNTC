#ifndef SHIFTERS_HPP
#define SHIFTERS_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <gmpxx.h>
#include "utils.hpp"

#include "Operator.hpp"


namespace flopoco{

	/** The Shifter class. Left and right shifters are perfectly
		 symmetrical, so both are instances of the Shifter class. Only the
		 name of the VHDL instance changes */
	class Shifter : public Operator
	{
	public:

		/** The types of shifting */
		typedef enum {
			Left, /**< Left Shifter */
			Right /**< Right Shifter */
		} ShiftDirection;
		
		/**
		 * The Shifter constructor
		 * @param[in]		target		the target device
		 * @param[in]		wIn			  the with of the input
		 * @param[in]		maxShift	the maximum shift amount
		 * @param[in]		direction	can be either Left of Right. Determines the shift direction
		 * @param[in]		wOut	optional size of the shifted output (-1 means: computed) 
		 * @param[in]		computeSticky optional computation of a sticky bit out of the possible discarded bits
		 **/
		Shifter(OperatorPtr parentOp, Target* target, int wIn, int maxShift, ShiftDirection dir, int wOut=-1, bool computeSticky=false, bool padWithOnes=false);


		/** Destructor */
		~Shifter();



		/**
		 * Emulate a correctly rounded division using MPFR.
		 * @param tc a TestCase partially filled with input values 
		 */
		void emulate(TestCase * tc);

		/** Returns the number of bits of the sift amount 
		 *@return number of bits of the shift amount
		 */
		int getShiftInWidth(){
			return wShiftIn;
		}


		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();

	protected:
		int wIn;          /**< the width of the input*/
		int maxShift;     /**< the maximum shift amount*/
		ShiftDirection direction;  /**< determines the shift direction. can be Left or Right */
		int wOut;         /**< the width of the output */
		int wShiftIn; 	 /**< the number of bits of the input which determines the shift amount*/

	private:
		bool computeSticky; /**< if true, computes the OR of all the shifted-out bits, and outputs it */
		bool inputPadBit; 		/**< if true, pad left with 0es, otherwise pad with ones  */
	};




}



#endif
