
#include "BitHeap.hpp"
#include "BitHeap/FirstFittingCompressionStrategy.hpp"
#include "BitHeap/ParandehAfsharCompressionStrategy.hpp"
#include "BitHeap/MaxEfficiencyCompressionStrategy.hpp"
#include "BitHeap/OptimalCompressionStrategy.hpp"
#include <algorithm>
namespace flopoco {

	BitHeap::BitHeap(Operator* op_, unsigned width_, string name_, int compressionType_) :
		msb(width_-1), lsb(0),
		width(width_),
		height(0),
		name(name_),
		op(op_),
		compressionType(compressionType_)
	{
		initialize();
	}


	BitHeap::BitHeap(Operator* op_, int msb_, int lsb_, string name_, int compressionType_) :
		msb(msb_), lsb(lsb_),
		width(msb_-lsb_+1),
		height(0),
		name(name_),
		op(op_),
		compressionType(compressionType_)
	{
		initialize();
	}


	BitHeap::~BitHeap() {
		//erase the bits from the bit vector, as well as from the history
		for(unsigned i=0; i<bits.size(); i++)
		{
			bits[i].clear();
		}
		bits.clear();

		for(unsigned i=0; i<bits.size(); i++)
		{
			history[i].clear();
		}
		history.clear();
	}


	void BitHeap::initialize()
	{
		stringstream s;

		srcFileName = op->getSrcFileName() + ":BitHeap"; // for REPORT to work
		guid = Operator::getNewUId();

		s << op->getName() << "_BitHeap_"<< name << "_" << guid;
		uniqueName_ = s.str();

		REPORT(DEBUG, "Creating BitHeap of width " << width);

		//set up the vector of lists of weighted bits, and the vector of bit uids
		for(unsigned i=0; i<width; i++)
		{
			bitUID.push_back(0);
			vector<Bit*> t, t2;
			bits.push_back(t);
			history.push_back(t2);
		}

		//initialize the constant bits
		constantBits = mpz_class(0);

		//initialize the VHDL code buffer
		vhdlCode.str("");

		//create a compression strategy
		compressionStrategy = nullptr;
		isCompressed = false;

		//create a Plotter object for the SVG output
		plotter = nullptr;
		drawCycleLine = true;
		drawCycleNumber = 1;
	}


	Bit * BitHeap::addBit(string name, int weight)
	{
		REPORT(FULL, "adding a bit at weight " << weight << " with rhs=" << name);

		if((weight < lsb) || (weight>msb))
		{
			REPORT(INFO, "WARNING: in addBit, weight=" << weight
					<< " out of the bit heap range ("<< msb << ", " << lsb << ")... ignoring it");
			return nullptr;
		}

		//create a new bit
		//	the bit's constructor also declares the signal
		Bit* bit = new Bit(this, name, weight, BitType::free);

		//insert the new bit so that the vector is sorted by bit (cycle, delay)
		insertBitInColumn(bit, weight-lsb);

		REPORT(DEBUG, "added bit named "  << bit->getName() << " on column " << weight
				<< " at cycle=" << bit->signal->getCycle() << " cp=" << bit->signal->getCriticalPath());

		printColumnInfo(weight);

		isCompressed = false;

		return bit;
	}


#if 0 // don't see where it is used

	void BitHeap::addBit(int weight, Signal *signal, int index)
	{
		// check this bit exists in the signal
		if((index < 0) || (index > signal->width()-1 ))
			THROWERROR("addBit: Index (=" << index << ") out of signal bit range");
		string rhs;
		if(signal->width()==1)
			rhs = signal->getName();
		else
			rhs = signal->getName() + of(index);

		addBit(weight, rhs);
	}

#endif


	void BitHeap::sortBitsInColumns(){
		for(unsigned int c = 0; c < bits.size(); c++){
			std::sort(bits[c].begin(), bits[c].end(), lexicographicOrdering);
		}
	}

