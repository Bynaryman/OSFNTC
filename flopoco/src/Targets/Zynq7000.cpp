/*
  A model of Zynq7000 FPGA (exact part: xc7z020clg484-1, used in the Zedboard) 

  Author : Florent de Dinechin

  This file is part of the FloPoCo project
  
  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA-Lyon
  2008-2016.
  All rights reserved.
*/

#include "Zynq7000.hpp"
#include <iostream>
#include <sstream>
#include "../utils.hpp"


namespace flopoco{


	
	Zynq7000::Zynq7000(): Target()	{
			id_             		= "Zynq7000";
			vendor_         		= "Xilinx";

			maxFrequencyMHz_		= 500;

			/////// Architectural parameters
			lutInputs_ = 6;
			possibleDSPConfig_.push_back(make_pair(25,18));
			whichDSPCongfigCanBeUnsigned_.push_back(false);
			sizeOfBlock_ 			= 36864;	// the size of a primitive block is 2^11 * 9
			        // The blocks are 36kb configurable as dual 18k so I don't know.


			//////// Delay parameters, copypasted from Vivado timing reports
			
		}

	Zynq7000::~Zynq7000() {};

	//TODO
	double Zynq7000::logicDelay(int inputs){
		double delay;
		if(inputs <= lutInputs())
			delay= addRoutingDelay(lutDelay_);
		else
			delay= addRoutingDelay(lutDelay_) * (inputs -lutInputs() + 1);
		TARGETREPORT("logicDelay(" << inputs << ") = " << delay*1e9 << " ns.");
		return  delay; 
	}


	double Zynq7000::adderDelay(int size, bool addRoutingDelay_) {
		double delay = adderConstantDelay_ + ((size)/4 -1)* carry4Delay_;
		if(addRoutingDelay_) {
			delay=addRoutingDelay(delay);
			TARGETREPORT("adderDelay(" << size << ") = " << delay*1e9 << " ns.");
		}
		return  delay; 
	};

	
	double Zynq7000::eqComparatorDelay(int size){
		// TODO Refine
		return addRoutingDelay( lutDelay_ + double((size-1)/(lutInputs_/2)+1)/4*carry4Delay_); 
	}
	
	double Zynq7000::eqConstComparatorDelay(int size){
		// TODO Refine
		return addRoutingDelay( lutDelay_ + double((size-1)/lutInputs_+1)/4*carry4Delay_ ); 
	}
	double Zynq7000::ffDelay() {
		return ffDelay_; 
	};
	
	double Zynq7000::addRoutingDelay(double d) {
		return(d+typicalLocalRoutingDelay_);
	};
	
	double Zynq7000::carryPropagateDelay() {
		return carry4Delay_/4 ; 
	};
	
	double Zynq7000::fanoutDelay(int fanout){

			/* Some data points from FPAdd 8 23, after synthesis  
				 net (fo=4, unplaced)         0.494     0.972    test/cmpAdder/X_d1_reg[33][1]
				 net (fo=102, unplaced)       0.412     3.377    test/cmpAdder/swap
				 net (fo=10, unplaced)        0.948     5.464    test/fracAdder/expDiff[6]
				 net (fo=15, unplaced)        0.472     6.261    test/cmpAdder/Y_d1_reg[29]_3
				 net (fo=2, unplaced)         1.122     7.507    test/cmpAdder/sticky_d1_i_29_n_0
				 net (fo=2, unplaced)         0.913     8.544    test/cmpAdder/sticky_d1_i_15_n_0
				 net (fo=1, unplaced)         0.902     9.570    test/cmpAdder/sticky_d1_i_37_n_0
				 net (fo=1, unplaced)         0.665    10.359    test/cmpAdder/sticky_d1_i_20_n_0
				 net (fo=2, unplaced)         0.913    11.396    test/cmpAdder/sticky_d1_i_6_n_0
	
				 The same, after place and route
				 
				 102		   2.138 
				 69	  		 2.901
				 24		  	 0.878
				 13			   1.103
				 6				 1.465
				 5  			 1.664
				 2				 0.841
				 2				 0.894
				 1				 0.640
				 C'est n'importe quoi.

			*/
		double delay= fanoutConstant_*(fanout); // and this is also completely random
		TARGETREPORT("fanoutDelay(" << fanout << ") = " << delay*1e9 << " ns.");
		return delay;
		
	}
	
	double Zynq7000::lutConsumption(int lutInputSize)
	{
		if (lutInputSize <= 5) {
			return .5;
		}
		switch (lutInputSize) {
			case 6:
				return 1.;
			case 7:
				return 2.;
			case 8:
				return 4.;
			default:
				return -1.;
		}
	}

	double Zynq7000::lutDelay(){
		return lutDelay_;
	}
	
	long Zynq7000::sizeOfMemoryBlock()
	{
		return sizeOfBlock_;	
	}
	
	double Zynq7000::tableDelay(int wIn, int wOut, bool logicTable){
		if(logicTable) {
			return logicDelay(wIn);
		}
		else {
			return RAMDelay_+ RAMToLogicWireDelay_;
		}
	}


