/*
  This file is part of the FloPoCo project

  Author : Florent de Dinechin, Matei Istoan

  Initial software.
  Copyright © INSA-Lyon, INRIA
  2014.

  All rights reserved.
*/


#include "Atan2Table.hpp"
using namespace std;


namespace flopoco{

	// A table for the atan(y/x)
	Atan2Table::Atan2Table(Target* target, int wIn_, int wOut_, int archType_,
			int msbA_, int msbB_, int msbC_,
			int msbD_, int msbE_, int msbF_,
			map<string, double> inputDelays) :
		Table(target, wIn_, wOut_, 0, ((1<<wIn_)-1), false),
		wIn(wIn_), wOut(wOut_), archType(archType_),
		msbA(msbA_), msbB(msbB_), msbC(msbC_),
		msbD(msbD_), msbE(msbE_), msbF(msbF_)
	{
		ostringstream name;
		srcFileName="Atan2Table";
		name <<"Atan2Table_" << wIn << "_" << wOut << "_" << archType;
		setNameWithFreqAndUID(name.str());
	}

	Atan2Table::~Atan2Table()
	{

	}

	mpz_class Atan2Table::function(int input)
	{
		int x, y;
		mpz_t result, temp, aux;
		mpfr_t a, b, c, cAux, d, e, f, tempMpfr, pi_mpfr;
		int wOutFrac;

		if((archType == 0) || (archType == 1))
		{
			wOutFrac = (wOut-msbA-msbB-msbC)/3;
		}else if(archType == 2)
		{
			wOutFrac = (wOut-msbA-msbB-msbC-msbD-msbE-msbF)/6;
		}else
		{
			THROWERROR("Error: in Atan2Table::function: unknown architecture type");
		}

		//recreate the value of x
		//	(representing the top half of the bits of input)
		x = input >> ((wIn+1)/2);
		y = input - (x<<((wIn+1)/2));
		x = (1 << ((wIn+1)/2 - 1)) + x;

		//init the mpfr variables
		mpfr_inits2(10000, a, b, c, cAux, d, e, f, tempMpfr, pi_mpfr, (mpfr_ptr) 0);

		//init the mpz variables
		mpz_inits(result, temp, aux, (mpz_ptr)0);

		//generate the table
		//obtain the parameters for the plane equation
		if(archType == 0)
		{
			generatePlaneParameters(x, y, a, b, c);
		}else if(archType == 1)
		{
			generateTaylorOrder1Parameters(x, y, a, b, c);
		}else if(archType == 2)
		{
			generateTaylorOrder2Parameters(x, y, a, b, c, d, e, f);
		}else
		{
			THROWERROR("Error: in Atan2Table::function: unknown  architecture type");
		}

		if((archType == 0) || (archType == 1))
		{
			//create ax and by, and update c
			mpfr_set(cAux, c, GMP_RNDN);
			mpfr_mul_si(tempMpfr, a, x, GMP_RNDN);
			mpfr_div_2ui(tempMpfr, tempMpfr, ((wIn+1)/2), GMP_RNDN);
			mpfr_add(cAux, cAux, tempMpfr, GMP_RNDN);
			mpfr_mul_si(tempMpfr, b, y, GMP_RNDN);
			mpfr_div_2ui(tempMpfr, tempMpfr, ((wIn+1)/2), GMP_RNDN);
			mpfr_add(cAux, cAux, tempMpfr, GMP_RNDN);

			//create Pi/2
			mpfr_const_pi(pi_mpfr, GMP_RNDN);
			//mpfr_div_si(pi_mpfr, pi_mpfr, 2, GMP_RNDN);

			//extract the result
			//extract A
			//	divide A by Pi/2
			mpfr_div(a, a, pi_mpfr, GMP_RNDN);
			mpfr_mul_2ui(a, a, wOutFrac, GMP_RNDN);
			mpfr_get_z(temp, a, GMP_RNDN);
			//	handle the negative constants and transform them to 2's complement
			if(mpfr_sgn(a) < 0)
			{
				mpz_set_ui(aux, 1);
				mpz_mul_2exp(aux, aux, msbA+wOutFrac);
				mpz_add(temp, aux, temp);
			}
			//	add A to the final result
			mpz_set(result, temp);

			//extract B
			//	divide B by Pi/2
			mpfr_div(b, b, pi_mpfr, GMP_RNDN);
			mpfr_mul_2ui(b, b, wOutFrac, GMP_RNDN);
			mpfr_get_z(temp, b, GMP_RNDN);
			//	handle the negative constants and transform them to 2's complement
			if(mpfr_sgn(b) < 0)
			{
				mpz_set_ui(aux, 1);
				mpz_mul_2exp(aux, aux, msbB+wOutFrac);
				mpz_add(temp, aux, temp);
			}
			//	add B to the final result
			mpz_mul_2exp(result, result, msbB+wOutFrac);
			mpz_add(result, result, temp);

			//extract updated C
			//	divide updated C by Pi/2
			mpfr_div(cAux, cAux, pi_mpfr, GMP_RNDN);
			mpfr_mul_2ui(cAux, cAux, wOutFrac, GMP_RNDN);
			mpfr_get_z(temp, cAux, GMP_RNDN);
			//	handle the negative constants and transform them to 2's complement
			if(mpfr_sgn(cAux) < 0)
			{
				mpz_set_ui(aux, 1);
				mpz_mul_2exp(aux, aux, msbC+wOutFrac);
				mpz_add(temp, aux, temp);
			}
			//	add updated C to the final result
			mpz_mul_2exp(result, result, msbC+wOutFrac);
			mpz_add(result, result, temp);
		}else if(archType == 2)
		{
			//create Pi/2
			mpfr_const_pi(pi_mpfr, GMP_RNDN);
			mpfr_div_si(pi_mpfr, pi_mpfr, 2, GMP_RNDN);

			//extract the result
			//extract A
			//	divide A by Pi/2
			mpfr_div(a, a, pi_mpfr, GMP_RNDN);
			mpfr_mul_2ui(a, a, wOutFrac, GMP_RNDN);
			mpfr_get_z(temp, a, GMP_RNDN);
			//	handle the negative constants and transform them to 2's complement
			if(mpfr_sgn(a) < 0)
			{
				mpz_set_ui(aux, 1);
				mpz_mul_2exp(aux, aux, msbA+wOutFrac);
				mpz_add(temp, aux, temp);
			}
			//	add A to the final result
			mpz_set(result, temp);

			//extract B
			//	divide A by Pi/2
			mpfr_div(b, b, pi_mpfr, GMP_RNDN);
			mpfr_mul_2ui(b, b, wOutFrac, GMP_RNDN);
			mpfr_get_z(temp, b, GMP_RNDN);
			//	handle the negative constants and transform them to 2's complement
			if(mpfr_sgn(b) < 0)
			{
				mpz_set_ui(aux, 1);
				mpz_mul_2exp(aux, aux, msbB+wOutFrac);
				mpz_add(temp, aux, temp);
			}
			//	add B to the final result
			mpz_mul_2exp(result, result, msbB+wOutFrac);
			mpz_add(result, result, temp);

			//extract C
			//	divide A by Pi/2
			mpfr_div(c, c, pi_mpfr, GMP_RNDN);
			mpfr_mul_2ui(c, c, wOutFrac, GMP_RNDN);
			mpfr_get_z(temp, c, GMP_RNDN);
			//	handle the negative constants and transform them to 2's complement
			if(mpfr_sgn(c) < 0)
			{
				mpz_set_ui(aux, 1);
				mpz_mul_2exp(aux, aux, msbC+wOutFrac);
				mpz_add(temp, aux, temp);
			}
			//	add C to the final result
			mpz_mul_2exp(result, result, msbC+wOutFrac);
			mpz_add(result, result, temp);

			//extract D
			//	divide A by Pi/2
			mpfr_div(d, d, pi_mpfr, GMP_RNDN);
			mpfr_mul_2ui(d, d, wOutFrac, GMP_RNDN);
			mpfr_get_z(temp, d, GMP_RNDN);
			//	handle the negative constants and transform them to 2's complement
			if(mpfr_sgn(d) < 0)
			{
				mpz_set_ui(aux, 1);
				mpz_mul_2exp(aux, aux, msbD+wOutFrac);
				mpz_add(temp, aux, temp);
			}
			//	add D to the final result
			mpz_mul_2exp(result, result, msbD+wOutFrac);
			mpz_add(result, result, temp);

			//extract E
			//	divide A by Pi/2
			mpfr_div(e, e, pi_mpfr, GMP_RNDN);
			mpfr_mul_2ui(e, e, wOutFrac, GMP_RNDN);
			mpfr_get_z(temp, e, GMP_RNDN);
			//	handle the negative constants and transform them to 2's complement
			if(mpfr_sgn(e) < 0)
			{
				mpz_set_ui(aux, 1);
				mpz_mul_2exp(aux, aux, msbE+wOutFrac);
				mpz_add(temp, aux, temp);
			}
			//	add E to the final result
			mpz_mul_2exp(result, result, msbE+wOutFrac);
			mpz_add(result, result, temp);

			//extract F
			//	divide A by Pi/2
			mpfr_div(f, f, pi_mpfr, GMP_RNDN);
			mpfr_mul_2ui(f, f, wOutFrac, GMP_RNDN);
			mpfr_get_z(temp, f, GMP_RNDN);
			//	handle the negative constants and transform them to 2's complement
			if(mpfr_sgn(f) < 0)
			{
				mpz_set_ui(aux, 1);
				mpz_mul_2exp(aux, aux, msbF+wOutFrac);
				mpz_add(temp, aux, temp);
			}
			//	add F to the final result
			mpz_mul_2exp(result, result, msbF+wOutFrac);
			mpz_add(result, result, temp);
		}else
		{
			THROWERROR("Error: in Atan2Table::function: unknown  architecture type");
		}

		//clean up the mpfr variables
		mpfr_clears(a, b, c, cAux, d, e, f, tempMpfr, pi_mpfr, (mpfr_ptr)0);

		//clean up the mpz variables
		mpz_clears(temp, aux, (mpz_ptr)0);

		return  mpz_class(result);
	}