	bool BitHeap::lexicographicOrdering(const Bit* bit1, const Bit* bit2){
		if( (bit1->signal->getCycle() < bit2->signal->getCycle()) ||
				( (bit1->signal->getCycle() < bit2->signal->getCycle()) &&
					(bit1->signal->getCriticalPath() < bit2->signal->getCriticalPath()) )  ){
			return true;
		}
		else{
			return false;
		}
	}

	void BitHeap::insertBitInColumn(Bit* bit, unsigned columnNumber)
	{
		vector<Bit*>::iterator it = bits[columnNumber].begin();
		bool inserted = false;

		//if the column is empty, then just insert the bit
		if(bits[columnNumber].size() == 0)
		{
			bits[columnNumber].push_back(bit);
			return;
		}

		//insert the bit in the column
		//	in the lexicographic order of the timing
		while(it != bits[columnNumber].end())
		{
			if((bit->signal->getCycle() < (*it)->signal->getCycle())
					|| ((bit->signal->getCycle() == (*it)->signal->getCycle())
							&& (bit->signal->getCriticalPath() < (*it)->signal->getCriticalPath())))
			{
				bits[columnNumber].insert(it, bit);
				inserted = true;
				break;
			}else{
				it++;
			}
		}

		if(inserted == false)
		{
			bits[columnNumber].insert(bits[columnNumber].end(), bit);
		}

		isCompressed = false;
	}


	void BitHeap::removeBit(int weight, int direction)
	{
		if((weight < lsb) || (weight > msb))
			THROWERROR("Weight " << weight << " out of the bit heap range ("<< msb << ", " << lsb << ") in removeBit");

		vector<Bit*> bitsColumn = bits[weight-lsb];

		//if dir=0 the bit will be removed from the beginning of the list,
		//	else from the end of the list of weighted bits
		if(direction == 0)
			bitsColumn.erase(bitsColumn.begin());
		else if(direction == 1)
			bitsColumn.pop_back();
		else
			THROWERROR("Invalid direction for removeBit: direction=" << direction);

		REPORT(DEBUG,"removed a bit from column " << weight);

		isCompressed = false;
	}


	void BitHeap::removeBit(int weight, Bit* bit)
	{
		if((weight < lsb) || (weight > msb))
			THROWERROR("Weight " << weight << " out of the bit heap range ("<< msb << ", " << lsb << ") in removeBit");

		bool bitFound = false;
		vector<Bit*> bitsColumn = bits[weight-lsb];
		vector<Bit*>::iterator it = bitsColumn.begin();

		//search for the bit and erase it,
		//	if it is present in the column
		while(it != bitsColumn.end())
		{
			if((*it)->getUid() == bit->getUid())
			{
				bitsColumn.erase(it);
				bitFound = true;
			}else{
				it++;
			}
		}
		if(bitFound == false)
			THROWERROR("Bit " << bit->getName() << " with uid=" << bit->getUid()
					<< " not found in column with weight=" << weight);

		REPORT(DEBUG,"removed bit " << bit->getName() << " from column " << weight);

		isCompressed = false;
	}


	void BitHeap::removeBits(int weight, unsigned count, int direction)
	{
		if((weight < lsb) || (weight > msb))
			THROWERROR("Weight " << weight << " out of the bit heap range ("<< msb << ", " << lsb << ") in removeBit");

		vector<Bit*> bitsColumn = bits[weight-lsb];
		unsigned currentCount = 0;

		if(count > bitsColumn.size())
		{
			REPORT(DEBUG, "WARNING: column with weight=" << weight << " only contains "
					<< bitsColumn.size() << " bits, but " << count << " are to be removed");
			count = bitsColumn.size();
		}

		while(currentCount < count)
		{
			removeBit(weight, direction);
		}

		isCompressed = false;
	}


	void BitHeap::removeBits(int msb, int lsb, unsigned count, int direction)
	{
		if(lsb < this->lsb)
			THROWERROR("LSB (=" << lsb << ") out of bitheap bit range  ("<< this->msb << ", " << this->lsb << ") in removeBit");
		if(msb > this->lsb)
			THROWERROR("MSB (=" << msb << ") out of bitheap bit range  ("<< this->msb << ", " << this->lsb << ") in removeBit");

		for(int i=lsb; i<=msb; i++)
		{
			unsigned currentWeight = lsb - this->lsb;

			removeBits(currentWeight, count, direction);
		}

		isCompressed = false;
	}


