/*
  A model of Virtex Ultrascale+(16nm) FPGA (exact part: xcvu3p-2-FFVC1517)

  Author : Ledoux Louis

  Additional comment:
    - note the speed grade of -2
    - CLB topology : https://www.xilinx.com/support/documentation/user_guides/ug574-ultrascale-clb.pdf
    - Primitive : https://www.xilinx.com/support/documentation/sw_manuals/xilinx2018_1/ug974-vivado-ultrascale-libraries.pdf
    - Max Freq : - https://www.xilinx.com/support/documentation/data_sheets/ds923-virtex-ultrascale-plus.pdf
                 - 1.3GHz on DSP : https://www.xilinx.com/support/documentation/ip_documentation/ru/c-addsub.html

  This file is part of the FloPoCo project

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA-Lyon
  2008-2016.
  All rights reserved.
*/



#ifndef VirtexUltrascalePlus_HPP
#define VirtexUltrascalePlus_HPP
#include "../Target.hpp"
#include <iostream>
#include <sstream>
#include <vector>


namespace flopoco{

	/** Class for representing an VirtexUltrascalePlus target */
	class VirtexUltrascalePlus : public Target
	{
	public:
		/** The default constructor. */
		VirtexUltrascalePlus();
		/** The destructor */
		~VirtexUltrascalePlus();
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


		// The eight 6-LUTs of a slice could be combined into a 9-LUT without outter CLB routing
		int maxLutInputs() { return 9; }

	private:

		// The following is copypasted from Vivado timing reports

		// Primitive Delays
		const double lut5Delay_ = 0.035e-9;  //
		const double lut6Delay_ = 0.148e-9;  //
		const double ffDelay_ = 0.150e-9;    // report_property -all [lindex [get_speed_models -of [get_bels SLICE_X0Y0/AFF]] 0]
		const double lut2Delay_ = 0.050e-9;  //

		// Fast Carry chain related
		const double carry8Delay_ = 0.015e-9;  // CIN -> COUT
		const double initCarryDelay_ = 0.115e-9;  // CARRY_S[x] -> CARRY_C[7]. x is 2 for this copy paste
		const double finalCarryDelay_ = 0.116e-9;  // CIN -> CARRY_O[x]. x is 5 for this copy paste

		// Nets delay
		const double interCarryNetDelay_ = 0.026e-9;  // COUT -> CIN inter CLB
		const double lut_to_carry_net = 0.011e-9;  // LUT_O -> CARRY_S[x] x is 5 for this copy paste

		// Constants to help with formulas
		// constant delay w/o nets. LUT2 reported by vivado that performs a xor b for the propagate signal fed in the carry chain
		const double adderConstantDelay_ = finalCarryDelay_ + initCarryDelay_ + lut2Delay_;
		const double adderConstantDelayWithNets_ = adderConstantDelay_ + lut_to_carry_net;

		// TODO
		const double fanoutConstant_ = 1e-9/65 ; /**< Somewhere in Vivado report, someday, there has appeared a delay of 1.5e-9 for fo=65 */
		// const double typicalLocalRoutingDelay_ = 0.5e-9;
		const double typicalLocalRoutingDelay_ = 0.180e-9;
		const double DSPMultiplierDelay_ = 0; // TODO
		const double RAMDelay_ = 1e-9; // TODO
		const double RAMToLogicWireDelay_= 0; // TODO
	};

}
#endif
