
#include "Bit.hpp"
#include "BitHeap.hpp"

using namespace std;

namespace flopoco
{

	Bit::Bit(BitHeap *bitheap_, string rhsAssignment_, int weight_, BitType type_) :
		weight(weight_), type(type_), bitheap(bitheap_), colorCount(0), rhsAssignment(rhsAssignment_)
	{
		std::ostringstream p;

		uid = bitheap->newBitUid(weight);
		p  << "bh" << bitheap->getGUid() << "_w" << weight << "_" << uid;
		string name = p.str();
		bitheap->getOp()-> vhdl << tab
			<< bitheap->getOp()->declare(name)
			<< " <= " << rhsAssignment << ";" << endl;
		
		signal = bitheap->getOp()->getSignalByName(name);
		
		compressor = nullptr;

	}


	Bit::Bit(BitHeap *bitheap_, Signal *signal_, int offset_, int weight_, BitType type_) :
		weight(weight_), type(type_), bitheap(bitheap_), colorCount(0)
	{
		std::ostringstream p;

		uid = bitheap->newBitUid(weight);
		p  << "bh" << bitheap->getGUid() << "_w" << weight << "_" << uid;
		string name = p.str();

		p.str("");
		p << signal->getName() << (signal->width()==1 ? "" : of(offset_));
		rhsAssignment = p.str();

		bitheap->getOp()->vhdl << tab
			 << bitheap->getOp()->declare(name)
			 << " <= " << rhsAssignment << ";" << endl;

		signal = bitheap->getOp()->getSignalByName(name);
		
		compressor = nullptr;
	}


	Bit::Bit(Bit* bit) {
		std::ostringstream p;

		weight = bit-> weight;
		type = bit->type;
		rhsAssignment = bit->rhsAssignment;

		bitheap = bit->bitheap;
		compressor = bit->compressor;

		uid = bitheap->newBitUid(weight);

		p  << "bh" << bitheap->getGUid() << "_w" << weight << "_" << uid;
		string name = p.str();

		bitheap->getOp()->vhdl << tab
													 << bitheap->getOp()->declare(name)
													 << " <= " << rhsAssignment << ";" << endl;
		signal = bitheap->getOp()->getSignalByName(name);

		colorCount = 0;
	}


	Bit::Bit()
	{
		weight = -1;
		type = BitType::free;

		bitheap = nullptr;
		compressor = nullptr;
		signal = nullptr;

		uid = -1;

		rhsAssignment = "";

		colorCount = 0;
	}


	Bit* Bit::clone()
	{
		Bit* newBit = new Bit();

		newBit->weight = weight;
		newBit->type = type;

		newBit->bitheap = bitheap;
		newBit->compressor = compressor;
		newBit->signal = signal;

		newBit->uid = uid;

		newBit->rhsAssignment = rhsAssignment;

		newBit->colorCount = colorCount;

		return newBit;
	}


	int Bit::getUid()
	{
		return uid;
	}


	void Bit::setCompressor(Compressor *compressor_)
	{
		compressor = compressor_;
	}


	Compressor* Bit::getCompressor()
	{
		return compressor;
	}

	string Bit::getName()
	{
		return signal->getName();
	}


	string Bit::getRhsAssignment()
	{
		return rhsAssignment;
	}


	bool operator< (Bit& b1, Bit& b2){
		return ((b1.signal->getCycle() < b2.signal->getCycle())
				|| (b1.signal->getCycle()==b2.signal->getCycle() && b1.signal->getCriticalPath()<b2.signal->getCriticalPath()));
	}

	bool operator<= (Bit& b1, Bit& b2){
		return ((b1.signal->getCycle() < b2.signal->getCycle())
				|| (b1.signal->getCycle()==b2.signal->getCycle() && b1.signal->getCriticalPath()<=b2.signal->getCriticalPath()));
	}

	bool operator> (Bit& b1, Bit& b2){
		return ((b1.signal->getCycle() > b2.signal->getCycle())
				|| (b1.signal->getCycle()==b2.signal->getCycle() && b1.signal->getCriticalPath()>b2.signal->getCriticalPath()));
	}

	bool operator>= (Bit& b1, Bit& b2){
		return ((b1.signal->getCycle() > b2.signal->getCycle())
				|| (b1.signal->getCycle()==b2.signal->getCycle() && b1.signal->getCriticalPath()>=b2.signal->getCriticalPath()));
	}

	bool operator== (Bit& b1, Bit& b2){
		return ((b1.signal->getCycle() == b2.signal->getCycle())
						&& (b1.signal->getCriticalPath() == b2.signal->getCriticalPath()));
	}

	bool operator!= (Bit& b1, Bit& b2){
		return  ((b1.signal->getCycle() != b2.signal->getCycle()) || (b1.signal->getCriticalPath() != b2.signal->getCriticalPath()));
	}
}