	void BitHeap::removeCompressedBits()
	{
		for(unsigned i=0; i<width; i++)
		{
			vector<Bit*>::iterator it = bits[i].begin(), lastIt = it;

			//search for the bits marked as compressed and erase them
			while(it != bits[i].end())
			{
				if((*it)->type == BitType::compressed)
				{
					bits[i].erase(it);
					it = lastIt;
				}else{
					lastIt = it;
					it++;
				}
			}
		}
	}


	void BitHeap::markBit(int weight, unsigned number, BitType type)
	{
		if((weight < lsb) || (weight > msb))
			THROWERROR("Weight (=" << weight << ") out of bitheap bit range ("<< msb << ", " << lsb << ")  in markBit");

		if(number >= bits[weight-lsb].size())
			THROWERROR("Column with weight=" << weight << " only contains "
					<< bits[weight].size() << " bits, but bit number=" << number << " is to be marked");

		bits[weight-lsb][number]->type = type;
	}


	void BitHeap::markBit(Bit* bit, BitType type)
	{
		bool bitFound = false;

		for(unsigned i=0; i<=width; i++)
		{
			vector<Bit*>::iterator it = bits[i].begin();

			//search for the bit
			while(it != bits[i].end())
			{
				if(((*it)->getName() == bit->getName()) && ((*it)->getUid() == bit->getUid()))
				{
					(*it)->type = type;
					bitFound = true;
					return;
				}else{
					it++;
				}
			}
		}
		if(bitFound == false)
			THROWERROR("Bit=" << bit->getName() << " with uid="
					<< bit->getUid() << " not found in bitheap");
	}


	void BitHeap::markBits(int msb, int lsb, BitType type, unsigned number)
	{
		if(lsb < this->lsb)
			THROWERROR("markBits: LSB (=" << lsb << ") out of bitheap bit range ("<< this->msb << ", " << this->lsb << ")");
		if(msb > this->lsb)
			THROWERROR("markBits: MSB (=" << msb << ") out of bitheap bit range ("<< this->msb << ", " << this->lsb << ")");

		for(int i=lsb; i<=msb; i++)
		{
			unsigned currentWeight = i - this->lsb;
			unsigned currentNumber = number;

			if(number >= bits[i].size())
			{
				REPORT(DEBUG, "Column with weight=" << currentWeight << " only contains "
						<< bits[i].size() << " bits, but number=" << number << " bits are to be marked");
				currentNumber = bits[i].size();
			}

			for(unsigned j=0; j<currentNumber; j++)
				markBit(currentWeight, j, type);
		}
	}


	void BitHeap::markBits(vector<Bit*> bits, BitType type)
	{
		for(unsigned i=0; i<bits.size(); i++)
			markBit(bits[i], type);
	}


	void BitHeap::markBits(Signal *signal, BitType type, int weight)
	{
		int startIndex;

		if(weight > msb)
		{
			THROWERROR("Error in markBits: weight cannot be more than the msb: weight=" << weight);
		}

		if(weight < lsb)
			startIndex = 0;
		else
			startIndex = weight - lsb;

		for(int i=startIndex; (unsigned)i<bits.size(); i++)
		{
			for(unsigned int j=0; j<bits[i].size(); j++)
			{
				if(bits[i][j]->getRhsAssignment().find(signal->getName()) != string::npos)
				{
					bits[i][j]->type = type;
				}
			}
		}
	}


	void BitHeap::markBitsForCompression()
	{
		for(unsigned i=0; i<width; i++)
		{
			for(unsigned j=0; j<bits[i].size(); j++)
			{
				if(bits[i][j]->type == BitType::justAdded)
				{
					bits[i][j]->type = BitType::free;
				}
			}
		}
	}


