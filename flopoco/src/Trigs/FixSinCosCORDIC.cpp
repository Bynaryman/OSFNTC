#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"

#include "FixSinCos.hpp"
#include "FixSinCosCORDIC.hpp"
#include "IntMult/IntMultiplier.hpp"
#include "IntAddSubCmp/IntAdder.hpp"
#include "ConstMult/FixRealKCM.hpp"

using namespace std;

namespace flopoco{


	//The wIn+1 below is for consistency with FixSinCos and FixSinOrCos interfaces.
	// TODO possibly fix all the code instead... This would enable sharing emulate() etc.
 
	FixSinCosCORDIC::FixSinCosCORDIC(OperatorPtr parentOp, Target* target, int wIn_, int wOut_, int reducedIterations_) 
		: FixSinCos(parentOp, target, -(wIn_-1)), reducedIterations(reducedIterations_)
	{

		wIn=-lsb+1; // TO REMOVE
		wOut=-lsb+1;
		
	int stage;
		srcFileName="FixSinCosCORDIC";
		setCopyrightString ( "Matei Istoan, Florent de Dinechin (2012-...)" );

		ostringstream name;
		name << "FixSinCosCORDIC_" << (reducedIterations==1?"reducedIterations":"") << wIn_ << "_" << wOut_;
		setNameWithFreqAndUID( name.str() );


		if(wIn<12){
			REPORT(INFO, "wIn=" << wIn << " is small, are you sure you don't want to tabulate this operator in a ROM?");
		}

		if (reducedIterations == 1)
			maxIterations=(wOut>>1)+1;
		else
			maxIterations = wOut+1;
		
#define ROUNDED_ROTATION 0 // 0:trunc 

#if ROUNDED_ROTATION
		REPORT(DETAILED, "Using rounding to the nearest of each rotation");
#endif
		
		//error analysis
		double eps;  //error in ulp
		eps=0.5; //initial rounding of kfactor
		double shift=0.25;
		for(stage=2; stage<=maxIterations; stage++){
			eps = eps + eps*shift + 1.0; // was 0.5 but it was wrong.
			shift *=0.5;
		}

		if (reducedIterations == 1) {
			eps+=2; // two multiplications in sequence, each truncated
		}

		eps+=1; // the final neg-by-not
		REPORT(DETAILED, "Error analysis computes eps=" << eps << " ulps (before final rounding)");

#if 0 // The following error analysis separates deltaS and deltaC, but it doesn't improve anytbing.
		// It should remain in the code to remember that it is not such a good idea
		
		double deltaC, deltaS;  //error in ulp
		deltaC=0.5; //initial rounding of kfactor
		deltaS=0; // exact
		 shift=0.25;
		for(stage=2; stage<=maxIterations; stage++){
			deltaC = deltaC + deltaS*shift + 1.0; // 1.0 assume truncation in the rotation.
			deltaS = deltaS + deltaC*shift + 1.0; // 1.0 assume truncation in the rotation.
		REPORT(DETAILED, "Error analysis computes deltaS=" << deltaS << "  and deltaC=" << deltaC << " )");
			shift *=0.5;
		}

		if (reducedIterations == 1) {
			deltaC+=2; // two multiplications in sequence, each truncated
			deltaS+=2; // two multiplications in sequence, each truncated
		}

		deltaS+=1; // the final neg-by-not
		deltaC+=1; // the final neg-by-not
		REPORT(DETAILED, "Error analysis computes deltaS=" << deltaS << "  and deltaC=" << deltaC << "  ulps (before final rounding)");
#endif
		
		// guard bits depend only on the number of iterations
		g = 1 + (int) floor(log2(eps));  
		
		// *********    internal precision and fixed-point alignment **************

 		// The input is as follows:
		// s has weight 2^0
		// q has weight 2^-1
		// o has weight 2^-2
		// Purpose: have the LSB of z, cosine, sine at weight -w
		// This means that the Cos and Sin datapath will have w+1 bits
		//   (sign at weight zero)
		// while the Z datapath starts on w-1 bits (sign bit at weight  -2,
		//  and decreasing, so the invariant is: sign bit at weight -stage-1)

		w = wOut + g; 

		REPORT(DETAILED, "wIn=" << wIn << " wOut=" << wOut 
		       << "   MaxIterations: " << maxIterations 
		       << "  Guard bits g=" << g << "  Neg. weight of LSBs w=" << w );
		
		//reduce the argument X to [0, 1/2)
		vhdl << tab << declare("sgn") << " <= X(" << wIn-1 << ");  -- sign" << endl;
		vhdl << tab << declare("q") << " <= X(" << wIn-2 << ");  -- quadrant" << endl;
		vhdl << tab << declare("o") << " <= X(" << wIn-3 << ");  -- octant" << endl;
		vhdl << tab << declare("sqo", 3) << " <= sgn & q & o;  -- sign, quadrant, octant" << endl;

		vhdl << tab << declare(getTarget()->logicDelay(),
													 "qrot0", 3) << " <= sqo +  \"001\"; -- rotate by an octant" << endl; 
		vhdl << tab << declare("qrot", 2) << " <= qrot0(2 downto 1); -- new quadrant: 00 is the two octants around the origin" << endl; 
		// y is built out of the remaining wIn-2 bits



		int zMSB=-2;   // -2 'cause we live in a quadrant, initial angle is in -0.25pi, 0.25pi
		int zLSB=-w-1; // better have a bit more accuracy here, it seems 
		// extract the bits below the octant bit, the new sign will be at the weight of the octant bit
		int sizeZ =  zMSB - zLSB +1;
 		int sizeY=wIn-2;
		vhdl << tab << declare("Yp", sizeZ ) << "<= " ;
		if(sizeZ >= sizeY) {
			vhdl << "X" << range(sizeY-1,0); // sizeY-1 = wIn-4,  i.e. MSB = -3
			if(sizeZ > sizeY)
				vhdl << " & " << zg(sizeZ-sizeY) << ";" << endl;
		}
		else // sizeZ < sizeY
			vhdl << "X" << range(sizeY-1, sizeY-sizeZ) <<";" << endl;

		vhdl << tab << "--  This Yp is in -pi/4, pi/4. Now start CORDIC with angle atan(1/2)" << endl;

		//create the C1, S1, X1 and D1 signals for the first stage
		mpfr_t temp, zatan;

 		mpfr_init2(kfactor, 10*w);
		mpfr_init2(temp, 10*w);
		mpfr_set_d(kfactor, 1.0, GMP_RNDN);
		for(int i=1; i<=maxIterations; i++){
			mpfr_set_d(temp, 1.0, GMP_RNDN);
			mpfr_div_2si(temp, temp, 2*i, GMP_RNDN);
			mpfr_add_d(temp, temp, 1.0, GMP_RNDN);
			
			mpfr_mul(kfactor, kfactor, temp, GMP_RNDN);
		}
		mpfr_sqrt(kfactor, kfactor, GMP_RNDN);
		mpfr_d_div(kfactor, 1.0, kfactor, GMP_RNDN);

		mpfr_mul(kfactor, kfactor, scale, GMP_RNDN);
		
		REPORT(DEBUG, "kfactor=" << printMPFR(kfactor));
		mpfr_clear(temp);
		
		// initialize the zatan mpfr. It will be cleared outside the loop
		mpfr_init2(zatan, 10*w);
			


		vhdl << tab << declare("Cos1", w+1) << " <= " <<  unsignedFixPointNumber(kfactor, 0, -w) << ";" 
		     << "-- scale factor, about " << printMPFR(kfactor) << endl;
		vhdl << tab << declare("Sin1", w+1) << " <= " << zg(w+1) << ";" << endl;
		vhdl << tab << declare("Z1", sizeZ) << "<= Yp;" << endl;
		vhdl << tab << declare("D1") << "<= Yp" << of(sizeZ-1) << ";" << endl;
		
				
		//create the stages of micro-rotations

		//build the cordic stages
		for(stage=1; stage<=maxIterations; stage++){
			vhdl << tab <<"----------- Iteration "<< stage << endl;
			//shift Xin and Yin with 2^n positions to the right
			// Cosine is always positive, but sine may be negative and thus need sign extend
			vhdl << tab << declare(join("CosShift", stage), w+1) << " <= " << zg(stage) << " & Cos" << stage << range(w, stage) << ";" <<endl;
			
			vhdl << tab << declare(join("sgnSin", stage))  << " <= " <<  join("Sin", stage)  <<  of(w) << ";" << endl; 
			vhdl << tab << declare(join("SinShift", stage), w+1) 
			     << " <= " << rangeAssign(w, w+1-stage, join("sgnSin", stage))   
			     << " & Sin" << stage << range(w, stage) << ";" << endl;
			
			// Critical path delay for one stage:
			// The data dependency is from one Z to the next 
			// We may assume that the rotations themselves overlap once the DI are known
			//manageCriticalPath(getTarget()->localWireDelay(w) + getTarget()->adderDelay(w) + getTarget()->lutDelay()));
 			// manageCriticalPath(getTarget()->localWireDelay(sizeZ) + getTarget()->adderDelay(sizeZ));
			

#if	ROUNDED_ROTATION
			vhdl << tab << declare(join("CosShiftRoundBit", stage)) << " <= " << join("Cos", stage)  << of(stage-1) << ";" << endl;
			vhdl << tab << declare(join("SinShiftRoundBit", stage)) << " <= " << join("Sin", stage) << of(stage-1) << ";" <<endl;
			vhdl << tab << declare(join("CosShiftNeg", stage), w+1) << " <= " << rangeAssign(w, 0, join("D", stage)) << " xor " << join("CosShift", stage)   << " ;" << endl;
			vhdl << tab << declare(join("SinShiftNeg", stage), w+1) << " <= (not " << rangeAssign(w, 0, join("D", stage)) << ") xor " << join("SinShift", stage)   << " ;" << endl;

			vhdl << tab << declare(join("Cos", stage+1), w+1) << " <= " 
			     << join("Cos", stage) << " + " << join("SinShiftNeg", stage) << " +  not (" << join("D", stage) << " xor " << join("SinShiftRoundBit", stage) << ") ;" << endl;

			vhdl << tab << declare(join("Sin", stage+1), w+1) << " <= " 
			     << join("Sin", stage) << " + " << join("CosShiftNeg", stage) << " + (" << join("D", stage) << " xor " << join("CosShiftRoundBit", stage) << ") ;" << endl;

#else
// truncation of the shifted operand
			vhdl << tab << declare(join("Cos", stage+1), w+1) << " <= " 
			     << join("Cos", stage) << " - " << join("SinShift", stage) << " when " << join("D", stage) << "=\'0\' else "
			     << join("Cos", stage) << " + " << join("SinShift", stage) << " ;" << endl;

			vhdl << tab << declare(join("Sin", stage+1), w+1) << " <= " 
			     << join("Sin", stage) << " + " << join("CosShift", stage) << " when " << join("D", stage) << "=\'0\' else "
			     << join("Sin", stage) << " - " << join("CosShift", stage) << " ;" << endl;

#endif			
			
			//create the constant signal for the arctan	
			mpfr_set_d(zatan, 1.0, GMP_RNDN);
			mpfr_div_2si(zatan, zatan, stage, GMP_RNDN);
			mpfr_atan(zatan, zatan, GMP_RNDN);
			mpfr_div(zatan, zatan, constPi, GMP_RNDN);
			REPORT(DEBUG, "stage=" << stage << "  atancst=" << printMPFR(zatan));		
			//create the arctangent factor to be added to Zin
									
			REPORT(DEBUG, "  sizeZ=" << sizeZ << "   zMSB="<<zMSB );

			if(stage<maxIterations || reducedIterations == 1) {
				// LSB is always -w 
				vhdl << tab << declare(join("atan2PowStage", stage), sizeZ) << " <= " << unsignedFixPointNumber(zatan, zMSB, zLSB) << ";" <<endl;
				
				vhdl << tab << declare(join("fullZ", stage+1), sizeZ) << " <= " 
				     << join("Z", stage) << " + " << join("atan2PowStage", stage) << " when " << join("D", stage) << "=\'1\' else "
				     << join("Z", stage) << " - " << join("atan2PowStage", stage) << " ;" << endl;
				vhdl << tab << declare(join("Z", stage+1), sizeZ-1) << " <= "<< join("fullZ", stage+1) << range(sizeZ-2, 0) << ";" << endl; 
				vhdl << tab << declare(join("D", (stage+1))) << " <= fullZ" << stage+1 << "(" << sizeZ-1 <<");" <<endl;
			}
			//decrement the size of Z
			sizeZ--;
			zMSB--;
		}
		
		// Give the time to finish the last rotation
		// manageCriticalPath( getTarget()->localWireDelay(w+1) + getTarget()->adderDelay(w+1) // actual CP delay
		//                    - (getTarget()->localWireDelay(sizeZ+1) + getTarget()->adderDelay(sizeZ+1))); // CP delay that was already added

			
		if(reducedIterations == 0){ //regular struture; all that remains is to assign the outputs correctly
			
			//assign output
			
			vhdl << tab << declare("redCos", w+1) << "<= " << join("Cos", stage) << ";" << endl;
			vhdl << tab << declare("redSin", w+1) << "<= " << join("Sin", stage) << ";" << endl;
			

		}
		else{	//reduced iterations structure; rotate by the remaining angle and then assign the angles
			
			vhdl << tab << "-- Reduced iteration: finish the computation by a rotation by Pi Z" << stage << endl; 
			
			vhdl << tab << declare("FinalZ", sizeZ+1) << " <= " << join("D", stage)<< " & " << join("Z", stage) << ";" << endl;

			//multiply X by Pi
			newInstance("FixRealConstMult",
									"piMultiplier",
									// unsigned here, the conversion to signed comes later
									"method=KCM signedIn=1 msbIn=" + to_string(zMSB+1)
									+ " lsbIn=" + to_string(zLSB)
									+ " lsbOut="  + to_string(zLSB)
									+ " constant=pi", 
									"X=>FinalZ",
									"R=>PiZ");

			// manageCriticalPath(getTarget()->localWireDelay(sizeZ) + getTarget()->DSPMultiplierDelay());

			sizeZ+=3; // +2 cause mult by pi, +1 because we add back the sign
			
			vhdl << tab << declare("CosTrunc", sizeZ) << " <= " << join("Cos", stage) << range(w, w-sizeZ+1) << ";" << endl;
			vhdl << tab << declare("SinTrunc", sizeZ) << " <= " << join("Sin", stage) << range(w, w-sizeZ+1) << ";" << endl;
			
			//multiply with the angle X to obtain the actual values for sine and cosine
			newInstance("IntMultiplier",
									"MultCosZ",
									"wX=" + to_string(sizeZ)
									+ " wY=" + to_string(sizeZ)
									+ " wOut=" + to_string(sizeZ)
									+ " signedIO=1", 
									"X=>CosTrunc, Y=>PiZ",
									"R=>CosTimesZTrunc");

			newInstance("IntMultiplier",
									"MultSinZ",
									"wX=" + to_string(sizeZ)
									+ " wY=" + to_string(sizeZ)
									+ " wOut=" + to_string(sizeZ)
									+ " signedIO=1", 
									"X=>SinTrunc, Y=>PiZ",
									"R=>SinTimesZTrunc");

			// manageCriticalPath(getTarget()->localWireDelay(w) + getTarget()->adderDelay(w));
			
			
			vhdl << tab << declare("ShiftedCosTimesZ", w+1) << " <= (" << w << " downto " << sizeZ << " => CosTimesZTrunc(" << sizeZ-1 << ")) & CosTimesZTrunc;"  << endl;
			vhdl << tab << declare("ShiftedSinTimesZ", w+1) << " <= (" << w << " downto " << sizeZ << " => SinTimesZTrunc(" << sizeZ-1 << ")) & SinTimesZTrunc;"  << endl;
			
			vhdl << tab << declare("redCos", w+1) << " <= " << join("Cos", stage) << " - ShiftedSinTimesZ;" << endl;
			vhdl << tab << declare("redSin", w+1) << " <= " << join("Sin", stage) << " + ShiftedCosTimesZ;" << endl;
						
		}


		
		vhdl << tab << "---- final reconstruction " << endl;
		

		// All this should fit in one level of LUTs
		// manageCriticalPath(getTarget()->localWireDelay(wOut) + getTarget()->lutDelay());

		vhdl << tab << declare("redCosNeg", w+1) << " <= (not redCos); -- negate by NOT, 1 ulp error"<< endl;
		vhdl << tab << declare("redSinNeg", w+1) << " <= (not redSin); -- negate by NOT, 1 ulp error"<< endl;
												   
		vhdl << tab << "with qrot select" << endl
		     << tab << tab << declare("CosX0", w+1) << " <= " << endl;
		vhdl << tab << tab << tab << " redCos    when \"00\"," << endl;
		vhdl << tab << tab << tab << " redSinNeg when \"01\"," << endl;
		vhdl << tab << tab << tab << " redCosNeg when \"10\"," << endl;
		vhdl << tab << tab << tab << " redSin    when others;" << endl;

		vhdl << tab << "with qrot select" << endl
		      << tab << tab << declare("SinX0", w+1) << " <= " << endl;
		vhdl << tab << tab << tab << " redSin    when \"00\"," << endl;
		vhdl << tab << tab << tab << " redCos    when \"01\"," << endl;
		vhdl << tab << tab << tab << " redSinNeg when \"10\"," << endl;
		vhdl << tab << tab << tab << " redCosNeg when others;" << endl;
		
		
		// manageCriticalPath( getTarget()->adderDelay(1+wOut+1));

		vhdl << tab << declare("roundedCosX", wOut+1) << " <= CosX0" << range(w, w-wOut) << " + " << " (" << zg(wOut) << " & \'1\');" << endl;
		vhdl << tab << declare("roundedSinX", wOut+1) << " <= SinX0" << range(w, w-wOut) << " + " << " (" << zg(wOut) << " & \'1\');" << endl;
														   
		vhdl << tab << "C <= roundedCosX" << range(wOut, 1) << ";" << endl;
		vhdl << tab << "S <= roundedSinX" << range(wOut, 1) << ";" << endl;


		mpfr_clears (zatan, NULL);		
	};


