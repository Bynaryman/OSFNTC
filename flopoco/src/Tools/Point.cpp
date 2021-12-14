/*
	A point in the 3D space

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
#include "Operator.hpp"
#include "Point.hpp"

using namespace std;

namespace flopoco {

	Point::Point(double _x, double _y, double _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}

	Point::Point(Point* P)
	{
		x = P->getX();
		y = P->getY();
		z = P->getZ();
	}

	double Point::getX()
	{
		return x;
	}

	void Point::setX(double newX)
	{
		x = newX;
	}

	double Point::getY()
	{
		return y;
	}

	void Point::setY(double newY)
	{
		y = newY;
	}

	double Point::getZ()
	{
		return z;
	}

	void Point::setZ(double newZ)
	{
		z = newZ;
	}


	Point::~Point()
	{

	}

}
