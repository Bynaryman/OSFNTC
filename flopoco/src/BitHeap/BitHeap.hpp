/*
  A class to manage and compress heaps of weighted bits in FloPoCo

  This file is part of the FloPoCo project

  Author : Florent de Dinechin, Matei Istoan

  Initial software.
  Copyright © INSA de Lyon, INRIA, CNRS, UCBL,
  2012-2017.
  All rights reserved.

*/
#ifndef __BITHEAP_HPP
#define __BITHEAP_HPP


#include <vector>
#include <sstream>

#include "Operator.hpp"
#include "Signal.hpp"

#include "BitHeap/Bit.hpp"
#include "BitHeap/Compressor.hpp"
#include "BitHeap/CompressionStrategy.hpp"

#include "IntAddSubCmp/IntAdder.hpp"



#define COMPRESSION_TYPE 0


namespace flopoco{


class Plotter;
class Bit;
class CompressionStrategy;

enum BitType : unsigned;


	class BitHeap
	{


	public:

		//the compression strategy needs access to the bitheap
		friend class CompressionStrategy;
		friend class FirstFittingCompressionStrategy;
		friend class ParandehAfsharCompressionStrategy;
		friend class MaxEfficiencyCompressionStrategy;
		friend class OptimalCompressionStrategy;
		/**
		 * @brief The constructor for an signed/unsigned integer bitheap
		 * @param op                the operator in which the bitheap is being built
		 * @param width              the width (maximum weight of the heap, or number of columns) of the bitheap
		 * @param name              a description of the heap that will be integrated into its unique name (empty by default)
		 * @param compressionType	the type of compression applied to the bit heap:
		 *								0 = using only compressors (default),
		 *								1 = using only an adder tree,
		 *								2 = using a mix of the two, with an
		 *									addition tree at the end of the
		 *									compression
		 */
		BitHeap(Operator* op, unsigned width, string name = "", int compressionType = COMPRESSION_TYPE);

		/**
		 * @brief The constructor for an signed/unsigned fixed-point bitheap
		 * @param op                the operator in which the bitheap is being built
		 * @param msb               the msb of the bitheap (maximum weight at which a bit can be inserted)
		 * @param lsb               the lsb of the bitheap (minimum weight at which a bit can be inserted)
		 * @param name              a description of the heap that will be integrated into its unique name (empty by default)
		 * @param compressionType	the type of compression applied to the bit heap:
		 *								0 = using only compressors (default),
		 *								1 = using only an adder tree,
		 *								2 = using a mix of the two, with an
		 *									addition tree at the end of the
		 *									compression
		 */
		BitHeap(Operator* op, int msb, int lsb, string name = "", int compressionType = COMPRESSION_TYPE);


		~BitHeap();


		/**
		 * @brief add a bit to the bit heap.
		 * @param weight             the weight of the bit to be added. It should be non-negative,
		 *                           for an integer bitheap, or between msb and lsb for a fixed-point bitheap.
		 * @param       the right-hand side VHDL code defining this bit.
		 * @param comment            a VHDL comment for this bit
		 * @return the newly added bit
		 */
		Bit* addBit(string name, int weight);


		/**
		 * @brief add a constant 1 to the bit heap.
		 * All the constant bits are added to the constantBits mpz,
		 * so we don't generate hardware to compress constants.
		 * @param weight            the weight of the 1 bit to be added
		 *                          (by default 0)
		 */
		void addConstantOneBit(int weight = 0);

		/**
		 * @brief subtract a constant 1 from the bitheap.
		 * @param weight             the weight to which the LSB of the (integer) constant should be added
		 *                          (by default 0)
		 */
		void subtractConstantOneBit(int weight = 0);

		/**
		 * @brief add a constant to the bitheap. It will be added to the constantBits mpz,
		 * so we don't generate hardware to compress constants.
		 * @param constant          the value to be added
		 * @param weight             the weight to which the LSB of the (integer) constant should be added
		 *                          (by default 0)
		 */
		void addConstant(mpz_class constant, int weight = 0);


		/**
		 * @brief subtract a constant from the bitheap. It will be subtracted from the constantBits mpz,
		 * so we don't generate hardware to compress constants.
		 * @param constant          the value to be subtracted
		 * @param weight            the weight of the LSB of constant
		 *                          (by default 0)
		 */
		void subtractConstant(mpz_class constant, int weight = 0);


		/**
		 * @brief add to the bitheap a signal
		 * @param signal            the signal being added
		 * @param weight             Bit of weight i of the signal will be sent to weight i+weight in the bit heap.
		 *                          (by default 0)
		 The signedness will be read from the signal itself
		 */
		void addSignal(string name, int weight = 0);


