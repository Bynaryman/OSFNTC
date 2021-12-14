#ifndef Virtex6_HPP
#define Virtex6_HPP
#include "../Target.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>


namespace flopoco{
	
	/** Class for representing an Virtex6 target */
	class Virtex6 : public Target
	{
	public:
		
	/** The default constructor. */
		Virtex6();
		/** The destructor */
		virtual ~Virtex6() {};
		
		/** overloading the virtual functions of Target
		 * @see the target class for more details
		 */
		double logicDelay(int inputs=0);
		double adderDelay(int size, bool addRoutingDelay=true);
		double adder3Delay(int size){return 0;}; // currently irrelevant for Xilinx
		double eqComparatorDelay(int size);
		double eqConstComparatorDelay(int size);
		double lutDelay();

		double DSPMultiplierDelay(){ return DSPMultiplierDelay_;}
		double DSPAdderDelay(){ return DSPAdderDelay_;}
		double DSPCascadingWireDelay(){ return DSPCascadingWireDelay_;}
		double DSPToLogicWireDelay(){ return DSPToLogicWireDelay_;}
		double LogicToDSPWireDelay(){ return DSPToLogicWireDelay_;}
		void   delayForDSP(MultiplierBlock* multBlock, double currentCp, int& cycleDelay, double& cpDelay);

		double RAMDelay() { return RAMDelay_; }
		double tableDelay(int wIn_, int wOut_, bool logicTable_);
		double RAMToLogicWireDelay() { return RAMToLogicWireDelay_; }
		double LogicToRAMWireDelay() { return RAMToLogicWireDelay_; }

		double carryPropagateDelay();
		double fanoutDelay(int fanout = 1);
		double ffDelay();
		bool   suggestSubmultSize(int &x, int &y, int wInX, int wInY);
		bool   suggestSubaddSize(int &x, int wIn);
		bool   suggestSubadd3Size(int &x, int wIn){return 0;}; // currently irrelevant for Xilinx
		bool   suggestSlackSubaddSize(int &x, int wIn, double slack);
		bool   suggestSlackSubadd3Size(int &x, int wIn, double slack){return 0;}; // currently irrelevant for Xilinx
		bool   suggestSlackSubcomparatorSize(int &x, int wIn, double slack, bool constant);

		int    getIntMultiplierCost(int wInX, int wInY);
		long   sizeOfMemoryBlock();
		DSP*   createDSP();
		int    getEquivalenceSliceDSP();
		int    getNumberOfDSPs();
		int    getIntNAdderCost(int wIn, int n);
		int maxLutInputs() {return 8;}
		double lutConsumption(int lutInputSize);

	private:
		double fastcarryDelay_; /**< The delay of the fast carry chain */
		double lutDelay_;       /**< The delay between two LUTs */
		double elemWireDelay_;  /**< The elementary wire dealy (for computing the distant wire delay) */

		double fdCtoQ_;         /**< The delay of the FlipFlop. Also contains an approximate Net Delay experimentally determined */
		double lut2_;           /**< The LUT delay for 2 inputs */
		double lut3_;           /**< The LUT delay for 3 inputs */
		double lut4_;           /**< The LUT delay for 4 inputs */
		double lut5_;           /**< The LUT delay for 5 inputs */
		double lut6_;           /**< The LUT delay for 6 inputs */
		double lut_net_;        /**< The LUT NET delay */
		double muxcyStoO_;      /**< The delay of the carry propagation MUX, from Source to Out*/
		double muxcyCINtoO_;    /**< The delay of the carry propagation MUX, from CarryIn to Out*/
		double ffd_;            /**< The Flip-Flop D delay*/
		double ff_net_;            /**< The Flip-Flop D net delay*/
		double muxf5_;          /**< The delay of the almighty mux F5*/
		double muxf7_;          /**< The delay of the even mightier mux F7*/
		double muxf7_net_;      /**< The NET delay of the even mightier mux F7*/
		double muxf8_;          /**< The delay of the mightiest mux F8*/
		double muxf8_net_;      /**< The NET delay of the mightiest mux F8*/
		double muxf_net_;       /**< The NET delay while inside the slice */
		double slice2sliceDelay_;       /**< This is approximate. It approximates the wire delays between Slices */
		double xorcyCintoO_;    /**< the S to O delay of the xor gate */
		double xorcyCintoO_net_;
		int nrDSPs_;			/**< Number of available DSPs on this target */
		int dspFixedShift_;		/**< The amount by which the DSP block can shift an input to the ALU */

		double DSPMultiplierDelay_;
		double DSPAdderDelay_;
		double DSPCascadingWireDelay_;
		double DSPToLogicWireDelay_;

		double RAMDelay_;
		double RAMToLogicWireDelay_;
		double logicWireToRAMDelay_;

	};

}
#endif
