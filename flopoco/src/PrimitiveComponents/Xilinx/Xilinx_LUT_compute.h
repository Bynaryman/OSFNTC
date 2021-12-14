#ifndef LUTCOMPUTE_H
#define LUTCOMPUTE_H

#include <vector>
#include <sstream>
#include <string>
#include <iomanip>
#include <iostream>

#include <stdint.h>
#include <inttypes.h>
#include <cassert>

using namespace std;

class lut_op {
    enum LUT_OP_TYPE {
        LUT_AND,
        LUT_OR,
        LUT_NOT,
        LUT_XOR,
        LUT_IN,
        LUT_NOOP
    };
  protected:
    int id;
    vector<lut_op> el;
    LUT_OP_TYPE type;

    bool eval_and( bool *in );
    bool eval_not( bool *in );
    bool eval_or( bool *in );
    bool eval_xor( bool *in );
    bool eval_in( bool *in );
    lut_op( LUT_OP_TYPE _t );
    lut_op( lut_op *op );
    lut_op &add( const lut_op &op );
  public:
    lut_op();
    lut_op( int id );

    lut_op operator &( const lut_op &op );
    lut_op operator |( const lut_op &op );
    lut_op operator ^( const lut_op &op );
    lut_op operator ~();
    void print( string pre = "" );
    bool eval( bool *in );
};

lut_op lut_in( int id );

class lut_init {
  protected:
    uint64_t init_content;
    lut_op base;
    bool has_two_out;
  public:
    lut_init( uint64_t init_content );
    lut_init( lut_op op );
    lut_init( lut_op op_o5, lut_op op_o6 );
    void compute();
    string get_hex();
    string get_dez();
    string truth_table();
    uint64_t get();
};

class lut5_init : public lut_init {
  public:
    lut5_init( lut_op op );
    string truth_table();
    string get_hex();
    string get_dez();
};
#endif // LUTCOMPUTE_H