		/**
		 * @brief subtract a signal from the bitheap
		 * @param signal            the signal
		 * @param weight             Bit of weight i of the signal will be sent to weight i+weight in the bit heap.
		 *                          (by default 0)
		 The signedness will be read from the signal itself
		 */
		void subtractSignal(string name, int weight = 0);


		/**
		 * @brief generate the VHDL for the bit heap.
		 * Uses the compression strategy set by the user, or the one created by default.
		 * To be called last by operators using BitHeap.
		 */
		void startCompression();

		/**
		 * @brief return the name of the compressed sum
		 */
		string getSumName();

		
		/**
		 * @brief remove a bit from the bitheap.
		 * @param weight  the weight of the bit to be removed
		 * @param direction if dir==0 the bit will be removed from the beginning of the list
		 *                  if dir==1 the bit will be removed from the end of the list
		 */
		void removeBit(int weight, int direction = 1);

		/**
		 * @brief remove a bit from the bitheap.
		 * @param weight  the weight of the bit to be removed
		 * @param bit the bit to be removed
		 */
		void removeBit(int weight, Bit* bit);

		/**
		 * @brief remove a bit from the bitheap.
		 * @param weight  the weight of the bit to be removed
		 * @param count the number of bits to remove
		 * @param direction if dir==0 the bit will be removed from the beginning of the list
		 *                  if dir==1 the bit will be removed from the end of the list
		 */
		void removeBits(int weight, unsigned count = 1, int direction = 1);

		/**
		 * @brief remove a bit from the bitheap.
		 * @param msb the column up until which to remove the bits
		 * @param lsb the column from which to remove the bits
		 * @param count the number of bits to remove
		 * @param direction if dir==0 the bit will be removed from the beginning of the list
		 *                  if dir==1 the bit will be removed from the end of the list
		 */
		void removeBits(int msb, int lsb, unsigned count = 1, int direction = 1);

		/**
		 * @brief remove the bits that have already been marked as compressed
		 */
		void removeCompressedBits();

		/**
		 * @brief mark a bit in the bitheap.
		 * @param weight the weight at which the bit is
		 * @param number the number of the bit in the respective column
		 * @param type how the bit should be marked
		 */
		void markBit(int weight, unsigned number, BitType type);

		/**
		 * @brief mark a bit in the bitheap.
		 * @param bit the bit to mark
		 * @param type how the bit should be marked
		 *        (by default as free)
		 */
		void markBit(Bit* bit, BitType type);

		/**
		 * @brief mark several bits in the bitheap.
		 * @param msb the column up until which to mark the bits
		 * @param lsb the column up until which to mark the bits
		 * @param number the number of bits to mark in each column
		 *               (by default=0, i.e. mark all bits)
		 * @param type how the bits should be marked
		 */
		void markBits(int msb, int lsb, BitType, unsigned number = 0);

		/**
		 * @brief mark several bits in the bitheap.
		 * @param bits the bits to mark
		 * @param type how the bits should be marked
		 */
		void markBits(vector<Bit*> bits, BitType type);

		/**
		 * @brief mark the bits of a signal added to the bitheap.
		 * @param signalName the signal who's bits to mark
		 * @param type how the bits should be marked
		 * @param weight the weight of the signal, so as to speed-up the search
		 */
		void markBits(Signal *signal, BitType type, int weight);

		/**
		 * @brief mark bits that are ready to be compressed as free
		 */
		void markBitsForCompression();

		/**
		 * @brief color the bits of the given type, with the given color
		 * @param type the type of the bits who's color is going to be changed
		 * @param newColor the new color of the bits
		 */
		void colorBits(BitType type, unsigned int newColor);


		/**
		 * @brief resize an integer bitheap to the new given size
		 * @param newWidth           the new size of the bitheap
		 * @param direction         the direction in which to add or remove columns
		 *                          (by default=0, the lsb, =1 for msb)
		 */
		void resizeBitheap(unsigned newWidth, int direction = 0);

		/**
		 * @brief resize a fixed-point bitheap to the new given format
		 * @param newMsb            the new msb of the bitheap
		 * @param newLsb            the new lsb of the bitheap
		 */
		void resizeBitheap(int newMsb, int newLsb);


		/**
		 * @brief merge the bitheap given as argument to this one
		 * NOTE: only bitheaps belonging to the same parent operator can be merged together
		 * @param bitheap           the bitheap to merge into this one
		 */
		void mergeBitheap(BitHeap* bitheap);



