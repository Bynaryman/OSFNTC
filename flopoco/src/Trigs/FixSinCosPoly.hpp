#ifndef _FIXSINCOSPOLY_H
#define _FIXSINCOSPOLY_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <gmpxx.h>

#include "FixSinCos.hpp"

#include "utils.hpp"


namespace flopoco{
	class FixSinCosPoly: public FixSinCos {
	public:
		FixSinCosPoly(OperatorPtr parentOp, Target * target, int lsb);
	
		~FixSinCosPoly();

	private:
		/** Builds a table returning  sin(Addr) and cos(Addr) accurate to 2^(-lsbOut-g)
				(beware, lsbOut is positive) where Addr is on a bits. 
				If g is not null, a rounding bit is added at weight 2^((-lsbOut-1)
				argRedCase=1 for full table, 4 for quadrant reduction, 8 for octant reduction.
				Result is signed if argRedCase=1 (msbWeight=0=sign bit), unsigned positive otherwise (msbWeight=-1).
		*/
		vector<mpz_class> buildSinCosTable(int wA, int lsbOut, int g, int argRedCase);
		int w; // should be removed when refactored for lsb
	};

}
#endif // header guard

