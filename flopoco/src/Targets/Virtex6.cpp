/*
  A model of Virtex-6 FPGA (FIXME exact part)

  Author : Bogdan Pasca

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2011.
  All rights reserved.
*/

#include "Virtex6.hpp"
#include <iostream>
#include <sstream>
#include "../utils.hpp"


namespace flopoco{


	/** The default constructor. */
	Virtex6::Virtex6() : Target()	{
			id_             		= "Virtex6";
			vendor_         		= "Xilinx";
			possibleDSPConfig_.push_back(make_pair(25,18));
			whichDSPCongfigCanBeUnsigned_.push_back(false);
			sizeOfBlock_ 			= 36864;	// the size of a primitive block is 2^11 * 18 (36Kb, can be used as 2 independent 2^11*9)
			maxFrequencyMHz_		= 500;
			// all these values are set more or less randomly, to match  virtex 6 more or less
			fastcarryDelay_ 		= 0.015e-9; //s
			elemWireDelay_  		= 0.313e-9; // Was 0.313, but more recent versions of ISE report variable delays around 0.400
			lutDelay_       		= 0.053e-9;
			// all these values are set precisely to match the Virtex6
			fdCtoQ_         		= 0.280e-9;
			lut2_           		= 0.053e-9;	//the gate delay, without the NET delay (~0.279e-9)
			lut3_           		= 0.053e-9;
			lut4_           		= 0.053e-9;
			lut5_           		= 0.053e-9;
			lut6_           		= 0.053e-9;
			lut_net_                = 0.279e-9;
			muxcyStoO_      		= 0.219e-9;
			muxcyCINtoO_    		= 0.015e-9;
			ffd_            		= -0.012e-9;
			ff_net_            		= 0.604e-9;
			muxf5_          		= 0.291e-9;
			muxf7_          		= 0.187e-9; //without the NET delay (~0.357e-9)
			muxf7_net_         		= 0.357e-9;
			muxf8_          		= 0.131e-9; //without the NET delay (~0.481e-9)
			muxf8_net_         		= 0.481e-9;
			muxf_net_         		= 0.279e-9;
			slice2sliceDelay_   	= 0.393e-9;
			xorcyCintoO_    		= 0.180e-9;
			xorcyCintoO_net_ 		= 0.604e-9;

			lutInputs_ 				= 6;
			nrDSPs_ 				= 160;
			dspFixedShift_ 			= 17;

			DSPMultiplierDelay_		= 1.638e-9;
			DSPAdderDelay_			= 1.769e-9;
			DSPCascadingWireDelay_	= 0.365e-9;
			DSPToLogicWireDelay_	= 0.436e-9;

			RAMDelay_				= 1.591e-9; //TODO
			RAMToLogicWireDelay_	= 0.279e-9; //TODO
			logicWireToRAMDelay_	= 0.361e-9; //TODO

			//---------------Floorplanning related----------------------
			multiplierPosition.push_back(15);
			multiplierPosition.push_back(47);
			multiplierPosition.push_back(55);
			multiplierPosition.push_back(107);
			multiplierPosition.push_back(115);
			multiplierPosition.push_back(147);

			memoryPosition.push_back(7);
			memoryPosition.push_back(19);
			memoryPosition.push_back(27);
			memoryPosition.push_back(43);
			memoryPosition.push_back(59);
			memoryPosition.push_back(103);
			memoryPosition.push_back(119);
			memoryPosition.push_back(135);
			memoryPosition.push_back(143);
			memoryPosition.push_back(155);
			memoryPosition.push_back(169);

			topSliceX = 169;
			topSliceY = 359;

			lutPerSlice = 4;
			ffPerSlice = 8;

			dspHeightInLUT = 3;		//3, actually
			ramHeightInLUT = 5;

			dspPerColumn = 143;
			ramPerColumn = 71;
			//----------------------------------------------------------

		}

	
	double Virtex6::logicDelay(int inputs){
		double delay;
		double unitDelay = lutDelay_ + elemWireDelay_;
		if(inputs <= lutInputs())
			delay = unitDelay;
		else if (inputs==lutInputs()+1) { // use mux
			delay=  muxf5_ + unitDelay; // TODO this branch never checked against reality
		}
		else if (inputs==lutInputs()+2) { // use mux
			delay=  muxf5_ + muxf7_ + muxf7_net_ + unitDelay; // TODO this branch never checked against reality
		}
		else
			delay= unitDelay * (inputs -lutInputs() + 1);
		TARGETREPORT("logicDelay(" << inputs << ") = " << delay*1e9 << " ns.");
		return delay;
	}
	
	
	double Virtex6::adderDelay(int size, bool addRoutingDelay) {
		double delay=lut2_ + muxcyStoO_ + double(size-1)*muxcyCINtoO_ + xorcyCintoO_ + elemWireDelay_;
		TARGETREPORT("adderDelay(" << size << ") = " << delay*1e9 << " ns.");
		return delay ;
	};

	

	double Virtex6::eqComparatorDelay(int size){
		double delay;
#if 0
		delay = lut2_ + muxcyStoO_ + double((size-1)/(lutInputs_/2)+1)*muxcyCINtoO_;
#else
		delay= adderDelay(size);
#endif
		TARGETREPORT("eqComparatorDelay(" << size << ") = " << delay*1e9 << " ns.");
		return delay;
	}

	double Virtex6::eqConstComparatorDelay(int size){
		double delay=  lut2_ + muxcyStoO_ + double((size-1)/lutInputs_+1)*muxcyCINtoO_  + xorcyCintoO_ + xorcyCintoO_net_;
		TARGETREPORT("eqConstComparatorDelay(" << size << ") = " << delay*1e9 << " ns.");
		return delay;
	}
	double Virtex6::ffDelay() {
		return fdCtoQ_ + ffd_; // removed ff_net_ 
	};

