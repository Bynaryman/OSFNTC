
/*============================================================================

This C source file is part of the SoftPosit Posit Arithmetic Package
by S. H. Leong (Cerlane).

Copyright 2017 2018 A*STAR.  All rights reserved.

This C source file was based on SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3d, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014 The Regents of the University of California.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions, and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 3. Neither the name of the University nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/


#include "platform.h"
#include "internals.h"


posit32_t i32_to_p32( int32_t iA ) {
	int_fast8_t k, log2 = 31;//length of bit (e.g. 4294966271) in int (32 but because we have only 32 bits, so one bit off to accommodate that fact)
	union ui32_p32 uZ;
	uint_fast32_t uiA;
	uint_fast32_t expA, mask = 0x80000000, fracA;
	bool sign;

	if (iA < -2147483135){ //-2147483648 to -2147483136 rounds to P32 value -2147483648
		uZ.ui = 0x80500000;
		return uZ.p;
	}

    sign = iA>>31;
    if(sign) iA = -iA &0xFFFFFFFF;

	if ( iA >2147483135)//2147483136 to 2147483647 rounds to P32 value (2147483648)=> 0x7FB00000
		uiA = 0x7FB00000;
	else if ( iA < 0x2 )
		uiA = (iA << 30);
	else {
		fracA = iA;
		while ( !(fracA & mask) ) {
			log2--;
			fracA <<= 1;
		}
		k = (log2 >> 2);
		expA = (log2 & 0x3) << (27 - k);
		fracA = (fracA ^ mask);
		uiA = (0x7FFFFFFF ^ (0x3FFFFFFF >> k)) | expA | fracA>>(k+4);

		mask = 0x8 << k;  //bitNPlusOne

		if (mask & fracA)
			if (((mask - 1) & fracA) | ((mask << 1) & fracA)) uiA++;

	}
	(sign) ? (uZ.ui = -uiA &0xFFFFFFFF) : (uZ.ui = uiA);
	return uZ.p;
}