	void Atan2Table::generatePlaneParameters(int x, int y, mpfr_t &fa, mpfr_t &fb, mpfr_t &fc)
	{
		Point *P1, *P2, *P3, *P4, *P5;
		Plane *plane;
		double a, b, c, d;
		mpfr_t aMpfr, bMpfr, cMpfr, dMpfr;
		double distance, distanceMin;
		double valI, valJ, centerValI, centerValJ, increment, halfIncrement;
		double distanceFromCenter;
		int k = (wIn+1)/2;

		//create the actual values for the points in which we compute the atan2
		valI = (1.0*x)/(1<<k);
		valJ = (1.0*y)/(1<<k);
		increment = 1.0/(1<<k);
		halfIncrement = increment/2.0;
		centerValI = valI + halfIncrement;
		centerValJ = valJ + halfIncrement;
		distanceFromCenter = 1.0/(1<<(wIn+2));

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
		plane = new Plane(P2, P3, P4, false);
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
		plane = new Plane(P2, P3, P5, false);
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
		plane = new Plane(P2, P4, P5, false);
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
		plane = new Plane(P3, P4, P5, false);
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
		double errorMax, errorMin, errorMean, errorAdjust;

		errorCenter = atan2(centerValJ, centerValI) - (a*centerValI + b*centerValJ + d);
		errorP1 = atan2(valJ, valI) - (a*valI + b*valJ + d);
		errorP2 = atan2(valJ, valI+increment) - (a*(valI+increment) + b*valJ + d);
		errorP3 = atan2(valJ+increment, valI) - (a*valI + b*(valJ+increment) + d);
		errorP4 = atan2(valJ+increment, valI+increment) - (a*(valI+increment) + b*(valJ+increment) + d);

		errorMax = max(5, errorCenter, errorP1, errorP2, errorP3, errorP4);
		errorMin = min(5, errorCenter, errorP1, errorP2, errorP3, errorP4);

		errorMean = ((errorMax>0 ? errorMax : -errorMax)+(errorMin>0 ? errorMin : -errorMin))/2.0;
		if((errorMax>=0) && (errorMin>=0))
		{
			errorAdjust = errorMin+(errorMax-errorMin)/2.0;
		}else if((errorMax>=0) && (errorMin<0))
		{
			errorAdjust = errorMax-errorMean;
		}else if((errorMax<0) && (errorMin<0))
		{
			errorAdjust = errorMax+(errorMin-errorMax)/2.0;
		}

		//equalize the maximum and the minimum error
		errorCenter -= errorAdjust;
		errorP1 -= errorAdjust;
		errorP2 -= errorAdjust;
		errorP3 -= errorAdjust;
		errorP4 -= errorAdjust;

		//compute the new equation of the plane
		P1 = new Point(valI, valJ, (a*valI + b*valJ + d)-errorAdjust);
		P2 = new Point(valI+increment, valJ, (a*(valI+increment) + b*valJ + d)-errorAdjust);
		P3 = new Point(valI, valJ+increment, (a*valI + b*(valJ+increment) + d)-errorAdjust);
		plane = new Plane(P1, P2, P3, true);

		mpfr_inits2(10000, aMpfr, bMpfr, cMpfr, dMpfr, (mpfr_ptr)0);

		mpfr_set(aMpfr, *plane->getAMpfr(), GMP_RNDN);
		mpfr_set(bMpfr, *plane->getBMpfr(), GMP_RNDN);
		mpfr_set(cMpfr, *plane->getCMpfr(), GMP_RNDN);
		mpfr_set(dMpfr, *plane->getDMpfr(), GMP_RNDN);

		mpfr_div(aMpfr, aMpfr, cMpfr, GMP_RNDN);
		mpfr_neg(aMpfr, aMpfr, GMP_RNDN);
		mpfr_div(bMpfr, bMpfr, cMpfr, GMP_RNDN);
		mpfr_neg(bMpfr, bMpfr, GMP_RNDN);
		mpfr_div(dMpfr, dMpfr, cMpfr, GMP_RNDN);
		mpfr_neg(dMpfr, dMpfr, GMP_RNDN);

		mpfr_set(fa, aMpfr, GMP_RNDN);
		mpfr_set(fb, bMpfr, GMP_RNDN);
		mpfr_set(fc, dMpfr, GMP_RNDN);

		mpfr_clears(aMpfr, bMpfr, cMpfr, dMpfr, (mpfr_ptr)0);

		delete plane;
		delete P1;
		delete P2;
		delete P3;
	}