	void BitHeap::colorBits(BitType type, unsigned int newColor)
	{
		for(unsigned i=0; i<width; i++)
		{
			for(unsigned j=0; j<bits[i].size(); j++)
			{
				if(bits[i][j]->type == type)
				{
					bits[i][j]->colorCount = newColor;
				}
			}
		}
	}



	void BitHeap::addConstantOneBit(int weight)
	{
		if( (weight < lsb) || (weight > msb) )
			THROWERROR("addConstantOneBit(): weight (=" << weight << ") out of bitheap bit range  ("<< this->msb << ", " << this->lsb << ")");

		constantBits += (mpz_class(1) << (weight-lsb));

		isCompressed = false;
	}


	void BitHeap::subtractConstantOneBit(int weight)
	{
		if( (weight < lsb) || (weight > msb) )
			THROWERROR("subtractConstantOneBit(): weight (=" << weight << ") out of bitheap bit range  ("<< this->msb << ", " << this->lsb << ")");

		constantBits -= (mpz_class(1) << (weight-lsb));

		isCompressed = false;
	}


	void BitHeap::addConstant(mpz_class constant, int weight)
	{
		if( (weight < lsb) || (weight+intlog2(constant) > msb) )
			THROWERROR("addConstant: Constant " << constant << " weighted by " << weight << " has bits out of the bitheap range ("<< this->msb << ", " << this->lsb << ")");

		constantBits += (constant << (weight-lsb));

		isCompressed = false;
	}



	void BitHeap::subtractConstant(mpz_class constant, int weight)
	{
		if( (weight < lsb) || (weight+intlog2(constant) > msb) )
			THROWERROR("subtractConstant: Constant " << constant << " weighted by " << weight << " has bits out of the bitheap range ("<< this->msb << ", " << this->lsb << ")");

		constantBits -= (constant << (weight-lsb));

		isCompressed = false;
	}







	void BitHeap::addSignal(string name, int weight)
	{

		REPORT(DEBUG, "addSignal	" << name << " weight=" << weight);
		Signal* s = op->getSignalByName(name);
		int sMSB = s->MSB();
		int sLSB = s->LSB();

		// No error reporting here: the current situation is that addBit will ignore bits thrown out of the bit heap (with a warning)

		if( (sLSB+weight < lsb) || (sMSB+weight > msb) )
			REPORT(0,"WARNING: addSignal(): " << name << " weighted by " << weight << " has bits out of the bitheap range ("<< this->msb << ", " << this->lsb << ")");

		if(s->isSigned()) { // We must sign-extend it, using the constant trick
			REPORT(DEBUG, "addSignal: this is a signed signal	");

			if(!op->getSignalByName(name)->isBus()) {
				// getting here doesn't make much sense
				THROWERROR("addSignal(): signal " << name << " is not a bus, but is signed : this looks like a bug");
			}

			// Now we have a bit vector but it might be of width 1, in which case the following loop is empty.
			for(int w=sLSB; w<=sMSB-1; w++) {
				addBit(name + of(w - sLSB), w + weight);
			}
			if(sMSB==this->msb) { // No point in complementing it and adding a constant bit
				addBit(name + of(sMSB - sLSB), sMSB + weight);
			}
			else{
				// complement the leading bit
				addBit("not " + name + of(sMSB - sLSB), sMSB + weight);
				// add the string of ones from this bit to the MSB of the bit heap
				for(int w=sMSB; w<=this->msb-weight; w++) {
					addConstantOneBit(w+weight);
				}
				REPORT(FULL, "addSignal: constant string now " << hex << constantBits); 
			}
		}

		else{ // unsigned
			REPORT(DEBUG, "addSignal: this is an unsigned signal	");
			if(!op->getSignalByName(name)->isBus())
				addBit(name, weight);
			else {// isBus: this is a bit vector
				for(int w=sLSB; w<=sMSB; w++) {
					addBit(name + of(w - sLSB), w + weight);
				}
			}
		}

		isCompressed = false;
	}


