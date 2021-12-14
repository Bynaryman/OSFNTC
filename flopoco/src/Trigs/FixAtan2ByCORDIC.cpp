
#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"

#include "FixAtan2ByCORDIC.hpp"
#include "IntAddSubCmp/IntAdder.hpp"
#include "ConstMult/FixRealKCM.hpp"
#include "ShiftersEtc/LZOC.hpp"
#include "ShiftersEtc/Shifters.hpp"
#include "FixFunctions/FixFunctionByPiecewisePoly.hpp"
#include "FixFunctions/BipartiteTable.hpp"


using namespace std;
// TODO reduce the CORDIC datapaths (Y with leading 0s and X with leading ones)
// Or write an exact version of the XY datapath

/* TODO Debugging:
There are still a few last-bit errors with the current setup in
./flopoco -verbose=3 -pipeline=no FixAtan2ByCORDIC 8 TestBenchFile -2


They are all for vectors of small norm 

One is unavoidable, it is atan2 (0,0)
   TODO add a don't care to the test framework?
*/
namespace flopoco{

	// TODO Options:
	// an option for outputting the norm of the vector as well (scaled or not)


	FixAtan2ByCORDIC::FixAtan2ByCORDIC(Target* target_, int wIn_, int wOut_, map<string, double> inputDelays_) :
 		FixAtan2(target_, wIn_, wOut_, inputDelays_)
	{
		int stage;
		srcFileName="FixAtan2ByCORDIC";
		setCopyrightString ( "Matei Istoan, Florent de Dinechin (2012-...)" );
		useNumericStd_Unsigned();

		ostringstream name;
		name << "FixAtan2ByCORDIC_" << wIn_ << "_" << wOut_ << "_uid" << getNewUId();
		setNameWithFreq( name.str() );
	
		mpfr_t  zatan; 
		mpfr_init2(zatan, 10*wOut);
	
		computeGuardBits();

		//Defining the various parameters according to method


		///////////// VHDL GENERATION
	
		/////////////////////////////////////////////////////////////////////////////
		// 
		//    First range reduction
		//
		/////////////////////////////////////////////////////////////////////////////

		buildQuadrantRangeReduction();

		// No scaling RR: just a copy
		vhdl << tab << declare("XRS", wIn-1) << " <=  XR;" << endl;
		vhdl << tab << declare("YRS", wIn-1) << " <=  YR;" << endl;
		
		int sizeZ=wOut-2+gA; // w-2 because two bits come from arg red 

		////////////////////////////////////////////////////////////////////////////
		// 
		//   	 CORDIC iterations	
		//
		////////////////////////////////////////////////////////////////////////////
		
		// Fixed-point considerations:
		// Y -> 0 and X -> K.sqrt(x1^2+y1^2)
		// Max value attained by X is sqrt(2)*K which is smaller than 2
		
		int zMSB=-1;      // -1 because these two bits have weight 0 and -1, but we must keep the sign
		int zLSB = zMSB-sizeZ+1;
		int sizeX = wIn+gXY;
		int sizeY = sizeX;
		
		
		vhdl << tab << declare("X1", sizeX) << " <= '0' & XRS & " << zg(sizeX-(wIn-1)-1) << ";" <<endl;			
		vhdl << tab << declare("Y1", sizeY) << " <= '0' & YRS & " << zg(sizeY-(wIn-1)-1) << ";" <<endl;			
		stage=1; 
 
		vhdl << tab << "--- Iteration " << stage << " : sign is known positive ---" << endl;
		vhdl << tab << declare(join("YShift", stage), sizeX) << " <= " << rangeAssign(sizeX-1, sizeX-stage, "'0'") << " & Y" << stage << range(sizeX-1, stage) << ";" << endl;
		vhdl << tab << declare(getTarget()->adderDelay(sizeX), join("X", stage+1), sizeX) << " <= " 
				 << join("X", stage) << " + " << join("YShift", stage) << " ;" << endl;
		
		vhdl << tab << declare(join("XShift", stage), sizeY) << " <= " << zg(stage) << " & X" << stage << range(sizeY-1, stage) << ";" <<endl;			
		vhdl << tab << declare(getTarget()->adderDelay(sizeY), join("Y", stage+1), sizeY) << " <= " 
				 << join( "Y", stage) << " - " << join("XShift", stage) << " ;" << endl;
		

		//create the constant signal for the arctan
		mpfr_set_d(zatan, 1.0, GMP_RNDN);
		mpfr_div_2si(zatan, zatan, stage, GMP_RNDN);
		mpfr_atan(zatan, zatan, GMP_RNDN);
		mpfr_div(zatan, zatan, constPi, GMP_RNDN);
		mpfr_t roundbit;
		mpfr_init2(roundbit, 30); // should be enough for anybody
		mpfr_set_d(roundbit, 1.0, GMP_RNDN);
		mpfr_div_2si(roundbit, roundbit, wOut, GMP_RNDN); // roundbit is in position 2^-wOut
		
		REPORT(DEBUG, "stage=" << stage << "  atancst=" << printMPFR(zatan));
		
		mpfr_add(zatan, zatan, roundbit, GMP_RNDN);
		vhdl << tab << declare("Z2", sizeZ) << " <= " << unsignedFixPointNumber(zatan, zMSB, zLSB) << "; -- initial atan, plus round bit" <<endl;
		

		for(stage=2; stage<=maxIterations;    stage++, sizeY--){
			// Invariant: sizeX-sizeY = stage-2
			vhdl << tab << "--- Iteration " << stage << " ---" << endl;
			
			vhdl << tab << declare(join("sgnY", stage))  << " <= " <<  join("Y", stage)  <<  of(sizeY-1) << ";" << endl;
			
			if(-2*stage+1 >= -wIn+1-gXY) { 
				vhdl << tab << declare(join("YShift", stage), sizeX) 
						 << " <= " << rangeAssign(sizeX-1, sizeX -(sizeX-sizeY+stage), join("sgnY", stage))   
						 << " & Y" << stage << range(sizeY-1, stage) << ";" << endl;
				
				vhdl << tab << declare(getTarget()->fanoutDelay(sizeX+1) + getTarget()->adderDelay(max(sizeX,sizeZ)),
															 join("X", stage+1), sizeX) << " <= " 
						 << join("X", stage) << " - " << join("YShift", stage) << " when " << join("sgnY", stage) << "=\'1\'     else "
						 << join("X", stage) << " + " << join("YShift", stage) << " ;" << endl;
			}
			else {	// autant pisser dans un violon
				vhdl << tab << declare(join("X", stage+1), sizeX) << " <= " << join("X", stage) << " ;" << endl;
			}
			
			vhdl << tab << declare(join("XShift", stage), sizeY) << " <= " << zg(2) << " & X" << stage << range(sizeX-1, sizeX - sizeY + 2) << ";" <<endl;			
			vhdl << tab << declare(join("YY", stage+1), sizeY) << " <= " 
					 << join("Y", stage) << " + " << join("XShift", stage) << " when " << join("sgnY", stage) << "=\'1\'     else "
					 << join("Y", stage) << " - " << join("XShift", stage) << " ;" << endl;
			vhdl << tab << declare(join("Y", stage+1), sizeY-1) << " <= " << join("YY", stage+1) << range(sizeY-2, 0) << ";" <<endl;
			
			
			//create the constant signal for the arctan
			mpfr_set_d(zatan, 1.0, GMP_RNDN);
			mpfr_div_2si(zatan, zatan, stage, GMP_RNDN);
			mpfr_atan(zatan, zatan, GMP_RNDN);
			mpfr_div(zatan, zatan, constPi, GMP_RNDN);
			REPORT(DEBUG, "stage=" << stage << "  atancst=" << printMPFR(zatan));		
			// rounding here in unsignedFixPointNumber()
			vhdl << tab << declare(join("atan2PowStage", stage), sizeZ) << " <= " << unsignedFixPointNumber(zatan, zMSB, zLSB) << ";" <<endl;
			vhdl << tab << declare(join("Z", stage+1), sizeZ) << " <= " 
					 << join("Z", stage) << " + " << join("atan2PowStage", stage) << " when " << join("sgnY", stage) << "=\'0\'      else "
					 << join("Z", stage) << " - " << join("atan2PowStage", stage) << " ;" << endl;
			
		} //end for loop
		
			// Give the time to finish the last rotation
			// manageCriticalPath( getTarget()->localWireDelay(w+1) + getTarget()->adderDelay(w+1) // actual CP delay
			//                     - (getTarget()->localWireDelay(sizeZ+1) + getTarget()->adderDelay(sizeZ+1))); // CP delay that was already added
		
		vhdl << tab << declare(getTarget()->adderDelay(2), "finalZ", wOut) << " <= Z" << stage << of(sizeZ-1) << " & Z" << stage << range(sizeZ-1, sizeZ-wOut+1) << "; -- sign-extended and rounded" << endl;

		buildQuadrantReconstruction();
	};