	DSP* Zynq7000::createDSP() 
	{
		int x, y;
		getMaxDSPWidths(x, y);
		
		/* create DSP block with constant shift of 17
		* and a maxium unsigned multiplier width (17x17) for this target
		*/
		DSP* dsp_ = new DSP(dspFixedShift_, x, y);
		
		return dsp_;
	}
	
	bool Zynq7000::suggestSubmultSize(int &x, int &y, int wInX, int wInY){
		
		getMaxDSPWidths(x, y);
		
		//	//try the two possible chunk splittings
		//	int score1 = int(ceil((double(wInX)/double(x)))+ceil((double(wInY)/double(y))));
		//	int score2 = int(ceil((double(wInY)/double(x)))+ceil((double(wInX)/double(y))));
		//	
		//	if (score2 < score1)
		//		getMaxDSPWidths(y,x);		
		
		if (wInX <= x)
			x = wInX;
		
		if (wInY <= y)
			y = wInY;
		return true;
	}	 
	
	bool Zynq7000::suggestSubaddSize(int &x, int wIn){
		int chunkSize = 2 + (int)floor( (1./frequency() - (fdCtoQ_ + slice2sliceDelay_ + lut2_ + muxcyStoO_ + xorcyCintoO_ + ffd_)) / muxcyCINtoO_ );
		x = min(chunkSize, wIn);		
		if (x > 0) 
			return true;
		else {
			x = min(2,wIn);		
			return false;
		} 
	};
	
	bool Zynq7000::suggestSlackSubaddSize(int &x, int wIn, double slack){
		int chunkSize =  1 + (int)floor( (1./frequency() - slack - (lut2_ + muxcyStoO_ + xorcyCintoO_)) / muxcyCINtoO_ );
		x = chunkSize;	
		x = min(chunkSize, wIn);	
		if (x > 0) 
			return true;
		else {
			x = min(2,wIn);		
			return false;
		} 
	};
	
//	bool Zynq7000::suggestSubaddSize(int &x, int wIn){
//		
//		int chunkSize = 1 + (int)floor( (1./frequency() - (lut2_ + muxcyStoO_ + xorcyCintoO_)) / muxcyCINtoO_ );
//		x = chunkSize;		
//		if (x > 1) 
//			return true;
//		else {
//			x = 2;		
//			return false;
//		} 
//	};
//	
//	bool Zynq7000::suggestSlackSubaddSize(int &x, int wIn, double slack){
//		
//		int chunkSize = 1 + (int)floor( (1./frequency() - slack - (lut2_ + muxcyStoO_ + xorcyCintoO_)) / muxcyCINtoO_ );
//		x = chunkSize;		
//		if (x > 1) 
//			return true;
//		else {
//			x = 2;		
//			return false;
//		} 
//	};
//	
	bool Zynq7000::suggestSlackSubcomparatorSize(int& x, int wIn, double slack, bool constant)
	{
		bool succes = true;
		
		if (!constant){
			x = (lutInputs_/2)*(((1./frequency() - slack) - (lut2_ + muxcyStoO_))/muxcyCINtoO_ - 1)+1;
		}else{
			x = (lutInputs_/2)*(((1./frequency() - slack) - (lut2_ + muxcyStoO_))/muxcyCINtoO_ - 1)+1;
		}
		if (x<lutInputs_){ //capture possible negative values
			x = lutInputs_;
			succes = false;
		}
		
		if (x> wIn)//saturation 
			x = wIn;
		
		return succes;
	}
	
