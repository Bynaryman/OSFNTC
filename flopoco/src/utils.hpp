#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include <vector>
#include <utility>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <map>
#include <math.h>

#include <inttypes.h>

#include <stdarg.h>


using namespace std;


namespace flopoco{

	/**
	 * This is the definition of a triplet type of data,
	 * as there is no such equivalent in the standard libraries.
	 * Remark: std::tuple can be used as well, but it is defined from C++11 onwards
	 */
	//------------------------------------------------------------------------------
	template<class _type1, class _type2, class _type3>
		struct triplet
	{
		typedef _type1 first_type;
		typedef _type2 second_type;
		typedef _type3 third_type;

		_type1 first;
		_type2 second;
		_type3 third;

		//constructors
		triplet(): first(_type1()), second(_type2()), third (_type3()) {};

		triplet(const _type1& first_, const _type2& second_, const _type3& third_): first(first_), second(second_), third(third_) {};
		triplet(_type1&& first_, _type2&& second_, _type3&& third_): first(move(first_)), second(move(second_)), third(move(third_)) {};

		template<class _type1_t, class _type2_t, class _type3_t>
			triplet(const triplet<_type1_t, _type2_t, _type3_t>& tmp) : first(tmp.first), second(tmp.second), third(tmp.third) {};
		template<class _type1_t, class _type2_t, class _type3_t>
			triplet(triplet<_type1_t, _type2_t, _type3_t>&& tmp) : first(move(tmp.first)), second(move(tmp.second)), third(move(tmp.third)) {};

		//assignment operator
		/*
		triplet& operator=(triplet&& tmp)
		{
			first  = tmp.first;
			second = tmp.second;
			third  = tmp.third;

			return *this;
		}
		*/

		triplet& operator=(const triplet& tmp)
		{
			first  = tmp.first;
			second = tmp.second;
			third  = tmp.third;

			return *this;
		}


		template<class _type1_t, class _type2_t, class _type3_t>
			triplet& operator=(triplet<_type1_t, _type2_t, _type3_t>&& tmp)
		{
			first  = move(tmp.first);
			second = move(tmp.second);
			third  = move(tmp.third);

			return *this;
		}

		/*
		template<class _type1_t, class _type2_t, class _type3_t>
			triplet& operator=(const triplet<_type1_t, _type2_t, _type3_t>& tmp)
		{
			first  = tmp.first;
			second = tmp.second;
			third  = tmp.third;

			return *this;
		}
		*/

		//swap function
		void swap(triplet&& tmp)
		{
			using std::swap;
			swap(first, tmp.first);
			swap(second, tmp.second);
			swap(third, tmp.third);
		}
	};

	//relational operators
	template<class _type1, class _type2, class _type3>
		inline bool operator==(const triplet<_type1, _type2, _type3>& tmp1, const triplet<_type1, _type2, _type3>& tmp2)
	{
		return ((tmp1.first == tmp2.first) && (tmp1.second == tmp2.second) && (tmp1.third == tmp2.third));
	}

	template<class _type1, class _type2, class _type3>
		inline bool operator!=(const triplet<_type1, _type2, _type3>& tmp1, const triplet<_type1, _type2, _type3>& tmp2)
	{
		return ((tmp1.first != tmp2.first) || (tmp1.second != tmp2.second) || (tmp1.third != tmp2.third));
	}

	template<class _type1, class _type2, class _type3>
		inline bool operator<(const triplet<_type1, _type2, _type3>& tmp1, const triplet<_type1, _type2, _type3>& tmp2)
	{
		return ((tmp1.first < tmp2.first)
				|| (!(tmp1.first < tmp2.first) && (tmp1.second < tmp2.second))
				|| (!(tmp1.first < tmp2.first) && !(tmp1.second < tmp2.second) && (tmp1.third < tmp2.third)));
	}

	template<class _type1, class _type2, class _type3>
		inline bool operator<=(const triplet<_type1, _type2, _type3>& tmp1, const triplet<_type1, _type2, _type3>& tmp2)
	{
		return ((tmp1.first <= tmp2.first)
				|| ((tmp1.first > tmp2.first) && (tmp1.second <= tmp2.second))
				|| ((tmp1.first > tmp2.first) && (tmp1.second > tmp2.second) && (tmp1.third <= tmp2.third)));
	}

	template<class _type1, class _type2, class _type3>
		inline bool operator>(const triplet<_type1, _type2, _type3>& tmp1, const triplet<_type1, _type2, _type3>& tmp2)
	{
		return ((tmp1.first > tmp2.first)
				|| (!(tmp1.first > tmp2.first) && (tmp1.second > tmp2.second))
				|| (!(tmp1.first > tmp2.first) && !(tmp1.second > tmp2.second) && (tmp1.third > tmp2.third)));
	}