	FixAtan2ByCORDIC::~FixAtan2ByCORDIC(){
	};




	void FixAtan2ByCORDIC::computeGuardBits(){	
#define ROUNDED_ROTATION 0 // 0:trunc 
	
#if ROUNDED_ROTATION
			REPORT(DEBUG, "Using rounded rotation trick");
#endif

			// ulp = weight of the LSB of the result is 2^(-wOut+1)
			// half-ulp is 2^-wOut
			// 1/pi atan(2^-w) < 1/2. 2^-w therefore after w-1 interations,  method error will be bounded by 2^-w 
			maxIterations = wOut-1;
			//error analysis for the (x,y) datapath
			double eps;  //error in ulp

			if(negateByComplement)
				eps=1; // initial neg-by-not
			else
				eps=0;

			double shift=0.5;
			for(int stage=1; stage<=maxIterations; stage++){
#if ROUNDED_ROTATION
				eps = eps + eps*shift + 0.5; // 0.5 assume rounding in the rotation.
#else
				eps = eps + eps*shift + 1.0; // 1.0 assume truncation in the rotation.
#endif
				shift *=0.5;
			}
			// guard bits depend only on the number of iterations
			gXY = (int) ceil(log2(eps));
			//gXY+=2; // experimental

			//error analysis for the A datapath
			eps = maxIterations*0.5; // only the rounding error in the atan constant
			gA = 1 + (int) ceil(log2(eps)); // +1 for the final rounding 
			//gA+=2; // experimental
			REPORT(DEBUG, "Error analysis computes eps=" << eps << " ulps on the XY datapath, hence  gXY=" << gXY);
			REPORT(DEBUG, "Error analysis computes eps=" << eps <<  " ulps on the A datapath, hence  gA=" << gA );
	} 





}

