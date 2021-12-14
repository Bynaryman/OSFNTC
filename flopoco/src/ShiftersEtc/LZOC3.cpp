/*
  A leading zero/one counter for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2011.
  All rights reserved.
*/
#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "LZOC3.hpp"

using namespace std;

namespace flopoco{
	void LZOC3::simpleTableCounting() {
		string dontCare(wIn_, '-');

		vhdl << "with countOnes select " << 
			declare(getTarget()->logicDelay(getTarget()->lutInputs()), "pretreated_input", wIn_) << 
			" <= I when '0'," << endl <<
			tab << "not(I) when '1', " << endl <<
			tab << "\"" << dontCare << "\" when others;" << endl;

		int nbValues = 1 << wIn_;
		vector<mpz_class> tableValues(nbValues);
		int currentCount = wIn_;
		int next = 1;
		for (int i = 0 ; i < nbValues ; ++i) {
			if (i >= next) {
				currentCount -= 1;
				next <<= 1;
			}
			tableValues[i] = mpz_class(currentCount);
		}
		
		Table* table = new Table(this, getTarget(), tableValues, "LUT_LZC", wIn_, wOut_);
		table->setShared();

		inPortMap("X", "pretreated_input");
		outPortMap("Y", "O");

		vhdl << instance(table, "LUT_Count");
	}

	void LZOC3::generateFinalBits(const string & entryName, const string& outName, int totalSize) {
		ostringstream name, concatenation, outConcat;

		for (int i = 0 ; i < wOut_ - lowBitCodeSize_ ; ++i) { // For each bit of the rsesult
			concatenation.str("");
			size_t nbBits = 0;
			for(auto j = 1 << i ; j <= totalSize ; j += (1 << (i+1))) { //Start from the bit value
				for(auto k = 0 ; k < (1 << i) and (j+k) <= totalSize; ++k) {
					if (j > (1 <<i) or k > 0) {
						concatenation << " or ";
					}
					// If the binary representation of j+k as the bit i set, add
					// it to the concatenation
					concatenation << entryName << of(j+k - 1);
					nbBits++;
				}
			}
			//nbBits should be smaller than totalSize / 2

			name.str("");
			name << "res_" << i;

			vhdl << declare(getTarget()->adderDelay(totalSize/2 + 1), name.str(), 1, false) <<
				"<=" << concatenation.str() << ";" << endl;
		}

		int highBitsSize = wOut_ - lowBitCodeSize_;

		for (int i = highBitsSize - 1 ; i > 0 ; --i) {
			outConcat << "res_" << i << " & ";
		}

		outConcat << "res_0";
		string rangeMap;
		if (highBitsSize == 1) {
			rangeMap = of(0);
		} else {
			rangeMap = range(highBitsSize - 1, 0);
		}
	
		//Concatenate all the bits to get the count
		vhdl << declare(.0, outName, highBitsSize) << rangeMap << " <= " 
			<< outConcat.str() << ";" << endl;
	}

	Table* LZOC3::buildDecodingTable(int nbLowBits) {
		vector<mpz_class> tableValues(1 << (nbLowBits + 2), mpz_class(0));	
		
		int offset = 3 << nbLowBits;
		for (int i = 1; i < (1 << nbLowBits); ++i) {
			tableValues[i + offset] = mpz_class(i);	
		}

		return new Table(this, getTarget(), tableValues, "cleanDecoder", nbLowBits+2, nbLowBits);
	}

	Table* LZOC3::buildEncodingTable(int blockSize, int nbLowBits) {
		vector<mpz_class> tableValues(1 << blockSize, mpz_class(0));
		
		int value = blockSize;
		int curIdx = 0;

		for (int i = 0 ; i < blockSize ; ++i) {
			int nextIdx = 1 << i;
			for(; curIdx < nextIdx; ++curIdx) {
				tableValues[curIdx] += value % blockSize;
			}
			value -= 1;
		}

		value = 1;
		curIdx = 1 << (blockSize - 1);

		for (int i = 2 ; i <= blockSize; ++i) {
			int nextIdx = curIdx + (1 << (blockSize - i));
			for(; curIdx < nextIdx; ++curIdx) {
				tableValues[curIdx] += value % blockSize;
			}	
			value += 1;
		}
		//TODO set the good encodings for the ones

		tableValues[0] += 1 << nbLowBits;
		tableValues[(1 << blockSize) - 1] += 1 << nbLowBits;

		return new Table(this, getTarget(), tableValues, "oneEncoding", blockSize, nbLowBits + 1);
	}