	int Zynq7000::getIntMultiplierCost(int wInX, int wInY){
		
		int cost = 0;
		int halfLut = lutInputs_/2;
		int cx = int(ceil((double) wInX/halfLut));	// number of chunks on X
		int cy = int(ceil((double) wInY/halfLut));  // number of chunks on Y
		int padX = cx*halfLut - wInX; 				// zero padding of X input
		int padY = cy*halfLut - wInY; 				// zero padding of Y input
		
		if (cx > cy) // set cx as the min and cy as the max
		{
			int tmp = cx;
			cx = cy;
			cy = tmp;
			tmp = padX;
			padX = padY;
			padY = tmp;
		}
		
		float p = (double)cy/(double)halfLut; // number of chunks concatenated per operand
		float r = p - floor(p); // relative error; used for detecting how many operands have ceil(p) chunks concatenated
		int chunkSize, aux;
		suggestSubaddSize(chunkSize, wInX+wInY);
		// int lastChunkSize = (wInX+wInY)%chunkSize;
		// int nr = ceil((double) (wInX+wInY)/chunkSize);
		
		
		if (r == 0.0) // all IntNAdder operands have p concatenated partial products
		{
			aux = halfLut*cx; // number of operands having p concatenated chunks
			
			if (aux <= 4)	
				cost = p*lutInputs_*(aux-2)*(aux-1)/2-(padX*cx*(cx+1)+padY*aux*(aux+1))/2; // registered partial products without zero paddings
			else
				cost = p*lutInputs_*3 + p*lutInputs_*(aux-4)-(padX*(cx+1)+padY*(aux+1)); // registered partial products without zero paddings including SRLs
		}
		else if (r > 0.5) // 2/3 of the IntNAdder operands have p concatenated partial products
			{
			aux = (halfLut-1)*cx; // number of operands having p concatenated chunks
			
			if (halfLut*cx <= 4)
				cost = ceil(p)*lutInputs_*(aux-2)*(aux-1)/2 + floor(p)*lutInputs_*((aux*cx)+(cx-2)*(cx-1)/2);// registered partial products without zero paddings
				else
				{
					if (aux - 4 > 0)
					{
						cost = ceil(p)*lutInputs_*3 - padY - 2*padX + 	// registered partial products without zero paddings
						(ceil(p)*lutInputs_-padY) * (aux-4) + 	// SRLs for long concatenations
						(floor(p)*lutInputs_*cx - cx*padY); 		// SRLs for shorter concatenations
					}	
					else
					{
						cost = ceil(p)*lutInputs_*(aux-2)*(aux-1)/2 + floor(p)*lutInputs_*((aux*cx)+(cx+aux-6)*(cx+aux-5)/2); // registered partial products without zero paddings
						cost += (floor(p)*lutInputs_-padY) * (aux+cx-4); // SRLs for shorter concatenations
					}
				}
		}	
		else if (r > 0) // 1/3 of the IntNAdder operands have p concatenated partial products
		{
			aux = (halfLut-1)*cx; // number of operands having p concatenated chunks
			
			if (halfLut*cx <= 4)
				cost = ceil(p)*lutInputs_*(cx-2)*(cx-1)/2 + floor(p)*lutInputs_*((aux*cx)+(aux-2)*(aux-1)/2);// registered partial products without zero paddings
				else
				{
					cost = ceil(p)*lutInputs_*(cx-2)*(cx-1)/2 + floor(p)*lutInputs_*((aux*cx)+(aux-2)*(aux-1)/2);// registered partial products without zero paddings
					if (cx - 4 > 0)
					{
						cost = ceil(p)*lutInputs_*3 - padY - 2*padX; // registered partial products without zero paddings
						cost += ceil(p)*lutInputs_*(cx-4) - (cx-4)*padY; // SRLs for long concatenations
						cost += floor(p)*lutInputs_*aux - aux*padY; // SRLs for shorter concatenations
					}	
					else
					{
						cost = ceil(p)*lutInputs_*(cx-2)*(cx-1)/2 + floor(p)*lutInputs_*((aux*cx)+(cx+aux-6)*(cx+aux-5)/2); // registered partial products without zero paddings
						cost += (floor(p)*lutInputs_-padY) * (aux+cx-4); // SRLs for shorter concatenations
					}
				}
		}
		aux = halfLut*cx;
		cost += p*lutInputs_*aux + halfLut*(aux-1)*aux/2; // registered addition results on each pipeline stage of the IntNAdder
		
		if (padX+padY > 0)
			cost += (cx-1)*(cy-1)*lutInputs_ + cx*(lutInputs_-padY) + cy*(lutInputs_-padX);
		else
			cost += cx*cy*lutInputs_; // LUT cost for small multiplications 
			
		return cost*5/8;
	};
	
	
	int Zynq7000::getEquivalenceSliceDSP(){
		int lutCost = 0;
		int x, y;
		getMaxDSPWidths(x,y);
		// add multiplier cost
		lutCost += getIntMultiplierCost(x, y);
		// add shifter and accumulator cost
		//lutCost += accumulatorLUTCost(x, y);
		return lutCost;
	}
	
	int Zynq7000::getNumberOfDSPs() 
	{
		return nrDSPs_; 		
	};
	
	int Zynq7000::getIntNAdderCost(int wIn, int n)
	{
		int chunkSize, lastChunkSize, nr, a, b, cost;
		
		suggestSubaddSize(chunkSize, wIn);
		lastChunkSize = wIn%chunkSize;
		nr = ceil((double) wIn/chunkSize);
		// IntAdder
		a = (nr-1)*nr*chunkSize/2 + (nr-1)*lastChunkSize;
		b = nr*lastChunkSize + (nr-1)*nr*chunkSize/2;
		
		if (nr > 2) // carry
		{
			a += (nr-1)*(nr-2)/2;
		}
		
		if (nr == 3) // SRL16E
		{
			a += chunkSize;	
		} 
		else if (nr > 3) // SRL16E, FDE
		{
			a += (nr-3)*chunkSize + 1;
			b += (nr-3)*chunkSize + 1;
		}
		// IntNAdder
		if (n >= 3)
		{
			a += (2*n-4)*wIn + 1;
			b += (2*n-5)*wIn;
		}
		
		cost = a+b*0.25;
		return cost;
	}
	
	void Zynq7000::delayForDSP(MultiplierBlock* multBlock, double currentCp, int& cycleDelay, double& cpDelay)
	{
		double targetPeriod, totalPeriod;
		
		targetPeriod = 1.0/frequency();
		totalPeriod = currentCp + DSPMultiplierDelay_;
		
		cycleDelay = floor(totalPeriod/targetPeriod);
		cpDelay = totalPeriod-targetPeriod*cycleDelay;
	}


}
