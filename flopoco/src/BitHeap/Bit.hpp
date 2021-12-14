#ifndef BIT_HPP
#define BIT_HPP

#include <string>
#include <iostream>

#include "utils.hpp"

#include "Signal.hpp"
#include "Compressor.hpp"
#include "BitHeap.hpp"

using namespace std;

namespace flopoco{

enum BitType : unsigned {
	justAdded,
	free,
	beingCompressed,
	compressed
};

class BitHeap;

	class Bit
	{
	public:

		/**
		 * Ordering by availability in time
		 */
		friend bool operator< (Bit& b1, Bit& b2);
		/**
		 * Ordering by availability in time
		 */
		friend bool operator<= (Bit& b1, Bit& b2);
		/**
		 * Ordering by availability in time
		 */
		friend bool operator> (Bit& b1, Bit& b2);
		/**
		 * Ordering by availability in time
		 */
		friend bool operator>= (Bit& b1, Bit& b2);
		/**
		 * Ordering by availability in time
		 */
		friend bool operator== (Bit& b1, Bit& b2);
		/**
		 * Ordering by availability in time
		 */
		friend bool operator!= (Bit& b1, Bit& b2);


		/**
		 * @brief Standard constructor
		 * @param bitheap the bitheap containing this bit
		 * @param rhsAssignment the code on the right-hand side of the assignment creating this bit
		 * @param weight the weight at which this bit is added to the bitheap, by default 0 (or the lsb of the bitheap)
		 * @param type the status of the bit, in terms of its processing
		 */
		Bit(BitHeap *bitheap, string rhsAssignment, int weight = 0, BitType type = BitType::free);

		/**
		 * @brief Standard constructor from an existing signal
		 * @param bitheap the bitheap containing this bit
		 * @param signal the signal which is on the right-hand side of the assignment
		 * @offset the offset at which the bit is extracted from in the right-hand side signal, by default 0
		 * @param weight the weight at which this bit is added to the bitheap, by default 0 (or the lsb of the bitheap)
		 * @param type the status of the bit, in terms of its processing
		 */
		Bit(BitHeap *bitheap, Signal *signal, int offset = 0, int weight = 0, BitType type = BitType::free);

		/**
		 * @brief Clone constructor
		 */
		Bit(Bit* bit);

		/**
		 * @brief a simple constructor, creating an empty bit
		 * 		  to be used in conjunction with the clone() method
		 */
		Bit();


		/**
		 * @brief clone a bit, without adding a new bit to the bitheap
		 */
		Bit* clone();

		/**
		 * @brief Return the uid of this bit inside the bitheap that contains it
		 */
		int getUid();

		/**
		 * @brief Set the compressor that compressed this bit
		 */
		void setCompressor(Compressor *compressor);

		/**
		 * @brief get the name of a bit
		 */
		string getName();

		/**
		 * @brief Return the compressor that compressed this bit
		 */
		Compressor* getCompressor();

		/**
		 * @brief return the rhsAssignment
		 */
		string getRhsAssignment();

	public:
		int weight;
		BitType type;

		BitHeap *bitheap;
		Compressor *compressor;
		Signal *signal;

		unsigned int colorCount;

	private:
		int uid;

		string rhsAssignment;

	};

}

#endif
