#ifndef POINT_HPP
#define POINT_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"


namespace flopoco {

	class Point {

	public:
		/**
		 * The Point constructor
		 * @param[in] x                 the X coordinate
		 * @param[in] y                 the Y coordinate
		 * @param[in] z                 the Z coordinate
		 **/
		Point(double x, double y, double z);

		/**
		 * The Point constructor
		 * @param[in] P                 the point to copy
		 **/
		Point(Point* P);

		/**
		 * return the X coordinate
		 */
		double getX();

		/**
		 * set the value of the X coordinate
		 */
		void setX(double newX);

		/**
		 * return the Y coordinate
		 */
		double getY();

		/**
		 * set the value of the Y coordinate
		 */
		void setY(double newY);

		/**
		 * return the Z coordinate
		 */
		double getZ();

		/**
		 * set the value of the Z coordinate
		 */
		void setZ(double newZ);


		/**
		 *  Destructor
		 */
		~Point();

	private:
		double x;					/**< the X coordinate */
		double y;					/**< the X coordinate */
		double z;					/**< the X coordinate */


	};

}
#endif