	LZOC3::LZOC3(Operator* parentOp, Target* target, int wIn, bool useMaxLut) :
		Operator(parentOp, target), wIn_(wIn) {

		srcFileName = "LZOC3";
		setCopyrightString("ACME Corp(2018)");

		ostringstream name; 
		name <<"LZOC3_"<< wIn_;

		if (useMaxLut) {
			name << "_max_lut";
		} else {
			name << "_standard_lut";
		}

		setNameWithFreqAndUID(name.str());

		// -------- Parameter set up -----------------
		wOut_ = intlog2(wIn);

		addInput("I", wIn_);
		addInput("countOnes", 1, false);
		addOutput("O", wOut_);


		auto lutWidth = target->lutInputs();
		if (useMaxLut) {
			lutWidth = target->maxLutInputs();
		}

		if (wIn_ <= lutWidth) {
			simpleTableCounting();
			return;
		}
		
		// Find the highest power of two we can fit on a non routed LUT
		blockSize_ = 1 << (intlog2(lutWidth));
		if (blockSize_ > lutWidth)
			blockSize_ >>= 1;
		lowBitCodeSize_ = intlog2(blockSize_ - 1);

		auto remainder = wIn_ % blockSize_;

				// Padding the inputs with constant ones to avoid the handling of
		// smaller inputs 
		// TODO : test if the tools optimise it or if special care is needed 
		ostringstream padStr;
		if (remainder == 0) 
			padStr<<"";
		else 
			padStr << "& ("<< (blockSize_- remainder) - 1  << " downto 0 => not(countOnes))";

		auto totalSize = (wIn_ / blockSize_) * blockSize_;
		if (remainder != 0) 
			totalSize += blockSize_;

		vhdl << tab << declare(0.0, "PaddedInput", totalSize) << 
			"<= I" << padStr.str() <<";"<<endl; //one padding if necessary 

		auto table = buildEncodingTable(blockSize_, lowBitCodeSize_);
		table->setShared();

		ostringstream outputName, secondOutputName, thirdOutputName;
		ostringstream concatenation;
		ostringstream secondName, thirdName;

		auto nbBlocks = totalSize / blockSize_;

		//Mapping the entry block to the encoding
		addFullComment("Splitting entry and computing the encoding");

		for (int i = 0; i < nbBlocks; ++i) {
			name.str("");
			name << "entry_block_" << i;
			vhdl << declare(0., name.str(), blockSize_) << "<= PaddedInput" << 
				range((i + 1) * blockSize_- 1, i * blockSize_) << ";" << endl;


			outputName.str("");
			outputName << "sub_block_" << i;

			inPortMap("X", name.str());
			outPortMap("Y", outputName.str());
			
			secondName.str("");
			secondName << "table_encoding_" << i;
			vhdl << instance(table, secondName.str());


			thirdName.str("");
			thirdName << "logic_bits_" << i;
			
			vhdl << declare(target->logicDelay(3), thirdName.str(), 2) <<
				" <= (not(countOnes xor " << name.str() << of(blockSize_ - 1) << 
				")) & (" << outputName.str() << of(lowBitCodeSize_) << 
				" and not(countOnes xor " << name.str() << of(blockSize_ - 1) <<
				"));" << endl;

			name.str("");
			name << "propagate_bit_" << i;

			secondName.str("");
			secondName << "enable_block_" << i;

			vhdl << declare(0., name.str(), 1, false) << " <= " << thirdName.str() <<
				of(0) << ";" << endl;

			vhdl << declare(0., secondName.str(), 1, false) << "<= " << 
				thirdName.str() << of(1) << ";" << endl;

			secondOutputName.str("");
			secondOutputName << "one_encoding_" << i;
			vhdl << declare(0.0, secondOutputName.str(), 2) << " <= " << 
				name.str() << " & '1';" << endl; 

			concatenation << secondOutputName.str();
			if (i < nbBlocks - 1)
				concatenation << " & ";

			thirdOutputName.str("");
			thirdOutputName << "low_bit_count_" << i; 

			vhdl << declare(0., thirdOutputName.str(), lowBitCodeSize_) << "<=" <<
				outputName.str() << range(lowBitCodeSize_ - 1, 0) << ";" << endl << endl;
		}

		addFullComment("Propagating the adjacent block information");

		vhdl << declare(target->adderDelay(2 * nbBlocks + 1, true), "propagated_result", 2 * nbBlocks + 1) << 
			"<= 1 + unsigned(\"0\" &" << concatenation.str() << ");" << endl;	

		vhdl << endl;

		concatenation.str("");
		concatenation << "propagated_result" << of(2*nbBlocks) << " & ";

		auto decoder = buildDecodingTable(lowBitCodeSize_);
		decoder->setShared();

		addFullComment("Removing the not propagated ones");

		// Clean the noisy ones
		for (int i = 0 ; i < nbBlocks; ++i) {
			auto curBlock = nbBlocks - 1 - i; 
			outputName.str("");
			outputName << "active_msbit_" << curBlock;

			auto down = i * 2;
			auto top = down + 1;

			vhdl << declare(target->logicDelay(2), outputName.str(), 1, false) << 
				"<= propagated_result" << of(top) << " and (not (propagated_result" <<
				of(down) << "));" << endl; 

			//Condition encoding here : TODO

			name.str("");
			name << "complete_input_" << curBlock;

			vhdl << declare(0., name.str(), lowBitCodeSize_ + 2) << " <= " <<
				 "enable_block_" << curBlock << " & " << outputName.str() << 
				 " & low_bit_count_" << curBlock << ";" << endl; 

			outputName.str("");
			outputName << "decoder_" << curBlock;

			secondOutputName.str("");
			secondOutputName << "cleaned_low_bit_" << curBlock;

			inPortMap("X", name.str());
			outPortMap("Y", secondOutputName.str());

			vhdl << instance(decoder, outputName.str()) << endl;
			
			concatenation << "active_msbit_" << i;
			if (i < nbBlocks - 1) {
				concatenation << " & ";
			}
			 vhdl << endl;
		}

		addFullComment("Gathering the msb one-hot encoding of the result");
		vhdl << declare(.0, "oneEncodingMSB", nbBlocks + 1) << "<=" <<
			concatenation.str() << ";" << endl;

		// discard right bit
		vhdl << declare(.0, "extended_val", nbBlocks) << "<= oneEncodingMSB" <<
			range(nbBlocks, 1) << ";" << endl << endl;

		string tempBufName = "high_weights_bits";

		addFullComment("Convert the one-hot encoding to base two encoding");
		generateFinalBits("extended_val", tempBufName, nbBlocks);

		ostringstream orstring;
		concatenation.str("");
		concatenation << tempBufName << " & ";

		addFullComment("OR-ing the cleaned lsb to get the result lsb");
		// lowBits wide or
		for (int i = 0 ; i < lowBitCodeSize_; ++i) {
			orstring.str("");
			for (int j = 0 ; j < nbBlocks ; ++j) {
				orstring << "cleaned_low_bit_" << j << of(i);
				if (j < nbBlocks - 1)
					orstring << " or ";
			}
			
			name.str("");
			name << "lsbres_" << i;
			vhdl << declare(target->adderDelay(nbBlocks), name.str(), 1, false) <<
				"<=" << orstring.str() << ";" << endl << endl;	
			concatenation << "lsbres_" << (lowBitCodeSize_ - 1 - i);
			if (i < lowBitCodeSize_ - 1)
				concatenation << " & ";
		}

		addFullComment("Concatenare msb and lsb");
		vhdl << "O" << "<=" << concatenation.str() << ";" << endl;
	}