	void BitHeap::subtractSignal(string name, int weight)
	{
		REPORT(DEBUG, "subtractSignal  " << name << " weight=" << weight);
		Signal* s = op->getSignalByName(name);
		int sMSB = s->MSB();
		int sLSB = s->LSB();

		// No error reporting here: the current situation is that addBit will ignore bits thrown out of the bit heap (with a warning)

		if( (sLSB+weight < lsb) || (sMSB+weight > msb) )
			REPORT(0,"WARNING: subtractSignal(): " << name << " weighted by " << weight << " has bits out of the bitheap range ("<< this->msb << ", " << this->lsb << ")");


		if(s->isSigned()) {
			// we subtract                            000000sxxxxxxx000 (shift=3)
			// which must be sign-extended to         sssssssxxxxxxx000
			//
			// so we must add to the bit heap         SSSSSSSXXXXXXX111 (capital means: complement)
			//                                                       +1
			// equivalently                           SSSSSSSXXXXXXX
			//                                                    +1
			// with the sign extension trick:         000000sXXXXXXX
			//                                      + 1111111      1
			REPORT(DEBUG, "subtractSignal: this is a signed signal	");

			// If the bit vector is of width 1, the following loop is empty.
			for(int w=sLSB; w<=sMSB; w++) {
				addBit((w != sMSB ? "not " : "") + name + of(w - sLSB), w + weight);
			}
			addConstantOneBit(sLSB+weight); // the +1
			// add the string of ones to the MSB of the bit heap
			for(int w=sMSB; w<=this->msb-weight; w++) {
				addConstantOneBit(w+weight);
			}
		}
		else{ // unsigned
			// we subtract                            000000xxxxxxxx000  (shift=3)
			//
			// so we must add to the bit heap         111111XXXXXXXX
			//                                                    +1
			REPORT(DEBUG, "subtractSignal: this is an unsigned signal	");
			for(int w=sLSB; w<=sMSB; w++) {
				addBit("not " + name + of(w - sLSB), w + weight);
			}
			addConstantOneBit(sLSB+weight); // the +1
			// add the string of ones to the MSB of the bit heap
			for(int w=sMSB+1; w<=this->msb-weight; w++) {
				addConstantOneBit(w+weight);
			}
		}
		isCompressed = false;
	}




	void BitHeap::resizeBitheap(unsigned newWidth, int direction)
	{
		if((direction != 0) && (direction != 1))
			THROWERROR("Invalid direction in resizeBitheap: direction=" << direction);
		if(newWidth < width)
		{
			REPORT(DEBUG, "WARNING: resizing the bitheap from width="
					<< width << " to width=" << newWidth);
			if(direction == 0){
				REPORT(DEBUG, "\t will loose the information in the lsb columns");
			}else{
				REPORT(DEBUG, "\t will loose the information in the msb columns");
			}
		}

		//add/remove the columns
		if(newWidth < width)
		{
			//remove columns
			if(direction == 0)
			{
				//remove lsb columns
				bits.erase(bits.begin(), bits.begin()+(width-newWidth));
				history.erase(history.begin(), history.begin()+(width-newWidth));
				bitUID.erase(bitUID.begin(), bitUID.begin()+(width-newWidth));
				lsb += width-newWidth;
			}else{
				//remove msb columns
				bits.resize(newWidth);
				history.resize(newWidth);
				bitUID.resize(newWidth);
				msb -= width-newWidth;
			}
		}else{
			//add columns
			if(direction == 0)
			{
				//add lsb columns
				vector<Bit*> newVal;
				bits.insert(bits.begin(), newWidth-width, newVal);
				history.insert(history.begin(), newWidth-width, newVal);
				bitUID.insert(bitUID.begin(), newWidth-width, 0);
				lsb -= width-newWidth;
			}else{
				//add msb columns
				bits.resize(newWidth-width);
				history.resize(newWidth-width);
				bitUID.resize(newWidth-width);
				msb += width-newWidth;
			}
		}

		//update the information inside the bitheap
		width = newWidth;
		height = getMaxHeight();
		for(int i=lsb; i<=msb; i++)
			for(unsigned j=0; j<bits[i-lsb].size(); j++)
				bits[i-lsb][j]->weight = i;

		isCompressed = false;
	}


