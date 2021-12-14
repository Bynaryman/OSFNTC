
/*
   A multiply-and-add in a single bit heap

Author:  Florent de Dinechin

This file is part of the FloPoCo project
developed by the Arenaire team at Ecole Normale Superieure de Lyon

Initial software.
Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
2012-2013.
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
#include "utils.hpp"
#include "Operator.hpp"
#include "FixMultAdd.hpp"
#include "IntMultiplier.hpp"

using namespace std;

namespace flopoco {
#if 0
#define vhdl parentOp->vhdl
#define declare parentOp->declare
#define inPortMap parentOp->inPortMap
#define outPortMap parentOp->outPortMap
#define instance parentOp->instance
#define manageCriticalPath parentOp->manageCriticalPath
#define getCriticalPath parentOp->getCriticalPath
#define setCycle parentOp->setCycle
#define oplist parentOp->getOpListR()
#endif



#if 0
 	// The virtual constructor TODO
	FixMultAdd::FixMultAdd (Operator* parentOp_, BitHeap* bitHeap_, Signal* x_, Signal* y_, int wX_, 
			int wY_, int wOut_, int lsbWeight_, bool negate_, bool signedIO_, float ratio_):
		Operator ( parentOp_->getTarget()), 
		wxDSP(0), wyDSP(0), wXdecl(wX_), wYdecl(wY_), wX(0), wY(0), wOut(wOut_), ratio(ratio_),  maxError(0.0), 
		parentOp(parentOp_), bitHeap(bitHeap_), lsbWeight(lsbWeight_),
		x(x_), y(y_), negate(negate_), signedIO(signedIO_) 
	{

		isOperator=false;
		// leave the compression to the parent op
	}

#endif




#define USEFIXIO 1

	// The constructor for a stand-alone operator, see the .hpp for parameter explanation
	FixMultAdd::FixMultAdd(Target* target, Signal* x_, Signal* y_, Signal* a_, int outMSB_, int outLSB_,
												 float ratio_, bool enableSuperTiles_, map<string, double> inputDelays_):
		Operator ( target, inputDelays_ ),
		x(x_), y(y_), a(a_),
		// wX(wX_), wY(wY_), wA(wA_), 
		wOut(outMSB_-outLSB_+1),
		outMSB(outMSB_), 
		outLSB(outLSB_), 
		ratio(ratio_), 
		enableSuperTiles(enableSuperTiles_) 
{

		srcFileName="FixMultAdd";
		setCopyrightString ( "Florent de Dinechin, 2012-2014" );

		signedIO = (x->isSigned() || y->isSigned());
		// TODO manage the case when one is signed and not the other.
		{
			ostringstream name;
			name <<"FixMultAdd_";
			// name << wX << "x" << wY << "p" << wA << "r" << wOut << "" << (signedIO?"s":"u");
			name << Operator::getNewUId();
			setNameWithFreqAndUID ( name.str() );
			REPORT(DEBUG, "Building " << name.str() );
		}

		// Set up the VHDL library style
		useNumericStd();

		wX = x->MSB() - x->LSB() +1;
		wY = y->MSB() - y->LSB() +1;
		wA = a->MSB() - a->LSB() +1;
		
		// Set up the IO signals
		xname="X";
		yname="Y";
		aname="A";
		rname="R";

#if USEFIXIO
		addFixInput ( xname, x->isSigned(), x->MSB(), x->LSB() );
		addFixInput ( yname, y->isSigned(), y->MSB(), y->LSB() );
		addFixInput ( aname, a->isSigned(), a->MSB(), a->LSB() );
		addFixOutput (rname, signedIO, outMSB, outLSB, possibleOutputs);
#else
		addInput ( xname,  wX);
		addInput ( yname,  wY);
		addInput ( aname,  wA);
		addOutput ( "R",  wOut, possibleOutputs);
		// Build internal fix-point signals
		vhdl << tab << declareFixPoint("iX", x->isSigned(), x->MSB(), x->LSB()) << " <= " << (x->isSigned()? "signed":"unsigned" ) << "(X);" << endl;
		vhdl << tab << declareFixPoint("iY", x->isSigned(), y->MSB(), y->LSB()) << " <= " << (x->isSigned()? "signed":"unsigned" ) << "(Y);" << endl;
		vhdl << tab << declareFixPoint("iA", x->isSigned(), a->MSB(), a->LSB()) << " <= " << (x->isSigned()? "signed":"unsigned" ) << "(A);" << endl;

#endif

		// Write the exact product
		int pMSB = x->MSB() + y->MSB() + 1;
		int pLSB = x->LSB() + y->LSB();

#if USEFIXIO
		vhdl << tab << declareFixPoint("P", signedIO, pMSB, pLSB)   << " <= X * Y;" << endl;
		resizeFixPoint("Aresized", "A", outMSB, outLSB);
#else
		vhdl << tab << declareFixPoint("P", signedIO, pMSB, pLSB)   << " <= iX * iY;" << endl;
		resizeFixPoint("Aresized", "iA", outMSB, outLSB);
#endif
		resizeFixPoint("Presized", "P", outMSB, outLSB);
			
		vhdl << tab << declareFixPoint("iR", signedIO,  outMSB, outLSB) << " <= Aresized + Presized;" << endl; 
#if USEFIXIO
		vhdl << tab << "R <= iR;" << endl; 
#else
		vhdl << tab << "R <= std_logic_vector(iR);" << endl; 
#endif



#if 0
		parentOp=this;

		// TODO the following assumes that lsbA is positive
		if(lsbA<0){
			THROWERROR("lsbA<0: this is currently unsupported, if you ask nicely we could fix it")
		}

		if(lsbPfull >= 0) { // no truncation needed, we can add all the bits of the product 
			wOutP = wX+wY; // the parameter we'll pass to IntMultiplier
			g=0;
			possibleOutputs=1; // No faithful rounding
			REPORT(DETAILED, " Exact architecture" )
		}
		else { // there is a truncation of the product
			// we will add g guard bits to the bit heap
			wOutP=msbP;
			g = IntMultiplier::neededGuardBits(wX, wY, wOutP); 
			possibleOutputs=2; // No faithful rounding
			REPORT(DETAILED, " Faithfully rounded architecture" )
		}

		// The bit heap
		bitHeap = new BitHeap(this, wOut+g, enableSuperTiles);


		// TODO should be a parameter to the bit heap constructor
		bitHeap->setSignedIO(signedIO);


		// initialize the critical path
		setCriticalPath(getMaxInputDelays ( inputDelays_ ));

		// TODO if it fits in a DSP block just write A*B+C
		
		fillBitHeap();

		bitHeap -> generateCompressorVHDL();			
		vhdl << tab << "R" << " <= " << bitHeap-> getSumName() << range(wOut+g-1, g) << ";" << endl;

#endif
	}




	FixMultAdd::~FixMultAdd() {
		if(mult) 
			free(mult);
	}


	FixMultAdd* FixMultAdd::newComponentAndInstance(Operator* op,
																									string instanceName,
																									string xSignalName,
																									string ySignalName,
																									string aSignalName,
																									string rSignalName,
																									int rMSB, 
																									int rLSB
																									) {
		FixMultAdd* f = new FixMultAdd(op->getTarget(), 
																	 op->getSignalByName(xSignalName), 
																	 op->getSignalByName(ySignalName), 
																	 op->getSignalByName(aSignalName), 
																	 rMSB, rLSB);   
		op->addSubComponent(f);
		op->inPortMap(f, "X", xSignalName);
		op->inPortMap(f, "Y", ySignalName);
		op->inPortMap(f, "A", aSignalName);
#if USEFIXIO
		op->outPortMap(f, "R", rSignalName);
		op->vhdl << op->instance(f, instanceName);
		op->getSignalByName(rSignalName)->promoteToFix(f->signedIO, rMSB, rLSB);
#else
		op->outPortMap(f, "R", join(rSignalName, "_slv"));
		op->vhdl << op->instance(f, instanceName);
		op->vhdl << tab << op->declareFixPoint(rSignalName,f->signedIO, rMSB, rLSB) << " <= " <<  "signed(" << (join(rSignalName, "_slv")) << ");" << endl;
#endif
		return f;
	}




	void FixMultAdd::fillBitHeap() {
		//  throw the addend to the bit heap
#if 0
		bitHeap -> addSignedBitVector(lsbA+g, aname, wA);
		
		//  throw the product to the bit heap
		int lsbWeight = msbP+1 - wOutP;
		// TODO we could read wX and wY from the signal.
		mult = new IntMultiplier (this, bitHeap, getSignalByName("X"), getSignalByName("Y"), wX, wY, wOutP, lsbWeight, false /*negate*/, signedIO, ratio);
