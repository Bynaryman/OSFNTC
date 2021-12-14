/*
  A class that manages fixed-point  polynomial approximation for FloPoCo (and possibly later for metalibm).

	At the moment, only on [0,1], no possibility to have it on [-1,1].
	Rationale: no point really as soon as we are not in the degenerate case alpha=0.

  Authors: Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © INSA-Lyon, ENS-Lyon, INRIA, CNRS, UCBL

  All rights reserved.

*/


/*
	 The function is assumed to have inputs in [0,1]

	 Stylistic remark: use index i for the subintervals, and j for the degree

*/
#include "VaryingPiecewisePolyApprox.hpp"
#include <sstream>
#include <limits.h>
#include <float.h>

namespace flopoco {

  VaryingPiecewisePolyApprox::VaryingPiecewisePolyApprox(FixFunction *f_, double targetAccuracy_, int lsbIn_, int msbOut_, int lsbOut_):
                lsbIn(lsbIn_), msbOut(msbOut_), lsbOut(lsbOut_), f(f_), targetAccuracy(targetAccuracy_)
	{
	        degree = 0;
		needToFreeF = false;
		srcFileName="VaryingPiecewisePolyApprox"; // should be somehow static but this is too much to ask me
		build();
	}


  VaryingPiecewisePolyApprox::VaryingPiecewisePolyApprox(string sollyaString_, double targetAccuracy_, int lsbIn_, int msbOut_, int lsbOut_):
                lsbIn(lsbIn_), msbOut(msbOut_), lsbOut(lsbOut_), targetAccuracy(targetAccuracy_)
	{
	        degree = 0;
		//  parsing delegated to FixFunction
		f = new FixFunction(sollyaString_, false /* on [0,1]*/);
		needToFreeF = true;
		srcFileName="VaryingPiecewisePolyApprox"; // should be somehow static but this is too much to ask me
		build();
	}



	VaryingPiecewisePolyApprox::~VaryingPiecewisePolyApprox()
	{
		if(needToFreeF)	free(f);
	}



	/** a local function to build g_i(x) = f(2^(-i)*x + i*2^-i) defined on [0,1]  on va la recentrer sur [-1, 1]*/

	sollya_obj_t VaryingPiecewisePolyApprox::buildSubIntervalFunction(sollya_obj_t fS, int i){
		stringstream s;
		s << "(1b-" << i << ")*(x+1)/4 + (1b-" << i << ")/2";
		string ss = s.str(); // do not use c_str directly on the stringstream, it is too transient (?)
		sollya_obj_t newxS = sollya_lib_parse_string(ss.c_str());
		sollya_obj_t giS = sollya_lib_substitute(fS,newxS);
		sollya_lib_clear_obj(newxS);
		return giS;
	}

  	sollya_obj_t VaryingPiecewisePolyApprox::buildFinalSubIntervalFunction(sollya_obj_t fS, int i){
		stringstream s;
		s << "(1b-" << i << ")*(x+1)/4";
		string ss = s.str(); // do not use c_str directly on the stringstream, it is too transient (?)
		sollya_obj_t newxS = sollya_lib_parse_string(ss.c_str());
		sollya_obj_t giS = sollya_lib_substitute(fS,newxS);
		sollya_lib_clear_obj(newxS);
		return giS;
	}