	template<class _type1, class _type2, class _type3>
		inline bool operator>=(const triplet<_type1, _type2, _type3>& tmp1, const triplet<_type1, _type2, _type3>& tmp2)
	{
		return ((tmp1.first >= tmp2.first)
				|| ((tmp1.first < tmp2.first) && (tmp1.second >= tmp2.second))
				|| ((tmp1.first < tmp2.first) && (tmp1.second < tmp2.second) && (tmp1.third >= tmp2.third)));
	}

	//create a new triplet
	template<class _type1, class _type2, class _type3>
		inline triplet<_type1, _type2, _type3> make_triplet(const _type1& tmp1, const _type2& tmp2, const _type3& tmp3)
	{
		return triplet<_type1, _type2, _type3>(tmp1, tmp2, tmp3);
	}


	//swap the content of two triplets
	/*
	template<class _type1, class _type2, class _type3>
		inline void swap(triplet<_type1, _type2, _type3>& tmp1, triplet<_type1, _type2, _type3>& tmp2)
	{
		tmp1.swap(tmp2);
	}

	template<class _type1, class _type2, class _type3>
		inline void swap(triplet<_type1, _type2, _type3>&& tmp1, triplet<_type1, _type2, _type3>& tmp2)
	{
		tmp1.swap(tmp2);
	}

	template<class _type1, class _type2, class _type3>
		inline void swap(triplet<_type1, _type2, _type3>& tmp1, triplet<_type1, _type2, _type3>&& tmp2)
	{
		tmp1.swap(tmp2);
	}
	*/
	//------------------------------------------------------------------------------

	/** This class contains all information about a random state
	 * it has to be used has a singleton to initialize and share a random state
	 * between calls to getLargeRandom function
	 */
	class FloPoCoRandomState {
		private:
			/** the function init could be called from several places, to avoid
			 *	multiple initialization this variable will be set to true on
			 * 	the first call to init, and then will trigger a quick return of init
			 * 	without a new complete initialization of the random state
			 **/
			static bool isInit_;

		public:
			/**
			 * public value to store currend gmp random state
			 */
			static gmp_randstate_t m_state;


			/**
			 * static public function to initialize random generator
			 * with a seed base on the integer n
			 * @param n the integer used to generate the seed
			 * @param force  if set will not consider the isInit_ flag
			 */
			static void init(int n, bool force = true);
	};

	/** Returns under the form of a string of given size, the unsigned binary representation of an integer.
	 * @param x the number to be represented in unsigned binary
	 * @param size the size of the output string
	 * @return the string binary representation of x
	 */
	string unsignedBinary(mpz_class x, int size);

	/** Return the binary representation of a floating point number in the
	 * FPLibrary/FloPoCo format
	 * @param x the number to be represented
	 * @param wE the width (in bits) of the result exponent
	 * @param wF the width (in bits) of the result fraction
	 */
	string fp2bin(mpfr_t x, int wE, int wF);

	/** Return the binary representation of a floating point number in the IEEE format
	 * @param x the number to be represented
	 * @param wE the width (in bits) of the result exponent
	 * @param wF the width (in bits) of the result fraction
	 */
	string ieee2bin(mpfr_t x, int wE, int wF);

	/** Return the binary representation of a floating point number in the IEEE format
	 * @param x the number to be represented
	 * @param wE the width (in bits) of the result exponent
	 * @param wF the width (in bits) of the result fraction
	 */
	string bin2ieee(mpz_class x, int wE, int wF);

	/** return the  bits ranging from msb to lsb of an MPFR, (total size msb-lsb+1)
	 * @param x the number to be represented
	 * @param msb the weight of the MSB.
	 * @param lsb the weight of the LSB
	 * @param[in] margins	integer argument determining the position of the quotes in the output string. The options are: -2= no quotes; -1=left quote; 0=both quotes 1=right quote
*/
	std::string unsignedFixPointNumber(mpfr_t x, int msb, int lsb, int margins=0);

	/** return the binary representation of an MPFR, with bits ranging from msb to lsb
	 * (total size msb-lsb+1), sign bit at weight msb
	 * @param x the number to be represented
	 * @param msb the weight of the MSB.
	 * @param lsb the weight of the LSB
	 * @param[in] margins	integer argument determining the position of the quotes in the output string. The options are: -2= no quotes; -1=left quote; 0=both quotes 1=right quote
*/
	std::string signedFixPointNumber(mpfr_t x, int msb, int lsb, int margins=0);


	/** Prints the binary representation of a integer on size bits
	 * @param o the output stream
	 * @param number [uint64_t] the number to be represented
	 * @param size the number of bits of the output
	 */
	void printBinNum(ostream& o, uint64_t x, int size);

