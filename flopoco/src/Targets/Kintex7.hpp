#ifndef Kintex7_HPP
#define Kintex7_HPP
#include "../Target.hpp"
#include <iostream>
#include <sstream>
#include <vector>


namespace flopoco{

	/** Class for representing an Kintex7 target */
	class Kintex7 : public Target
	{
	public:
		/** The default constructor. */  
		Kintex7();
		/** The destructor */
		~Kintex7();
		/** overloading the virtual functions of Target
		 * @see the target class for more details 
		 */

		// Overloading virtual methods of Target 
		double logicDelay(int inputs);

		double adderDelay(int size, bool addRoutingDelay=true);

		double eqComparatorDelay(int size);
		double eqConstComparatorDelay(int size);
		
		double lutDelay();
		double addRoutingDelay(double d);
		double fanoutDelay(int fanout = 1);
		double ffDelay();
		long   sizeOfMemoryBlock();
		double tableDelay(int wIn, int wOut, bool logicTable);

		// The following is a list of methods that are not totally useful: TODO in Target.hpp
		double adder3Delay(int size){return 0;}; // currently irrelevant for Xilinx
		double carryPropagateDelay(){return 0;};
		double DSPMultiplierDelay(){ 
			return 0;
			// TODO: return DSPMultiplierDelay_;
		}
		double DSPAdderDelay(){
			// TODO: return DSPAdderDelay_;
			return 0;
		}
		double DSPCascadingWireDelay(){
			// return DSPCascadingWireDelay_;
			return 0;
		}
		double DSPToLogicWireDelay(){
			// return DSPToLogicWireDelay_;
			return 0;
		}
		int    getEquivalenceSliceDSP();

		void   delayForDSP(MultiplierBlock* multBlock, double currentCp, int& cycleDelay, double& cpDelay);
		
		bool   suggestSlackSubaddSize(int &x, int wIn, double slack) {return false;}; // TODO
		bool   suggestSlackSubadd3Size(int &x, int wIn, double slack){return 0;}; // currently irrelevant for Xilinx
		bool   suggestSubaddSize(int &x, int wIn);
		bool   suggestSubadd3Size(int &x, int wIn){return 0;}; // currently irrelevant for Xilinx
		bool   suggestSlackSubcomparatorSize(int &x, int wIn, double slack, bool constant);// used in IntAddSubCmp/IntComparator.cpp:
		double LogicToDSPWireDelay(){ return 0;} //TODO

		double RAMDelay() { return RAMDelay_; }
		double LogicToRAMWireDelay() { return RAMToLogicWireDelay_; }
		double lutConsumption(int lutInputSize);

		/** 
		 * The four 6-LUTs of a slice can be combined without routing into a 
		 * 8-LUT
		 */
		int maxLutInputs() { return 8; }
#if 0
		
		double RAMToLogicWireDelay() { return RAMToLogicWireDelay_; }
		

		bool   suggestSubmultSize(int &x, int &y, int wInX, int wInY);
		
		int    getIntMultiplierCost(int wInX, int wInY);
		int    getNumberOfDSPs();
		int    getIntNAdderCost(int wIn, int n);	
#endif
		
	private:

		// The following is copypasted from Vivado timing reports
		const double lut5Delay_ = 0.043e-9;       /**< The delay of a LUT, without any routing (cut from vivado timing report)*/
		const double lut6Delay_ = 0.119e-9;       /**< The delay of a LUT, without any routing (cut from vivado timing report)*/
		const double ffDelay_ = 0.216e-9;       /**< The delay of a flip-flop, without any routing  (cut from vivado timing report)*/
		const double carry4Delay_ = 0.049e-9;    /**< The delay in the middle of the fast carry chain   */
		const double adderConstantDelay_  =  0.124e-9 + 0.260e-9 + 0.159e-9; /**< includes a LUT delay and the initial and final carry4delays*/
		const double fanoutConstant_ = 1e-9/65 ; /**< Somewhere in Vivado report, someday, there has appeared a delay of 1.5e-9 for fo=65 */
		const double typicalLocalRoutingDelay_ = 0.5e-9;
		const double DSPMultiplierDelay_ = 0; // TODO
		const double RAMDelay_ = 1e-9; // TODO
		const double RAMToLogicWireDelay_= 0; // TODO
	};

}
#endif
