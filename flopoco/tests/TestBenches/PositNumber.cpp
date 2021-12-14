#define BOOST_TEST_DYN_LINK   
#define BOOST_TEST_MODULE NumberFormatTest 

#include <boost/test/unit_test.hpp>
#include <gmpxx.h>
#include "TestBenches/PositNumber.hpp"
#ifdef SOFTPOSIT
#include "softposit.h"
#endif

#include <iostream>
using namespace std;

using namespace flopoco;

BOOST_AUTO_TEST_CASE(TEST_POSIT_8_Addition)
{
	mpfr_t mpfrstore0, mpfrstore1, addres;

	mpfr_init2(mpfrstore0, 128);
	mpfr_init2(mpfrstore1, 128);
	mpfr_init2(addres, 128);

	PositNumber p0{8, 0};
	PositNumber p1{8, 0};
	PositNumber res{8, 0};

	uint8_t enc0 = 0;

	do {
		if (enc0 == (1 << 7)) {
			enc0 += 1;
			continue;
		}
		uint8_t enc1 = 0;
		p0 = mpz_class{enc0};
		p0.getMPFR(mpfrstore0);
		posit8_t sp0 = castP8(enc0);

		do {
			if (enc1 == (1 << 7)) {
				enc1 += 1;
				continue;
			}

			p1 = mpz_class{enc1};
			p1.getMPFR(mpfrstore1);
			posit8_t sp1 = castP8(enc1);

			posit8_t spres = p8_add(sp0, sp1);

			mpfr_add(addres, mpfrstore0, mpfrstore1, MPFR_RNDN);

			res = addres;

			mpz_class spresmpz{castUI(spres)};
			mpz_class pnres = res.getSignalValue();

			BOOST_REQUIRE_MESSAGE(pnres == spresmpz, "Error for encoding pair " <<
								  static_cast<uint32_t>(enc0) << " " << static_cast<uint32_t>(enc1) << "\n" <<
								  mpfr_get_d(addres, MPFR_RNDN) << "\n" << mpfr_get_d(mpfrstore0, MPFR_RNDN) << "\n" <<
								  mpfr_get_d(mpfrstore1, MPFR_RNDN) << "\nSPRes : " << spresmpz.get_str(2) <<
								  "\npnres : " << pnres.get_str(2));
			enc1 += 1;
		} while(enc1 != 0);
		enc0 += 1;
	} while (enc0 != 0);
}

void checkOreganeValues(string str1, string str2)
{
	mpz_class input1{str1, 2};
	mpz_class input2{str2, 2};

	PositNumber p1{32, 2, input1};
	PositNumber p2{32, 2, input2};

	mpfr_t mpstore1, mpstore2, mpres;
	mpfr_init2(mpstore1, 512);
	mpfr_init2(mpstore2, 512);
	mpfr_init2(mpres, 1024);

	uint32_t i1 = static_cast<uint32_t>(input1.get_ui());
	uint32_t i2 = static_cast<uint32_t>(input2.get_ui());

	p1.getMPFR(mpstore1);
	posit32_t sp1 = castP32(i1);
	double spconv = convertP32ToDouble(sp1);
	double pnconv = mpfr_get_d(mpstore1, MPFR_RNDN);

	BOOST_REQUIRE_MESSAGE(spconv == pnconv, "Wrong value decoded for input1");

	p2.getMPFR(mpstore2);
	posit32_t sp2 = castP32(i2);
	spconv = convertP32ToDouble(sp2);
	pnconv = mpfr_get_d(mpstore2, MPFR_RNDN);

	BOOST_REQUIRE_MESSAGE(spconv == pnconv, "Wrong value decoded for input2");

	mpfr_add(mpres, mpstore1, mpstore2, MPFR_RNDN);

	p1 = mpres;
	mpz_class pnres = p1.getSignalValue();
	posit32_t spaddres = p32_add(sp1, sp2);
	uint32_t spaddval = castUI(spaddres);
	mpz_class spaddsig{spaddval};

	BOOST_REQUIRE_MESSAGE(spaddsig == pnres, "SoftPosit result differs from computed result :  PositNumber result\n" <<
						  pnres.get_str(2) << "\nSoftposit result\n" << spaddsig.get_str(2));
}

BOOST_AUTO_TEST_CASE(Posit32TestAdditionSample) {
	checkOreganeValues(
				"11001100111100111101110100111011",
				"00111001100101111100001111111000");

	checkOreganeValues(
				"00011110100101000100000101000101",
				"11101100100111000111010001101110"
				);

	checkOreganeValues(
				"11111010101011101110110111100001",
				"11010101010011011100010000000101"
			);

	checkOreganeValues(
				"11010101110000101010110000011001",
				"11110100011000101100101100000110"
			);

	checkOreganeValues(
				"00000000100100010001000100001100",
				"00011100001010110000011111011100"
			);
}

