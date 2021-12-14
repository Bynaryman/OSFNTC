#ifndef COMPRESSOR_HPP
#define COMPRESSOR_HPP


#include <iostream>
#include <sstream>
#include <vector>
#include <gmpxx.h>
#include <stdio.h>
#include <string>

#include "gmp.h"
#include "mpfr.h"

#include "../Operator.hpp"
#include "../utils.hpp"

namespace flopoco
{



	/**
	 * The Compressor class generates basic compressors, that converts
	 * one or several columns of bits with the same weight into a row
	 * of bits of increasing weights.
	 */
	class Compressor : public Operator
	{
	public:

		/**
		 * A basic constructor
		 */
        Compressor(Operator* parentOp, Target * target, vector<int> heights, float area = 0.0, bool compactView = false);

        /*!
         * An empty constructor, needed to derive from this class without generating code
         */
        Compressor(Operator* parentOp, Target *target);

		/**
		 * Destructor
		 */
		~Compressor();


		/**
		 * Return the height of column @column
		 */
		unsigned getColumnSize(int column);

		/**
		 * Return the size of the compressed result
		 */
		int getOutputSize();

		/**
		 * Mark the compressor as having been used for a compression
		 * Return the new value of compressorUsed
		 */
		bool markUsed();

		/**
		 * Mark the compressor as not having been used for a compression
		 * Return the new value of compressorUsed
		 */
		bool markUnused();


		void emulate(TestCase * tc);

		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		/** Register the factory */
		static void registerFactory();

	public:
		vector<int> heights;                /**< the heights of the columns */
		vector<int> outHeights;             /**< the heights of the columns of the output, if this is a partially compressed result */
		int wIn;                            /**< the number of bits at the input of the compressor */
		int wOut;                           /**< size of the output */
		float area;							/**< size of the compressor in LUT-equivalents */

    protected:

        void setWordSizes();
        void createInputsAndOutputs();

		int maxVal;                         /**< the maximum value that the compressor can count up to */
		bool compactView;                   /**< print the VHDL in a more compact way, or not */
		bool compressorUsed;                /**< whether this compressor has been used for a compression, or not */
	};

	class BasicCompressor
	{
	public:
        BasicCompressor(Operator* parentOp_, Target * target, vector<int> heights, float area = 0.0, string type = "combinatorial", bool compactView = false);

		~BasicCompressor();
		/**
		 * returns pointer to the compressor. In that compressor
		 * the constructor will generate vhdl code.
		 */
        virtual Compressor* getCompressor();

		/**
		 * 	@brief returns the amount of different inputcolumns
		 * 	@param middleLength if compressor is variable, middleLength must be set correctly.
		 * 		if its not a variable compressor, middleLength = 0
		 */
		virtual unsigned int getHeights(unsigned int middleLength = 0);

		/**
		 *	@brief returns the amount of different outputcolumns
		 * 	@param middleLength if compressor is variable, middleLength must be set correctly.
		 * 		if its not a variable compressor, middleLength = 0
		 */
		virtual unsigned int getOutHeights(unsigned int middleLength = 0);

		/**
		 * 	@brief returns the amount of inputs for a given inputcolumn of the compressor
		 * 	@param column specifies the column of the compressor
		 * 	@param middleLength if compressor is variable, middleLength must be set correctly.
		 * 		if its not a variable compressor, middleLength = 0
		 */
		virtual unsigned int getHeightsAtColumn(unsigned int column, bool ilpGeneration = false, unsigned int middleLength = 0);

		/**
		 * 	@brief returns the amount of outputs for a given outputcolumn of the compressor
		 * 	@param column specifies the column of the compressor
		 * 	@param middleLength if compressor is variable, middleLength must be set correctly.
		 * 		if its not a variable compressor, middleLength = 0
		 */
		virtual unsigned int getOutHeightsAtColumn(unsigned int column, bool ilpGeneration = false, unsigned int middleLength = 0);

		/**
		 * 	@brief returns efficiency of the compressor. efficiency is computed with the formula
		 * 	(number of inputbits - number of outputbits) / area. If area is zero, the efficiency is zero as well.
		 * 	@param middleLength if compressor is variable, middleLength must be set correctly
		 */
        virtual double getEfficiency(unsigned int middleLength = 0);

		/**
		 *	@brief returns the amount of LUT-equivalents, the compressor needs to be implemented
		 * 	@param middleLength if compressor is variable, middleLength must be set correctly.
		 * 		if its not a variable compressor, middleLength = 0
		 */
        virtual float getArea(unsigned int middleLength = 0);

        /**
         *	@brief returns a string of the compressor e.g. for debugging.
         */
		string getStringOfIO();

        Operator* parentOp;
		Target* target;
		vector<int> heights;                /**< the heights of the columns */
		vector<int> outHeights;             /**< the heights of the columns of the output, if this is a partially compressed result */
		float area;							/**< size of the compressor in LUT-equivalents */
		string type; 						/**< combinatorial or variableLength */
		int wOut;


		Compressor* compressor; 				/**< if getCompressor() is being called for the first time, the pointer of the generated compressor will be saved here. */

	private:
		bool compactView;
	};
}

#endif