	void Atan2Table::generateTaylorOrder1Parameters(int x, int y, mpfr_t &fa, mpfr_t &fb, mpfr_t &fc)
	{
		mpfr_t centerValI, centerValJ, increment, temp, temp2;
		int k = (wIn+1)/2;

		mpfr_inits2(10000, centerValI, centerValJ, increment, temp, temp2, (mpfr_ptr)0);

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
		//create A
		mpfr_set(fa, centerValJ, GMP_RNDN);
		mpfr_div(fa, fa, temp, GMP_RNDN);
		mpfr_neg(fa, fa, GMP_RNDN);
		//create B
		mpfr_set(fb, centerValI, GMP_RNDN);
		mpfr_div(fb, fb, temp, GMP_RNDN);
		//create C
		mpfr_atan2(fc, centerValJ, centerValI, GMP_RNDN);

		mpfr_clears(centerValI, centerValJ, increment, temp, temp2, (mpfr_ptr)0);
	}

	void Atan2Table::generateTaylorOrder2Parameters(int x, int y, mpfr_t &fa, mpfr_t &fb, mpfr_t &fc, mpfr_t &fd, mpfr_t &fe, mpfr_t &ff)
	{
		mpfr_t centerValI, centerValJ, increment, temp, tempSqr, temp2;
		int k = (wIn+1)/2;

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