		/**
		 * @brief returns the name of the compressed sum, with the range (msb, lsb)
		 * @param msb the msb for the range
		 * @param lsb the lsb for the range
		 */
		string getSumName(int msb, int lsb);


		/**
		 * @brief return the maximum height of the bitheap
		 */
		unsigned getMaxHeight();

		/**
		 * @brief return the height a column (bits not yet compressed)
		 * @param weight the column
		 */
		unsigned getColumnHeight(int weight);

		/**
		 * @brief is compression necessary, or not
		 */
		bool compressionRequired();

		/**
		 * @brief return the plotter object
		 */
		Plotter* getPlotter();

		/**
		 * @brief return the parent operator of this bitheap
		 */
		Operator* getOp();

		/**
		 * @brief return the UID of the bit heap
		 */
		int getGUid();

		/**
		 * @brief return the unique name of the bit heap
		 */
		string getName();

		/**
		 * @brief return a fresh uid for a bit of weight w
		 * @param weight            the weight of the bit
		 */
		int newBitUid(unsigned weight);

		/**
		 * @brief print the informations regarding a column
		 * @param weight the weight of the column
		 */
		void printColumnInfo(int weight);

		/**
		 * @brief print the informations regarding the whole bitheap
		 */
		void printBitHeapStatus();

		/**
		 * @brief return the bits currently stored in the bitheap
		 */
		vector<vector<Bit*>> getBits();

		/**
		 * @brief return the compression strategy
		 */
		CompressionStrategy* getCompressionStrategy();

		/**
		 *	@brief sorts the bits in each column in lexicographical order
		 */
		void sortBitsInColumns();

	protected:

		/**
		 * @brief factored code for initializing a bitheap inside the constructor
		 */
		void initialize();

		/**
		 * @brief insert a bit into a column of bits, so that the column is ordered
		 * in increasing lexicographical order on (cycle, critical path)
		 * @param bit the bit to insert
		 * @param columnNumber the index of the column of bits into which to insert the bit
		 */
		void insertBitInColumn(Bit* bit, unsigned columnNumber);

		// Quick hack for The Book
		void latexPlot();

			//TODO is all the following useful at all? 
		void initializeDrawing();

		void closeDrawing(int offsetY);

		void drawConfiguration(int offsetY);

		void drawBit(int cnt, int w, int turnaroundX, int offsetY, int c);

		/**
		 *	@brief returns true, if bit1 < bit2 (in lexicographicOrdering)
		 *	@param bit1 is first bit
		 *	@parma bit2 is second bit
		 */
		static bool lexicographicOrdering(const Bit* bit1, const Bit* bit2);


	public:
		int msb;                                    /**< The maximum weight a bit can have inside the bitheap */
		int lsb;                                    /**< The minimum weight a bit can have inside the bitheap */
		unsigned width;                              /**< The width of the bitheap */
		int height;                                 /**< The current maximum height of any column of the bitheap */
		string name;                                /**< The name of the bitheap */

	private:
		Operator* op;

		vector<vector<Bit*> > bits;                 /**< The bits currently contained in the bitheap, ordered into columns by weight in the bitheap,
		                                                 and by arrival time of the bits, i.e. lexicographic order on (cycle, cp), inside each column. */
		vector<vector<Bit*> > history;              /**< All the bits that have been added (and possibly removed at some point) to the bitheap. */
		mpz_class constantBits;						/**< The sum of all the constant bits that need to be added to the bit heap
				                                                 (constants added to the bitheap, for rounding, two's complement etc) */

		ostringstream vhdlCode;                     /**< The VHDL code buffer */

		CompressionStrategy* compressionStrategy;   /**< The compression strategy used to compress the bitheap */
		bool isCompressed;                          /**< Has the bitheap already been compressed, or not */
		int compressionType;						/**< The type of compression performed:
		                                                 0=using only compressors, 1=using adder trees, 2=mixed, using compressors and adders*/

		vector<int> bitUID;                         /**< A unique identifier for the bits in this bitheap (for each column) */
		int guid;                                   /**< The global UID for this bit heap, useful in operators managing several bit heaps */

		ofstream fileFig;                           /**< The file stream for the bitheap and bitheap compression figures */
		ostringstream fig;                          /**< The stream for the bitheap and bitheap compression figures */
		bool drawCycleLine;                         /**< Draw lines between cycle, or not */
		int drawCycleNumber;                        /**< Draw the cycle number, or not */
		Plotter* plotter;                           /**< The plotter used to draw the bitheap and bitheap compression process */


		// For error reporting to work
		string srcFileName;
		string uniqueName_;
	};


}
#endif
