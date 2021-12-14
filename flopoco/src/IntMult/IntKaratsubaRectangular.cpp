#include <iostream>

#include "IntKaratsubaRectangular.hpp"
#include "IntMult/DSPBlock.hpp"
#include "assert.h"

using namespace std;

namespace flopoco{


IntKaratsubaRectangular:: IntKaratsubaRectangular(Operator *parentOp, Target* target, int wX, int wY, bool useKaratsuba, bool useRectangularTiles) :
	Operator(parentOp,target), wX(wX), wY(wY), wOut(wX+wY), useKaratsuba(useKaratsuba), useRectangularTiles(useRectangularTiles)
{

    ostringstream name;
    name << "IntKaratsubaRectangular_" << wX << "x" << wY;
    setCopyrightString("Martin Kumm");
    setNameWithFreqAndUID(name.str());

    useNumericStd();

    addInput ("X", wX);
    addInput ("Y", wY);
    addOutput("R", wOut);


	if(useRectangularTiles)
	{
		TileBaseMultiple=8;
		TileWidthMultiple=2;
		TileHeightMultiple=3;
		TileWidth=TileBaseMultiple*TileWidthMultiple;
		TileHeight=TileBaseMultiple*TileHeightMultiple;
		REPORT(INFO, "using rectangular tiles of size " << TileWidth << "x" << TileHeight << " " << (useKaratsuba==1 ? "with" : "without") << " Karatsuba");
	}
	else
	{
		TileBaseMultiple=17;
		TileWidthMultiple=1;
		TileHeightMultiple=1;
		TileWidth=TileBaseMultiple*TileWidthMultiple;
		TileHeight=TileBaseMultiple*TileHeightMultiple;
		REPORT(INFO, "using rectangular tiles of size " << TileWidth << "x" << TileHeight << " " << (useKaratsuba==1 ? "with" : "without") << " Karatsuba");
	}


    for(int x=0; x < wX/TileWidth; x++)
    {
        vhdl << tab << declare("a" + to_string(TileWidthMultiple*x),TileWidth) << " <= X(" << (x+1)*TileWidth-1 << " downto " << x*TileWidth << ");" << endl;
    }
    for(int y=0; y < wY/TileHeight; y++)
    {
        vhdl << tab << declare("b" + to_string(TileHeightMultiple*y),TileHeight) << " <= Y(" << (y+1)*TileHeight-1 << " downto " << y*TileHeight << ");" << endl;
    }

	bitHeap = new BitHeap(this, wOut+10);

//	double maxTargetCriticalPath = 1.0 / getTarget()->frequency() - getTarget()->ffDelay();
//	multDelay = 3*maxTargetCriticalPath;
	multDelay = 0;

	if(useRectangularTiles)
	{
		if(wX == 64 && wY == 72)
		{
			//multipliers required for sharing in Karatsuba
			createMult(0, 0);
			createMult(2, 0);
			createMult(0, 3);
			createMult(4, 0);
			createMult(2, 3);
			createMult(4, 3);
			createMult(2, 6);
			createMult(6, 3);
			createMult(4, 6);
			createMult(6, 6);

			//applying the Karatsuba shared multipliers:
			createRectKaratsuba(0,6,6,0);
		}
		else if(wX == 96 && wY == 96)
		{
			//multipliers required for sharing in Karatsuba
			createMult(0, 0);
			createMult(2, 0);
			createMult(0, 3);
			createMult(4, 0);
			createMult(2, 3);
			createMult(4, 3);
			createMult(6, 6);
			createMult(8, 6);
			createMult(6, 9);
			createMult(10, 6);
			createMult(8, 9);
			createMult(10, 9);

			//applying the Karatsuba shared multipliers:
			createRectKaratsuba(0,6,6,0);
			createRectKaratsuba(2,6,8,0);
			createRectKaratsuba(0,9,6,3);
			createRectKaratsuba(4,6,10,0);
			createRectKaratsuba(2,9,8,3);
			createRectKaratsuba(4,9,10,3);
		}
		else if(wX == 112 && wY == 120)
		{
			//multipliers required for sharing in Karatsuba
			createMult(0, 0);
			createMult(2, 0);
			createMult(4, 0);
			createMult(0, 3);
			createMult(2, 3);
			createMult(4, 3);
			createMult(6, 6);
			createMult(8, 6);
			createMult(10, 6);
			createMult(6, 9);
			createMult(8, 9);
			createMult(10, 9);
			createMult(12, 12);

			//applying the Karatsuba shared multipliers:
			createRectKaratsuba(0,6,6,0);
			createRectKaratsuba(2,6,8,0);
			createRectKaratsuba(4,6,10,0);
			createRectKaratsuba(0,9,6,3);
			createRectKaratsuba(2,9,8,3);
			createRectKaratsuba(4,9,10,3);
			createRectKaratsuba(0,12,12,0);
			createRectKaratsuba(6,12,12,6);

			//multipliers not used for Karatsuba:
			createMult(12, 3);
			createMult(12, 9);
			createMult(2, 12);
			createMult(4, 12);
			createMult(8, 12);
			createMult(10, 12);
		}
		else
		{
			THROWERROR("There is no predifined solution for the given input word sizes, sorry.")
		}
	}
	else
	{
		if((wX % TileWidth != 0) || (wY % TileHeight != 0)) THROWERROR("Input word size must be divisable by " << TileWidth << ", sorry");
		if(wX != wY) THROWERROR("X and Y input word sizes must be identical in square mode, sorry");

		int noOfTilesPerRow = wX/TileWidth;
		int noOfTilesPerCol = wY/TileHeight;

		assert(noOfTilesPerRow == noOfTilesPerCol);

		for(int i=0; i < noOfTilesPerRow; i++)
		{
			createMult(i, i);
		}
		for(int y=0; y < noOfTilesPerCol; y++)
		{
			for(int x=y+1; x < noOfTilesPerRow; x++)
			{
				createRectKaratsuba(y,x,x,y);
			}
		}
	}
    //compress the bitheap
    bitHeap -> startCompression();

    vhdl << tab << "R" << " <= " << bitHeap->getSumName() <<
            range(wOut-1, 0) << ";" << endl;

}

void IntKaratsubaRectangular::createMult(int i, int j)
{
/*
	vhdl << tab << declare(multDelay,"c" + to_string(i) + "_" + to_string(j),TileWidth+TileHeight) << " <= std_logic_vector(unsigned(a" <<  + i << " ) * unsigned(b" <<  + j << "));" << endl;
*/
	REPORT(DEBUG, "implementing a" << i << " * b" << j << " with weight " << (i+j)*TileBaseMultiple << " (" << (i+j) << " x " << TileBaseMultiple << ")");
	if(!isSignalDeclared("a" + to_string(i) + "se"))
		vhdl << tab << declare("a" + to_string(i) + "se",18) << " <= std_logic_vector(resize(unsigned(a" << i << "),18));" << endl;
	if(!isSignalDeclared("b" + to_string(j) + "se"))
		vhdl << tab << declare("b" + to_string(j) + "se",25) << " <= std_logic_vector(resize(unsigned(b" << j << "),25));" << endl;
	if(!isSignalDeclared("zero25"))
		vhdl << tab << declare("zero25",25) << " <= (others => '0');" << endl;

	newInstance( "DSPBlock", "dsp" + to_string(i) + "_" + to_string(j), "wX=25 wY=18 usePreAdder=1 preAdderSubtracts=1 isPipelined=0 xIsSigned=1 yIsSigned=1","X1=>b" + to_string(j) + "se, X2=>zero25, Y=>a" + to_string(i) + "se", "R=>c" + to_string(i) + "_" + to_string(j));


	bitHeap->addSignal("c" + to_string(i) + "_" + to_string(j),(i+j)*TileBaseMultiple);
}

void IntKaratsubaRectangular::createRectKaratsuba(int i, int j, int k, int l)
{
	REPORT(FULL, "createRectKaratsuba(" << i << "," << j << "," << k << "," << l << ")");
	assert(i+j == k+l);

	if(useKaratsuba)
	{
		REPORT(INFO, "implementing a" << i << " * b" << j << " + a" << k << " * b" << l << " with weight " << (i+j) << " as (a" << i << " - a" << k << ") * (b" << j << " - b" << l << ") + a" << i << " * b" << l << " + a" << k << " * b" << j);

		if(!isSignalDeclared("d" + to_string(i) + "_" + to_string(k)))
			vhdl << tab << declare("d" + to_string(i) + "_" + to_string(k),18) << " <= std_logic_vector(signed(resize(unsigned(a" << i << ")," << 18 << ")) - signed(resize(unsigned(a" << k << ")," << 18 << ")));" << endl;
		declare("k" + to_string(i) + "_" + to_string(j) + "_" + to_string(k) + "_" + to_string(l),43);

		if(!isSignalDeclared("b" + to_string(l) + "se"))
			vhdl << tab << declare("b" + to_string(l) + "se",25) << " <= std_logic_vector(resize(unsigned(b" << l << "),25));" << endl;
		if(!isSignalDeclared("b" + to_string(j) + "se"))
			vhdl << tab << declare("b" + to_string(j) + "se",25) << " <= std_logic_vector(resize(unsigned(b" << j << "),25));" << endl;

		newInstance( "DSPBlock", "dsp" + to_string(i) + "_" + to_string(j) + "_" + to_string(k) + "_" + to_string(l), "wX=25 wY=18 usePreAdder=1 preAdderSubtracts=1 isPipelined=0 xIsSigned=1 yIsSigned=1","X1=>b" + to_string(j) + "se, X2=>b" + to_string(l) + "se, Y=>d" + to_string(i) + "_" + to_string(k), "R=>k" + to_string(i) + "_" + to_string(j) + "_" + to_string(k) + "_" + to_string(l));

		int wDSPOut;
		if(useRectangularTiles)
			wDSPOut=41; //18+24-1=41 bits necessary
		else
			wDSPOut=35; //18+18-1=35 bits necessary

		getSignalByName(declare("kr" + to_string(i) + "_" + to_string(j) + "_" + to_string(k) + "_" + to_string(l),wDSPOut))->setIsSigned();
		vhdl << tab << "kr" << i << "_" << j << "_" << k << "_" << l << " <= k" << i << "_" << j << "_" << k << "_" << l << "(" << wDSPOut-1 << " downto 0);" << endl;
		bitHeap->addSignal("kr" + to_string(i) + "_" + to_string(j) + "_" + to_string(k) + "_" + to_string(l),(i+j)*TileBaseMultiple);
		bitHeap->addSignal("c" + to_string(i) + "_" + to_string(l),(i+j)*TileBaseMultiple);
		bitHeap->addSignal("c" + to_string(k) + "_" + to_string(j),(i+j)*TileBaseMultiple);
	}
	else
	{
		createMult(i, j);
		createMult(k, l);
	}
}

void IntKaratsubaRectangular::createSquareKaratsuba(int i, int j, int k, int l)
{
}

void IntKaratsubaRectangular::emulate(TestCase* tc){
    mpz_class svX = tc->getInputValue("X");
    mpz_class svY = tc->getInputValue("Y");
    mpz_class svR = svX * svY;
    tc->addExpectedOutput("R", svR);
}

OperatorPtr IntKaratsubaRectangular::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args)
{
    int wX,wY;
	bool useKaratsuba,useRectangularTiles;

    UserInterface::parseStrictlyPositiveInt(args, "wX", &wX);
	UserInterface::parseStrictlyPositiveInt(args, "wY", &wY);
	UserInterface::parseBoolean(args, "useKaratsuba", &useKaratsuba);
	UserInterface::parseBoolean(args, "useRectangularTiles", &useRectangularTiles);

	return new IntKaratsubaRectangular(parentOp,target,wX,wY,useKaratsuba,useRectangularTiles);
}

