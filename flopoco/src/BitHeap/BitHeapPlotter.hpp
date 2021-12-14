/*
   A class used for plotting various drawings in SVG format

   This file is part of the FloPoCo project

Author : Florent de Dinechin, Kinga Illyes, Bogdan Popa, Matei Istoan

Initial software.
Copyright © INSA de Lyon, ENS-Lyon, INRIA, CNRS, UCBL
2016.
All rights reserved.

*/

#ifndef BITHEAPPLOTTER_HPP
#define BITHEAPPLOTTER_HPP

#include <vector>
#include <iostream>
#include <fstream>

#include "BitHeap.hpp"
#include "Bit.hpp"


//pattern fill for compressed bits
//	COMPRESSED_BITS_PATTERN = 1 use pattern to fill drawings of compressed bits
//	COMPRESSED_BITS_PATTERN = 0 fill drawings of compressed bits with white

#define COMPRESSED_BITS_PATTERN 1


namespace flopoco
{

class Bit;
class BitHeap;

	class BitheapPlotter
	{

	public:

		class Snapshot
		{
			public:

				Snapshot(vector<vector<Bit*> > bits, int maxHeight, Bit* soonestBit, Bit *soonestCompressibleBit);


				/**
				 * ordering by availability time
				 */
				friend bool operator< (Snapshot& b1, Snapshot& b2);
				/**
				 * ordering by availability time
				 */
				friend bool operator<= (Snapshot& b1, Snapshot& b2);
				/**
				 * ordering by availability time
				 */
				friend bool operator== (Snapshot& b1, Snapshot& b2);
				/**
				 * ordering by availability time
				 */
				friend bool operator!= (Snapshot& b1, Snapshot& b2);
				/**
				 * ordering by availability time
				 */
				friend bool operator> (Snapshot& b1, Snapshot& b2);
				/**
				 * ordering by availability time
				 */
				friend bool operator>= (Snapshot& b1, Snapshot& b2);


				vector<vector<Bit*> > bits;                          /*< the bits inside a bitheap at the given moment in time wen the snapshot is taken */
				int maxHeight;                                       /*< the height of the bitheap at the given moment in time wen the snapshot is taken */
				Bit *soonestBit;                                     /*< the soonest bit at the given moment in time when the snapshot is taken */
				Bit *soonestCompressibleBit;                         /*< the soonest compressible bit at the given moment in time when the snapshot is taken */

				string srcFileName;
		};

		/**
		 * @brief constructor
		 */
		BitheapPlotter(BitHeap* bitheap);

		/**
		 * @brief destructor
		 */
		~BitheapPlotter();

		/**
		 * @brief take a snapshot of the bitheap's current state
		 * @param soonestBit the bit in the bitheap with the earliest timing
		 * @param soonestCompressibleBit the bit in the bitheap with the earliest timing,
		 * 			which is part of a column that can be compressed
		 */
		void takeSnapshot(Bit *soonestBit, Bit *soonestCompressibleBit);

		/**
		 * @brief plot all the bitheap's stages
		 * 			including the initial content and all the compression stages
		 */
		void plotBitHeap();


	private:

		/**
		 * @brief draws the initial bitheap
		 */
		void drawInitialBitheap();

		/**
		 * @brief draws the stages of the bitheap's compression
		 */
		void drawBitheapCompression();

		/**
		 * @brief initialize the plotter
		 */
		void initializePlotter(bool isInitial);

		/**
		 * @brief add the closing instructions to the plotter
		 */
		void closePlotter(int offsetY, int turnaroundX);

		/**
		 * @brief draw the initial content of the bitheap, before the start of the compression
		 */
		void drawInitialConfiguration(Snapshot *snapshot, int offsetY, int turnaroundX);

		/**
		 * @brief draw the state of the bitheap
		 */
		void drawConfiguration(int index, Snapshot *snapshot, int offsetY, int turnaroundX);

		/**
		 * @brief draws a single bit
		 */
		void drawBit(int index, int turnaroundX, int offsetY, Bit *bit);

		/*
		 * @brief write the SVG initialization functions
		 */
		void addECMAFunction();

		ofstream fig;
		ofstream fig2;

		vector<Snapshot*> snapshots;

		string srcFileName;

		BitHeap* bitheap;
	};
}

#endif
