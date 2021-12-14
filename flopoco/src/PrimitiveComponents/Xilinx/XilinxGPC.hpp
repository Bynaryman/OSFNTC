#pragma once

#include "Operator.hpp"
#include "utils.hpp"
#include "BitHeap/Compressor.hpp"

namespace flopoco
{
    class XilinxGPC : public Compressor
	{
	public:

		/**
		 * A basic constructor
		 */
        XilinxGPC(Operator *parentOp, Target * target, vector<int> heights);

		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		/** Register the factory */
		static void registerFactory();

	public:
        void emulate(TestCase *tc, vector<int> heights);

	private:
	};

    class BasicXilinxGPC : public BasicCompressor
	{
	public:
		BasicXilinxGPC(Operator* parentOp_, Target * target, vector<int> heights);

		virtual Compressor* getCompressor();


	};
}
