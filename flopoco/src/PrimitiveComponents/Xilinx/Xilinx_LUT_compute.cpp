#include "Xilinx_LUT_compute.h"

lut_op lut_in( int id ) {
    lut_op in( id );
    return in;
}

lut_init::lut_init( uint64_t init_content )
{
    this->init_content = init_content;
}

lut_init::lut_init( lut_op op ) {
    has_two_out = false;
    base = op;
    compute();
}

lut_init::lut_init( lut_op op_o5 , lut_op op_o6 ) {
    has_two_out = true;
    lut5_init o5( op_o5 );
    lut5_init o6( op_o6 );
    init_content = ( o5.get() & 0xFFFFFFFF ) | ( o6.get() & 0xFFFFFFFF00000000 );
}

void lut_init::compute() {
    init_content = 0;
    bool in[6];

    for( short i = 0; i < 64; i++ ) {
        for( int b = 0; b < 6; b++ )
            in[b] = i & ( 1 << b );

        if( base.eval( in ) )
            init_content |= ( 1LL << ( long long )i );
    }
}

string lut_init::get_hex() {
    stringstream o;
    o << "x\"" << setw( 16 ) << setfill( '0' ) << std::hex << init_content;
    o << "\"";
    return o.str();
}

string lut_init::get_dez() {
    stringstream o;
    o << init_content;
    return o.str();
}

string lut_init::truth_table() {
    stringstream o;

    if( has_two_out ) {
        o  << "Truth table for " << get_hex() << ":" << endl << endl;
        o << "dez: i4 |i3 |i2 |i1 |i0 ||o6 |o5 " << endl;
        o << "_________________________________" << endl;

        for( short i = 0; i < 32; i++ ) {
            if( i < 10 ) o << " ";

            bool lut_c = ( init_content & ( 1LL << ( long long )i ) );
            bool lut_c2 = ( init_content & ( 1LL << ( long long )( i + 32 ) ) );
            o << " " << i << ":  " << ( i & 0x10 ? 1 : 0 ) << " | " << ( i & 0x8 ? 1 : 0 ) << " | "
              << ( i & 0x4 ? 1 : 0 ) << " | " << ( i & 0x2 ? 1 : 0 ) << " | " << ( i & 0x1 ? 1 : 0 ) << " || "
              << ( lut_c2 ? "1" : "0" ) << " | " << ( lut_c ? "1" : "0" ) << endl;
        }
    } else {
        o  << "Truth table for " << get_hex() << ":" << endl << endl;
        o << "dez: i5 |i4 |i3 |i2 |i1 |i0 ||out" << endl;
        o << "_________________________________" << endl;

        for( short i = 0; i < 64; i++ ) {
            if( i < 10 ) o << " ";

            bool lut_c = ( init_content & ( 1LL << ( long long )i ) );
            o << " " << i << ":  " << ( i & 0x20 ? 1 : 0 ) << " | " << ( i & 0x10 ? 1 : 0 ) << " | " << ( i & 0x8 ? 1 : 0 ) << " | "
              << ( i & 0x4 ? 1 : 0 ) << " | " << ( i & 0x2 ? 1 : 0 ) << " | " << ( i & 0x1 ? 1 : 0 ) << " || " << ( lut_c ? "1" : "0" ) << endl;
        }
    }

    return o.str();
}

string lut5_init::truth_table() {
    stringstream o;
    o  << "Truth table for " << get_hex() << ":" << endl << endl;
    o << "dez: i4 |i3 |i2 |i1 |i0 ||out" << endl;
    o << "_________________________________" << endl;

    for( short i = 0; i < 32; i++ ) {
        if( i < 10 ) o << " ";

        bool lut_c = ( init_content & ( 1LL << ( long long )i ) );
        o << " " << i << ":  " << ( i & 0x10 ? 1 : 0 ) << " |" << ( i & 0x8 ? 1 : 0 ) << " | "
          << ( i & 0x4 ? 1 : 0 ) << " | " << ( i & 0x2 ? 1 : 0 ) << " | " << ( i & 0x1 ? 1 : 0 ) << " || " << ( lut_c ? 1 : 0 ) << endl;
    }

    return o.str();
}

lut5_init::lut5_init( lut_op op ) : lut_init( op ) {
}