	/** Prints the binary representation of a integer on size bits
	 * @param o the output stream
	 * @param number the number to be represented
	 * @param size the number of bits of the output
	 */
	void printBinNumGMP(ostream& o, mpz_class number, int size);

	/** returns a string for a mpfr_t using the mantissa b exponent notation */
	string printMPFR(mpfr_t x);

	/** Prints the binary representation of a positive integer on size bits
	 * @param o the output stream
	 * @param number the number to be represented
	 * @param size the number of bits of the output
	 */
	void printBinPosNumGMP(ostream& o, mpz_class number, int size);

	/** Function which rounds a FP on a given number of bits
	 * @param number the numer to be rounded
	 * @param bits the number of bits of the result
	 * @return the rounded number on "bits" bits
	 */
	double iround(double number, int bits);

	/** Function which truncates a FP on a given number of bits
	 * @param number the numer to be truncated
	 * @param bits the number of bits of the result
	 * @return the truncated number on "bits" bits
	 */
	double itrunc(double number, int bits);

	/** Function which returns floor a FP on a given number of bits
	 * @param number the numer to be floored
	 * @param bits the number of bits of the result
	 * @return the floored number on "bits" bits
	 */
	double ifloor(double number, int bits);

	/** Function which returns the maximum exponent on wE bits
	 * @param wE the number of bits
	 * @return the maximum exponent on wE bits
	 */
	mpz_class maxExp(int wE);

	/** Function which returns the minimum exponent on wE bits
	 * @param wE the number of bits
	 * @return the minimum exponent on wE bits
	 */
	mpz_class minExp(int wE);

	/** Function which returns the bias for an exponent on wE bits
	 * @param wE the number of bits of the exponent
	 * @return the bias corresponding to this exponent
	 */
	mpz_class bias(int wE);

	/** 2 to the power function.
	 * @param power the power at which 2 is raised
	 * @return 2^power
	 */
	double intpow2(int power);


	/** 2 to the power function.
	 * @param power the power at which 2 is raised
	 * @return 2^power
	 */
	mpz_class mpzpow2(unsigned int power);



	/** 2 ^ (- minusPower). Exact, no round
	 * @param minusPower - the power at which 2 will be raised
	 * @return 2 ^ (- minusPower)
	 */
	//	double invintpow2(int minusPower);

	/** How many bits does it take to write number.
	 * @param number the number to be represented (floating point)
	 * @return the number of bits
	 */
	int intlog2(double number);

	/** computes a logarithm in a given base
	 * @param base base of the logarithm
	 * @param number the number to be represented (floating point)
	 * @return the result is bits (ceil)
	 */
	int intlog(mpz_class base, mpz_class number);


	/** How many bits does it take to write number.
	 * @param number the number to be represented (mpz_class)
	 * @return the number of bits
	 */
	int intlog2(mpz_class number);

	/** How many bits are set at 1 in the mnumber.
	 * @param number the number of which we count the bits at 1
	 * @return the number of these bits
	 */
	mpz_class popcnt(mpz_class number);



	/** Maximum.
	 * @param[double] x first number
	 * @param[double] y second number
	 * @return maximum between x and y
	 */
	inline double max(double x, double y) {return (x > y ? x : y);}

	/** Maximum.
	 * @param[int] count the number of parameters which follows
	 * @return maximum between the variable number of arguments
	 */
	double max(int count, ...);

	/** Maximum.
	 * @param[int] count the number of parameters which follows
	 * @return maximum between the variable number of arguments
	 */
	int maxInt(int count, ...);

	/** Minimum.
	 * @param[double] x first number
	 * @param[double] y second number
	 * @return minimum between x and y
	 */
	inline double min(double x, double y) {return (x < y ? x : y);}

	/** Minimum.
	 * @param[int] count the number of parameters which follows
	 * @return minimum between the variable number of arguments
	 */
	double min(int count, ...);

	/** Minimum.
	 * @param[int] count the number of parameters which follows
	 * @return minimum between the variable number of arguments
	 */
	int minInt(int count, ...);

	/** Maximum.
	 * @param[int] x first number
	 * @param[int] y second number
	 * @return maximum between x and y
	 */
	inline int max(int x, int y) {return (x > y ? x : y);}

	/** Minimum.
	 * @param[int] x first number
	 * @param[int] y second number
	 * @return minimum between x and y
	 */
	inline int min(int x, int y) {return (x < y ? x : y);}

	/** Maximum.
	 * @param[int] x first number
	 * @param[int] y second number
	 * @return maximum between x and y
	 */
	inline mpz_class max(mpz_class x, mpz_class y) {return (x > y ? x : y);}

