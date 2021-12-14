#ifndef FixMultAddS_HPP
#define FixMultAddS_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "IntMultiplier.hpp"
#include "BitHeap.hpp"
#include "Plotter.hpp"


namespace flopoco {

	/*
	  Definition of the DSP use threshold r:
	  Consider a submultiplier block, by definition smaller than (or equal to) a DSP in both dimensions
	  So: r=1 means any multiplier that does not fills a DSP goes to logic
	  r=0       any multiplier, even very small ones, go to DSP

	  (submultiplier area)/(DSP area) is between 0 and 1
	  if (submultiplier area)/(DSP area) is larger than r then use a DSP for it 
	*/




	/** The FixMultAdd class computes A+X*Y
	    X*Y may be placed anywhere with respect to A;
	    the product will be truncated when relevant.
	    The result is specified as its LSB, MSB.

			Note on signed numbers:
			* The bit heap manages signed addition in any case, so wether the addend is signed or not is irrelevant
			* The product may be signed, or not. 
	*/

	class FixMultAdd : public Operator {

	public:
		/**
		 * The FixMultAdd generic constructor computes x*y+a, faithful to outLSB. 
		 * @param[in] target            target device
		 * @param[in] x                 Signal (should be of fixed-point type)
		 * @param[in] y                 Signal (should be of fixed-point type)
		 * @param[in] a                 Signal (should be of fixed-point type) 
		 * @param[in] outMSB            weight of the MSB of the product 
		 * @param[in] outLSB            weight of the LSB of the product
		 * @param[in] ratio             DSP block use ratio
		 * @param[in] enableSuperTiles  if true, supertiles will decrease resource consumption but increase latency
		 **/
		FixMultAdd(Target* target, Signal* x, Signal* y, Signal* a, int outMSB, int outLSB,
		           float ratio = 0.7, bool enableSuperTiles=true, map<string, double> inputDelays = emptyDelayMap);


		/* TODO *
		 * The virtual FixMultAdd constructor adds all the multiplier bits to some bitHeap, but no more.
		 * @param[in] parentOp      the Operator to which VHDL code will be added
		 * @param[in] bitHeap       the BitHeap to which bits will be added
		 * @param[in] x            a Signal from which the x input should be taken
		 * @param[in] y            a Signal from which the y input should be taken
		 * @param[in] wX             X multiplier size (including sign bit if any)
		 * @param[in] wY             Y multiplier size (including sign bit if any)
		 * @param[in] wOut         wOut size for a truncated multiplier (0 means full multiplier)
		 * @param[in] lsbWeight     the weight, within this BitHeap, corresponding to the LSB of the multiplier output. 
		 *                          Note that there should be enough bits below for guard bits in case of truncation.
		 The method neededGuardBits() provides this information.
		 * @param[in] negate     if true, the multiplier result is subtracted from the bit heap 
		 * @param[in] signedIO     false=unsigned, true=signed
		 * @param[in] ratio            DSP block use ratio
		 **/
		//			FixMultAdd (Operator* parentOp, BitHeap* bitHeap,  Signal* x, Signal* y, 
		//			int wX, int wY, int wOut, int lsbWeight, bool negate, bool signedIO, float ratio);


		/**
		 *  Destructor
		 */
		~FixMultAdd();

		/** Generates a component, and produces VHDL code for the instance inside an operator */
		static FixMultAdd* newComponentAndInstance(Operator* op,
																string instanceName,
																string xSignalName,
																string ySignalName,
																string aSignalName,
																string rSignalName,
																int rMSB, 
																int rLSB
																);


		void fillBitHeap();
		/**
		 * The emulate function.
		 * @param[in] tc               a test-case
		 */
		void emulate ( TestCase* tc );

		void buildStandardTestCases(TestCaseList* tcl);

		Signal* x;
		Signal* y;
		Signal* a;
		int wX;                      /**<  */
		int wY;                      /**<  */
		int wA;                      /**<  */
		int outMSB;                   /**<  */
		int outLSB;                   /**<  */
		int wOut;                   /**< size of the result */
		int msbP;                   /**< weight +1 of the MSB product */
		int lsbPfull;               /** equal to msbP - wX -wY */
		int lsbA;                  /**< weight of the LSB of A */
		bool signedIO;               /**< if true, inputs and outputs are signed. */
		double ratio;               /**< between 0 and 1, the area threshhold for using DSP blocks versus logic*/
		bool enableSuperTiles;     /**< if true, supertiles are built (fewer resources, longer latency */
		string xname;              /**< VHDL name */
		string yname;              /**< VHDL name */
		string aname;              /**< VHDL name */
		string rname;              /**< VHDL name */
		int g ;                    /**< the number of guard bits if the product is truncated */
		int maxWeight;             /**< The max weight for the bit heap of this multiplier, wOut + g*/
		int wOutP;                 /**< size of the product (not counting the guard bits) */
		double maxError;     /**< the max absolute value error of this multiplier, in ulps of the result. Should be 0 for untruncated, 1 or a bit less for truncated.*/  
		double initialCP;     /**< the initial delay, getMaxInputDelays ( inputDelays_ ).*/  
		int possibleOutputs;  /**< 1 if the operator is exact, 2 if it is faithfully rounded */

	private:
		Operator* parentOp;  /**< For a virtual multiplier, adding bits to some external BitHeap, 
		                        this is a pointer to the Operator that will provide the actual vhdl stream etc. */
		BitHeap* bitHeap;    /**< The heap of weighted bits that will be used to do the additions */
		IntMultiplier *mult; /**< the virtual multiplier */
		Plotter* plotter;

	};

}
#endif