	LZOC3::~LZOC3() {}

	void LZOC3::emulate(TestCase* tc)
	{
		mpz_class si   = tc->getInputValue("I");
		mpz_class sozb = tc->getInputValue("countOnes");
		mpz_class so;

		int j;
		int bit = (sozb == 0) ? 0 : 1;
		for (j = 0; j < wIn_; j++)
			{
				if (mpz_tstbit(si.get_mpz_t(), wIn_ - j - 1) != bit)
					break;
			}

		so = j;
		tc->addExpectedOutput("O", so);
	}

	OperatorPtr LZOC3::parseArguments(OperatorPtr parentOp, Target *target, std::vector<std::string> &args) {
		int wIn;
		bool useMaxLut;
		UserInterface::parseStrictlyPositiveInt(args, "wIn", &wIn);
		UserInterface::parseBoolean(args, "useLargeLut", &useMaxLut);
		return new LZOC3(parentOp, target, wIn, useMaxLut);
	}

	void LZOC3::registerFactory(){
		UserInterface::add("LZOC3", // name
				"A leading zero counter. The output size is computed.",
				"ShiftersLZOCs", // category
				"",
				"wIn(int): input size in bits;\
				useLargeLut(bool)=false: Use max unrouted lut size to build the encoding;", // This string will be parsed
				"", // no particular extra doc needed
				LZOC3::parseArguments
				) ;
	}
}
