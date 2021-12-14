/*
   IEEE-compatible floating-point numbers for FloPoCo

Author: L. Forget

This file is part of the FloPoCo project
developed by the Arenaire team at Ecole Normale Superieure de Lyon

Initial software.
Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
2008-2010.
All rights reserved.
*/
#include <cassert>
#include "PositNumber.hpp"

#ifndef DEBUGPOSIT
#define DEBUG_OUT(status) {}
#else
#include <iostream>
#define DEBUG_OUT(status) { std::cerr << status << std::endl; }
#endif

namespace flopoco{
	mpz_class PositNumber::twoComplement(mpz_class val) 
	{
		// Verify that we will not kill real information
		assert (val >= 0 && val < (mpz_class(1) << (width_ - 1))); 
		DEBUG_OUT("PositNumber::twoComplement(mpz_class): val=" << val <<
				" encoding : " << val.get_str(2));
		mpz_class result = val;
		result += mpz_class(1) << (width_); // Ensure we have "leading zeros" up
		//Invert the value
		mpz_class allones = (mpz_class(1) << (width_ + 1)) - 1;
		mpz_xor(result.get_mpz_t(), result.get_mpz_t(), allones.get_mpz_t());
		result += 1;	
		result = result & (allones >> 2);
		DEBUG_OUT("PositNumber::twoComplement(mpz_class): result encoding :" << 
				result.get_str(2));
		return result;
	}

	mpz_class PositNumber::constructRange(int rangeRL)
	{
		if (rangeRL < 0) {
			if (rangeRL > 1 - width_) {
				return mpz_class(1);
			} 
			assert(rangeRL == 1 - width_);
			return mpz_class(0);
		}

		mpz_class result = (mpz_class(1) << rangeRL) - 1;
		if (rangeRL < width_ - 1)
			return result * 2;
		assert(rangeRL == width_ - 1);
		return result;
	}

	PositNumber::PositNumber(int width, int eS):width_(width), eS_(eS)
	{
		if (width < 2) {
			throw std::string("PositNumber::constructor: width should be greater than 2");
		}
	}

	PositNumber::PositNumber(int width, int eS, mpfr_t m):PositNumber(width, eS)
	{
		*this = m;
	}

	PositNumber::PositNumber(int width, int eS, SpecialValue v):PositNumber(width, eS)	
	{
		switch(v)  {
			case NaR: 
				sign_ = 1;
				rangeRL_ = width - 1;
				break;
			case Zero: 
				sign_ = 0;
				rangeRL_ = -width + 1;
				break;
		}
		exponentShift_ = 0;
		mantissa_ = 0;
	}

	PositNumber::PositNumber(int width, int eS, mpz_class signalValue)
		:PositNumber(width, eS)
	{
		*this = signalValue;
	}

	/**
	 * Converts the currently stored PositNumber to an mpfr_t
	 * @param[out] m a preinitialized mpfr_t where to store the floating point
	 */
	void PositNumber::getMPFR(mpfr_t m) 
	{
		//TODO throw exception if posit doesn't fit in MPFR
		if (mantissa_ == 0) {
			if (sign_ == 1) {
				DEBUG_OUT("PositNumber::getMPFR(mpfr_t): set infinity");
				mpfr_set_inf(m, 1);
			} else {
				DEBUG_OUT("PositNumber::getMPFR(mpfr_t): set zero");
				mpfr_set_zero(m, 1);
			}
			return;
		}

		int occupied_bits = eS_ + abs(rangeRL_) + 2;
		size_t precision = (occupied_bits > width_) ? 0 : width_ - occupied_bits;
		precision += 1;
		DEBUG_OUT("PositNumber::getMPFR(mpfr_t): precision="<<precision);

		int64_t range = rangeRL_;
		if (range > 0) {
			range -= 1;
		}
		int64_t exp = range * (1 << eS_) + exponentShift_ + 1;

		exp -= precision;
		DEBUG_OUT("PositNumber::getMPFR(mpfr_t): exponent="<<exp);
		mpfr_set_prec(m, precision);
		mpfr_set_z_2exp(m, mantissa_.get_mpz_t(), exp, MPFR_RNDN);	
		if (sign_== 1) {
			mpfr_neg(m, m, MPFR_RNDN);
		}
	}

