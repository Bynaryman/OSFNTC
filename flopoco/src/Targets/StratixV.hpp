#ifndef StratixV_HPP
#define  StratixV_HPP
#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "../Target.hpp"


namespace flopoco{

	/** Class for representing an StratixV target */
	class StratixV : public Target
	{
	public:

		/** The default constructor. */  
		StratixV();
	
		/** The destructor */
		virtual ~StratixV() {}

		/** overloading the virtual functions of Target
		 * @see the target class for more details 
		 */
		double logicDelay(int inputs);
		double adderDelay(int size, bool addRoutingDelay=true);
		double adder3Delay(int size);
		double eqComparatorDelay(int size);
		double eqConstComparatorDelay(int size);
		double lutDelay();

		double carryPropagateDelay();
		
		double DSPMultiplierDelay(){ return DSPMultiplierDelay_;}
		double DSPAdderDelay(){ return DSPAdderDelay_;} //TODO
		double DSPCascadingWireDelay(){ return DSPCascadingWireDelay_;}//TODO
		double DSPToLogicWireDelay (){ return DSPToLogicWireDelay_;}	
		double LogicToDSPWireDelay (){ return DSPToLogicWireDelay_;}
		void delayForDSP(MultiplierBlock* multBlock, double currentCp, int& cycleDelay, double& cpDelay);
		
		
		double RAMDelay() { return RAMDelay_; }
		double RAMToLogicWireDelay() { return RAMToLogicWireDelay_; }
		double LogicToRAMWireDelay() { return RAMToLogicWireDelay_; }

		int maxLutInputs() {return 6;}
		double lutConsumption(int lutInputSize);
		
		void   getAdderParameters(double &k1, double &k2, int size);
		double fanoutDelay(int fanout = 1);
		double ffDelay();
		double distantWireDelay(int n);
		bool   suggestSubmultSize(int &x, int &y, int wInX, int wInY);
		bool   suggestSubaddSize(int &x, int wIn);
		bool   suggestSubadd3Size(int &x, int wIn);
		bool   suggestSlackSubaddSize(int &x, int wIn, double slack);
		bool   suggestSlackSubadd3Size(int &x, int wIn, double slack);
		bool   suggestSlackSubcomparatorSize(int &x, int wIn, double slack, bool constant);
		
		int    getIntMultiplierCost(int wInX, int wInY);
		long   sizeOfMemoryBlock();
		DSP*   createDSP(); 
		int    getEquivalenceSliceDSP();
		int    getIntNAdderCost(int wIn, int n);
		int*   getDSPMultiplierWidths(){return multiplierWidth_;};
		int    getNrDSPMultiplier(){return nrConfigs_;};	
	
	private:

		double fastcarryDelay_; 		/**< The delay of the fast carry chain */
		double lut2lutDelay_;   		/**< The delay between two LUTs */
		double ffDelay_;   				/**< The delay between two flipflops (not including elemWireDelay_) */
		double elemWireDelay_;  		/**< The elementary wire dealy (for computing the distant wire delay) */
		double lutDelay_;      	 		/**< The LUT delay (in seconds)*/
	
		// Added by Sebi
		double lut2_;           		/**< The LUT delay for 2 inputs */
		double lut3_;           		/**< The LUT delay for 3 inputs */
		double lut4_;           		/**< The LUT delay for 4 inputs */
		double innerLABcarryDelay_;		/**< The wire delay between the upper and lower parts of a LAB --> R4 & C4 interconnects */	
		double interLABcarryDelay_;		/**< The approximate wire between two LABs --> R24 & C16 interconnects */	
		double shareOutToCarryOut_;		/**< The delay between the shared arithmetic out of one LAB and the carry out of the following LAB */	
		double muxStoO_;				/**< The delay of the MUX right after the 3-LUT of a LAB */	
		double fdCtoQ_;					/**< The delay of the FlipFlop. Also contains an approximate Net Delay experimentally determined */	
		double carryInToSumOut_;		/**< The delay between the carry in and the adder outup of one LAB */
		int    almsPerLab_;				/**< The number of ALMs contained by a LAB */
	
		// DSP parameters
		int		totalDSPs_;				/**< The total number of DSP blocks available on this target */	
		int		nrConfigs_;				/**< The number of distinct predefinded multiplier widths */
		int 	multiplierWidth_[5];	/**< The multiplier width available */
		double multiplierDelay_[5];		/**< The corresponding delay for each multiplier width available */
		double inputRegDelay_[5];		/**< The input register delay to DSP block for each multiplier width available */
		double pipe2OutReg2Add; 		/**< The DPS block pipeline register to output register delay in two-multipliers adder mode */
		double pipe2OutReg4Add; 		/**< The DPS block pipeline register to output register delay in four-multipliers adder mode */
	
		double DSPMultiplierDelay_;
		double DSPAdderDelay_;
		double DSPCascadingWireDelay_;
		double DSPToLogicWireDelay_;
		
		double RAMDelay_;
		double RAMToLogicWireDelay_;
	
	};
}
#endif
