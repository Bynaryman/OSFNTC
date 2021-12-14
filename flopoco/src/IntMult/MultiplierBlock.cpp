#include "MultiplierBlock.hpp"

using namespace std;


namespace flopoco{


		MultiplierBlock::MultiplierBlock(int wX, int wY, int lsbX, int lsbY, string input1, string input2, int weightShift_, int cycle_) :
			wX(wX), wY(wY), lsbX(lsbX), lsbY(lsbY), weightShift(weightShift_), inputName1(input1), inputName2(input2)
		{
			srcFileName = ":MultiplierBlock";
			cycle = cycle_;
			//before update weight=lsbX+lsbY-weightShift;
			weight = lsbX+lsbY+weightShift;
			previous = NULL;
			next = NULL;
		}


		bool MultiplierBlock::operator <= (MultiplierBlock* b){
			if ((weight<=b->getWeight()))
				return true;
			else
				return false;
		}

		void MultiplierBlock::setSignalName(string name)
		{
			signalName=name;
		}

		void MultiplierBlock::setSignalLength(int length)
		{
			signalLength=length;
		}

		string MultiplierBlock::getSigName()
		{
			return signalName;
		}

		int MultiplierBlock::getSigLength()
		{
			return signalLength;
		}

		int MultiplierBlock::getWeight()
		{
			return weight;
		}

		int MultiplierBlock::getwX()
		{
			return wX;
		}

		int MultiplierBlock::getwY()
		{
			return wY;
		}

		int MultiplierBlock::gettopX()
		{
			return lsbX;
		}

		int MultiplierBlock::gettopY()
		{
			return lsbY;
		}


		int MultiplierBlock::getbotX()
		{
			return lsbX+wX;
		}

		int MultiplierBlock::getbotY()
		{
			return lsbY+wY;
		}


		int MultiplierBlock::getCycle()
		{
			return cycle;
		}


		void MultiplierBlock::setNext(MultiplierBlock* b)
		{
			this->next=b;
		}

		MultiplierBlock* MultiplierBlock::getNext()
		{
			if(next!=NULL)
				return next;
			else
				return NULL;
		}



		MultiplierBlock* MultiplierBlock::getPrevious()
		{
			if(previous!=NULL)
				return previous;
			else
				return NULL;
		}

		void MultiplierBlock::setPrevious(MultiplierBlock* b)
		{
			this->previous=b;
		}


		//TODO improve the chaining
		// - pass the full target
		// - use the "double multiplier mode" on Altera
		// ...
		bool MultiplierBlock::canBeChained(MultiplierBlock* nextMultiplierBlock, bool isXilinx)
		{
			//for now just the stupid chaining
			if (isXilinx)
			{
				if(neighbors(nextMultiplierBlock))
					return true;
				else
					return false;
			}
			//Altera chaining
			else
			{
				if((next==NULL) && (previous==NULL) && (nextMultiplierBlock->getNext()==NULL) && (nextMultiplierBlock->getPrevious()==NULL)
						&& (abs(wX-nextMultiplierBlock->getwX())<=1) && (abs(wY-nextMultiplierBlock->getwY())<=1))
				{
					if((wX>18) || (wY>18))
						return false;
					else
					{
						if((getWeight() == nextMultiplierBlock->getWeight()) && (this != nextMultiplierBlock))
							return true;
						else
							return false;
					}
				}
				else
				{
					return false;
				}
			}
		}

		bool MultiplierBlock::neighbors(MultiplierBlock* next)
		{
			if((((this->lsbX==next->gettopX()) &&
					(this->lsbY==next->gettopY()+17)) ||
				((this->lsbY==next->gettopY()) &&
					(this->lsbX==next->gettopX()+17)) )  ||

				(((this->lsbX==next->gettopX()) &&
					(this->lsbY==next->gettopY()-17)) ||
				((this->lsbY==next->gettopY()) &&
					(this->lsbX==next->gettopX()-17)) ))
				return true;
			else
				return false;

		}

}
