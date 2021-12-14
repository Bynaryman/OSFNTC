#ifndef PLANE_HPP
#define PLANE_HPP

#include <vector>
#include <sstream>
#include <gmp.h>
#include <gmpxx.h>
#include <mpfr.h>
#include "utils.hpp"
#include "../Operator.hpp"
#include "Point.hpp"


namespace flopoco {

	class Plane {

	public:
		/**
		 * The Plane constructor: constructs the equation of the plane given by the points P1, P2, P3
		 * 	and having the form ax+by+cz+d=0
		 * @param[in] P1                 the first point
		 * @param[in] P2                 the second point
		 * @param[in] P3                 the third point
		 * @param[in] initMpfr			 whether to compute the values of the plane parameters using MPFR
		 * 									(defaults to no, to speed thing up)
		 *
		 * The equation of the plane is expressed as ax+by+cz+d=0, starting from
		 * 		|x  y  z  1|
		 * 		|x1 y1 z1 1| = 0, 				where (x1, y1, z1), (x2, y2, z2), (x3, y3, z3) are the coordinates of the points P1, P2, P3, respectively
		 * 		|x2 y2 z2 1|
		 * 		|x3 y3 z3 1|
		 * 	which is equivalent to
		 * 		|x-x1  y-y1  z-z1 |
		 * 		|x2-x1 y2-y1 z2-z1| = 0
		 * 		|x3-x1 y3-y1 z3-z1|
		 * 	By developing the equation, and by identifying the parameters, we get
		 * 		a = (y2-y1)(z3-z1) - (z2-z1)(y3-y1)
		 * 		b = (z2-z1)(x3-x1) - (x2-x1)(z3-z1)
		 * 		c = (x2-x1)(y3-y1) - (y2-y1)(x3-x1)
		 * 		d = x1[(z2-z1)(y3-y1) - (y2-y1)(z3-z1)] + y1[(x2-x1)(z3-z1) - (z2-z1)(x3-x1)] + z1[(y2-y1)(x3-x1) - (x2-x1)(y3-y1)]
		 *
		 **/
		Plane(Point* P1, Point* P2, Point* P3, bool initMpfr = false);

		/**
		 * The Plane constructor: constructs the equation of the plane given by the parameters a, b, c, d
		 * 	and having the form ax+by+cz+d=0
		 */
		Plane(double a, double b, double c, double d, bool initMpfr = false);

		/**
		 * compute the parameters of the plane, given the three points
		 */
		void computeParameters();

		/**
		 * Return true if the points P1, P2, P3 are collinear, false otherwise
		 * @param[in] P1, P2, P3         the three points
		 *
		 * The three points are collinear if the area formed by the points is zero.
		 * The area of the triangle can be computed as:
		 * 		A = 1/2*|P1P2 x P1P3|
		 * where
		 * 		P1P2 = (x2-x1)i + (y2-y1)j + (z2-z1)k
		 * 		P1P3 = (x3-x1)i + (y3-y1)j + (z3-z1)k
		 * 		P1P2 and P1P3 are vectors
		 * The cross product can be computed as:
		 * 					  |i       j     k  |
		 * 		P1P2 x P1P3 = |x2-x1 y2-y1 z2-z1|
		 * 					  |x3-x1 y3-y1 z3-z1|
		 * Computing the cross product |P1P2 x P1P3| is done as:
		 * 						 	 |y1 z1 1|^2 + |z1 x1 1|^2 + |x1 y1 1|^2
		 * 		|P1P2 x P1P3| =	sqrt(|y2 z2 1|     |z2 x2 1|     |x2 y2 1|   )
		 * 							 |y3 z3 1|     |z3 x3 1|     |x3 y3 1|
		 * Which means that all the three determinants must be equal to zero.
		 * This gives the equalities:
		 *		(x2-x1)/(x3-x1) = (y2-y1)/(y3-y1) = (z2-z1)/(z3-z1)
		 *
		 */
		bool pointsCollinear(Point* P1, Point* P2, Point* P3);

		/**
		 * Compute the distance from the point P, given as a parameter, to the plane
		 * NOTE: the functions makes the assumption that the parameters
		 * 			a, b, c and d of the plane have already been computed
		 *
		 * The distance can be computed using the formula:
		 *		d(P, plane) = (ax0 + by0 + cz0 + d)/sqrt(a^2+b^2+c^2)
		 * where x0, y0 and z0 are the coordinates of the point P
		 */
		double distanceToPlane(Point* P);

		/**
		 * Compute the distance from the point P, given as a parameter, to the plane
		 * 	using MPFR for doing the computations
		 * NOTE: the functions makes the assumption that the parameters
		 * 			a, b, c and d of the plane have already been computed
		 *
		 * The distance can be computed using the formula:
		 *		d(P, plane) = (ax0 + by0 + cz0 + d)/sqrt(a^2+b^2+c^2)
		 * where x0, y0 and z0 are the coordinates of the point P
		 */
		mpfr_t* distanceToPlaneMpfr(Point* P);

		/**
		 * return the value of a
		 */
		double getA();

		/**
		 * return the value of a, computed using MPFR
		 */
		mpfr_t* getAMpfr();

		/**
		 * return the value of b
		 */
		double getB();

		/**
		 * return the value of b, computed using MPFR
		 */
		mpfr_t* getBMpfr();

		/**
		 * return the value of c
		 */
		double getC();

		/**
		 * return the value of c, computed using MPFR
		 */
		mpfr_t* getCMpfr();

		/**
		 * return the value of d
		 */
		double getD();

		/**
		 * return the value of d, computed using MPFR
		 */
		mpfr_t* getDMpfr();


		/**
		 *  Destructor
		 */
		~Plane();

	private:
		Point* P1;					/**< The first point defining the plane */
		Point* P2;					/**< The second point defining the plane */
		Point* P3;					/**< The third point defining the plane */

		double x1, y1, z1;			/**< Coordinates of P1, for easier access */
		double x2, y2, z2;			/**< Coordinates of P2, for easier access */
		double x3, y3, z3;			/**< Coordinates of P3, for easier access */

		double a;					/**< The parameters (a, b, c and d) for the plane's equation */
		double b;
		double c;
		double d;

		mpfr_t distance;
		mpfr_t aMpfr;
		mpfr_t bMpfr;
		mpfr_t cMpfr;
		mpfr_t dMpfr;
	};

}
#endif
