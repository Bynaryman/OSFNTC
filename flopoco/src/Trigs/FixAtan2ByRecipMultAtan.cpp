
#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"

#include "FixAtan2ByRecipMultAtan.hpp"
#include "IntMult/IntMultiplier.hpp"
#include "IntAddSubCmp/IntAdder.hpp"
#include "ConstMult/FixRealKCM.hpp"
#include "ShiftersEtc/LZOC.hpp"
#include "ShiftersEtc/Shifters.hpp"
#include "FixFunctions/FixFunctionByPiecewisePoly.hpp"
#include "FixFunctions/BipartiteTable.hpp"


using namespace std;

/* TODO Debugging:
There are still a few last-bit errors with the current setup
One is unavoidable, it is atan2 (0,0)
   TODO add a don't care to the test framework?
*/
namespace flopoco{
	// an an option for outputting the norm of the vector as well (scaled or not)



	FixAtan2ByRecipMultAtan::FixAtan2ByRecipMultAtan(Target* target_, int wIn_, int wOut_, int degree, map<string, double> inputDelays_) :
		FixAtan2(target_, wIn_, wOut_, inputDelays_)
	{
		//int stage;
		srcFileName="FixAtan2ByRecipMultAtan";
		setCopyrightString ( "Matei Istoan, Florent de Dinechin (2012-...)" );
		useNumericStd_Unsigned();

#define MULTDEBUG 1
#if MULTDEBUG
		// The following blob is to remove when the multiplier version works
		// along with a setPlainVHDL near the end
		bool wasPlainVHDL=true;
		if(!getTarget()->plainVHDL()) {
			REPORT(0, "setting plainVHDL to true while we debug it");
			getTarget()->setPlainVHDL(true);
			wasPlainVHDL=false;
		}
#endif
		REPORT(DEBUG, "  degree=" << degree);

		ostringstream name;
		name << "FixAtan2ByRecipMultAtan_" << wIn_ << "_" << wOut_ << "_uid" << getNewUId();
		setNameWithFreq( name.str() );

		///////////// VHDL GENERATION

		// Range reduction code is shared, defined in FixAtan2
		buildQuadrantRangeReduction();
		buildScalingRangeReduction();

		//int sizeZ=wOut-2; // w-2 because two bits come from arg red

		//FixFunctionByPiecewisePoly* recipTable;
		Operator* recipTable;
		//FixFunctionByPiecewisePoly* atanTable;
		Operator* atanTable;
		int msbRecip, lsbRecip, msbProduct, lsbProduct, msbAtan, lsbAtan;
		msbAtan = -2; // bits 0 and -1 come from the range reduction
		lsbAtan = -wOut+1;
		msbRecip = 0; // 2/(1+x) in 0..1 for x in 0..1
		msbProduct = -1 ; // y/x between 0 and 1 but the faithful product may overflow a bit.
		if(degree==0) { // both tables are correctly rounded
			lsbRecip = -wOut+1; // see error analysis in the Arith2015 paper. It says we should have -w, but exhaustive test show that -w+1 work :)
			lsbProduct = -wOut+1; // It says we should have -w, but exhaustive test show that -w+1 work :)
		}
		else{ // both tables are faithful
			lsbRecip = -wOut; // see error analysis in the paper
			lsbProduct = -wOut;
		}
		// recip table computes 2/(1+x) once we have killed the MSB of XRS, which is always 1.
		vhdl << tab << declare("XRm1", wIn-2) << " <= XRS" << range(wIn-3,0)  << "; -- removing the MSB which is constantly 1" << endl;
		ostringstream invfun;
		invfun << "2/(1+x)-1b"<<lsbRecip;
		if(degree==1) {
		//BipartiteTable *deltaX2Table
		recipTable = new BipartiteTable(getTarget(),
			invfun.str(),
			-wOut+2,  // XRS was between 1/2 and 1. XRm1 is between 0 and 1/2
			msbRecip + 1, // +1 because internally uses signed arithmetic and we want an unsigned result
			lsbRecip);
	}
		else {
		recipTable = new FixFunctionByPiecewisePoly(getTarget(),
			invfun.str(),
			-wOut+2,  // XRS was between 1/2 and 1. XRm1 is between 0 and 1/2
			msbRecip + 1, // +1 because internally uses signed arithmetic and we want an unsigned result
			lsbRecip,
			degree,
			true /*finalRounding*/
			);
	}
		recipTable->changeName(join("reciprocal_uid", getNewUId()));
		addSubComponent(recipTable);
		inPortMap(recipTable, "X", "XRm1");
		outPortMap(recipTable, "Y", "R0");
		vhdl << instance(recipTable, "recipTable");

		syncCycleFromSignal("R0");

		vhdl << tab << declareFixPoint("R", false, msbRecip, lsbRecip) << " <= unsigned(R0" << range(msbRecip-lsbRecip  , 0) << "); -- removing the sign  bit" << endl;
		vhdl << tab << declareFixPoint("YRU", false, -1, -wOut+1) << " <= unsigned(YRS);" << endl;

		if(getTarget()->plainVHDL()) { // generate a "*"
		manageCriticalPath(getTarget()->DSPMultiplierDelay());
		vhdl << tab << declareFixPoint("P", false, msbRecip -1 +1, lsbRecip-wOut+1) << " <= R*YRU;" << endl;
		resizeFixPoint("PtruncU", "P", msbProduct, lsbProduct);
		vhdl << tab << declare("P_slv", msbProduct-lsbProduct+1)  << " <=  std_logic_vector(PtruncU);" << endl;
	}
		else{ // generate an IntMultiplier
		manageCriticalPath(getTarget()->DSPMultiplierDelay()); // This should be replaced with a method of IntMultiplier or something
		IntMultiplier::newComponentAndInstance(this,
			"divMult",     // instance name
			"R",  // x
			"YRU", // y
			"P",       // p
			msbProduct, lsbProduct
			);
	}


		ostringstream atanfun;
		atanfun << "atan(x)/pi";
		if(degree==1) {
		atanTable = new BipartiteTable(getTarget(),
			atanfun.str(),
			lsbProduct,
			msbAtan,
			lsbAtan);
	}
		else {
		atanTable  = new FixFunctionByPiecewisePoly(getTarget(),
			atanfun.str(),
			lsbProduct,
			msbAtan,
			lsbAtan,
			degree,
			true /*finalRounding*/
			);
	}
		atanTable->changeName(join("atan_uid", getNewUId()));
		addSubComponent(atanTable);
		inPortMap(atanTable, "X", "P_slv");
		outPortMap(atanTable, "Y", "atanTableOut");
		vhdl << instance(atanTable, "atanTable");
		syncCycleFromSignal("atanTableOut");

		vhdl << tab << declare("finalZ", wOut) << " <= \"00\" & atanTableOut;" << endl;

		// Reconstruction code is shared, defined in FixAtan2
		buildQuadrantReconstruction();

#if MULTDEBUG
		getTarget()->setPlainVHDL(wasPlainVHDL);
#endif
	};


	FixAtan2ByRecipMultAtan::~FixAtan2ByRecipMultAtan(){
	};






}

