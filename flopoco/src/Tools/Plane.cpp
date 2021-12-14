/*
	A plane in the 3D space

	Author: Matei Istoan

	This file is part of the FloPoCo project

	Initial software.
	Copyright © INSA Lyon, INRIA, CNRS, UCBL,
	2012-2014.
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
#include "../Operator.hpp"
#include "Plane.hpp"

using namespace std;

namespace flopoco {

	Plane::Plane(Point* P1_, Point* P2_, Point* P3_, bool initMpfr):
			P1(P1_), P2(P2_), P3(P3_),
			x1(P1_->getX()), y1(P1_->getY()), z1(P1_->getZ()),
			x2(P2_->getX()), y2(P2_->getY()), z2(P2_->getZ()),
			x3(P3_->getX()), y3(P3_->getY()), z3(P3_->getZ())
	{
		P1 = new Point(P1_);
		P2 = new Point(P2_);
		P3 = new Point(P3_);

		mpfr_inits2(10000, distance, aMpfr, bMpfr, cMpfr, dMpfr, (mpfr_ptr)0);

		computeParameters();

		if(initMpfr)
		{
			getAMpfr();
			getBMpfr();
			getCMpfr();
			getDMpfr();
		}
	}

	Plane::Plane(double a_, double b_, double c_, double d_, bool initMpfr)
	{
		a = a_;
		b = b_;
		c = c_;
		d = d_;

		P1 = NULL;
		P2 = NULL;
		P3 = NULL;

		x1 = 0; y1 = 0; z1 = 0;
		x2 = 0; y2 = 0; z2 = 0;
		x3 = 0; y3 = 0; z3 = 0;

		mpfr_inits2(10000, distance, aMpfr, bMpfr, cMpfr, dMpfr, (mpfr_ptr)0);

		if(initMpfr)
		{
			mpfr_set_d(aMpfr, a, GMP_RNDN);
			mpfr_set_d(bMpfr, b, GMP_RNDN);
			mpfr_set_d(cMpfr, c, GMP_RNDN);
			mpfr_set_d(dMpfr, d, GMP_RNDN);
		}
	}

	void Plane::computeParameters()
	{
		a = (y2-y1)*(z3-z1) - (z2-z1)*(y3-y1);
		b = (z2-z1)*(x3-x1) - (x2-x1)*(z3-z1);
		c = (x2-x1)*(y3-y1) - (y2-y1)*(x3-x1);
		d = x1*((z2-z1)*(y3-y1) - (y2-y1)*(z3-z1))
				+ y1*((x2-x1)*(z3-z1) - (z2-z1)*(x3-x1))
				+ z1*((y2-y1)*(x3-x1) - (x2-x1)*(y3-y1));
	}

	bool Plane::pointsCollinear(Point* P1, Point* P2, Point* P3)
	{
		double x1=P1->getX(), y1=P1->getY(), z1=P1->getZ();
		double x2=P2->getX(), y2=P2->getY(), z2=P2->getZ();
		double x3=P3->getX(), y3=P3->getY(), z3=P3->getZ();
		double xRatio, yRatio, zRatio;

		//sanity checks
		if(x1==x3)
			return (x1==x2);
		if(y1==y3)
			return (y1==y2);
		if(z1==z3)
			return (z1==z2);

		xRatio = (x2-x1)/(x3-x1);
		yRatio = (y2-y1)/(y3-y1);
		zRatio = (z2-z1)/(z3-z1);

		if((xRatio==yRatio) && (yRatio==zRatio) && (xRatio==zRatio))
			return true;
		else
			return false;
	}

	double Plane::distanceToPlane(Point* P)
	{
		double x0=P->getX(), y0=P->getY(), z0=P->getZ();

		return ((a*x0+b*y0+c*z0+d) / sqrt(a*a+b*b+c*c));
	}

	mpfr_t* Plane::distanceToPlaneMpfr(Point* P)
	{
		double x0 = P->getX(), y0 = P->getY(), z0 = P->getZ();
		mpfr_t temp, temp2;

		mpfr_inits2(10000, temp, temp2, (mpfr_ptr)0);

		mpfr_set(temp2, aMpfr, GMP_RNDN);
		mpfr_sqr(temp2, temp2, GMP_RNDN);
		mpfr_set(temp, temp2, GMP_RNDN);
		mpfr_set(temp2, bMpfr, GMP_RNDN);
		mpfr_sqr(temp2, temp2, GMP_RNDN);
		mpfr_add(temp, temp, temp2, GMP_RNDN);
		mpfr_set(temp2, cMpfr, GMP_RNDN);
		mpfr_sqr(temp2, temp2, GMP_RNDN);
		mpfr_add(temp, temp, temp2, GMP_RNDN);

		mpfr_sqrt(distance, temp, GMP_RNDN);

		mpfr_mul_d(temp2, aMpfr, x0, GMP_RNDN);
		mpfr_set(temp, temp2, GMP_RNDN);
		mpfr_mul_d(temp2, bMpfr, y0, GMP_RNDN);
		mpfr_add(temp, temp, temp2, GMP_RNDN);
		mpfr_mul_d(temp2, cMpfr, z0, GMP_RNDN);
		mpfr_add(temp, temp, temp2, GMP_RNDN);
		mpfr_mul_d(temp2, dMpfr, z0, GMP_RNDN);
		mpfr_add(temp, temp, temp2, GMP_RNDN);

		mpfr_div(distance, temp, distance, GMP_RNDN);

		mpfr_clears(temp, temp2, (mpfr_ptr)0);

		return &distance;
	}

	double Plane::getA()
	{
		return a;
	}

	mpfr_t* Plane::getAMpfr()
	{
		mpfr_t temp, temp2;

		mpfr_inits2(10000, temp, temp2, (mpfr_ptr)0);

		mpfr_set_d(aMpfr, y2, GMP_RNDN);
		mpfr_sub_d(aMpfr, aMpfr, y1, GMP_RNDN);
		mpfr_set_d(temp, z3, GMP_RNDN);
		mpfr_sub_d(temp, temp, z1, GMP_RNDN);
		mpfr_mul(aMpfr, aMpfr, temp, GMP_RNDN);
		mpfr_set_d(temp, z2, GMP_RNDN);
		mpfr_sub_d(temp, temp, z1, GMP_RNDN);
		mpfr_set_d(temp2, y3, GMP_RNDN);
		mpfr_sub_d(temp2, temp2, y1, GMP_RNDN);
		mpfr_mul(temp, temp, temp2, GMP_RNDN);
		mpfr_sub(aMpfr, aMpfr, temp, GMP_RNDN);

		mpfr_clears(temp, temp2, (mpfr_ptr)0);

		return &aMpfr;
	}

	double Plane::getB()
	{
		return b;
	}

	mpfr_t* Plane::getBMpfr()
	{
		mpfr_t temp, temp2;

		mpfr_inits2(10000, temp, temp2, (mpfr_ptr)0);

		b = (z2-z1)*(x3-x1) - (x2-x1)*(z3-z1);

		mpfr_set_d(bMpfr, z2, GMP_RNDN);
		mpfr_sub_d(bMpfr, bMpfr, z1, GMP_RNDN);
		mpfr_set_d(temp, x3, GMP_RNDN);
		mpfr_sub_d(temp, temp, x1, GMP_RNDN);
		mpfr_mul(bMpfr, bMpfr, temp, GMP_RNDN);
		mpfr_set_d(temp, x2, GMP_RNDN);
		mpfr_sub_d(temp, temp, x1, GMP_RNDN);
		mpfr_set_d(temp2, z3, GMP_RNDN);
		mpfr_sub_d(temp2, temp2, z1, GMP_RNDN);
		mpfr_mul(temp, temp, temp2, GMP_RNDN);
		mpfr_sub(bMpfr, bMpfr, temp, GMP_RNDN);

		mpfr_clears(temp, temp2, (mpfr_ptr)0);

		return &bMpfr;
	}

	double Plane::getC()
	{
		return c;
	}

	mpfr_t* Plane::getCMpfr()
	{
		mpfr_t temp, temp2;

		mpfr_inits2(10000, temp, temp2, (mpfr_ptr)0);

		mpfr_set_d(cMpfr, x2, GMP_RNDN);
		mpfr_sub_d(cMpfr, cMpfr, x1, GMP_RNDN);
		mpfr_set_d(temp, y3, GMP_RNDN);
		mpfr_sub_d(temp, temp, y1, GMP_RNDN);
		mpfr_mul(cMpfr, cMpfr, temp, GMP_RNDN);
		mpfr_set_d(temp, y2, GMP_RNDN);
		mpfr_sub_d(temp, temp, y1, GMP_RNDN);
		mpfr_set_d(temp2, x3, GMP_RNDN);
		mpfr_sub_d(temp2, temp2, x1, GMP_RNDN);
		mpfr_mul(temp, temp, temp2, GMP_RNDN);
		mpfr_sub(cMpfr, cMpfr, temp, GMP_RNDN);

		mpfr_clears(temp, temp2, (mpfr_ptr)0);

		return &cMpfr;
	}

	double Plane::getD()
	{
		return d;
	}

	mpfr_t* Plane::getDMpfr()
	{
		mpfr_t temp, temp2, temp3;

		mpfr_inits2(10000, temp, temp2, temp3, (mpfr_ptr)0);

		mpfr_set_d(temp, z2, GMP_RNDN);
		mpfr_sub_d(temp, temp, z1, GMP_RNDN);
		mpfr_set_d(temp2, y3, GMP_RNDN);
		mpfr_sub_d(temp2, temp2, y1, GMP_RNDN);
		mpfr_mul(temp, temp, temp2, GMP_RNDN);
		mpfr_set_d(temp2, y2, GMP_RNDN);
		mpfr_sub_d(temp2, temp2, y1, GMP_RNDN);
		mpfr_set_d(temp3, z3, GMP_RNDN);
		mpfr_sub_d(temp3, temp3, z1, GMP_RNDN);
		mpfr_mul(temp2, temp2, temp3, GMP_RNDN);
		mpfr_sub(temp3, temp, temp2, GMP_RNDN);
		mpfr_mul_d(temp3, temp3, x1, GMP_RNDN);
		mpfr_set(dMpfr, temp3, GMP_RNDN);

		mpfr_set_d(temp, x2, GMP_RNDN);
		mpfr_sub_d(temp, temp, x1, GMP_RNDN);
		mpfr_set_d(temp2, z3, GMP_RNDN);
		mpfr_sub_d(temp2, temp2, z1, GMP_RNDN);
		mpfr_mul(temp, temp, temp2, GMP_RNDN);
		mpfr_set_d(temp2, z2, GMP_RNDN);
		mpfr_sub_d(temp2, temp2, z1, GMP_RNDN);
		mpfr_set_d(temp3, x3, GMP_RNDN);
		mpfr_sub_d(temp3, temp3, x1, GMP_RNDN);
		mpfr_mul(temp2, temp2, temp3, GMP_RNDN);
		mpfr_sub(temp3, temp, temp2, GMP_RNDN);
		mpfr_mul_d(temp3, temp3, y1, GMP_RNDN);
		mpfr_add(dMpfr, dMpfr, temp3, GMP_RNDN);

		mpfr_set_d(temp, y2, GMP_RNDN);
		mpfr_sub_d(temp, temp, y1, GMP_RNDN);
		mpfr_set_d(temp2, x3, GMP_RNDN);
		mpfr_sub_d(temp2, temp2, x1, GMP_RNDN);
		mpfr_mul(temp, temp, temp2, GMP_RNDN);
		mpfr_set_d(temp2, x2, GMP_RNDN);
		mpfr_sub_d(temp2, temp2, x1, GMP_RNDN);
		mpfr_set_d(temp3, y3, GMP_RNDN);
		mpfr_sub_d(temp3, temp3, y1, GMP_RNDN);
		mpfr_mul(temp2, temp2, temp3, GMP_RNDN);
		mpfr_sub(temp3, temp, temp2, GMP_RNDN);
		mpfr_mul_d(temp3, temp3, z1, GMP_RNDN);
		mpfr_add(dMpfr, dMpfr, temp3, GMP_RNDN);

		mpfr_clears(temp, temp2, temp3, (mpfr_ptr)0);

		return &dMpfr;
	}

	Plane::~Plane()
	{
		if(P1)
			delete P1;
		if(P2)
			delete P2;
		if(P3)
			delete P3;

		mpfr_clears(distance, aMpfr, bMpfr, cMpfr, dMpfr, (mpfr_ptr)0);
	}

}