BOOST_AUTO_TEST_CASE(PositTestIntConvertTwoWaysNonZeroES) {
	mpz_class z;	
	int width = 12;
	int eS = 3;
	PositNumber p(width, eS);
	for (int i = 0 ; i < (1 << width) ; ++i) {
		z = i;
		p = z;
		auto ret = p.getSignalValue();
		BOOST_REQUIRE_MESSAGE(
				ret == z, 
				"PositTestIntConvertTwoWays:: error for idx " << i << "\n value is " <<
				ret << "(" << ret.get_str(2) << ")"
			);
	}
}

BOOST_AUTO_TEST_CASE(PositTestIntConvertTwoWaysZeroES) {
	mpz_class z;	
	int width = 12;
	int eS = 0;
	PositNumber p(width, eS);
	for (int i = 0 ; i < 1 << width ; ++i) {
		z = i;
		p = z;
		auto ret = p.getSignalValue();
		BOOST_REQUIRE_MESSAGE(
				ret == z, 
				"PositTestIntConvertTwoWays:: error for idx " << i << "\n value is " <<
				ret << "(" << ret.get_str(2) << ")"
			);
	}
}

#ifdef SOFTPOSIT
BOOST_AUTO_TEST_CASE(PositTestMPFRConvertTwoWaysZeroES) {
	posit8_t pos8;
	PositNumber p(8, 0);
	uint8_t counter = 0;
	mpfr_t mpstore;
	mpfr_init2(mpstore, 53);
	do {
		pos8 = castP8(counter);
		double pos8val = convertP8ToDouble(pos8);
		//Tetsing int -> mpfr
		mpz_class tmp(counter);
		p = tmp;
		p.getMPFR(mpstore);
		double flopocoPositVal = mpfr_get_d(mpstore, MPFR_RNDN);
		BOOST_REQUIRE_MESSAGE(flopocoPositVal == pos8val, "Error for value " << 
				(int)(counter)); 
		
		//Testing mpfr -> int
		p = pos8val;
		auto sigVal = p.getSignalValue();
		BOOST_REQUIRE_MESSAGE(sigVal == counter, "Error for cast of mpfr for value " <<
			   (int)(counter));	
		counter++;
	} while (counter != 0);

	mpfr_clear(mpstore);
}

BOOST_AUTO_TEST_CASE(PositTestMPFRConvertTwoWaysPosit16) {
	posit16_t posit;
	PositNumber p(16, 1);
	uint16_t counter = 0;
	mpfr_t mpstore;
	mpfr_init2(mpstore, 53);
	do {
		posit = castP16(counter);
		double positVal = convertP16ToDouble(posit);
		mpz_class tmp(counter);
		p = tmp;
		p.getMPFR(mpstore);
		double flopocoPositVal = mpfr_get_d(mpstore, MPFR_RNDN);
		BOOST_REQUIRE_MESSAGE(flopocoPositVal == positVal, "Error for value " << 
				(int)(counter)); 
		p = positVal;
		auto sigVal = p.getSignalValue();
		BOOST_REQUIRE_MESSAGE(sigVal == counter, "Error for cast of mpfr for value " <<
			   (int)(counter));	
		counter++;
	} while (counter != 0);
	mpfr_clear(mpstore);
}

#ifdef POSIT32TEST
BOOST_AUTO_TEST_CASE(PositTestMPFRConvertTwoWaysPosit32) {
	posit32_t posit;
	PositNumber p(32, 2);
	uint32_t counter = 0;
	mpfr_t mpstore;
	mpfr_init2(mpstore, 53);
	do {
		posit = castP32(counter);
		double positVal = convertP32ToDouble(posit);
		mpz_class tmp(counter);
		p = tmp;
		p.getMPFR(mpstore);
		double flopocoPositVal = mpfr_get_d(mpstore, MPFR_RNDN);
		BOOST_REQUIRE_MESSAGE(flopocoPositVal == positVal, "Error for value " << 
				(int)(counter)); 
		p = positVal;
		auto sigVal = p.getSignalValue();
		BOOST_REQUIRE_MESSAGE(sigVal == counter, "Error for cast of mpfr for value " <<
			   (int)(counter));	
		counter++;
	} while (counter != 0);
	mpfr_clear(mpstore);
}
#endif
#endif