#endif
	}




	void FixMultAdd::emulate ( TestCase* tc ) {
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");
		mpz_class svA = tc->getInputValue("A");
		mpz_class svP, svR;
		mpz_class twoToWR = (mpz_class(1) << (wOut));
		mpz_class twoToWRm1 = (mpz_class(1) << (wOut-1));

		if (! signedIO){
			svP = svX * svY;
			svR = svA<<lsbA;
			//align the product
			if(lsbPfull>=0)
				svR += svP << lsbPfull;
			else 
				svR += (svP >>(-lsbPfull));

			tc->addExpectedOutput("R", svR);
			if(possibleOutputs==2) {
				svR++;
				svR &= (twoToWR -1);
				tc->addExpectedOutput("R", svR);
			}
		}

		else{ // Manage signed digits
			mpz_class twoToWX = (mpz_class(1) << (wX));
			mpz_class twoToWXm1 = (mpz_class(1) << (wX-1));
			mpz_class twoToWY = (mpz_class(1) << (wY));
			mpz_class twoToWYm1 = (mpz_class(1) << (wY-1));
			mpz_class twoToWA = (mpz_class(1) << (wA));
			mpz_class twoToWAm1 = (mpz_class(1) << (wA-1));

			if (svX >= twoToWXm1)
				svX -= twoToWX;

			if (svY >= twoToWYm1)
				svY -= twoToWY;

			if (svA >= twoToWAm1)
				svA -= twoToWA;

			svP = svX * svY; //signed
			if(lsbPfull>=0) { // no truncation
				svR = (svA << lsbA) + (svP << lsbPfull); // signed
				// manage two's complement at output
				if ( svR < 0)
					svR += twoToWR; 
				tc->addExpectedOutput("R", svR);
				return;
			}
			else {//lsbPfull<0
				int shift=-lsbPfull;
				// fully accurate result, product-anchored
				svR = (svA<<(lsbA + shift)) + svP; 
				// manage its two's complement
				if ( svR < 0)
					svR += (mpz_class(1) << (wOut+shift));
				// shift back to place: truncation
				svR = svR >> shift;
				tc->addExpectedOutput("R", svR); // this was rounded down
				svR++;
				svR &= (twoToWR -1);
				tc->addExpectedOutput("R", svR);
			}
		}
	}



	void FixMultAdd::buildStandardTestCases(TestCaseList* tcl)
	{
#if 0
		TestCase *tc;

		mpz_class x, y;

		// 1*1
		x = mpz_class(1); 
		y = mpz_class(1); 
		tc = new TestCase(this); 
		tc->addInput("X", x);
		tc->addInput("Y", y);
		emulate(tc);
		tcl->add(tc);

		// -1 * -1
		x = (mpz_class(1) << wXdecl) -1; 
		y = (mpz_class(1) << wYdecl) -1; 
		tc = new TestCase(this); 
		tc->addInput("X", x);
		tc->addInput("Y", y);
		emulate(tc);
		tcl->add(tc);

		// The product of the two max negative values overflows the signed multiplier
		x = mpz_class(1) << (wXdecl -1); 
		y = mpz_class(1) << (wYdecl -1); 
		tc = new TestCase(this); 
		tc->addInput("X", x);
		tc->addInput("Y", y);
		emulate(tc);
		tcl->add(tc);
#endif
	}




}
