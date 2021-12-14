/*
   An arctangent(y/x) implementation

	Author:  Florent de Dinechin, Matei Istoan

	This file is part of the FloPoCo project

	Initial software.
	Copyright © INSA Lyon, INRIA, CNRS, UCBL,
	2014.
	All rights reserved.
*/


#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "../utils.hpp"
#include "Operator.hpp"
#include "FixAtan2ByBivariateApprox.hpp"

using namespace std;

namespace flopoco {


	// The constructor for a stand-alone operator
	FixAtan2ByBivariateApprox::FixAtan2ByBivariateApprox(Target* target_, int wIn_, int wOut_, int architectureType_, map<string, double> inputDelays_):
		FixAtan2(target_, wIn_, wOut_, inputDelays_), architectureType(architectureType_)
	{
		srcFileName = "FixAtan2ByBivariateApprox";
		setCopyrightString("Florent de Dinechin, Matei Istoan, 2014");
		// build the name
		ostringstream name;
		name <<"FixAtan2ByBivariateApprox_" << vhdlize(wIn) << "_" << vhdlize(wOut) << "_archType_" << vhdlize(architectureType);
		setNameWithFreq( name.str() );

		//double ratio = getTarget()->unusedHardMultThreshold(); // TODO this should be automatic/default: check

		//initialize global variables
		maxValA = -1;		//this is the absolute value, so should be always non-negative, once used
		maxValB = -1;
		maxValC = -1;
		maxValD = -1;
		maxValE = -1;
		maxValF = -1;

		useNumericStd_Unsigned();

		///////////// VHDL GENERATION

		// Range reduction code is shared, defined in FixAtan2
		buildQuadrantRangeReduction();
		buildScalingRangeReduction();


		//build the architecture
		if((architectureType == 0) || (architectureType == 1))
		{
			//based on the plane's equation or using a first order polynomial
			//	(same architecture, but the parameters are generated differently)

			//check if the selected type of architecture can achieve the required accuracy
			//	also, determine the size of the parts of the input
			//	signals to use as an address for the table
			int k;
			int guardBitsSum, guardBitsApprox;

			guardBitsSum = 2;				// the error when computing Ax+By+C is at most 2.5 ulps, so we'll need 2 extra guard bits
			guardBitsApprox = 2;			// determined when computing the parameters for the tables
			g = guardBitsApprox + guardBitsSum;

			k = checkArchitecture(architectureType);
			kSize = k;

			//determine the size of the constants to store in the table
			// maxValA, maxValB and maxValC should have been set by the call to checkArchitecture()
			int msbA = intlog2(maxValA)+1;
			int msbB = intlog2(maxValB)+1;
			int msbC = intlog2(maxValC)+1;
			int maxMSB = maxInt(3, msbA, msbB, msbC);

			if(getTarget()->plainVHDL())
			{
				//split the input signals, and create the address signal for the table

				//manage the pipeline
				manageCriticalPath(getTarget()->localWireDelay());

				//save the critical path
				tempCriticalPath2 = getCriticalPath();

				vhdl << tab << declare("XHigh", k-1) << " <= std_logic_vector(XRS" << range(wIn-3, wIn-1-k) << ");" << endl;
				vhdl << tab << declare("YHigh", k)   << " <= std_logic_vector(YRS" << range(wIn-2, wIn-1-k) << ");" << endl;
				vhdl << tab << declare("atan2TableInput", 2*k-1) << " <= std_logic_vector(XHigh) & std_logic_vector(YHigh);" << endl;
				/*
				vhdl << tab << declare("XHigh", k-1) << " <= std_logic_vector(X" << range(wIn-3, wIn-1-k) << ");" << endl;
				vhdl << tab << declare("YHigh", k)   << " <= std_logic_vector(Y" << range(wIn-2, wIn-1-k) << ");" << endl;
				vhdl << tab << declare("atan2TableInput", 2*k-1) << " <= std_logic_vector(XHigh) & std_logic_vector(YHigh);" << endl;
				*/

				//create the table for atan(y/x)
				Atan2Table *table = new Atan2Table(getTarget(), 2*k-1, msbA+msbB+msbC+3*(wOut-1+g),
													architectureType,
													msbA, msbB, msbC,
													0, 0, 0, 			//place-holders for msbD, msbE, msbF
													inDelayMap("atan2TableInput", getCriticalPath()) );

				//add the table to the operator
				addSubComponent(table);
				//useSoftRAM(table);
				useHardRAM(table);
				inPortMap (table , "X", "atan2TableInput");
				outPortMap(table , "Y", "atan2TableOutput");
				vhdl << instance(table , "Atan2Table");

				//manage the pipeline
				syncCycleFromSignal("atan2TableOutput");

				//split the output of the table to the corresponding parts A, B and C
				vhdl << tab << declareFixPoint("coeffC", true, msbC-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbC+(wOut-1+g)-1, 0) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffB", true, msbB-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbB+msbC+2*(wOut-1+g)-1, msbC+(wOut-1+g)) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffA", true, msbA-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbA+msbB+msbC+3*(wOut-1+g)-1, msbB+msbC+2*(wOut-1+g)) << ");" << endl;

				resizeFixPoint("C_sgnExtended", "coeffC", maxMSB+1, -wOut+1-g);

				//manage the pipeline
				//	save the critical path
				tempCriticalPath = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("XRS");
				syncCycleFromSignal("YRS");
				setCriticalPath(tempCriticalPath2);
				manageCriticalPath(getTarget()->localWireDelay());

				vhdl << tab << declareFixPoint("XLow", true, -k, -wIn+1) << " <= signed('0' & XRS" << range(wIn-1-k-1, 0) << ");" << endl;
				vhdl << tab << declareFixPoint("YLow", true, -k, -wIn+1) << " <= signed('0' & YRS" << range(wIn-1-k-1, 0) << ");" << endl;
				/*
				vhdl << tab << declareFixPoint("XLow", true, -k, -wIn+1) << " <= signed('0' & X" << range(wIn-1-k-1, 0) << ");" << endl;
				vhdl << tab << declareFixPoint("YLow", true, -k, -wIn+1) << " <= signed('0' & Y" << range(wIn-1-k-1, 0) << ");" << endl;
				*/

				//manage the pipeline
				setCycleFromSignal("coeffA");
				syncCycleFromSignal("coeffB");
				setCriticalPath(tempCriticalPath);
				manageCriticalPath(getTarget()->DSPMultiplierDelay());

				//create A*X_low and B*Y_low
				vhdl << tab << declareFixPoint("AXLow", true, msbA-k, -wOut+1-g-wIn+1) << " <= coeffA * XLow;" << endl;
				vhdl << tab << declareFixPoint("BYLow", true, msbB-k, -wOut+1-g-wIn+1) << " <= coeffB * YLow;" << endl;
				//align A*X_low and B*Y_low to the output format
				resizeFixPoint("AXLow_sgnExtended", "AXLow", maxMSB-1, -wOut+1-g);
				resizeFixPoint("BYLow_sgnExtended", "BYLow", maxMSB-1, -wOut+1-g);

				//manage the pipeline
				manageCriticalPath(getTarget()->adderDelay(maxMSB+wOut+g+1));

				//add everything up
				vhdl << tab << declareFixPoint("AXLowAddBYLow", true, maxMSB, -wOut+1-g)
						<< " <= (AXLow_sgnExtended(AXLow_sgnExtended'HIGH) & AXLow_sgnExtended) + (BYLow_sgnExtended(BYLow_sgnExtended'HIGH) & BYLow_sgnExtended);" << endl;

				//manage the pipeline
				manageCriticalPath(getTarget()->adderDelay(maxMSB+wOut+g+2));

				vhdl << tab << declareFixPoint("AXLowAddBYLowAddC", true, maxMSB+1, -wOut+1-g) << " <= (AXLowAddBYLow(AXLowAddBYLow'HIGH) & AXLowAddBYLow) + C_sgnExtended;" << endl;

				//manage the pipeline
				manageCriticalPath(getTarget()->adderDelay(wOut+1));

				//extract the final result
				resizeFixPoint("Rtmp", "AXLowAddBYLowAddC", 1, -wOut);
				vhdl << tab << declareFixPoint("Rtmp_rndCst", true, 1, -wOut) << " <= signed(std_logic_vector\'(\"" << zg(wOut+1, -2) << "1\"));" << endl;
				vhdl << tab << declareFixPoint("Rtmp_rnd", true, 1, -wOut) << " <= Rtmp + Rtmp_rndCst;" << endl;

				//manage the pipeline
				manageCriticalPath(getTarget()->localWireDelay());

				//return the result
				/*
				resizeFixPoint("Rtmp_stdlv", "Rtmp_rnd", 0, -wOut+1);
				vhdl << tab << declare("R_int", wOut) << " <= std_logic_vector(Rtmp_stdlv);" << endl;
				*/
				resizeFixPoint("Rtmp_stdlv", "Rtmp_rnd", -2, -wOut+1);
				vhdl << tab << declare("R_int", wOut-2) << " <= std_logic_vector(Rtmp_stdlv);" << endl;

				//vhdl << tab << "R <= std_logic_vector(Rtmp_stdlv);" << endl;

			}else
			{
				//create the bitheap
				bitHeap = new BitHeap(this, (maxMSB+2)+wOut+g);

				//manage the pipeline
				setCycleFromSignal("XRS");
				syncCycleFromSignal("YRS");
				manageCriticalPath(getTarget()->localWireDelay());

				//create the input signals for the table
				vhdl << tab << declare("XHigh", k-1) << " <= std_logic_vector(XRS" << range(wIn-3, wIn-1-k) << ");" << endl;
				vhdl << tab << declare("YHigh", k)   << " <= std_logic_vector(YRS" << range(wIn-2, wIn-1-k) << ");" << endl;
				/*
				vhdl << tab << declare("XHigh", k-1) << " <= std_logic_vector(X" << range(wIn-3, wIn-1-k) << ");" << endl;
				vhdl << tab << declare("YHigh", k)   << " <= std_logic_vector(Y" << range(wIn-2, wIn-1-k) << ");" << endl;
				*/

				vhdl << tab << declare("atan2TableInput", 2*k-1) << " <= std_logic_vector(XHigh) & std_logic_vector(YHigh);" << endl;

				//create the table for atan(y/x)
				Atan2Table *table = new Atan2Table(getTarget(), 2*k-1, msbA+msbB+msbC+3*(wOut-1+g),
													architectureType,
													msbA, msbB, msbC,
													0, 0, 0, 			//place-holders for msbD, msbE, msbF
													inDelayMap("atan2TableInput", getCriticalPath()) );

				//add the table to the operator
				addSubComponent(table);
				//useSoftRAM(table);
				useHardRAM(table);
				inPortMap (table , "X", "atan2TableInput");
				outPortMap(table , "Y", "atan2TableOutput");
				vhdl << instance(table , "Atan2Table");

				//manage the pipeline
				syncCycleFromSignal("atan2TableOutput");
				manageCriticalPath(getTarget()->localWireDelay());

				//extract signals A, B and C
				vhdl << tab << declare("coeffC", msbC+wOut-1+g) << " <= atan2TableOutput"
						<< range(msbC+(wOut-1+g)-1, 0) << ";" << endl;
				vhdl << tab << declare("coeffB", msbB+wOut-1+g) << " <= atan2TableOutput"
						<< range(msbB+msbC+2*(wOut-1+g)-1, msbC+(wOut-1+g)) << ";" << endl;
				vhdl << tab << declare("coeffA", msbA+wOut-1+g) << " <= atan2TableOutput"
						<< range(msbA+msbB+msbC+3*(wOut-1+g)-1, msbB+msbC+2*(wOut-1+g)) << ";" << endl;

				//create Ax and By
				vhdl << tab << declare("XLow", wIn-k) << " <= '0' & XRS" << range(wIn-1-k-1, 0) << ";" << endl;
				vhdl << tab << declare("YLow", wIn-k) << " <= '0' & YRS" << range(wIn-1-k-1, 0) << ";" << endl;
				/*
				vhdl << tab << declare("XLow", wIn-k) << " <= '0' & X" << range(wIn-1-k-1, 0) << ";" << endl;
				vhdl << tab << declare("YLow", wIn-k) << " <= '0' & Y" << range(wIn-1-k-1, 0) << ";" << endl;
				*/

				/*IntMultiplier* multAx;
				multAx = new IntMultiplier(this,								//parent operator
											 bitHeap,							//the bit heap that performs the compression
											 getSignalByName("XLow"),			//first input to the multiplier (a signal)
											 getSignalByName("coeffA"),				//second input to the multiplier (a signal)
											 -wIn+1,							//offset of the LSB of the multiplier in the bit heap
											 false, //negate,					//whether to subtract the result of the multiplication from the bit heap
											 true,								//signed/unsigned operator
											 ratio);							//DSP ratio
											 */

				/*IntMultiplier* multBy;
				multBy = new IntMultiplier(this,								//parent operator
											 bitHeap,							//the bit heap that performs the compression
											 getSignalByName("YLow"),			//first input to the multiplier (a signal)
											 getSignalByName("coeffB"),				//second input to the multiplier (a signal)
											 -wIn+1,							//offset of the LSB of the multiplier in the bit heap
											 false, //negate,					//whether to subtract the result of the multiplication from the bit heap
											 true,								//signed/unsigned operator
											 ratio);	*/						//DSP ratio

				bitHeap->addSignedBitVector(0,									//weight of signal in the bit heap
											"coeffC",								//name of the signal
											msbC+wOut-1+g,						//size of the signal added
											0,									//index of the lsb in the bit vector from which to add the bits of the addend
											false);								//if we are correcting the index in the bit vector with a negative weight

				//add the rounding bit - take into consideration the final alignment
				bitHeap->addConstantOneBit(g-1);

				//compress the bit heap
				bitHeap -> generateCompressorVHDL();

				//extract the result - take into consideration the final alignment
				vhdl << tab << declare("R_int", wIn-2) << " <= " << bitHeap->getSumName() << range(wOut+g-1-2, g) << ";" << endl;
				//vhdl << tab << "R <= " << bitHeap->getSumName() << range(wOut+g-1, g) << ";" << endl;
			}

		}else if(architectureType == 2)
		{
			//based on an order 2 Taylor approximating polynomial

			//check if the selected type of architecture can achieve the required accuracy
			//	also, determine the size of the parts of the input
			//	signals to use as an address for the table
			int k;
			int guardBitsSum, guardBitsApprox;

			guardBitsSum = 2;				// the error when computing Ax+By+C+Dx^2+Ey^2+Fxy is at most 2.5 ulps, so we'll need 2 extra guard bits
			guardBitsApprox = 2;			// determined when computing the parameters for the tables
			g = guardBitsApprox + guardBitsSum;

			k = checkArchitecture(architectureType);
			kSize = k;

			//determine the size of the constants to store in the table
			// maxValA, maxValB and maxValC should have been set by the call to checkArchitecture()
			int msbA = intlog2(maxValA)+1;
			int msbB = intlog2(maxValB)+1;
			int msbC = intlog2(maxValC)+1;
			int msbD = intlog2(maxValD)+1;
			int msbE = intlog2(maxValE)+1;
			int msbF = intlog2(maxValF)+1;
			int maxMSB = maxInt(6, msbA, msbB, msbC, msbD, msbE, msbF);

			if(getTarget()->plainVHDL())
			{
				//manage the pipeline
				manageCriticalPath(getTarget()->localWireDelay());
				//save the critical path
				tempCriticalPath = getCriticalPath();

				//split the input signals, and create the address signal for the table
				/*
				vhdl << tab << declare("XHigh", k-1) << " <= std_logic_vector(X" << range(wIn-3, wIn-1-k) << ");" << endl;
				vhdl << tab << declare("YHigh", k)   << " <= std_logic_vector(Y" << range(wIn-2, wIn-1-k) << ");" << endl;
				*/
				vhdl << tab << declare("XHigh", k-1) << " <= std_logic_vector(XRS" << range(wIn-3, wIn-1-k) << ");" << endl;
				vhdl << tab << declare("YHigh", k)   << " <= std_logic_vector(YRS" << range(wIn-2, wIn-1-k) << ");" << endl;

				vhdl << endl;
				vhdl << tab << declare("atan2TableInput", 2*k-1) << " <= std_logic_vector(XHigh) & std_logic_vector(YHigh);" << endl;
				vhdl << endl;

				//create the table for atan(y/x)
				Atan2Table *table = new Atan2Table(getTarget(), 2*k-1, msbA+msbB+msbC+msbD+msbE+msbF+6*(wOut-1+g),
													architectureType,
													msbA, msbB, msbC, msbD, msbE, msbF,
													inDelayMap("atan2TableInput", getCriticalPath()) );

				//add the table to the operator
				addSubComponent(table);
				//useSoftRAM(table);
				useHardRAM(table);
				inPortMap (table , "X", "atan2TableInput");
				outPortMap(table , "Y", "atan2TableOutput");
				vhdl << instance(table , "Atan2Table");
				vhdl << endl;

				//manage the pipeline
				syncCycleFromSignal("atan2TableOutput");
				setCriticalPath(table->getOutputDelay("Y"));
				manageCriticalPath(getTarget()->localWireDelay());

				//split the output of the table to the corresponding parts A, B, C, D, E and F
				vhdl << tab << declareFixPoint("coeffF", true, msbF-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbF+1*(wOut-1+g)-1, 0) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffE", true, msbE-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbE+msbF+2*(wOut-1+g)-1, msbF+1*(wOut-1+g)) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffD", true, msbD-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbD+msbE+msbF+3*(wOut-1+g)-1, msbE+msbF+2*(wOut-1+g)) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffC", true, msbC-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbC+msbD+msbE+msbF+4*(wOut-1+g)-1, msbD+msbE+msbF+3*(wOut-1+g)) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffB", true, msbB-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbB+msbC+msbD+msbE+msbF+5*(wOut-1+g)-1, msbC+msbD+msbE+msbF+4*(wOut-1+g)) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffA", true, msbA-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbA+msbB+msbC+msbD+msbE+msbF+6*(wOut-1+g)-1, msbB+msbC+msbD+msbE+msbF+5*(wOut-1+g)) << ");" << endl;
				vhdl << endl;

				//align the signals to the output format to the output format
				resizeFixPoint("C_sgnExt", "coeffC", maxMSB-1, -wOut+1-g);

				//save the critical path
				tempCriticalPath2 = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("XRS");
				syncCycleFromSignal("YRS");
				setCriticalPath(tempCriticalPath);
				manageCriticalPath(getTarget()->lutDelay() + getTarget()->localWireDelay());

				vhdl << tab << declareFixPoint("DeltaX", true, -k-1,  -wIn+1) << " <= signed(not(XRS(" << of(wIn-k-2) << ")) & XRS" << range(wIn-k-3, 0) << ");" << endl;
				vhdl << tab << declareFixPoint("DeltaY", true, -k-1,  -wIn+1) << " <= signed(not(YRS(" << of(wIn-k-2) << ")) & YRS" << range(wIn-k-3, 0) << ");" << endl;
				vhdl << tab << declare("DeltaX_stdlv", wIn-k-1) << " <= std_logic_vector(DeltaX);" << endl;
				vhdl << tab << declare("DeltaY_stdlv", wIn-k-1) << " <= std_logic_vector(DeltaY);" << endl;
				/*
				vhdl << tab << declareFixPoint("DeltaX", true, -k-1,  -wIn+1) << " <= signed(not(X(" << of(wIn-k-2) << ")) & X" << range(wIn-k-3, 0) << ");" << endl;
				vhdl << tab << declareFixPoint("DeltaY", true, -k-1,  -wIn+1) << " <= signed(not(Y(" << of(wIn-k-2) << ")) & Y" << range(wIn-k-3, 0) << ");" << endl;
				*/
				vhdl << endl;

				//save the critical path
				tempCriticalPath = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("coeffA");
				syncCycleFromSignal("DeltaX");
				setCriticalPath(tempCriticalPath2);
				manageCriticalPath(getTarget()->DSPMultiplierDelay());

				//create A*DeltaX and B*DeltaY
				vhdl << tab << declareFixPoint("A_DeltaX", true, msbA-k-1,  -wOut+1-g-wIn+1) << " <= coeffA * DeltaX;" << endl;
				vhdl << tab << declareFixPoint("B_DeltaY", true, msbB-k-1,  -wOut+1-g-wIn+1) << " <= coeffB * DeltaY;" << endl;
				vhdl << endl;

				//align the signals to the output format to the output format
				resizeFixPoint("A_DeltaX_sgnExt", "A_DeltaX", maxMSB-1, -wOut+1-g);
				resizeFixPoint("B_DeltaY_sgnExt", "B_DeltaY", maxMSB-1, -wOut+1-g);

				//save the critical path
				double criticalPathA_DeltaX = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("DeltaX");
				syncCycleFromSignal("DeltaY");
				setCriticalPath(tempCriticalPath);
				manageCriticalPath(getTarget()->DSPMultiplierDelay());

				//create DeltaX^2, DeltaY^2 and DeltaX*DeltaY
				/*
				vhdl << tab << declareFixPoint("DeltaX2", true, -2*k-1,  -2*wIn+2) << " <= DeltaX * DeltaX;" << endl;
				vhdl << tab << declareFixPoint("DeltaY2", true, -2*k-1,  -2*wIn+2) << " <= DeltaY * DeltaY;" << endl;
				*/
				vhdl << tab << declareFixPoint("DeltaX_DeltaY", true, -2*k-1,  -2*wIn+2) << " <= DeltaX * DeltaY;" << endl;

				//save the critical path
				double criticalPathDeltaX_DeltaY = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("DeltaX");
				setCriticalPath(tempCriticalPath);
				manageCriticalPath(getTarget()->DSPMultiplierDelay());

				BipartiteTable *deltaX2Table = new BipartiteTable(getTarget(),
																	join("(2^(-", k, "))*(x^2)"),
																	//-wIn+k+1, -1, -2*wIn+2*k+2,
																	-wIn+k+1, -1, -wOut+1-g+2*k);
																	//inDelayMap("DeltaX", getCriticalPath()) );
				//add the table to the operator
				addSubComponent(deltaX2Table);
				//useSoftRAM(deltaX2Table);
				useHardRAM(deltaX2Table);
				inPortMap (deltaX2Table , "X", "DeltaX_stdlv");
				outPortMap(deltaX2Table , "Y", "DeltaX2_stdlv");
				vhdl << instance(deltaX2Table , "DeltaX2Table");
				vhdl << endl;

				//manage the pipeline
				setCycleFromSignal("DeltaY");
				setCriticalPath(tempCriticalPath);
				manageCriticalPath(getTarget()->DSPMultiplierDelay());

				BipartiteTable *deltaY2Table = new BipartiteTable(getTarget(),
																	join("(2^(-", k, "))*(x^2)"),
																	//-wIn+k+1, -1, -2*wIn+2*k+2,
																	-wIn+k+1, -1, -wOut+1-g+2*k);
																	//inDelayMap("DeltaY", getCriticalPath()) );
				//add the table to the operator
				addSubComponent(deltaY2Table);
				//useSoftRAM(deltaY2Table);
				useHardRAM(deltaY2Table);
				inPortMap (deltaY2Table , "X", "DeltaY_stdlv");
				outPortMap(deltaY2Table , "Y", "DeltaY2_stdlv");
				vhdl << instance(deltaY2Table , "DeltaY2Table");
				vhdl << endl;

				//manage the pipeline
				manageCriticalPath(getTarget()->localWireDelay());

				//convert to fixed point in VHDL
				//vhdl << tab << declareFixPoint("DeltaX2", true, -2*k-1,  -2*wIn+2) << " <= signed(DeltaX2_stdlv);" << endl;
				//vhdl << tab << declareFixPoint("DeltaY2", true, -2*k-1,  -2*wIn+2) << " <= signed(DeltaY2_stdlv);" << endl;
				vhdl << tab << declareFixPoint("DeltaX2", true, -2*k-1,  -wOut+1-g) << " <= signed(DeltaX2_stdlv);" << endl;
				vhdl << tab << declareFixPoint("DeltaY2", true, -2*k-1,  -wOut+1-g) << " <= signed(DeltaY2_stdlv);" << endl;

				//manage the pipeline
				manageCriticalPath(getTarget()->localWireDelay());

				//align the products, discard the extra lsb-s
				resizeFixPoint("DeltaX2_short", "DeltaX2", -2*k-1, -wOut+1-g);
				resizeFixPoint("DeltaY2_short", "DeltaY2", -2*k-1, -wOut+1-g);
				resizeFixPoint("DeltaX_DeltaY_short", "DeltaX_DeltaY", -2*k-1, -wOut+1-g);
				vhdl << endl;

				//save the critical path
				double criticalPathDeltaX2 = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("coeffD");
				syncCycleFromSignal("coeffE");
				syncCycleFromSignal("coeffF");
				syncCycleFromSignal("DeltaX2_short");
				syncCycleFromSignal("DeltaY2_short");
				syncCycleFromSignal("DeltaX_DeltaY_short");
				manageCriticalPath(getTarget()->DSPMultiplierDelay());

				//create D*DeltaX^2, E*DeltaY^2 and F*DeltaX*DeltaY
				/*
				vhdl << tab << declareFixPoint("D_DeltaX2", true, msbD-2*k-1,  -2*wIn-wOut-g) << " <= D * DeltaX2;" << endl;
				vhdl << tab << declareFixPoint("E_DeltaY2", true, msbE-2*k-1,  -2*wIn-wOut-g) << " <= E * DeltaY2;" << endl;
				vhdl << tab << declareFixPoint("F_DeltaX_DeltaY", true, msbF-2*k-1,  -2*wIn-wOut-g) << " <= F * DeltaX_DeltaY;" << endl;
				*/

				//manage the pipeline
				setCycleFromSignal("coeffD");
				syncCycleFromSignal("DeltaX2_short");
				if(getCycleFromSignal("coeffD") > getCycleFromSignal("DeltaX2_short"))
					setCriticalPath(tempCriticalPath2);
				else
					setCriticalPath(criticalPathDeltaX2);
				manageCriticalPath(getTarget()->DSPMultiplierDelay());

				vhdl << tab << declareFixPoint("D_DeltaX2", true, msbD-2*k-1,  -2*(wOut-1+g)) << " <= coeffD * DeltaX2_short;" << endl;

				//save the critical path
				double criticalPathD_DeltaX2 = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("coeffE");
				syncCycleFromSignal("DeltaY2_short");
				if(getCycleFromSignal("coeffE") > getCycleFromSignal("DeltaY2_short"))
					setCriticalPath(tempCriticalPath2);
				else
					setCriticalPath(criticalPathDeltaX2);
				manageCriticalPath(getTarget()->DSPMultiplierDelay());

				vhdl << tab << declareFixPoint("E_DeltaY2", true, msbE-2*k-1,  -2*(wOut-1+g)) << " <= coeffE * DeltaY2_short;" << endl;

				//save the critical path
				double criticalPathE_DeltaY2 = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("coeffF");
				syncCycleFromSignal("DeltaX_DeltaY_short");
				if(getCycleFromSignal("coeffF") > getCycleFromSignal("DeltaX_DeltaY_short"))
					setCriticalPath(tempCriticalPath2);
				else
					setCriticalPath(criticalPathDeltaX_DeltaY);
				manageCriticalPath(getTarget()->DSPMultiplierDelay());

				vhdl << tab << declareFixPoint("F_DeltaX_DeltaY", true, msbF-2*k-1,  -2*(wOut-1+g)) << " <= coeffF * DeltaX_DeltaY_short;" << endl;
				vhdl << endl;

				//save the critical path
				double criticalPathF_DeltaX_DeltaY = getCriticalPath();

				//align the signals to the output format to the output format
				resizeFixPoint("D_DeltaX2_sgnExt", "D_DeltaX2", maxMSB-1, -wOut+1-g);
				resizeFixPoint("E_DeltaY2_sgnExt", "E_DeltaY2", maxMSB-1, -wOut+1-g);
				resizeFixPoint("F_DeltaX_DeltaY_sgnExt", "F_DeltaX_DeltaY", maxMSB-1, -wOut+1-g);
				vhdl << endl;

				//manage the pipeline
				setCycleFromSignal("A_DeltaX_sgnExt");
				syncCycleFromSignal("B_DeltaY_sgnExt");
				setCriticalPath(criticalPathA_DeltaX);
				manageCriticalPath(getTarget()->adderDelay(wOut+g+1));

				//add everything up
				vhdl << tab << declareFixPoint("Sum1", true, maxMSB, -wOut+1-g)
						<< " <= (A_DeltaX_sgnExt(A_DeltaX_sgnExt'HIGH) & A_DeltaX_sgnExt) + (B_DeltaY_sgnExt(B_DeltaY_sgnExt'HIGH) & B_DeltaY_sgnExt);" << endl;

				//save the critical path
				double criticalPathSum1 = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("D_DeltaX2_sgnExt");
				setCriticalPath(criticalPathD_DeltaX2);
				manageCriticalPath(getTarget()->adderDelay(wOut+g+1));

				vhdl << tab << declareFixPoint("Sum2", true, maxMSB, -wOut+1-g)
						<< " <= (C_sgnExt(C_sgnExt'HIGH) & C_sgnExt) + (D_DeltaX2_sgnExt(D_DeltaX2_sgnExt'HIGH) & D_DeltaX2_sgnExt);" << endl;

				//save the critical path
				double criticalPathSum2 = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("E_DeltaY2_sgnExt");
				syncCycleFromSignal("F_DeltaX_DeltaY_sgnExt");
				if(getCycleFromSignal("E_DeltaY2_sgnExt") > getCycleFromSignal("F_DeltaX_DeltaY_sgnExt"))
					setCriticalPath(criticalPathE_DeltaY2);
				else
					setCriticalPath(criticalPathF_DeltaX_DeltaY);
				manageCriticalPath(getTarget()->adderDelay(wOut+g+1));

				vhdl << tab << declareFixPoint("Sum3", true, maxMSB, -wOut+1-g)
						<< " <= (E_DeltaY2_sgnExt(E_DeltaY2_sgnExt'HIGH) & E_DeltaY2_sgnExt) + (F_DeltaX_DeltaY_sgnExt(F_DeltaX_DeltaY_sgnExt'HIGH) & F_DeltaX_DeltaY_sgnExt);" << endl;

				//save the critical path
				double criticalPathSum3 = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("Sum1");
				syncCycleFromSignal("Sum2");
				if(getCycleFromSignal("Sum1") > getCycleFromSignal("Sum2"))
					setCriticalPath(criticalPathSum1);
				else
					setCriticalPath(criticalPathSum2);
				manageCriticalPath(getTarget()->adderDelay(wOut+g+1));

				vhdl << tab << declareFixPoint("Sum4", true, maxMSB+1, -wOut+1-g)
						<< " <= (Sum1(Sum1'HIGH) & Sum1) + (Sum2(Sum2'HIGH) & Sum2);" << endl;

				//save the critical path
				double criticalPathSum4 = getCriticalPath();

				//manage the pipeline
				setCycleFromSignal("Sum4");
				syncCycleFromSignal("Sum3");
				if(getCycleFromSignal("Sum3") > getCycleFromSignal("Sum4"))
					setCriticalPath(criticalPathSum3);
				else
					setCriticalPath(criticalPathSum4);
				manageCriticalPath(getTarget()->adderDelay(wOut+g+1));

				vhdl << tab << declareFixPoint("Sum5", true, maxMSB+2, -wOut+1-g)
						<< " <= (Sum4(Sum4'HIGH) & Sum4) + (Sum3(Sum3'HIGH) & Sum3(Sum3'HIGH) & Sum3);" << endl;
				vhdl << endl;

				//manage the pipeline
				manageCriticalPath(getTarget()->adderDelay(wOut+2));

				//extract the final result
				resizeFixPoint("Rtmp", "Sum5", 1, -wOut);
				vhdl << tab << declareFixPoint("Rtmp_rndCst", true, 1, -wOut) << " <= signed(std_logic_vector\'(\"" << zg(wOut+1, -2) << "1\"));" << endl;
				vhdl << tab << declareFixPoint("Rtmp_rnd", true, 1, -wOut) << " <= Rtmp + Rtmp_rndCst;" << endl;

				//manage the pipeline
				manageCriticalPath(getTarget()->localWireDelay());

				//return the result
				resizeFixPoint("Rtmp_stdlv", "Rtmp_rnd", -1, -wOut+2);
				vhdl << tab << declare("R_int", wOut-2) << " <= std_logic_vector(Rtmp_stdlv);" << endl;
			}else
			{
				//manage the pipeline
				setCycleFromSignal("XRS");
				syncCycleFromSignal("YRS");
				manageCriticalPath(getTarget()->localWireDelay());

				//create the bitheap
				bitHeap = new BitHeap(this, (maxMSB+2)+wOut+g);

				//create the input signals for the table
				/*
				vhdl << tab << declare("XHigh", k-1) << " <= std_logic_vector(X" << range(wIn-3, wIn-1-k) << ");" << endl;
				vhdl << tab << declare("YHigh", k)   << " <= std_logic_vector(Y" << range(wIn-2, wIn-1-k) << ");" << endl;
				*/
				vhdl << tab << declare("XHigh", k-1) << " <= std_logic_vector(XRS" << range(wIn-3, wIn-1-k) << ");" << endl;
				vhdl << tab << declare("YHigh", k)   << " <= std_logic_vector(YRS" << range(wIn-2, wIn-1-k) << ");" << endl;
				vhdl << endl;
				vhdl << tab << declare("atan2TableInput", 2*k-1) << " <= std_logic_vector(XHigh) & std_logic_vector(YHigh);" << endl;
				vhdl << endl;

				//create the table for atan(y/x)
				Atan2Table *table = new Atan2Table(getTarget(), 2*k-1, msbA+msbB+msbC+msbD+msbE+msbF+6*(wOut-1+g),
													architectureType,
													msbA, msbB, msbC, msbD, msbE, msbF,
													inDelayMap("atan2TableInput", getCriticalPath()) );

				//add the table to the operator
				addSubComponent(table);
				//useSoftRAM(table);
				useHardRAM(table);
				inPortMap (table , "X", "atan2TableInput");
				outPortMap(table , "Y", "atan2TableOutput");
				vhdl << instance(table , "Atan2Table");
				vhdl << endl;

				//manage the pipeline
				syncCycleFromSignal("atan2TableOutput");
				setCriticalPath(table->getOutputDelay("Y"));
				manageCriticalPath(getTarget()->localWireDelay());

				//split the output of the table to the corresponding parts A, B, C, D, E and F
				vhdl << tab << declareFixPoint("coeffF", true, msbF-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbF+1*(wOut-1+g)-1, 0) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffE", true, msbE-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbE+msbF+2*(wOut-1+g)-1, msbF+1*(wOut-1+g)) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffD", true, msbD-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbD+msbE+msbF+3*(wOut-1+g)-1, msbE+msbF+2*(wOut-1+g)) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffC", true, msbC-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbC+msbD+msbE+msbF+4*(wOut-1+g)-1, msbD+msbE+msbF+3*(wOut-1+g)) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffB", true, msbB-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbB+msbC+msbD+msbE+msbF+5*(wOut-1+g)-1, msbC+msbD+msbE+msbF+4*(wOut-1+g)) << ");" << endl;
				vhdl << tab << declareFixPoint("coeffA", true, msbA-1,  -wOut+1-g) << " <= signed(atan2TableOutput"
						<< range(msbA+msbB+msbC+msbD+msbE+msbF+6*(wOut-1+g)-1, msbB+msbC+msbD+msbE+msbF+5*(wOut-1+g)) << ");" << endl;
				vhdl << endl;

				//create A, B, C, D, E, F as logic vectors
				vhdl << tab << declare("A_stdlv", msbA+wOut-1+g) << " <= std_logic_vector(coeffA);" << endl;
				vhdl << tab << declare("B_stdlv", msbB+wOut-1+g) << " <= std_logic_vector(coeffB);" << endl;
				vhdl << tab << declare("D_stdlv", msbD+wOut-1+g) << " <= std_logic_vector(coeffD);" << endl;
				vhdl << tab << declare("E_stdlv", msbE+wOut-1+g) << " <= std_logic_vector(coeffE);" << endl;
				vhdl << tab << declare("F_stdlv", msbF+wOut-1+g) << " <= std_logic_vector(coeffF);" << endl;

				//create DeltaX and DeltaY
				/*
				vhdl << tab << declareFixPoint("DeltaX", true, -k-1,  -wIn+1) << " <= signed(not(X(" << of(wIn-k-2) << ")) & X" << range(wIn-k-3, 0) << ");" << endl;
				vhdl << tab << declareFixPoint("DeltaY", true, -k-1,  -wIn+1) << " <= signed(not(Y(" << of(wIn-k-2) << ")) & Y" << range(wIn-k-3, 0) << ");" << endl;
				*/
				vhdl << tab << declareFixPoint("DeltaX", true, -k-1,  -wIn+1) << " <= signed(not(XRS(" << of(wIn-k-2) << ")) & XRS" << range(wIn-k-3, 0) << ");" << endl;
				vhdl << tab << declareFixPoint("DeltaY", true, -k-1,  -wIn+1) << " <= signed(not(YRS(" << of(wIn-k-2) << ")) & YRS" << range(wIn-k-3, 0) << ");" << endl;

				vhdl << tab << declare("DeltaX_stdlv", wIn-1-k) << " <= std_logic_vector(DeltaX);" << endl;
				vhdl << tab << declare("DeltaY_stdlv", wIn-1-k) << " <= std_logic_vector(DeltaY);" << endl;
				vhdl << endl;

				//save the critical path
				tempCriticalPath = getCriticalPath();

				//create A*DeltaX
				/*IntMultiplier* multADeltaX;
				multADeltaX = new IntMultiplier(this,							//parent operator
											 bitHeap,							//the bit heap that performs the compression
											 getSignalByName("DeltaX_stdlv"),	//first input to the multiplier (a signal)
											 getSignalByName("A_stdlv"),		//second input to the multiplier (a signal)
											 -wIn+1,							//offset of the LSB of the multiplier in the bit heap
											 false, //negate,					//whether to subtract the result of the multiplication from the bit heap
											 true,								//signed/unsigned operator
											 ratio);							//DSP ratio
											 */
				vhdl << endl;

				//create B*DeltaY
				/*IntMultiplier* multBDeltaY;
				multBDeltaY = new IntMultiplier(this,								//parent operator
												 bitHeap,							//the bit heap that performs the compression
												 getSignalByName("DeltaY_stdlv"),	//first input to the multiplier (a signal)
												 getSignalByName("B_stdlv"),		//second input to the multiplier (a signal)
												 -wIn+1,							//offset of the LSB of the multiplier in the bit heap
												 false, //negate,					//whether to subtract the result of the multiplication from the bit heap
												 true,								//signed/unsigned operator
												 ratio);							//DSP ratio
												 */
				vhdl << endl;

				//Add C
				bitHeap->addSignedBitVector(0,									//weight of signal in the bit heap
											"coeffC",								//name of the signal
											msbC+wOut-1+g,						//size of the signal added
											0,									//index of the lsb in the bit vector from which to add the bits of the addend
											false);								//if we are correcting the index in the bit vector with a negative weight
				vhdl << endl;

				//manage the pipeline
				setCycleFromSignal("DeltaX");
				syncCycleFromSignal("DeltaY");
				setCriticalPath(tempCriticalPath);
				manageCriticalPath(getTarget()->DSPMultiplierDelay());

				//create DeltaX^2, DeltaY^2 and DeltaX*DeltaY
				//vhdl << tab << declareFixPoint("DeltaX2", true, -2*k-1,  -2*wIn+2) << " <= DeltaX * DeltaX;" << endl;
				//vhdl << tab << declareFixPoint("DeltaY2", true, -2*k-1,  -2*wIn+2) << " <= DeltaY * DeltaY;" << endl;
				vhdl << tab << declareFixPoint("DeltaX_DeltaY", true, -2*k-1,  -2*wIn+2) << " <= DeltaX * DeltaY;" << endl;
				vhdl << endl;

				BipartiteTable *deltaX2Table = new BipartiteTable(getTarget(),
																	join("(2^(-", k, "))*(x^2)"),
																	//-wIn+k+1, -1, -2*wIn+2*k+2,
																	-wIn+k+1, -1, -wOut+1-g+2*k,
																	inDelayMap("DeltaX", getCriticalPath()) );
				//add the table to the operator
				addSubComponent(deltaX2Table);
				//useSoftRAM(deltaX2Table);
				useHardRAM(deltaX2Table);
				inPortMap (deltaX2Table , "X", "DeltaX_stdlv");
				outPortMap(deltaX2Table , "Y", "DeltaX2_stdlv");
				vhdl << instance(deltaX2Table , "DeltaX2Table");
				vhdl << endl;

				BipartiteTable *deltaY2Table = new BipartiteTable(getTarget(),
																	join("(2^(-", k, "))*(x^2)"),
																	//-wIn+k+1, -1, -2*wIn+2*k+2,
																	-wIn+k+1, -1, -wOut+1-g+2*k,
																	inDelayMap("DeltaY", getCriticalPath()) );
				//add the table to the operator
				addSubComponent(deltaY2Table);
				//useSoftRAM(deltaY2Table);
				useHardRAM(deltaY2Table);
				inPortMap (deltaY2Table , "X", "DeltaY_stdlv");
				outPortMap(deltaY2Table , "Y", "DeltaY2_stdlv");
				vhdl << instance(deltaY2Table , "DeltaY2Table");
				vhdl << endl;

				//convert to fixed point in VHDL
				//vhdl << tab << declareFixPoint("DeltaX2", true, -2*k-1,  -2*wIn+2) << " <= signed(DeltaX2_stdlv);" << endl;
				//vhdl << tab << declareFixPoint("DeltaY2", true, -2*k-1,  -2*wIn+2) << " <= signed(DeltaY2_stdlv);" << endl;
				vhdl << tab << declareFixPoint("DeltaX2", true, -2*k-1,  -wOut+1-g) << " <= signed(DeltaX2_stdlv);" << endl;
				vhdl << tab << declareFixPoint("DeltaY2", true, -2*k-1,  -wOut+1-g) << " <= signed(DeltaY2_stdlv);" << endl;

				//manage the pipeline
				manageCriticalPath(getTarget()->localWireDelay());

				//align the products, discard the extra lsb-s
				resizeFixPoint("DeltaX2_short", "DeltaX2", -2*k-1, -wOut+1-g);
				resizeFixPoint("DeltaY2_short", "DeltaY2", -2*k-1, -wOut+1-g);
				resizeFixPoint("DeltaX_DeltaY_short", "DeltaX_DeltaY", -2*k-1, -wOut+1-g);
				vhdl << endl;

				//create D*DeltaX^2, E*DeltaY^2 and F*DeltaX*DeltaY

				//create D*DeltaX^2
				//	convert the terms to std_logic_vector
				vhdl << tab << declare("DeltaX2_short_stdlv", wOut-1+g-2*k) << " <= std_logic_vector(DeltaX2_short);" << endl;
				//	multiply
				/*IntMultiplier* multDDeltaX2;
				multDDeltaX2 = new IntMultiplier(this,								//parent operator
												 bitHeap,							//the bit heap that performs the compression
												 getSignalByName("DeltaX2_short_stdlv"),	//first input to the multiplier (a signal)
												 getSignalByName("D_stdlv"),		//second input to the multiplier (a signal)
												 -wOut+1-g,							//offset of the LSB of the multiplier in the bit heap
												 false, //negate,					//whether to subtract the result of the multiplication from the bit heap
												 true,								//signed/unsigned operator
												 ratio);							//DSP ratio
												 */
				vhdl << endl;

				//	create E*DeltaY^2
				//	convert the terms to std_logic_vector
				vhdl << tab << declare("DeltaY2_short_stdlv", wOut-1+g-2*k) << " <= std_logic_vector(DeltaY2_short);" << endl;
				//	multiply
				/*IntMultiplier* multEDeltaY2;
				multEDeltaY2 = new IntMultiplier(this,								//parent operator
												 bitHeap,							//the bit heap that performs the compression
												 getSignalByName("DeltaY2_short_stdlv"),	//first input to the multiplier (a signal)
												 getSignalByName("E_stdlv"),		//second input to the multiplier (a signal)
												 -wOut+1-g,							//offset of the LSB of the multiplier in the bit heap
												 false, //negate,					//whether to subtract the result of the multiplication from the bit heap
												 true,								//signed/unsigned operator
												 ratio);							//DSP ratio
												 */
				vhdl << endl;

				//	create F*DeltaX_DeltaY
				//	convert the terms to std_logic_vector
				vhdl << tab << declare("DeltaX_DeltaY_short_stdlv", wOut-1+g-2*k) << " <= std_logic_vector(DeltaX_DeltaY_short);" << endl;
				//	multiply
				/*IntMultiplier* multFDeltaXDeltaY;
				multFDeltaXDeltaY = new IntMultiplier(this,								//parent operator
													 bitHeap,							//the bit heap that performs the compression
													 getSignalByName("DeltaX_DeltaY_short_stdlv"),	//first input to the multiplier (a signal)
													 getSignalByName("F_stdlv"),		//second input to the multiplier (a signal)
													 -wOut+1-g,							//offset of the LSB of the multiplier in the bit heap
													 false, //negate,					//whether to subtract the result of the multiplication from the bit heap
													 true,								//signed/unsigned operator
													 ratio);							//DSP ratio
													 */
				vhdl << endl;

				//add the rounding bit - take into consideration the final alignment
				bitHeap->addConstantOneBit(g-1);

				//compress the bit heap
				bitHeap -> generateCompressorVHDL();

				//extract the result - take into consideration the final alignment
				//vhdl << tab << "R <= " << bitHeap->getSumName() << range(wOut+g-1, g) << ";" << endl;
				vhdl << tab << declare("R_int", wOut-2) << " <= " << bitHeap->getSumName() << range(wOut+g-1-1, g+1) << ";" << endl;
			}

		}else if(architectureType == 3)
		{
			//based on rotating using a table (for sine and cosine)
			//	and then using the Taylor series for (1/x)
			THROWERROR("in FixAtan2ByBivariateApprox constructor: architecture not yet implemented");
		}else
		{
			THROWERROR("in FixAtan2ByBivariateApprox constructor: invalid value for the requested architecture type");
		}

		vhdl << tab << declare("finalZ", wOut) << " <= \"00\" & R_int;" << endl;

		// Reconstruction code is shared, defined in FixAtan2
		buildQuadrantReconstruction();

	}


	FixAtan2ByBivariateApprox::~FixAtan2ByBivariateApprox()
	{

	}

	FixAtan2ByBivariateApprox* FixAtan2ByBivariateApprox::newComponentAndInstanceNumericStd(Operator* op,
															int wIn,
															int wOut)
	{
		//TODO
		return 0;
	}

	int FixAtan2ByBivariateApprox::checkArchitecture(int archType)
	{
		int k = (wIn/4);

		double /*error,*/ errorMax, errorMin;
		bool errorSatisfied;
		double errorLimit = 1.0/(1 << (wOut-1));

		cout << "Beginning computations for error checking method based on the "
				<< (archType==0 ? "plane's equation" : (archType==1 ? "Taylor polynomial of order 1" : "Taylor polynomial of order 2"))
				<< " Atan2, with wIn=" << wIn << " and wOut=" << wOut << endl;

		cout << tab << "Computing the parameters for the a, b and d tables" << endl;
		errorSatisfied = false;
		while(!errorSatisfied)
		{
			bool runLoop = true;
			cout << tab << tab << "trying k=" << k << endl;

			errorMax = 0.0;
			errorMin = 1.0;

			for(int j=0; (j<(1<<k)-1 && runLoop); j++)
			{
				for(int i=(1<<(k-1)); (i<((1<<k)-1) && runLoop); i++)
				{
					Point *P1, *P2, *P3, *P4, *P5;
					Plane *plane;
					double a, b, c, d, e, f;
					double distance, distanceMin;
					double valI, valJ, centerValI, centerValJ, increment, halfIncrement;
					double distanceFromCenter;

					//create the actual values for the points in which we compute the atan2
					valI = 1.0*i/(1<<k);
					valJ = 1.0*j/(1<<k);
					increment = 1.0/(1<<k);
					halfIncrement = increment/2.0;
					centerValI = valI + halfIncrement;
					centerValJ = valJ + halfIncrement;
					distanceFromCenter = 1.0/(1<<(wIn+1));

					/**
					 * version 1: compute the values stored in the tables using the equation of the plane
					 */
					if(archType == 0)
					{
						//create the points
						P1 = new Point(centerValI,						centerValJ,						atan2(centerValJ, 						centerValI						));
						P2 = new Point(centerValI,						centerValJ+distanceFromCenter,	atan2(centerValJ+distanceFromCenter, 	centerValI						));
						P3 = new Point(centerValI+distanceFromCenter,	centerValJ, 					atan2(centerValJ, 						centerValI+distanceFromCenter	));
						P4 = new Point(centerValI, 						centerValJ-distanceFromCenter, 	atan2(centerValJ-distanceFromCenter, 	centerValI			  			));
						P5 = new Point(centerValI-distanceFromCenter,	centerValJ, 					atan2(centerValJ, 						centerValI-distanceFromCenter	));

						//create all the planes given by all the possible combinations of the 4 points
						// and determine the minimum distance from the plane given by three of the points
						// to the remaining point
						//the plane given by P2, P3 and P4
						plane = new Plane(P2, P3, P4);
						distance = P1->getZ() - ((-P1->getX()*plane->getA() - P1->getY()*plane->getB() - plane->getD())/plane->getC());
						if(distance<0)
							distance *= -1;
						distanceMin = distance;
						a = plane->getA();
						b = plane->getB();
						c = plane->getC();
						d = plane->getD();
						delete plane;

						//the plane given by P2, P3 and P5
						plane = new Plane(P2, P3, P5);
						distance = P1->getZ() - ((-P1->getX()*plane->getA() - P1->getY()*plane->getB() - plane->getD())/plane->getC());
						if(distance<0)
							distance *= -1;
						if(distance<distanceMin)
						{
							distanceMin = distance;
							a = plane->getA();
							b = plane->getB();
							c = plane->getC();
							d = plane->getD();
						}
						delete plane;

						//the plane given by P2, P4 and P5
						plane = new Plane(P2, P4, P5);
						distance = P1->getZ() - ((-P1->getX()*plane->getA() - P1->getY()*plane->getB() - plane->getD())/plane->getC());
						if(distance<0)
							distance *= -1;
						if(distance<distanceMin)
						{
							distanceMin = distance;
							a = plane->getA();
							b = plane->getB();
							c = plane->getC();
							d = plane->getD();
						}
						delete plane;

						//the plane given by P3, P4 and P5
						plane = new Plane(P3, P4, P5);
						distance = P1->getZ() - ((-P1->getX()*plane->getA() - P1->getY()*plane->getB() - plane->getD())/plane->getC());
						if(distance<0)
							distance *= -1;
						if(distance<distanceMin)
						{
							distanceMin = distance;
							a = plane->getA();
							b = plane->getB();
							c = plane->getC();
							d = plane->getD();
						}
						delete plane;

						//update the variables for use in the error measurement
						a = -(a/c);
						b = -(b/c);
						d = -(d/c);

						delete P1;
						delete P2;
						delete P3;
						delete P4;
						delete P5;

						//re-adjust the plane coordinates so as to minimize
						//	the error at the corners and in the center
						double errorP1, errorP2, errorP3, errorP4;
						double errorCenter;
						double errorMaxx, errorMinn, errorMean, errorAdjust;

						errorCenter = atan2(centerValJ, centerValI) - (a*centerValI + b*centerValJ + d);
						errorP1 = atan2(valJ, valI) - (a*valI + b*valJ + d);
						errorP2 = atan2(valJ, valI+increment) - (a*(valI+increment) + b*valJ + d);
						errorP3 = atan2(valJ+increment, valI) - (a*valI + b*(valJ+increment) + d);
						errorP4 = atan2(valJ+increment, valI+increment) - (a*(valI+increment) + b*(valJ+increment) + d);

						errorMaxx = max(5, errorCenter, errorP1, errorP2, errorP3, errorP4);
						errorMinn = min(5, errorCenter, errorP1, errorP2, errorP3, errorP4);

						errorMean = (fabs(errorMaxx)+fabs(errorMinn))/2.0;
						if((errorMaxx>=0) && (errorMinn>=0))
						{
							errorAdjust = errorMinn+(errorMaxx-errorMinn)/2.0;
						}else if((errorMaxx>=0) && (errorMinn<0))
						{
							errorAdjust = errorMaxx-errorMean;
						}else if((errorMaxx<0) && (errorMinn<0))
						{
							errorAdjust = errorMaxx+(errorMinn-errorMaxx)/2.0;
						}

						//equalize the maximum and the minimum error
						errorCenter -= errorAdjust;
						errorP1 -= errorAdjust;
						errorP2 -= errorAdjust;
						errorP3 -= errorAdjust;
						errorP4 -= errorAdjust;

						//test if any of the errors are above the error limit
						if((errorCenter>errorLimit) || (errorP1>errorLimit) || (errorP2>errorLimit) || (errorP2>errorLimit) || (errorP2>errorLimit))
						{
							cout << tab << tab << tab << "not worth continuing: distance from corners after adjustments is greater than the error limit" << endl;
							cout << tab << tab << tab << tab << "distance from center=" << errorCenter << " P1=" << errorP1 << " P2=" << errorP2
									<< " P3=" << errorP3 << " P4=" << errorP4 << " error limit=" << errorLimit << endl;

							runLoop = false;
							errorMax = errorMaxx-errorAdjust;
							errorMin = errorMinn-errorAdjust;
							break;
						}

						//compute the new equation of the plane
						P1 = new Point(valI, valJ, (a*valI + b*valJ + d)-errorAdjust);
						P2 = new Point(valI+increment, valJ, (a*(valI+increment) + b*valJ + d)-errorAdjust);
						P3 = new Point(valI, valJ+increment, (a*valI + b*(valJ+increment) + d)-errorAdjust);
						plane = new Plane(P1, P2, P3);
						a = plane->getA();
						b = plane->getB();
						c = plane->getC();
						d = plane->getD();
						a = -(a/c);
						b = -(b/c);
						d = -(d/c);
						delete plane;
						delete P1;
						delete P2;
						delete P3;
					}

					/**
					 * version 2: compute the values stored in the tables using a Taylor polynomial (of order 1)
					 * 	then atan(y/x) = a*x + b*y + c
					 */
					else if(archType == 1)
					{
						a = -(1.0*centerValJ)/(centerValI*centerValI+centerValJ*centerValJ);
						b = (1.0*centerValI)/(centerValI*centerValI+centerValJ*centerValJ);
						c = atan2(centerValJ, centerValI);

						//test if any of the errors are above the error limit
						double errorCenter 	= (a*centerValI + b*centerValJ + c) - atan2(centerValJ, centerValI);
						if(errorCenter<0)
							errorCenter = -errorCenter;
						double errorP1 		= (a*valI + b*valJ + c) - atan2(valJ, valI);
						if(errorP1<0)
							errorP1 = -errorP1;
						double errorP2 		= (a*(valI+increment) + b*valJ + c) - atan2(valJ, (valI+increment));
						if(errorP2<0)
							errorP2 = -errorP2;
						double errorP3 		= (a*valI + b*(valJ+increment) + c) - atan2((valJ+increment), valI);
						if(errorP3<0)
							errorP3 = -errorP3;
						double errorP4 		= (a*(valI+increment) + b*(valJ+increment) + c) - atan2((valJ+increment), (valI+increment));
						if(errorP4<0)
							errorP4 = -errorP4;
						if((errorCenter>errorLimit) || (errorP1>errorLimit) || (errorP2>errorLimit)
								|| (errorP3>errorLimit) || (errorP4>errorLimit))
						{
							cout << tab << tab << tab << "not worth continuing: distance from corners is greater than the error limit" << endl;
							cout << tab << tab << tab << tab << "distance from center=" << errorCenter << " P1=" << errorP1 << " P2=" << errorP2
									<< " P3=" << errorP3 << " P4=" << errorP4 << " error limit=" << errorLimit << endl;

							runLoop = false;
							errorMax = max(4, errorP1, errorP2, errorP3, errorP4);
							errorMin = min(5, errorP1, errorP2, errorP3, errorP4);
							break;
						}
					}


					/**
					 * version 3: compute the values stored in the tables using a Taylor polynomial (of order 2)
					 * 	then atan(x/y) = a*x + b*y + c + d*x^2 + e*y^2 + f*x*y
					 */
					else if(archType == 2)
					{
						double denominator = centerValI*centerValI + centerValJ*centerValJ;
						double denominatorSqr = denominator*denominator;

						a = -(2.0*centerValJ)/(denominator);
						b = (2.0*centerValI)/(denominator);
						c = atan2(centerValJ, centerValI);
						d = (1.0*centerValI*centerValJ)/(denominatorSqr);
						e = -(1.0*centerValI*centerValJ)/(denominatorSqr);
						f = (1.0*centerValJ*centerValJ-1.0*centerValI*centerValI)/(denominatorSqr);

						//test if any of the errors are above the error limit
						double errorCenter 	= (a*centerValI + b*centerValJ + c + d*centerValI*centerValI + e*centerValJ*centerValJ + f*centerValI*centerValJ)
																- atan2(centerValJ, centerValI);
						if(errorCenter<0)
							errorCenter = -errorCenter;
						double errorP1 		= (a*valI + b*valJ + c + d*valI*valI + e*valJ*valJ + f*valI*valJ)
																- atan2(valJ, valI);
						if(errorP1<0)
							errorP1 = -errorP1;
						double errorP2 		= (a*(valI+increment) + b*valJ + c + d*(valI+increment)*(valI+increment) + e*valJ*valJ + f*(valI+increment)*valJ)
																- atan2(valJ, (valI+increment));
						if(errorP2<0)
							errorP2 = -errorP2;
						double errorP3 		= (a*valI + b*(valJ+increment) + c + d*valI*valI + e*(valJ+increment)*(valJ+increment) + f*valI*(valJ+increment))
																- atan2((valJ+increment), valI);
						if(errorP3<0)
							errorP3 = -errorP3;
						double errorP4 		= (a*(valI+increment) + b*(valJ+increment) + c
								+ d*(valI+increment)*(valI+increment) + e*(valJ+increment)*(valJ+increment) + f*(valI+increment)*(valJ+increment))
																- atan2((valJ+increment), (valI+increment));
						if(errorP4<0)
							errorP4 = -errorP4;
						if((errorCenter>errorLimit) || (errorP1>errorLimit) || (errorP2>errorLimit)
								|| (errorP3>errorLimit) || (errorP4>errorLimit))
						{
							cout << tab << tab << tab << "not worth continuing: distance from corners is greater than the error limit" << endl;
							cout << tab << tab << tab << tab << "distance from center=" << errorCenter << " P1=" << errorP1 << " P2=" << errorP2
									<< " P3=" << errorP3 << " P4=" << errorP4 << " error limit=" << errorLimit << endl;

							runLoop = false;
							errorMax = max(4, errorP1, errorP2, errorP3, errorP4);
							errorMin = min(4, errorP1, errorP2, errorP3, errorP4);
							break;
						}
					}
					else
					{
						THROWERROR("Error: unknown type of architecture to check");
					}

					//determine the maximum values of A, B and D (or C, depending on the architecture)
					if((archType == 0) || (archType == 1) || (archType == 2))
					{
						double auxA, auxB, auxC, auxD, auxE, auxF;

						// maximum value of A/(Pi/2)
						auxA = ((a < 0) ? -a : a);
						auxA = auxA / (M_PI/2.0);
						if(auxA > maxValA)
							maxValA = auxA;
						// maximum value of B
						auxB = ((b < 0) ? -b : b);
						auxB = auxB / (M_PI/2.0);
						if(auxB > maxValB)
							maxValB = auxB;
						if(archType == 0)
						{
							// maximum value of D
							auxD = d + a*valI + b*valJ;
							auxD = ((auxD < 0) ? -auxD : auxD);
							auxD = auxD / (M_PI/2.0);
							if(auxD > maxValC)
								maxValC = auxD;
						}else if(archType == 1)
						{
							// maximum value of C
							auxC = c + a*valI + b*valJ;
							auxC = ((auxC < 0) ? -auxC : auxC);
							auxC = auxC / (M_PI/2.0);
							if(auxC > maxValC)
								maxValC = auxC;
						}else if(archType == 2)
						{
							// maximum value of C
							auxC = ((c < 0) ? -c : c);
							auxC = auxC / (M_PI/2.0);
							if(auxC > maxValC)
								maxValC = auxC;
							// maximum value of D
							auxD = ((d < 0) ? -d : d);
							auxD = auxD / (M_PI/2.0);
							if(auxD > maxValD)
								maxValD = auxD;
							// maximum value of E
							auxE = ((e < 0) ? -e : e);
							auxE = auxE / (M_PI/2.0);
							if(auxE > maxValE)
								maxValE = auxE;
							// maximum value of F
							auxF = ((f < 0) ? -f : f);
							auxF = auxF / (M_PI/2.0);
							if(auxF > maxValF)
								maxValF = auxF;
						}
					}


					//now check the error against all the points in the plane, at the given resolution
					/*
					for(int n=0; (n<(1<<(wIn-k)) && runLoop); n++)
						for(int m=0; (m<(1<<(wIn-k)) && runLoop); m++)
						{
							double valIPrime, valJPrime;

							//create the actual values for the atan2
							valIPrime = 1.0*((i<<(wIn-k))+n)/(1<<wIn);
							valJPrime = 1.0*((j<<(wIn-k))+m)/(1<<wIn);

							double referenceValue = atan2(valJPrime, valIPrime);
							double computedValue;

							if(archType == 0){
								// v1
								computedValue  = a*valIPrime + b*valJPrime + d;
							}else if(archType == 1){
								// v2
								computedValue  = a*valIPrime + b*valJPrime + c;
							}else{
								// v3
								computedValue  = a*valIPrime + b*valJPrime + c + d*valIPrime*valIPrime + e*valJPrime*valJPrime + f*valIPrime*valJPrime;
							}

							error = referenceValue-computedValue;
							if(error < 0)
								error *= -1;

							if(error > errorMax)
								errorMax = error;
							if((error < errorMin) && !((n==0)&&(m==0)))
								errorMin = error;

							if(errorMax > errorLimit)
							{
								cout << tab << tab << tab << "not worth continuing: error=" << errorMax << " error limit=" << errorLimit << endl;
								cout << tab << tab << tab << tab << "i=" << i << " j=" << j << " n=" << n << " m=" << m << endl;

								runLoop = false;
								break;
							}
						}
						*/
				}

				cout << "\r                                                                                                                   \r";
				cout <<  tab << tab << tab << std::setprecision(4) << ((1.0*j/((1<<k)-1))*100) << "% done"
						<< tab << "current maximum error=" << errorMax << " current minimum error=" << errorMin << " error limit=" << errorLimit;
				cout.flush();
			}

			cout << endl << tab << tab << tab << "computed the maximum error" << endl;

			//check to see if the current precision k is enough
			//	if not, increase k and compute the maximum error again
			if(errorMax < errorLimit)
			{
				errorSatisfied = true;
			}else
			{
				cout << tab << tab << tab << "error limit of " << errorLimit << " not satisfied at k=" << k
						<< ", with wIn=" << wIn << " and wOut=" << wOut << endl;
				cout << tab << tab << tab << tab << "actual error errorMax=" << errorMax << endl;

				k++;
			}
		}

		cout << "Error limit of " << (1.0/(1 << (wOut-1))) << " satisfied at k=" << k
				<< ", with wIn=" << wIn << " and wOut=" << wOut << endl;
		if((archType == 0) || (archType == 1) || (archType == 2))
		{
			cout << tab << "Computed the maximum values of the table parameters (divided by Pi/2): maxValA="
					<< maxValA << " maxValB=" << maxValB;
			if(archType == 0)
				cout << " maxValD=" << maxValC << endl;
			else if(archType == 1)
				cout << " maxValC=" << maxValC << endl;
			else
				cout << " maxValC=" << maxValC << " maxValD=" << maxValD
					<< " maxValE=" << maxValE << " maxValF=" << maxValF << endl;
		}

		return k;
	}

	void FixAtan2ByBivariateApprox::generateTaylorOrder2Parameters(int x, int y, mpfr_t &fa, mpfr_t &fb, mpfr_t &fc, mpfr_t &fd, mpfr_t &fe, mpfr_t &ff)
	{
		mpfr_t centerValI, centerValJ, increment, temp, tempSqr, temp2;
		int k = kSize;

		mpfr_inits2(10000, centerValI, centerValJ, increment, temp, tempSqr, temp2, (mpfr_ptr)0);

		mpfr_set_si(increment, 1, GMP_RNDN);
		mpfr_div_2si(increment, increment, k+1, GMP_RNDN);
		mpfr_set_si(centerValI, x, GMP_RNDN);
		mpfr_div_2si(centerValI, centerValI, k, GMP_RNDN);
		mpfr_add(centerValI, centerValI, increment, GMP_RNDN);
		mpfr_set_si(centerValJ, y, GMP_RNDN);
		mpfr_div_2si(centerValJ, centerValJ, k, GMP_RNDN);
		mpfr_add(centerValJ, centerValJ, increment, GMP_RNDN);

		mpfr_sqr(temp, centerValI, GMP_RNDN);
		mpfr_sqr(temp2, centerValJ, GMP_RNDN);
		mpfr_add(temp, temp, temp2, GMP_RNDN);
		mpfr_sqr(tempSqr, temp, GMP_RNDN);
		//create A
		mpfr_set(fa, centerValJ, GMP_RNDN);
		mpfr_div(fa, fa, temp, GMP_RNDN);
		mpfr_neg(fa, fa, GMP_RNDN);
		//create B
		mpfr_set(fb, centerValI, GMP_RNDN);
		mpfr_div(fb, fb, temp, GMP_RNDN);
		//create C
		mpfr_atan2(fc, centerValJ, centerValI, GMP_RNDN);
		//create D
		mpfr_set(fd, centerValI, GMP_RNDN);
		mpfr_mul(fd, fd, centerValJ, GMP_RNDN);
		mpfr_div(fd, fd, tempSqr, GMP_RNDN);
		//create E
		mpfr_set(fe, fd, GMP_RNDN);
		mpfr_neg(fe, fe, GMP_RNDN);
		//create F
		mpfr_sqr(ff, centerValJ, GMP_RNDN);
		mpfr_sqr(temp2, centerValI, GMP_RNDN);
		mpfr_sub(ff, ff, temp2, GMP_RNDN);
		mpfr_div(ff, ff, tempSqr, GMP_RNDN);

		//clean-up
		mpfr_clears(centerValI, centerValJ, increment, temp, tempSqr, temp2, (mpfr_ptr)0);
	}





}