	void BitHeap::resizeBitheap(int newMsb, int newLsb)
	{
		if((newMsb < newLsb))
			THROWERROR("Invalid arguments in resizeBitheap: newMsb=" << newMsb << " newLsb=" << newLsb);
		if(newMsb<msb || newLsb>lsb)
		{
			REPORT(DEBUG, "WARNING: resizing the bitheap from msb="
					<< msb << " lsb=" << lsb << " to newMsb=" << newMsb << " newLsb=" << newLsb);
			if(newMsb < msb){
				REPORT(DEBUG, "\t will loose the information in the msb columns");
			}
			if(newLsb>lsb){
				REPORT(DEBUG, "\t will loose the information in the lsb columns");
			}
		}

		//resize on the lsb, if necessary
		if(newLsb > lsb)
			resizeBitheap(msb-newLsb+1, 0);
		//resize on the msb, if necessary
		if(newMsb < msb)
			resizeBitheap(newMsb-newLsb+1, 1);

		isCompressed = false;
	}


	void BitHeap::mergeBitheap(BitHeap* bitheap)
	{
		if(op->getName() != bitheap->op->getName())
			THROWERROR("Cannot merge bitheaps belonging to different operators!");

		//resize if necessary
		if(width < bitheap->width)
		{
			if(lsb > bitheap->lsb)
				resizeBitheap(msb, bitheap->lsb);
			if(msb < bitheap->msb)
				resizeBitheap(bitheap->msb, lsb);
		}
		//add the bits
		for(int i=bitheap->lsb; i<bitheap->msb; i++) {
			for(unsigned j=0; j<bitheap->bits[i-bitheap->lsb].size(); j++)
				insertBitInColumn(bitheap->bits[i-bitheap->lsb][j], i-bitheap->lsb + (lsb-bitheap->lsb));
		}
		//make the bit all point to this bitheap
		for(unsigned i=0; i<width; i++)
			for(unsigned j=0; j<bits[i].size(); j++)
				bits[i][j]->bitheap = this;

		isCompressed = false;
	}


		// Quick hack for The Book
	void BitHeap::latexPlot() {
		ofstream file;

		ostringstream fileName;
		fileName << "BitHeap_initial_" << getOp()->getName() << "_bitheap_" << getGUid() << ".tex";

		try
		{
			file.open(fileName.str(), ios::out);

			file << tab << "% This file is automatically generated using FloPoCo and can be compiled with the help of dot_diag_macros.tex" << endl;
			file << tab << "\\begin{dotdiag}{" << width << "}" << endl;
			file << tab << tab << "\\begin{scope}" << endl;

			mpz_class c = constantBits;
			for (int i = lsb; i <= msb; i++)
			{
				int w = i - lsb;
				int h = getColumnHeight(w);
				if (c % 2 == 1)
					h++;
				c = c >> 1;
				if (h > 0)
					file << tab << tab << "\\drawdotcol{" << i << "}{" << h << "}" << endl;
				//			cout << "i=" <<i  << "  "<<  h << endl;
			}
			file << tab << tab << "\\end{scope}" << endl;
			file << tab << "\\end{dotdiag}" << endl;
			file.close();
		} catch (std::string &s) {
			cerr << "Exception while generating " << fileName.str() << endl;
		}
	}
	
