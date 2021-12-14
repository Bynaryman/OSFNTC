#ifndef Zynq7000_HPP
#define Zynq7000_HPP
#include "../Target.hpp"
#include <iostream>
#include <sstream>
#include <vector>


namespace flopoco{

	/** Class for representing an Zynq7000 target */
	class Zynq7000 : public Target
	{
	public:
		/** The default constructor. */  
		Zynq7000();
		/** The destructor */
		~Zynq7000();
		/** overloading the virtual functions of Target
		 * @see the target class for more details 
		 */

		double logicDelay(int inputs);

		double adderDelay(int size, bool addRoutingDelay=true);

		double adder3Delay(int size){return 0;}; // currently irrelevant for Xilinx
		double eqComparatorDelay(int size);
		double eqConstComparatorDelay(int size);

		
		double DSPMultiplierDelay(){ return DSPMultiplierDelay_;}
		double DSPAdderDelay(){ return DSPAdderDelay_;}
		double DSPCascadingWireDelay(){ return DSPCascadingWireDelay_;}
		double DSPToLogicWireDelay(){ return DSPToLogicWireDelay_;}
		double LogicToDSPWireDelay(){ return DSPToLogicWireDelay_;}
		void   delayForDSP(MultiplierBlock* multBlock, double currentCp, int& cycleDelay, double& cpDelay);
		
		double RAMDelay() { return RAMDelay_; }
		double RAMToLogicWireDelay() { return RAMToLogicWireDelay_; }
		double LogicToRAMWireDelay() { return RAMToLogicWireDelay_; }
		
		double carryPropagateDelay();
		double addRoutingDelay(double d);
		double fanoutDelay(int fanout = 1);
		double lutDelay();
		double ffDelay();

		bool   suggestSubmultSize(int &x, int &y, int wInX, int wInY);
		bool   suggestSubaddSize(int &x, int wIn);
		bool   suggestSubadd3Size(int &x, int wIn){return 0;}; // currently irrelevant for Xilinx
		bool   suggestSlackSubaddSize(int &x, int wIn, double slack);
		bool   suggestSlackSubadd3Size(int &x, int wIn, double slack){return 0;}; // currently irrelevant for Xilinx
		bool   suggestSlackSubcomparatorSize(int &x, int wIn, double slack, bool constant);
		
		int    getIntMultiplierCost(int wInX, int wInY);
		long   sizeOfMemoryBlock();
		double tableDelay(int wIn, int wOut, bool logicTable); 
		DSP*   createDSP(); 
		int    getEquivalenceSliceDSP();
		int    getNumberOfDSPs();
		int    getIntNAdderCost(int wIn, int n);	
		int maxLutInputs() {return 8;}
		double lutConsumption(int lutInputSize);

		
	private:


		const double lutDelay_ = 0.124e-9;       /**< The delay of a LUT, without any routing (cut from vivado timing report)*/
		const double carry4Delay_ = 0.114e-9;    /**< The delay of the fast carry chain */
		const double ffDelay_ = 0.518e-9;       /**< The delay of a flip-flop, without any routing  (cut from vivado timing report)*/
		const double adderConstantDelay_  = 0.532e-9 + 0.222e-9; /**< includes a LUT delay and the initial and final carry4delays*/
		const double fanoutConstant_ = 2e-9/200 ; /**< Somewhere in Vivado report, someday, there has appeared a delay of 2e-9 for fo=200 */
		const double typicalLocalRoutingDelay_ = 0.5e-9;
		const double DSPMultiplierDelay_ = 0; // TODO
		const double RAMDelay_ = 0; // TODO
		const double RAMToLogicWireDelay_= 0; // TODO


		// From there on, obsolete stuff
		double lut2_;           /**< The LUT delay for 2 inputs */
		double lut3_;           /**< The LUT delay for 3 inputs */
		double lut4_;           /**< The LUT delay for 4 inputs */
		double fdCtoQ_;         /**< The delay of the FlipFlop. Also contains an approximate Net Delay experimentally determined */
		double muxcyStoO_;      /**< The delay of the carry propagation MUX, from Source to Out*/
		double muxcyCINtoO_;    /**< The delay of the carry propagation MUX, from CarryIn to Out*/
		double ffd_;            /**< The Flip-Flop D delay*/
		double muxf5_;          /**< The delay of the almighty mux F5*/
		double slice2sliceDelay_;       /**< This is approximate. It approximates the wire delays between Slices */
		double xorcyCintoO_;    /**< the S to O delay of the xor gate */
		int nrDSPs_;			/**< Number of available DSPs on this target */
		int dspFixedShift_;		/**< The amount by which the DSP block can shift an input to the ALU */
		
		double DSPAdderDelay_;
		double DSPCascadingWireDelay_;
		double DSPToLogicWireDelay_;

	};

}
#endif