void IntKaratsubaRectangular::registerFactory(){
    UserInterface::add("IntKaratsubaRectangular", // name
                       "Implements a large unsigned Multiplier using rectangular shaped tiles as appears for Xilinx FPGAs. Currently limited to specific, hand-optimized sizes",
                       "BasicInteger", // categories
                       "",
					   "wX(int): size of input X;wY(int): size of input Y;useKaratsuba(bool)=1: Uses Karatsuba when set to 1, instead a standard tiling without sharing is used.;useRectangularTiles(bool)=1: Uses rectangular tiles when set to 1, otherwise quadratic tiles are used",
                       "",
					   IntKaratsubaRectangular::parseArguments,
					   IntKaratsubaRectangular::unitTest
                       ) ;
}

TestList IntKaratsubaRectangular::unitTest(int index)
{
	// the static list of mandatory tests
	TestList testStateList;
	vector<pair<string,string>> paramList;

	//test Karatsuba with square tiles:
	for(int w=17; w <= 136; w+=17)
	{
		paramList.push_back(make_pair("wX", to_string(w)));
		paramList.push_back(make_pair("wY", to_string(w)));
		paramList.push_back(make_pair("useKaratsuba", "1"));
		paramList.push_back(make_pair("useRectangularTiles", "0"));
		testStateList.push_back(paramList);
		paramList.clear();
	}

	//test Karatsuba and standard tiling with rectangular tiles:
	for(int useKaratsuba=0; useKaratsuba < 2; useKaratsuba++)
	{
		paramList.push_back(make_pair("wX", "64"));
		paramList.push_back(make_pair("wY", "72"));
		paramList.push_back(make_pair("useKaratsuba", to_string(useKaratsuba)));
		testStateList.push_back(paramList);
		paramList.clear();

		paramList.push_back(make_pair("wX", "96"));
		paramList.push_back(make_pair("wY", "96"));
		paramList.push_back(make_pair("useKaratsuba", to_string(useKaratsuba)));
		testStateList.push_back(paramList);
		paramList.clear();

		paramList.push_back(make_pair("wX", "112"));
		paramList.push_back(make_pair("wY", "120"));
		paramList.push_back(make_pair("useKaratsuba", to_string(useKaratsuba)));
		testStateList.push_back(paramList);
		paramList.clear();
	}

	return testStateList;
}


}