	void BitHeap::startCompression()
	{
		if (op->getTarget()->generateFigures())
			latexPlot();

		if(op->getTarget()->getCompressionMethod().compare("heuristicMaxEff") == 0)
		{
			compressionStrategy = new MaxEfficiencyCompressionStrategy(this);
		}
		else if(op->getTarget()->getCompressionMethod().compare("heuristicPA") == 0)
		{
			compressionStrategy = new ParandehAfsharCompressionStrategy(this);
		}
		else if(op->getTarget()->getCompressionMethod().compare("heuristicFirstFit") == 0)
		{
			compressionStrategy = new FirstFittingCompressionStrategy(this);
		}
		else if(op->getTarget()->getCompressionMethod().compare("optimal") == 0)
		{
			compressionStrategy = new OptimalCompressionStrategy(this,false);
		}
		else if(op->getTarget()->getCompressionMethod().compare("optimalMinStages") == 0)
		{
			compressionStrategy = new OptimalCompressionStrategy(this,true);
		}
		else
		{
			THROWERROR("compression " << op->getTarget()->getCompressionMethod() << " unknown!");
		}

		REPORT(DETAILED,"Using compression method " << op->getTarget()->getCompressionMethod());
		//create a new compression strategy, if one isn't present already
		//if(compressionStrategy == nullptr)
		//start the compression
        compressionStrategy->startCompression();
        //mark the bitheap compressed
		isCompressed = true;
	}


	string BitHeap::getSumName()
	{
		return join("bitheapResult_bh", guid);
	}


	string BitHeap::getSumName(int msb, int lsb)
	{
		return join(join("bitheapResult_bh", guid), range(msb, lsb));
	}


	unsigned BitHeap::getMaxHeight()
	{
		unsigned maxHeight = 0;

		for(unsigned i=0; i<bits.size(); i++)
			if(bits[i].size() > maxHeight)
				maxHeight = bits[i].size();
		height = maxHeight;

		return height;
	}


	unsigned BitHeap::getColumnHeight(int weight)
	{
		if((weight < lsb) || (weight > msb))
			THROWERROR("Invalid argument for getColumnHeight: weight=" << weight);

		return bits[weight-lsb].size();
	}


	bool BitHeap::compressionRequired()
	{
		if(getMaxHeight() < 3){
			return false;
		}else if(height > 3){
			return true;
		}else{
			for(unsigned i=1; i<bits.size(); i++)
				if(bits[i].size() > 2)
					return true;
			return false;
		}
	}


	Plotter* BitHeap::getPlotter()
	{
		return plotter;
	}


	Operator* BitHeap::getOp()
	{
		return op;
	}


	int BitHeap::getGUid()
	{
		return guid;
	}


	string BitHeap::getName()
	{
		return name;
	}


	int BitHeap::newBitUid(unsigned weight)
	{
		if(((int)weight < lsb) || ((int)weight > msb))
			THROWERROR("Invalid argument for newBitUid: weight=" << weight);

		int returnVal = bitUID[weight-lsb];

		bitUID[weight-lsb]++;
		return returnVal;
	}


	void BitHeap::printColumnInfo(int weight)
	{
		if((weight < lsb) || (weight > msb))
			THROWERROR("Invalid argument for printColumnInfo: weight=" << weight);

		for(unsigned i=0; i<bits[weight-lsb].size(); i++)
		{
			REPORT(FULL, "\t column weight=" << weight << " name=" << bits[weight-lsb][i]->getName()
					<< " cycle=" << bits[weight-lsb][i]->signal->getCycle()
					<< " criticaPath=" << bits[weight-lsb][i]->signal->getCriticalPath());
		}
	}


	void BitHeap::printBitHeapStatus()
	{
		REPORT(DEBUG, "Bitheap status:");
		for(unsigned w=0; w<bits.size(); w++)
		{
			REPORT(DEBUG, "Column weight=" << w << ":\t height=" << bits[w].size());
			printColumnInfo(w);
		}
	}


	vector<vector<Bit*>> BitHeap::getBits()
	{
		return bits;
	}


	CompressionStrategy* BitHeap::getCompressionStrategy()
	{
		return compressionStrategy;
	}


	void BitHeap::initializeDrawing()
	{

	}


	void BitHeap::closeDrawing(int offsetY)
	{

	}


	void BitHeap::drawConfiguration(int offsetY)
	{

	}

	void BitHeap::drawBit(int cnt, int w, int turnaroundX, int offsetY, int c)
	{

	}



} /* namespace flopoco */