	// split into smaller and smaller intervals until the function can be approximated by a polynomial of degree given by degree.
	void VaryingPiecewisePolyApprox::build()
	{
		ostringstream cacheFileName;
		cacheFileName << "VaryingPiecewisePoly_"<<vhdlize(f->description) << "_" << degree << "_" << targetAccuracy << ".cache";

		// Test existence of cache file, create it if necessary
		openCacheFile(cacheFileName.str());

		if(!cacheFile.is_open())
		{
			//********************** Do the work, then write the cache *********************
			sollya_obj_t fS = f->fS; // no need to free this one
			sollya_obj_t rangeS;

			rangeS  = sollya_lib_parse_string("[-1;1]");
			// TODO test with [-1,1] which is the whole point of current refactoring.
			// There needs to be a bit of logic here because rangeS should be [-1,1] by default to exploit signed arithmetic,
			// except in the case when alpha=0 because then rangeS should be f->rangeS (and should not be freed)
			
			// Limit alpha to 24, because alpha will be the number of bits input to a table
			// it will take too long before that anyway
			tabulateRest = true;
			int biggestNbInterval = -lsbIn -6; //une fois qu'on a beaucoup shifté, ça coûte rien de tabuler
			
			for (nbInterval=0; nbInterval<biggestNbInterval; nbInterval++)
			{
			        REPORT(DETAILED, " Testing nbInterval=" << nbInterval );

				sollya_obj_t giS = buildSubIntervalFunction(fS, nbInterval);
				if(DEBUG <= UserInterface::verbose)
				  sollya_lib_printf("> VaryingPiecewisePolyApprox: nbInterval=%d, testing  %b \n", nbInterval, giS);
				int degreeInf, degreeSup;
				BasicPolyApprox::guessDegree(giS, rangeS, targetAccuracy, &degreeInf, &degreeSup);
				sollya_lib_clear_obj(giS);
				degree = max(degreeSup, degree);

				giS = buildFinalSubIntervalFunction(fS, nbInterval);
				if(DEBUG <= UserInterface::verbose)
				  sollya_lib_printf("> VaryingPiecewisePolyApprox, Interval rest : nbInterval=%d, testing  %b \n", nbInterval, giS);
				BasicPolyApprox::guessDegree(giS, rangeS, targetAccuracy, &degreeInf, &degreeSup);
				sollya_lib_clear_obj(giS);
				
				if (degreeSup<=degree) {
				  tabulateRest=false;
				  break;
				}
				else {
				  REPORT(DEBUG, "   nbInterval=" << nbInterval+1 << " failed." );
				}
				
			} // end for loop on i
			nbInterval++; nbInterval++;
			
			REPORT(INFO, "Found nbInterval=" << nbInterval)
			
			if (tabulateRest==true) {
			    REPORT(INFO, "The smallest interval will be tabulated");
			    nbInterval --; //parce qu'on utilise ce nombre pour les approx poly
			}
			
			// Compute the LSB of each coefficient. Minimum value is:
			LSB = floor(log2(targetAccuracy*degree));
			REPORT(DEBUG, "To obtain target accuracy " << targetAccuracy << " with a degree-"<< degree
					<<" polynomial, we compute coefficients accurate to LSB="<<LSB);
			// It is pretty sure that adding intlog2(degree) bits is enough for FPMinimax.

			// The main loop starts with the almost hopeless LSB defined above, then tries to push it down, a0 first, then all the others.
			// If guessdegree returned an interval, it tries lsbAttemptsMax times, then gives up and tries to increase the degree.
			// Otherwise lsbAttemptsMax is ignored, but in practice success happens earlier anyway

			int lsbAttemptsMax = intlog2(degree)+1;
			int lsbAttempts=0; // a counter of attempts to move the LSB down, caped by lsbAttemptsMax
			// bool a0Increased=false; // before adding LSB bits to everybody we try to add them to a0 only.

			bool success=false;
			while(!success) {
				// Now fill the vector of polynomials, computing the coefficient parameters along.
				//		LSB=INT_MAX; // very large
				// MSB=INT_MIN; // very small
				approxErrorBound = 0.0;
				BasicPolyApprox *p;

				REPORT(DETAILED, "Computing the actual polynomials ");
				// initialize the vector of MSB weights
				for (int j=0; j<=degree; j++) {
					MSB.push_back(INT_MIN);
				}

				for (int i=0; i<nbInterval; i++) {
					REPORT(DETAILED, " ... computing polynomial approx for interval " << i << " / "<< nbInterval);
					// Recompute the substitution. No big deal.
					sollya_obj_t giS;
					if (i==nbInterval-1 && not tabulateRest) {
					  giS = buildFinalSubIntervalFunction(fS, i-1);
					}
					else {
					  giS = buildSubIntervalFunction(fS, i);
					}
					
					p = new BasicPolyApprox(giS, degree, LSB, true);
					poly.push_back(p);
					if (approxErrorBound < p->approxErrorBound){
						REPORT(DEBUG, "   new approxErrorBound=" << p->approxErrorBound );
						approxErrorBound = p->approxErrorBound;
					}
					if (approxErrorBound>targetAccuracy){
						break;
					}

					// Now compute the englobing MSB for each coefficient
					for (int j=0; j<=degree; j++) {
						// if the coeff is zero, we can set its MSB to anything, so we exclude this case
						if (  (!p->coeff[j]->isZero())  &&  (p->coeff[j]->MSB > MSB[j])  )
							MSB[j] = p->coeff[j]->MSB;
					}
					sollya_lib_clear_obj(giS);
					

				} // end for loop on j

				if (approxErrorBound < targetAccuracy) {
					REPORT(INFO, " *** Success! Final approxErrorBound=" << approxErrorBound << "  is smaller than target accuracy: " << targetAccuracy  );
					success=true;
				}
				else {
					REPORT(INFO, "Measured approx error:" << approxErrorBound << " is larger than target accuracy: " << targetAccuracy
							<< ". Increasing LSB and starting over. Thank you for your patience");
					//empty poly
					for (auto i:poly)
						free(i);
					while(!poly.empty())
						poly.pop_back();

					if(lsbAttempts<=lsbAttemptsMax) {
						lsbAttempts++;
						LSB--;
					}
					else {
					        LSB+=lsbAttempts;
						lsbAttempts=0;
						degree ++;
						REPORT(INFO, "guessDegree mislead us, increasing degree to " << degree << " and starting over");
						//alpha++;
						//nbInterval=1<<alpha;
						//REPORT(INFO, "guessDegree mislead us, increasing alpha to " << alpha << " and starting over");
					}
				}
			} // end while(!success)

			// Now we need to resize all the coefficients of degree i to the largest one
			// TODO? we could also check if one of the coeffs is always positive or negative, and optimize generated code accordingly
			for (int i=0; i<nbInterval; i++) {
				for (int j=0; j<=degree; j++) {
					// REPORT(DEBUG "Resizing MSB of coeff " << j << " of poly " << i << " : from " << poly[i] -> coeff[j] -> MSB << " to " <<  MSB[j]);
					poly[i] -> coeff[j] -> changeMSB(MSB[j]);
					// REPORT(DEBUG, "   Now  " << poly[i] -> coeff[j] -> MSB);
				}
			}

			//Compute the table
			if (tabulateRest==true) {
			  REPORT(INFO, "Tabulating the values for the last interval");
			  sollya_obj_t giS = buildFinalSubIntervalFunction(fS, nbInterval-1);
			  char *buf;
			  size_t sz;
			  sz = sollya_lib_snprintf(NULL, 0, "%b", giS);
			  if (sz==(size_t)NULL)
			    REPORT(INFO, "No function, oupsie");
			  buf = (char *)malloc(sz + 1);
			  sollya_lib_snprintf(buf, sz+1,"%b", giS);
			  FixFunction* g = new FixFunction(buf, true, -5, msbOut, lsbOut);
			  for (int i=0; i<(1<<6);i++) {
			    mpz_class rNorD, ru;
			    g->eval(mpz_class(i), rNorD, ru, true);
			    table.push_back(rNorD);
			  }
			  sollya_lib_clear_obj(giS);
			  free(buf);
			  free(g);
			}
				    
			// Write the cache file
			writeToCacheFile(cacheFileName.str());

			//cleanup the sollya objects
			sollya_lib_clear_obj(rangeS);
		}
		else
		{
			REPORT(INFO, "Polynomial data cache found: " << cacheFileName.str());
			//********************** Just read the cache *********************
			readFromCacheFile(cacheFileName.str());
		} // end if cache

		// Check if all the coefficients of a given degree are of the same sign
		checkCoefficientsSign();

#if 0 //experimental, WIP
		cerr << "***************************************"<<endl;
		computeSigmaMSBs();
		cerr << "***************************************"<<endl;
#endif

		// A bit of reporting
		createPolynomialsReport();
	}


