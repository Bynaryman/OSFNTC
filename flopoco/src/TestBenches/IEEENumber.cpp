/*
  IEEE-compatible floating-point numbers for FloPoCo

  Author: F. de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon
  
  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
  2008-2010.
  All rights reserved.

  */

#include "IEEENumber.hpp"
#include "utils.hpp"


namespace flopoco{


	IEEENumber::IEEENumber(int wE, int wF)
		: wE(wE), wF(wF)
	{
		if (wE > 30)
			throw std::string("IEEENumber::IEEENumber: Exponents larger than 30 bits are not supported.");
	}

	IEEENumber::IEEENumber(int wE, int wF, mpfr_t mp_, int ternaryRoundInfo)
		: wE(wE), wF(wF)
	{
		if (wE > 30)
			throw std::string("IEEENumber::IEEENumber: Exponents larger than 30 bits are not supported.");
		setMPFR( mp_, ternaryRoundInfo);
	}




	IEEENumber::IEEENumber(int wE, int wF, double x)
		: wE(wE), wF(wF)
	{
		mpfr_t mp;
		mpfr_init2(mp, 1+wF);
		int ternaryRoundInfo = mpfr_set_d(mp, x, MPFR_RNDN);
		setMPFR(mp, ternaryRoundInfo);
	}
	

	IEEENumber::IEEENumber(int wE, int wF, SpecialValue v)	
		: wE(wE), wF(wF)
	{
		if (wE > 30)
			throw std::string("IEEENumber::IEEENumber: Exponents larger than 30 bits are not supported.");
		switch(v)  {
		case plusInfty: 
			sign = 0;
			exponent = (mpz_class(1) << wE) -1;
			mantissa = 0;
			break;
		case minusInfty: 
			sign = 1;
			exponent = (mpz_class(1) << wE) -1;
			mantissa = 0;
			break;
		case plusZero: 
			sign = 0;
			exponent = 0;
			mantissa = 0;
			break;
		case minusZero: 
			sign = 1;
			exponent = 0;
			mantissa = 0;
			break;
		case NaN: 
			sign = getLargeRandom(1);
			exponent = (mpz_class(1) << wE) -1;
			mantissa = getLargeRandom(wF);
			if(mantissa==0) mantissa++; // we want a NaN, not an infinity
			break;
		case smallestSubNormal:
			sign = 0;
			exponent = 0;
			mantissa = mpz_class(1);
			break;
		case greatestSubNormal:
			sign = 0;
			exponent = 0;
			mantissa = (mpz_class(1) << wF) -1;
			break;
		case minusGreatestSubNormal:
			sign = 1;
			exponent = 0;
			mantissa = (mpz_class(1) << wF) -1;
			break;
		case smallestNormal:
			sign = 0;
			exponent = mpz_class(1);
			mantissa = 0;
			break;
		case greatestNormal:
			sign = 0;
			exponent = (mpz_class(1) << wE) -2;
			mantissa = (mpz_class(1) << wF) -1;
			break;
		}
		//		cout << "exponent=" << exponent << " mantissa=" << mantissa << endl;
	}


	IEEENumber::IEEENumber(int wE, int wF, mpz_class z)
		: wE(wE), wF(wF)
	{
		if (wE > 30)
			throw std::string("IEEENumber::IEEENumber: Using exponents larger than 30 bits is not supported.");
		operator=(z);
	}

