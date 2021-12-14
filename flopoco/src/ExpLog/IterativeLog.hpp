#ifndef __ITERATIVELOG_HPP
#define __ITERATIVELOG_HPP
#include <vector>
#include <sstream>

#include "Operator.hpp"
#include "ShiftersEtc/LZOC.hpp"
#include "ShiftersEtc/LZOCShifterSticky.hpp"
#include "ShiftersEtc/Shifters.hpp"
#include "Table.hpp"


namespace flopoco{

#define MAXRRSTAGES 2000 // 4000 bits of accuracy should be enough for anybody

	class IterativeLog : public Operator
	{
	protected:

		/**	@brief A table of reciprocals for the first iteration */
		class FirstInvTable : public Table
		{
		public:
			FirstInvTable(Target* target, int p1, int w1);
			~FirstInvTable();
			mpz_class function(int x);
			//  int check_accuracy(int wF);
			int    double2input(double x);
			double input2double(int x);
			mpz_class double2output(double x);
			double output2double(mpz_class x);
			double maxMulOut;
			double minMulOut;
		};

		class FirstLogTable : public Table
		{
		public:
			FirstLogTable(Target *target, int p1, int w1,  FirstInvTable* fit, IterativeLog* op_);
			~FirstLogTable();
			FirstInvTable* fit;
			mpz_class function(int x);
			int    double2input(double x);
			double input2double(int x);
			mpz_class double2output(double x);
			double output2double(mpz_class x);
		private:
			IterativeLog* op;
		};

		class OtherLogTable : public Table
		{
		public:
			OtherLogTable(Target* target, int wIn, int wOut, int which, int ai, int pi);
			~OtherLogTable();
			mpz_class function(int x);
			int    double2input(double x);
			double input2double(int x);
			mpz_class double2output(double x);
			double output2double(mpz_class x);
			/** Which table is that ? useful because the first is different from the others.*/
			int which;
			/** ai is needed to determine epsiloni */
			int ai;
			/** pi is needed to determine epsiloni */
			int pi;
			int outputPrecision;
		};


	public:
		IterativeLog(Target* target, int wE, int wF, int inTableSize=0, map<string, double> inputDelays = emptyDelayMap);
		~IterativeLog();

		//		Overloading the virtual functions of Operator
		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);
		/**Overloading the function of Operator with a function that tests only positive FP numbers (full range)*/
		TestCase* buildRandomTestCase(int i);
		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		static void registerFactory();

		int wE, wF;

		/** The input sizes to the successive tables*/
		int a[MAXRRSTAGES];

		/** The intermediate precision: at step i, the exp part is bounded by
			 1 <= m < 1+2^-p[i]*/
		int p[MAXRRSTAGES];

		/** The useful size of the product Ai*Zi, and hence of the subword of Zi input to the mult.  */
		int psize[MAXRRSTAGES];


		/** size of the virtual mantissa:
			 1.000(p[i] zeroes)00000zzzzzzz
			 the zzzz are the bits of Zi, which will be actually computed  */
		int sfullZ[MAXRRSTAGES];

		/** size of Zi before truncation, should be equal to s[i]+t[i] */
		int sbt[MAXRRSTAGES];

		/** The number of non-zero bits of Zi, will be limited to wF+g */
		int s[MAXRRSTAGES];

		/** The numbers of bits of the full Zi to truncate to limit the size to wF+g */
		int t[MAXRRSTAGES];


		// The max number of stages
		int stages;

		int sfinal; // equal to s[stages+1]

		int pfinal;  // equal to s[stages+1]

		// The guard bits
		int gLog;


		// A global boolean flag disabling the simulation of the fullsize mantissa
		// as soon as it would be more than 64 bits
		int fullSizeSim;

		// The target precision: numbers may be truncated so that their LSB has weight -target_prec
		int target_prec;

		// Various subcomponents
		Shifter *ao_lshift;   // ao stands for "almost one"
		Shifter *ao_rshift;
		LZOC *lzoc;
		LZOCShifterSticky *final_norm;
	};
}
#endif