	FixSinCosCORDIC::~FixSinCosCORDIC(){
		mpfr_clears (scale, kfactor, constPi, NULL);		
	 };



}




#if 0 
		// Rough simulation, for debug purpose
		double c,cc,s,ss,z, z0, dd,p,scale;
		int d;
		const double pi=3.14159265358979323846264338327950288419716939937508;
		c=mpfr_get_d(kfactor, GMP_RNDN);
		s=0.0;
		z=0.15625; // 1/6
		z0=z;
		p=0.5;
		scale=(double) (1<<19);
		for(stage=1; stage<=maxIterations; stage++){
			if(z>=0) d=0; else d=1;
			if(d==0) dd=1.0; else dd=-1.0;
			cc = c - dd*p*s;
			ss = s + dd*p*c;
			cout << stage << "\t atan=" << atan(p)/pi<< "  \t d=" << d << "\t z=" << z << "\t c=" << c << "\t s=" << s;
			cout  << "      \t z=" << (int)(z*scale) << "  \t c=" << (int)(c*scale*4) << "\t s=" << (int)(s*scale*4) << endl;
			z = z - dd*atan(p)/pi;
			c=cc;
			s=ss;
			p=p*0.5;
		}		
		cout  << "Should be  \t\t\t\t\t\t c=" << cos(pi*z0) << "  \t s=" << sin(pi*z0) << endl;
		//		manageCriticalPath(getTarget()->localWireDelay(wcs + g) + getTarget()->lutDelay());
#endif		