	/** Minimum.
	 * @param[mpz_class] x first number
	 * @param[mpz_class] y second number
	 * @return minimum between x and y
	 */
	inline mpz_class min(mpz_class x, mpz_class y) {return (x < y ? x : y);}

	/**
	 * Generate a very big random number.
	 * Due to rereusage of a PRNG, this function might be suboptimal.
	 * @param n bit-width of the target random number.
	 * @return an mpz_class representing the random number.
	 */
	mpz_class getLargeRandom(int n);

	/**
	 * A zero generator method which takes as input two arguments and returns a string of zeros with quotes as stated by the second argurment
	 * @param[in] n		    integer argument representing the number of zeros on the output string
	 * @param[in] margins	integer argument determining the position of the quotes in the output string. The options are: -2= no quotes; -1=left quote; 0=both quotes 1=right quote
	 * @return returns a string of zeros with the corresonding quotes given by margins
	 **/
	string zg(int n, int margins=0);

	/**
	* A one generator method which takes as input two arguments and returns a string of zeros with quotes as stated by the second argurment
	* @param[in] n		    integer argument representing the number of zeros on the output string
	* @param[in] margins	integer argument determining the position of the quotes in the output string. The options are: -2= no quotes; -1=left quote; 0=both quotes 1=right quote
	* @return returns a string of zeros with the corresonding quotes given by margins
	**/
	string og(int n, int margins=0);

	/**
	 * Generate an integer that is of the form : 111....11 (with the number of
	 * ones given by n)
	 */
	int oneGenerator(int n);


	/**
	 * Turns an arbitrary string (e.g. Sollya expression or FP number) to
	 * part of a valid VHDL identifier. May (and usually will) loose information.
	 * Looks ugly.
	 * May begin with a digit.
	 * @param[in] expr		expression to convert
	 **/
	string vhdlize(string const & expr);

	/**
	 * Turns a double to a string that will go in a valid VHDL name
	 */
	string  vhdlize(double num);

	/**
	 * Turns a (possibly negative) int to a string that will go in a valid VHDL name
	 */
	string vhdlize(int num);

	string mpz2string(mpz_class x);

	/** Helper function for VHDL output: concatenates an id and a number.
		 vhdl << join("z", i) << ...
		 is a (rather useless) shorthand for
		 vhdl << "z" << i << ...
		 Its main advantage is that join("z", i) can also be used inside
		 declare(),  use(), etc. */
	string join( std::string id, int n);

	string join( std::string id, int n1, int n2);
	string join( std::string id, int n1, int n2, int n3);
	string join( std::string id, string sep, int n);
	string join( std::string id1, int n, std::string id2);
	string join( std::string id, int n, std::string id2 , int n2);
	string join( std::string id, int n, std::string id2 , int n2, std::string id3);
	string join( std::string id, int n, std::string id2 , int n2, std::string id3, int n3);
	string join( std::string id, std::string id2 , int n2, std::string id3);

	/** Same for concatenating two ids. Maybe + would do? */
	string join( std::string id, std::string n);

	/** Same for concatenating three ids. Maybe + would do? */
	string join( std::string id, std::string id2, std::string id3);

	/** Helper function for VHDL output: returns (left downto right)
	 */
	string range(int left, int right);

	/** Helper function for VHDL output: returns "(left downto right => s)"
	 */
	string rangeAssign( int left, int right, std::string s);

	/** Helper function for VHDL output: returns "(x)"
	 */
	string of( int x);


	/** Helper function for VHDL output: returns s, padded left and right with zeroes.
			TODO this function should be removed
	 */
	string align( int left, string s, int right );

	/**
	 *@param[in] inputDelays	map between signal names and their respective delays
	 *@return the maximum delay of the input map
	 */
	double getMaxInputDelays( map<string, double> inputDelays );

	map<string, double> inDelayMap(string s, double d);

	/**
	 *@param[in] inputDelays	map between signal names and their respective delays
	 *@return prints the delays
	 */
	string printInputDelays( map <string, double> inputDelays);

	string printMapContent( map <string, int> inputDelays);

	string printVectorContent( vector< pair<string, int> > table);

	string to_lowercase(const std::string& s);

	/** a function that converts a bit vector (an mpz input to emulate()) to its signed value */
	mpz_class bitVectorToSigned(mpz_class x, int size);

	/** a function that converts a signed mpz_class to the corresponding bit vector represented as two's complement on size bits (sign bit included), to be used in the output of emulate() */
	mpz_class signedToBitVector(mpz_class x, int size);

	/** A function to help VHDL casts */
	string std_logic_vector(const string& s );

	/** A helper function when displaying comments: center str to a certain width */
	string center(const string& str, char padchar=' ', int width=80);

	/** A helper function that will convert a signal name into its lowercase version */
	string toLower(const string& str);
}




#endif