	double Virtex6::carryPropagateDelay() {
		return  fastcarryDelay_;
	};

	double Virtex6::fanoutDelay(int fanout){
		// TODO the values used here were found experimentally using Planahead 14.7
		double delay;
#if 0 // All this commented out by Florent to better match ISE critical path report
		// Then plugged back in because it really depends.
		if(fanout <= 16)
			delay =  elemWireDelay_ + (double(fanout) * 0.063e-9);
		else if(fanout <= 32)
			delay =  elemWireDelay_ + (double(fanout) * 0.035e-9);
		else if(fanout <= 64)
			delay =  elemWireDelay_ + (double(fanout) * 0.030e-9);
		else if(fanout <= 128)
			delay =  elemWireDelay_ + (double(fanout) * 0.017e-9);
		else if(fanout <= 256)
			delay =  elemWireDelay_ + (double(fanout) * 0.007e-9);
		else if(fanout <= 512)
			delay =  elemWireDelay_ + (double(fanout) * 0.004e-9);
		else
			delay =  elemWireDelay_ + (double(fanout) * 0.002e-9);
		delay/=2;
#else
			delay =  elemWireDelay_ + (double(fanout) * 0.001e-9);
#endif
		//cout << "localWireDelay(" << fanout << ") estimated to "<< delay*1e9 << "ns" << endl;
		TARGETREPORT("fanoutDelay(" << fanout << ") = " << delay*1e9 << " ns.");
		return delay;
	};

	double Virtex6::lutDelay(){
		return lutDelay_ ;
	};

	double Virtex6::tableDelay(int wIn_, int wOut_, bool logicTable_){
		double totalDelay = 0.0;
		int i;

		if(logicTable_)
		{
			if(wIn_ <= lutInputs_)
				//table fits inside a LUT
				totalDelay = fanoutDelay(wOut_) + lut6_ + lut_net_;
			else if(wIn_ <= lutInputs_+2){
				//table fits inside a slice
				double delays[] = {lut6_, muxf7_, muxf8_};

				totalDelay = fanoutDelay(wOut_*(int)intpow2(wIn_-lutInputs_));
				for(i=lutInputs_; i<=wIn_; i++){
					totalDelay += delays[i-lutInputs_];
				}
				totalDelay += muxf_net_;
			}else{
				//table requires resources from multiple slices
				double delays[] = {lut6_, 0, muxf7_};
				double delaysNet[] = {lut_net_, lut_net_, muxf7_net_};

				totalDelay = fanoutDelay(wOut_*(int)intpow2(wIn_-lutInputs_)) + lut6_ + muxf7_ + muxf8_ + muxf8_net_;
				for(i=lutInputs_+3; i<=wIn_; i++){
					totalDelay += delays[(i-lutInputs_)%(sizeof(delays)/sizeof(*delays))];
				}
				i--;
				totalDelay += delaysNet[(i-lutInputs_)%(sizeof(delays)/sizeof(*delays))];
			}
		}else
		{
			totalDelay = RAMDelay_ + RAMToLogicWireDelay_;
		}

		return totalDelay;
	}

	long Virtex6::sizeOfMemoryBlock()
	{
		return sizeOfBlock_;
	};


	DSP* Virtex6::createDSP()
	{
		int x, y;
		getMaxDSPWidths(x, y);

		/* create DSP block with constant shift of 17
		* and a maxium unsigned multiplier width (17x17) for this target
		*/
		DSP* dsp_ = new DSP(dspFixedShift_, x, y);

		return dsp_;
	};

	bool Virtex6::suggestSubmultSize(int &x, int &y, int wInX, int wInY){

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

	bool Virtex6::suggestSubaddSize(int &x, int wIn){
		int chunkSize = 2 + (int)floor( (1./frequency() - (fdCtoQ_ + slice2sliceDelay_ + lut2_ + muxcyStoO_ + xorcyCintoO_ + ffd_)) / muxcyCINtoO_ );
		x = min(chunkSize, wIn);
		if (x > 0)
			return true;
		else {
			x = min(2,wIn);
			return false;
		}
	};

	bool Virtex6::suggestSlackSubaddSize(int &x, int wIn, double slack){
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

//	bool Virtex6::suggestSubaddSize(int &x, int wIn){
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
//	bool Virtex6::suggestSlackSubaddSize(int &x, int wIn, double slack){
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
	bool Virtex6::suggestSlackSubcomparatorSize(int& x, int wIn, double slack, bool constant)
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

	int Virtex6::getIntMultiplierCost(int wInX, int wInY){

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


	int Virtex6::getEquivalenceSliceDSP(){
		int lutCost = 0;
		int x, y;
		getMaxDSPWidths(x,y);
		// add multiplier cost
		lutCost += getIntMultiplierCost(x, y);
		// add shifter and accumulator cost
		//lutCost += accumulatorLUTCost(x, y);
		return lutCost;
	}

	int Virtex6::getNumberOfDSPs()
	{
		return nrDSPs_;
	};

	int Virtex6::getIntNAdderCost(int wIn, int n)
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

	double Virtex6::lutConsumption(int lutInputSize)
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

	void Virtex6::delayForDSP(MultiplierBlock* multBlock, double currentCp, int& cycleDelay, double& cpDelay)
	{
		double targetPeriod, totalPeriod;

		targetPeriod = 1.0/frequency();
		totalPeriod = currentCp + DSPMultiplierDelay_;

		cycleDelay = floor(totalPeriod/targetPeriod);
		cpDelay = totalPeriod-targetPeriod*cycleDelay;
	}
}