	mpz_class VaryingPiecewisePolyApprox::getCoeff(int i, int d){
		BasicPolyApprox* p = poly[i];
		FixConstant* c = p->coeff[d];
		return c->getBitVectorAsMPZ();
	}


	void VaryingPiecewisePolyApprox::openCacheFile(string cacheFileName)
	{
		cacheFile.open(cacheFileName.c_str(), ios::in);

		// check for bogus .cache file
		if(cacheFile.is_open() && cacheFile.peek() == std::ifstream::traits_type::eof())
		{
			cacheFile.close();
			std::remove(cacheFileName.c_str());
			cacheFile.open(cacheFileName.c_str(), ios::in); // of course this one will fail
		}
	}


	void VaryingPiecewisePolyApprox::writeToCacheFile(string cacheFileName)
	{
		cacheFile.open(cacheFileName.c_str(), ios::out);

		cacheFile << "Polynomial data cache for " << cacheFileName << endl;
		cacheFile << "Erasing this file is harmless, but do not try to edit it." <<endl;

		cacheFile << degree <<endl;
		cacheFile << nbInterval << endl;
		cacheFile << LSB <<endl;
		cacheFile << tabulateRest << endl;

		for (int j=0; j<=degree; j++) {
			cacheFile << MSB[j] << endl;
		}

		cacheFile << approxErrorBound << endl;

		// now write the coefficients themselves
		for(int i=0; i<nbInterval; i++){
		
			for (int j=0; j<=degree; j++)
			{
				cacheFile <<  poly[i] -> coeff[j] -> getBitVectorAsMPZ() << endl;
			}
		}

		if (tabulateRest==true) {
		  for (int i=0; i<(1<<6); i++) {
		    cacheFile << table[i]<< endl;
		  }
		}
	}