	void IEEENumber::setMPFR(mpfr_t mp_, int ternaryRoundInfo){
		mpfr_t mp; // will hold a rounded copy of the number
#if 0
		if (1+wF < mpfr_get_prec(mp_))
			throw std::string("IEEENumber::setMPFR  the constructor with mpfr initialization demands for this mp nuber a precision smaller than 1+wF to avoid any rounding.");			
#endif
		// emin and emax are specified for a mantissa in (0.5, 1)
		// The formula should evaluate to -1073 for doubles, see MPFR doc;
		int emin = -(1<<(wE-1)) - wF + 3; // -1024 - 52 + 3 
		mpfr_set_emin (emin);
		// The formula should evaluatempfr_t mp to 1024 for doubles, see MPFR doc;
		int emax = (1<<(wE-1));
		mpfr_set_emax (emax);

		mpfr_init2(mp, 1+wF);
		mpfr_set(mp, mp_, MPFR_RNDN);
		mpfr_subnormalize (mp, ternaryRoundInfo, MPFR_RNDN);		
		/* NaN */
		if (mpfr_nan_p(mp))	{
				sign = 0;
				exponent = (1<<wE)-1;
				mantissa = (1<<wF)-1; // qNaN
			}
		else {
			// all the other values are signed
			sign = mpfr_signbit(mp) == 0 ? 0 : 1;

			/* Inf */
			if (mpfr_inf_p(mp))	{
				exponent = (1<<wE)-1;
				mantissa = 0;
			}
			else {

				/* Zero */
				if (mpfr_zero_p(mp)) {
					exponent = 0;
					mantissa = 0;
				}
				else{

					/* Normal and subnormal numbers */
					mpfr_abs(mp, mp, MPFR_RNDN);
					
					/* Get exponent
					 * mpfr_get_exp() return exponent for significant in [1/2,1)
					 * but we use [1,2). Hence the -1.
					 */
					mp_exp_t exp = mpfr_get_exp(mp)-1;

					//cout << "exp=" << exp <<endl;
					if(exp + ((1<<(wE-1))-1) <=0) {			// subnormal
						// TODO manage double rounding to subnormals
						exponent=0;
						/* Extract mantissa */
						mpfr_mul_2si(mp, mp, wF-1+((1<<(wE-1))-1), MPFR_RNDN);
						mpfr_get_z(mantissa.get_mpz_t(), mp,  MPFR_RNDN);
						//cout << "subnormal! " << wF + (exp + ((1<<(wE-1))-1)) << " mantissa=" << mantissa << endl;						
					}
					else { // Normal number
						/* Extract mantissa */
						mpfr_div_2si(mp, mp, exp, MPFR_RNDN); // exact operation
						mpfr_sub_ui(mp, mp, 1, MPFR_RNDN);    // exact operation
						mpfr_mul_2si(mp, mp, wF, MPFR_RNDN);  // exact operation
						mpfr_get_z(mantissa.get_mpz_t(), mp,  MPFR_RNDN); // exact operation
						
			
						// Due to rounding, the mantissa might overflow (i.e. become bigger
						// then we expect). 
						if (mantissa == mpz_class(1) << wF)
							{
								exp++;
								mantissa = 0;
							}

						if (mantissa >= mpz_class(1) << wF)
							throw std::string("Mantissa is too big after conversion to VHDL signal.");
						if (mantissa < 0)
							throw std::string("Mantissa is negative after conversion to VHDL signal.");
			
						/* Bias  exponent */
						exponent = exp + ((1<<(wE-1))-1);
						
						/* Handle overflow */
						if (exponent >= (1<<wE))
							{
								exponent = (1<<wE) -1;
								mantissa = 0;
							}
					}
				}
			}
			
		}
		mpfr_clear(mp);
	}



	mpz_class IEEENumber::getSignalValue()
	{
		/* Sanity checks */
		if ((sign != 0) && (sign != 1))
			throw std::string("IEEENumber::getSignal: sign is invalid.");
		if ((exponent < 0) || (exponent >= (1<<wE)))
			throw std::string("IEEENumber::getSignal: exponent is invalid.");
		if ((mantissa < 0) || (mantissa >= (mpz_class(1)<<wF)))
			throw std::string("IEEENumber::getSignal: mantissa is invalid.");
		return ((( sign << wE) + exponent) << wF) + mantissa;
	}



	void IEEENumber::getMPFR(mpfr_t mp)
	{

		/* NaN */
		if ( (exponent==((1<<wE)-1)) && mantissa!=0 )
			{
				mpfr_set_nan(mp);
				return;
			}

		/* Infinity */
		if ((exponent==((1<<wE)-1)) && mantissa==0)	{
			mpfr_set_inf(mp, (sign == 1) ? -1 : 1);
			return;
		}

		/* Zero and subnormal numbers */
		if (exponent==0)	{
			mpfr_set_z(mp, mantissa.get_mpz_t(), MPFR_RNDN);
			mpfr_div_2si(mp, mp, wF + ((1<<(wE-1))-2), MPFR_RNDN);
			// Sign 
			if (sign == 1)
				mpfr_neg(mp, mp, MPFR_RNDN);
			return;
		} // TODO Check it works with signed zeroes
	
		/* „Normal” numbers
		 * mp = (-1) * (1 + (mantissa / 2^wF)) * 2^unbiased_exp
		 * unbiased_exp = exp - (1<<(wE-1)) + 1
		 */
		mpfr_set_prec(mp, wF+1);
		mpfr_set_z(mp, mantissa.get_mpz_t(), MPFR_RNDN);
		mpfr_div_2si(mp, mp, wF, MPFR_RNDN);
		mpfr_add_ui(mp, mp, 1, MPFR_RNDN);
	
		mp_exp_t exp = exponent.get_si();
		exp -= ((1<<(wE-1))-1);
		mpfr_mul_2si(mp, mp, exp, MPFR_RNDN);

		// Sign 
		if (sign == 1)
			mpfr_neg(mp, mp, MPFR_RNDN);
	}


	
	
	IEEENumber& IEEENumber::operator=(mpz_class s)
	{
		//		cerr << "s=" << s << endl;
		mantissa = s & ((mpz_class(1) << wF) - 1); s = s >> wF;
		exponent = s & ((mpz_class(1) << wE) - 1); s = s >> wE;
		sign = s & mpz_class(1); s = s >> 1;

		if (s != 0)
			throw std::string("IEEENumber::operator= s is bigger than expected.");

		return *this;
	}




	void IEEENumber::getPrecision(int &wE, int &wF)
	{
		wE = this->wE;
		wF = this->wF;
	}


#if 0
	IEEENumber& IEEENumber::operator=(double x)
	{
		mpfr_t mp;
		mpfr_init2(mp, 53);
		mpfr_set_d(mp, x, MPFR_RNDN);

		operator=(mp);

		mpfr_clear(mp);

		return *this;
	}
#endif
	
}