string lut5_init::get_hex() {
    stringstream o;
    o << "x\"" << setw( 8 ) << setfill( '0' ) << std::hex << ( init_content & 0xFFFFFFFF );
    o << "\"";
    return o.str();
}

string lut5_init::get_dez() {
    stringstream o;
    o << ( init_content & 0xFFFFFFFF );
    return o.str();
}

uint64_t lut_init::get() {
    return init_content;
}


lut_op::lut_op() {
    type = LUT_NOOP;
}

lut_op::lut_op( int id_ ) {
    id = id_;
    type = LUT_IN;
}

void lut_op::print( string pre ) {
    switch( type ) {
    case LUT_OR:
        cout << "LUT_OR";
        break;

    case LUT_AND:
        cout << "LUT_AND";
        break;

    case LUT_NOT:
        cout << "LUT_NOT";
        break;

    case LUT_XOR:
        cout << "LUT_XOR";
        break;

    case LUT_IN:
        cout << "LUT_IN(" << id << ")";
        break;

    default:
        cout << "LUT_NOOP";
        break;
    }

    cout << endl;

    for( int i = 0; i < ( int )el.size(); i++ ) {
        if( i == ( int )el.size() - 1 ) {
            cout << pre << "*---";
            el[i].print( pre + "    " );
        } else {
            cout << pre << "|---";
            el[i].print( pre + "|   " );
        }
    }
}

lut_op::lut_op( LUT_OP_TYPE t_ ) {
    type = t_;
}

lut_op::lut_op( lut_op *op ) {
    id = op->id;
    type = op->type;
    el = op->el;
}

lut_op lut_op::operator &( const lut_op &op ) {
    lut_op t( this );

    if( type == LUT_NOOP ) {
        t.type = LUT_AND;
        t.add( op );
        return t;
    } else if( type == LUT_AND ) {
        t.add( op );
        return t;
    } else {
        lut_op new_op( LUT_AND );
        new_op.add( t ).add( op );
        return new_op;
    }
}

lut_op lut_op::operator |( const lut_op &op ) {
    lut_op t( this );

    if( type == LUT_NOOP ) {
        t.type = LUT_OR;
        t.add( op );
        return t;
    } else if( type == LUT_OR ) {
        t.add( op );
        return t;
    } else {
        lut_op new_op( LUT_OR );
        new_op.add( t ).add( op );
        return new_op;
    }
}

lut_op lut_op::operator ^( const lut_op &op ) {
    lut_op t( this );

    if( type == LUT_NOOP ) {
        t.type = LUT_XOR;
        t.add( op );
        return t;
    } else if( type == LUT_XOR ) {
        t.add( op );
        return t;
    } else {
        lut_op new_op( LUT_XOR );
        new_op.add( t ).add( op );
        return new_op;
    }
}

lut_op lut_op::operator ~() {
    lut_op t( this );
    lut_op new_op( LUT_NOT );
    new_op.add( t );
    return new_op;
}

lut_op &lut_op::add( const lut_op &op ) {
    el.push_back( op );
    return *this;
}


bool lut_op::eval( bool *in ) {
    switch( type ) {
    case LUT_OR:
        return eval_or( in );

    case LUT_AND:
        return eval_and( in );

    case LUT_NOT:
        return eval_not( in );

    case LUT_XOR:
        return eval_xor( in );

    case LUT_IN:
        return eval_in( in );

    default:
        if( el.size() > 0 )
            return el[0].eval( in );
        else
            return false;
    }
}

bool lut_op::eval_in( bool *in ) {
    return in[id];
}

bool lut_op::eval_or( bool *in ) {
    assert( el.size() >= 1 );
    bool res = el[0].eval( in );

    for( int i = 1; i < ( int )el.size(); i++ )
        res |= el[i].eval( in );

    return res;
}

bool lut_op::eval_xor( bool *in ) {
    assert( el.size() >= 1 );
    bool res = el[0].eval( in );

    for( int i = 1; i < ( int )el.size(); i++ )
        res ^= el[i].eval( in );

    return res;
}

bool lut_op::eval_and( bool *in ) {
    assert( el.size() >= 1 );
    bool res = el[0].eval( in );

    for( int i = 1; i < ( int )el.size(); i++ )
        res &= el[i].eval( in );

    return res;
}

bool lut_op::eval_not( bool *in ) {
    return !( el[0].eval( in ) );
}