	void VaryingPiecewisePolyApprox::readFromCacheFile(string cacheFileName)
	{
		string line;

		getline(cacheFile, line); // ignore the first line which is a comment
		getline(cacheFile, line); // ignore the second line which is a comment

		cacheFile >> degree;
		cacheFile >> nbInterval;
		cacheFile >> LSB;
		cacheFile >> tabulateRest;

		for (int j=0; j<=degree; j++) {
			int msb;
			cacheFile >> msb;
			MSB.push_back(msb);
		}

		cacheFile >> approxErrorBound;

		for (int i=0; i<nbInterval; i++) {
			vector<mpz_class> coeff;
			for (int j=0; j<=degree; j++) {
				mpz_class c;
				cacheFile >> c;
				coeff.push_back(c);
			}
			BasicPolyApprox* p = new BasicPolyApprox(degree,MSB,LSB,coeff);
			poly.push_back(p);
		}
		if (tabulateRest==true) {
		  for (int i=0; i<(1<<6); i++) {
		    mpz_class c;
		    cacheFile >> c;
		    table.push_back(c);
		  }
		}
	}


	void VaryingPiecewisePolyApprox::checkCoefficientsSign()
	{
		for (int j=0; j<=degree; j++) {
			mpz_class mpzsign = (poly[0]->coeff[j]->getBitVectorAsMPZ()) >> (MSB[j]-LSB);
			coeffSigns.push_back((mpzsign==0?+1:-1));
			for (int i=1; i<nbInterval; i++) {
				mpzsign = (poly[i]->coeff[j]->getBitVectorAsMPZ()) >> (MSB[j]-LSB);
				int sign = (mpzsign==0 ? 1 : -1);
				if (sign != coeffSigns[j])
					coeffSigns[j] = 0;
			}
		}
	}


	void VaryingPiecewisePolyApprox::createPolynomialsReport()
	{
		int totalOutputSize;

		REPORT(INFO,"Parameters of the approximation polynomials: ");
		REPORT(INFO,"  Degree=" << degree	<< "  TableSize=" << intlog2(nbInterval)
				<< "    maxApproxErrorBound=" << approxErrorBound  << "    common coeff LSB="  << LSB);

		totalOutputSize=0;
		for (int j=0; j<=degree; j++) {
			int size = MSB[j]-LSB + (coeffSigns[j]==0? 1 : 0);
			totalOutputSize += size ;
			REPORT(INFO,"      MSB["<<j<<"] = \t" << MSB[j] << "\t size=" << size
					<< (coeffSigns[j]==0? "\t variable sign " : "\t constant sign ") << coeffSigns[j]);
		}

		REPORT(INFO, "  Total size of the coeff table is " << nbInterval << " x " << totalOutputSize << " bits");
	}


	OperatorPtr VaryingPiecewisePolyApprox::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args)
	{
		string f;
		double ta;
		int lsIn;
		int msOut;
		int lsOut;

		UserInterface::parseString(args, "f", &f);
		UserInterface::parseFloat(args, "targetAcc", &ta);
		UserInterface::parseInt(args, "lsbIn", &lsIn);
		UserInterface::parseInt(args, "msbOut", &msOut);
		UserInterface::parseInt(args, "lsbOut", &lsOut);

		VaryingPiecewisePolyApprox *ppa = new VaryingPiecewisePolyApprox(f, ta, lsIn, msOut, lsOut);
		cout << "Accuracy is " << ppa->approxErrorBound << " ("<< log2(ppa->approxErrorBound) << " bits)";

		return NULL;
	}



	void VaryingPiecewisePolyApprox::registerFactory()
	{
		UserInterface::add("VaryingPiecewisePolyApprox", // name
				   "Helper/Debug feature, does not generate VHDL. Uniformly segmented piecewise polynomial approximation of function f, accurate to targetAcc on [0,1)",
				   "FunctionApproximation",
				   "",
				   "\
f(string): function to be evaluated between double-quotes, for instance \"exp(x*x)\"; \
targetAcc(real): the target approximation errror of the polynomial WRT the function;\
lsbIn(int): input of x;\
msbOut(int): output of f(x);\
lsbOut(int): output of f(x)",
				   "",
				   VaryingPiecewisePolyApprox::parseArguments
				   ) ;
	}



} //namespace
