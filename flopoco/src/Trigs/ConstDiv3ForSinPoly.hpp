/*
  Integer division by a small constant.
  
  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon
  
  Author : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
  2008-2010.
  All rights reserved.

*/
#ifndef __ConstDiv3ForSinPoly_HPP
#define __ConstDiv3ForSinPoly_HPP
#include <vector>
#include <list>
#include <sstream>

#include "Operator.hpp"
#include "Target.hpp"
#include "Table.hpp"


namespace flopoco{


	class ConstDiv3ForSinPoly : public Operator
	{
	public:

		/** 
		 * This table inputs a number X on alpha + gammma bits, and computes its 
		 * Euclidean division by d: X=dQ+R, which it returns as the bit string 
		 * Q & R
		 */ 
		vector<mpz_class> buildEuclideanDiv3Table(int delta_, bool lastTable_);
#if 0
		class EuclideanDiv3Table: public Table 
		{
		public:
			int d;
			int alpha;
			int gamma;
			int delta;
			bool lastTable;
			mpz_class function(int x);
		};
#endif



		/** 
		 * The constructor 
		 * @param d The divisor.
		 * @param n The size of the input X.
		 * @param alpha The size of the chunk, or, use radix 2^alpha
		 */

		ConstDiv3ForSinPoly(Operator* parentOp, Target* target, int wIn, int d=3, int alpha=-1, int nbZeros=0, bool remainderOnly=false);
		~ConstDiv3ForSinPoly();
		
		// Overloading the virtual functions of Operator
		// void outputVHDL(std::ostream& o, std::string name);
		
		void emulate(TestCase * tc);

	public:
		int quotientSize();   													/**< Size in bits of the quotient output */
		int remainderSize();  													/**< Size in bits of a remainder; gamma=ceil(log2(d-1)) */

	private:
		int wIn;																/**< Size in bits of the input X */
		int d;																	/**< Divisor*/
		bool remainderOnly;												/**< if true, only the remainder will be computed. If false, quotient will be computed */
		int alpha;																/**< Size of the chunk (should be between 1 and 16)*/
		int nbZeros;															/**< Number of zero bits that should be interleaved between the bits of the number */
		int gamma;																/**< Size in bits of a remainder; gamma=ceil(log2(d-1)) */
		int qSize;																/**< Size in bits of the quotient output */
		
	};

}
#endif