	mpz_class PositNumber::getSignalValue()
	{
		/* Sanity checks */
		if ((sign_ != 0) && (sign_ != 1))
			throw std::string("PositNumber::getSignal: sign is invalid.");

		int rangeMaxWidth = width_ - 1;
		if (mantissa_ == 0) {
			return sign_ << (width_ - 1);
		}
		if (rangeRL_ < -rangeMaxWidth || rangeRL_ > rangeMaxWidth) 
			throw std::string("PositNumber::getSignal: range is invalid");

		int rangeWidth = abs(rangeRL_) + 1; // Encoding of range r is of length r + 1

		// But we don't need the extra bit when the range fill the remaining
		// bits
		if (rangeWidth > width_ - 1)
			rangeWidth = width_ - 1;

		DEBUG_OUT("PositNumber::getSignalValue(): rangeWidth=" << rangeWidth);

		int eSWidth = eS_;
		if (width_ - (1 + rangeWidth) < eSWidth) 
			eSWidth = width_ - (1 + rangeWidth);
		size_t padWidth = eS_ - eSWidth; 
		DEBUG_OUT("PositNumber::getSignalValue(): eSWidth=" << eSWidth);
		DEBUG_OUT("PositNumber::getSignalValue(): padWidth=" << padWidth);

		if (exponentShift_ < 0 ||
				exponentShift_ >= (1 << eS_) || 
				(eSWidth == 0 && exponentShift_ != 0) ||
				(eSWidth != 0 && padWidth != 0 && (exponentShift_ % (1 << padWidth)) != 0)
		   ) {
			throw std::string("PositNumber::getSignal: exponentShift is Invalid");
		}

		size_t mantissaWidth = width_ - (1 + rangeWidth + eSWidth); 

		if (mantissa_ >= (1 << (mantissaWidth + 1)) || 
				(mantissaWidth > 0 && mantissa_ < (1 << mantissaWidth)) || 
				mantissa_ < 0 
		   ) {
			throw std::string("PositNumber::getSignal: mantissa is invalid");
		}

		DEBUG_OUT("PositNumber::getSignalValue(): mantissaWidth=" << mantissaWidth);

		mpz_class result = constructRange(rangeRL_);
		DEBUG_OUT("PositNumber::getSignalValue(): RangeEncoding=" << 
				result.get_str(2));

		if (eSWidth > 0) {
			result <<= eSWidth;
			DEBUG_OUT("PositNumber::getSignalValue(): padded_result: " <<
				   result.get_str(2));	
			DEBUG_OUT("PositNumber::getSignalValue(): exponentShift_ encoding : " << 
					mpz_class(exponentShift_).get_str(2));
			auto paddedES = exponentShift_ >> padWidth;
			result |= paddedES;
			DEBUG_OUT("PositNumber::getSignalValue():"
					" Range + exponentShift encoding : " << result.get_str(2));
		}

		if (mantissaWidth > 0) {
			mpz_class beheadedMantissa = mantissa_ & ((mpz_class(1) << mantissaWidth) - 1);
			result <<= mantissaWidth;
			result += beheadedMantissa;
		}

		if (sign_ == 1) { // If sign is negative, compute 2 complement
			result = twoComplement(result);
			result |= mpz_class(1) << (width_ - 1);
		}
		return result;
	}

	PositNumber& PositNumber::operator=(double d)
	{
		mpfr_t value;
		mpfr_init2(value, 53);
		mpfr_set_d(value, d, MPFR_RNDN);
		assert(d == mpfr_get_d(value, MPFR_RNDN));
		*this = value;
		mpfr_clear(value);
		return *this;
	}

