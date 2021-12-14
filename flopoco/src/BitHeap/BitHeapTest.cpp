
#include "BitHeapTest.hpp"


using namespace std;


namespace flopoco{


	BitheapTest::BitheapTest(Target * target_, int msb_, int lsb_, int nbInputs_, bool isSigned_, int weight_)
		: Operator(target_),
		  nbInputs(nbInputs_),
		  weight(weight_),
		  size(msb_-lsb_+1),
		  msb(msb_), lsb(lsb_),
		  isSigned(isSigned_)
	{
		ostringstream name;
		int currentWidth, currentWeight;

		name << "BitheapTest_msb_" << vhdlize(msb) << "_lsb_" << vhdlize(lsb)
				<< "_nbInputs_" << vhdlize(nbInputs);
		setNameWithFreqAndUID(name.str());

		//srand(time(NULL));
		srand(1);

		if(nbInputs == -1)
		{
			nbInputs = abs(rand() % 100);
		}

		for(int i=0; i<nbInputs; i++)
		{
			currentWidth  = abs(rand() % (size-1)) + 1;

			if(weight == -1)
				currentWeight = abs(rand() % (size-1));
			else
				currentWeight = weight;
			weights.push_back(currentWeight);

			addInput(join("X", i), currentWidth, (currentWidth > 1));

			REPORT(DEBUG, "Added an input of size=" << currentWidth << " at weight=" << currentWeight);
		}
		addOutput("R", size);

		bitheap = new BitHeap(this, msb, lsb);

		for(int i=0; i<nbInputs; i++)
			bitheap->addSignal(join("X", i), weights[i]);

		bitheap->startCompression();

		vhdl << tab << "R <= " << bitheap->getSumName(size-1, 0) << ";" << endl;
	}


	BitheapTest::~BitheapTest(){
	}


	void BitheapTest::emulate(TestCase * tc)
	{
		mpz_class r = 0;

		for(int i=0; i<nbInputs; i++)
		{
			mpz_class sx = tc->getInputValue(join("X", i));

			r += sx << weights[i];
		}

		r = r % (mpz_class(1) << size);

		tc->addExpectedOutput("R", r);
	}


	OperatorPtr BitheapTest::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int msb_, lsb_, nbInputs_, weight_;
		bool isSigned_;

		UserInterface::parseInt(args, "msb", &msb_);
		UserInterface::parseInt(args, "lsb", &lsb_);
		UserInterface::parseInt(args, "nbInputs", &nbInputs_);
		UserInterface::parseBoolean(args, "isSigned", &isSigned_);
		UserInterface::parseInt(args, "weight", &weight_);

		return new BitheapTest(target, msb_, lsb_, nbInputs_, isSigned_, weight_);
	}

	void BitheapTest::registerFactory(){
		UserInterface::add("BitheapTest", // name
				"A random test generator for the bitheap.",
				"BasicInteger", // categories
				"",
				"msb(int): the msb of the bitheap; \
				lsb(int): the lsb of the bitheap; \
				nbInputs(int): the number of signals to add to the bitheap; \
				isSigned(bool): the signness of the bitheap;\
				weight(int): the weight of the inputs in the bitheap;",
				"",
				BitheapTest::parseArguments
		) ;
	}
}