	PositNumber& PositNumber::operator=(mpfr_t mp_)
	{
		mpfr_t mp, minpos, maxpos;
		mpfr_init2(mp, mpfr_get_prec(mp_));
		mpfr_set(mp, mp_, GMP_RNDN);
		mpfr_init2(minpos, 1);
		mpfr_init2(maxpos, 1);
		mpfr_set_ui(minpos, 1, MPFR_RNDN);
		mpfr_set_ui(maxpos, 1, MPFR_RNDN);

		/* NaR */
		if (mpfr_nan_p(mp) || mpfr_inf_p(mp)) {
			DEBUG_OUT("PositNumber::operator=(mpfr_t): NaR")
			sign_ = 1;
			exponentShift_ = 0;
			mantissa_ = 0;
			rangeRL_ = width_ - 1;
			mpfr_clear(mp);
			mpfr_clear(minpos);
			mpfr_clear(maxpos);
			return *this;
		}

		/*Zero*/
		if (mpfr_zero_p(mp)) {
			DEBUG_OUT("PositNumber::operator=(mpfr_t): Zero")
			sign_ = 0;
			rangeRL_ = 1  - width_;
			mantissa_ = 0;
			exponentShift_ = 0;
			mpfr_clear(mp);
			mpfr_clear(minpos);
			mpfr_clear(maxpos);
			return *this;
		}

		// all the other values are signed
		sign_ = mpfr_signbit(mp) == 0 ? 0 : 1;
		DEBUG_OUT("PositNumber::operator=(mpfr_t): sign_=" << sign_)

		mpfr_abs(mp, mp, GMP_RNDN);

		/*Get exponent
		  mpfr_get_exp() return exponent for significant in [1/2,1)
		  but we use [1,2). Hence the -1.
		  */
		mp_exp_t exp = mpfr_get_exp(mp)-1;
		DEBUG_OUT("PositNumber::operator=(mpfr_t): exp=" << exp)

		// Compute the number of bits used to store this exponent
		int64_t useedPower = 1 << eS_;
		DEBUG_OUT("PositNumber::operator=(mpfr_t): useedPower=" << useedPower)

		//handle overflow
		mpfr_set_exp(minpos, (2 - width_) * useedPower);
		mpfr_set_exp(maxpos, (width_ - 1) * useedPower);

		if (mpfr_cmp(mp, maxpos) >= 0) {
			DEBUG_OUT("PositNumber::operator=(mpfr_t): positive exponent saturation")
			mantissa_ = 1;
			rangeRL_ = width_ - 1;
			exponentShift_ = 0;
			mpfr_clear(mp);
			mpfr_clear(minpos);
			mpfr_clear(maxpos);
			return *this;
		}

		if (mpfr_cmp(mp, minpos) <= 0) {
			DEBUG_OUT("PositNumber::operator=(mpfr_t): negative exponent saturation")
			mantissa_ = 1;
			rangeRL_ = 2 - width_;
			exponentShift_ = 0;
			mpfr_clear(mp);
			mpfr_clear(minpos);
			mpfr_clear(maxpos);
			return *this;
		}
		mpfr_clear(minpos);
		mpfr_clear(maxpos);

		int64_t normalRangeRL = exp / useedPower;
		int32_t expShift = exp % useedPower;

		auto precision = mpfr_get_prec(mp);

		if (exp < 0 && expShift != 0) {
			normalRangeRL -= 1;	
			expShift = useedPower + expShift;
		}

		if (normalRangeRL >= 0) {
			normalRangeRL += 1;
		}
		DEBUG_OUT("PositNumber::operator=(mpfr_t): normalRangeRL="<<normalRangeRL);
		DEBUG_OUT("PositNumber::operator=(mpfr_t): expShift="<<expShift);
		DEBUG_OUT("PositNumber::operator=(mpfr_t): precision="<<precision);

		mpfr_mul_2si(mp, mp, precision - exp, GMP_RNDN);
		mpfr_get_z(mantissa_.get_mpz_t(), mp,  GMP_RNDN);
		DEBUG_OUT("PositNumber::operator=(mpfr_t): mantissa="<< mantissa_ <<
				" encoding : " << mantissa_.get_str(2))
		//Cleaning mantissa for encoding (removing leading One)
		mantissa_ &= (mpz_class(1) << (precision)) - 1;
		DEBUG_OUT("PositNumber::operator=(mpfr_t): cleared mantissa=" << mantissa_ << " encoding : " << mantissa_.get_str(2))
		mpfr_clear(mp);

		mpz_class exactCoding = constructRange(normalRangeRL) << (eS_);
		DEBUG_OUT("PositNumber::operator=(mpfr_t): cleared rangecoding=" << exactCoding.get_str(2));
		exactCoding |= mpz_class(expShift);
		DEBUG_OUT("PositNumber::operator=(mpfr_t): cleared rangecoding|expshift=" << exactCoding.get_str(2));
		exactCoding <<= precision;
		exactCoding |= mantissa_;
		DEBUG_OUT("PositNumber::operator=(mpfr_t): exactCoding=" << exactCoding.get_str(2));
		// We have already filtered the extreme range so we know we need an
		// extra bit to store the range boundary inside the encoding
		int totalWidth = precision + 2 + eS_ + abs(normalRangeRL);
		DEBUG_OUT("PositNumber::operator=(mpfr_t): totalWidth=" << totalWidth)

		if (normalRangeRL == width_ - 1) {
			totalWidth -= 1;
		}
		if (totalWidth < width_) {
			size_t padding = width_ - totalWidth;
			exactCoding <<= padding;
			DEBUG_OUT("PositNumber::operator=(mpfr_t): padding with "<< padding << "zeros. Complete encoding is " << exactCoding.get_str(2))
		} else if (totalWidth > width_) {
			DEBUG_OUT("PositNumber::operator=(mpfr_t): Too much information, some will be truncated")
			DEBUG_OUT("PositNumber::operator=(mpfr_t): mantissa_=" << mantissa_.get_str(2))

			size_t extrabits = totalWidth - width_;
			DEBUG_OUT("PositNumber::operator=(mpfr_t): extrabits=" << extrabits)
			mpz_class truncatedCoding = exactCoding >> extrabits;
			DEBUG_OUT("PositNumber::operator=(mpfr_t): truncatedCoding=" << truncatedCoding.get_str(2))
			auto isPair = (truncatedCoding & 1) == 0;
			DEBUG_OUT("PositNumber::operator=(mpfr_t): isPair=" << isPair)
			mpz_class guardMask = mpz_class(1) << (extrabits - 1);
			auto setGuardBit = (exactCoding & guardMask) > 0;
			DEBUG_OUT("PositNumber::operator=(mpfr_t): RoundBit=" << setGuardBit)
			auto setStickyBit = (exactCoding & (guardMask - 1)) > 0;
			DEBUG_OUT("PositNumber::operator=(mpfr_t): RoundBit=" << setGuardBit)
			auto roundUp = setGuardBit and ((not isPair) or setStickyBit);
			DEBUG_OUT("PositNumber::operator=(mpfr_t): RoundUp=" << roundUp)
			exactCoding = truncatedCoding + ((roundUp) ? 1 : 0);
		}

		if (sign_ == 1) {
			DEBUG_OUT("PositNumber::operator=(mpfr_t): encoding value")
			exactCoding = twoComplement(exactCoding);
			exactCoding |= mpz_class(1) << (width_ - 1);
		}

		*this = exactCoding;
		return *this;
	}

	PositNumber& PositNumber::operator=(mpz_class signalValue)
	{
		DEBUG_OUT("PositNumber::operator=(mpz_class): affecting the value " << 
					signalValue.get_str(2))
		if (signalValue >= (mpz_class(1) << width_)) {
			throw std::string("PositNumber::constructor from mpz_class:"
					" SignalValue is too big");
		}

		if (signalValue < 0) {
			throw std::string("PositNumber::constructor from mpz_class: I don't"
					" know how to handle negative inputs");
		}

		sign_ = (signalValue >= (mpz_class(1) << (width_ - 1))) ? 1 : 0;
		signalValue &= ((mpz_class(1) << (width_ - 1)) - 1); //Clean the sign bit

		if (signalValue == 0) {
				DEBUG_OUT("PositNumber::operator=(mpz_class): Extreme case");
				exponentShift_ = 0;
				rangeRL_ = -width_ + 1;
				mantissa_ = 0;
				return *this;
		}

		DEBUG_OUT("PositNumber::operator=(mpz_class): sign=" << sign_);

		if (sign_ == 1) { //Decode with 2 complement
			DEBUG_OUT("PositNumber::operator=(mpz_class): cleaned signal : " <<
					signalValue);
			signalValue = twoComplement(signalValue);
			DEBUG_OUT("PositNumber::operator=(mpz_class): two complement decoding "
					"of value : " << signalValue.get_str(2));
		}

		mpz_class mask = mpz_class(1) << (width_ - 2);
		bool isRangeNegative = ((signalValue & mask) == 0);

		DEBUG_OUT("PositNumber::operator=(mpz_class): isRangeNegative : " << isRangeNegative);

		int rangeRL = 1;
		for (int pos = 0 ; pos < width_ - 2 ; ++pos) {
			mask >>= 1;
			bool curValue = ((signalValue & mask) == 0);
			//unset the corresponding bit is one to "clean" the signal 
			//from range
			if (curValue != isRangeNegative) {
				break;
			}
			rangeRL += 1;
		}

		rangeRL_ = rangeRL;

		if (isRangeNegative) {
			rangeRL_ *= -1;
		}

		DEBUG_OUT("PositNumber::operator=(mpz_class): rangeRL_=" << rangeRL_);
		//Handle the two extremes
		if (rangeRL == width_ - 1) {
			exponentShift_ = 0;
			mantissa_ = 1;
			return *this;
		}

		int remainingBitWidth = width_ - (rangeRL + 2);	
		//Cleaning the signal 
		signalValue &= (mpz_class(1) << remainingBitWidth) - 1;
		int shiftNum = remainingBitWidth - eS_;	
		if (shiftNum < 0)
			shiftNum = 0;
		auto expsh = signalValue;
		expsh >>= shiftNum;
		exponentShift_ = mpz_get_si(expsh.get_mpz_t());
		shiftNum = eS_ - remainingBitWidth;
		if (shiftNum < 0)
			shiftNum = 0;	
		exponentShift_ <<= shiftNum;
		DEBUG_OUT("PositNumber::operator=(mpz_class): exponentShift_=" << exponentShift_);
		remainingBitWidth = width_ - (rangeRL + 2 + eS_);	
		if (remainingBitWidth < 0)
			remainingBitWidth = 0;

		mantissa_ = (mpz_class(1) << remainingBitWidth) +
			(((mpz_class(1) << remainingBitWidth) - 1) & signalValue);

		DEBUG_OUT("PositNumber::operator=(mpz_class): mantissa_=" << mantissa_);
		return *this;
	}

	PositNumber& PositNumber::operator=(PositNumber fp)
	{
		/* Pass this through MPFR to lose precision */
		mpfr_t mp;
		mpfr_init(mp);	// XXX: Precision set in operator=
		fp.getMPFR(mp);
		operator=(mp);
		mpfr_clear(mp);
		return *this;
	}

	void PositNumber::getPrecision(int &width, int &eS)
	{
		width = width_;
		eS = eS_;
	}
}
