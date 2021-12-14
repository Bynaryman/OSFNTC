#include "BooleanEquation.hpp"
#include <stdexcept>

bool_eq_operators::bool_eq_op *bool_eq::getOpCopy() const {
    if( op_ != nullptr ) {
        return op_->getCopy();
    } else {
        return nullptr;
    }
}

bool_eq::bool_eq(): op_( nullptr ) {
}

void bool_eq::getInputs( std::set<unsigned long> &inputs ) const {
    if( op_ != nullptr ) {
        op_->getInputs( inputs );
    }
}

bool_eq::bool_eq( const bool_eq &rhs ) : op_( rhs.getOpCopy() ) {
}

bool_eq::bool_eq( bool_eq_operators::bool_eq_op *op ): op_( op ) {
}

bool_eq::~bool_eq() {
    if( op_ != nullptr ) {
        delete op_;
    }
}

bool bool_eq::eval( const std::vector<bool> &input ) const {
    if( op_ != nullptr ) {
        return op_->eval( input );
    } else {
        return false;
    }
}

std::vector<unsigned long> bool_eq::getInputs() const {
    std::set<unsigned long> inputs;
    getInputs( inputs );
    return std::vector<unsigned long>( inputs.begin(), inputs.end() );
}

unsigned long bool_eq::getInputCount() const {
    std::set<unsigned long> inputs;
    getInputs( inputs );
    return inputs.size();
}

bool_eq &bool_eq::operator =( const bool_eq &rhs ) {
    if( op_ != nullptr ) {
        delete op_;
    }

    op_ = rhs.getOpCopy();
    return *this;
}

bool_eq bool_eq::operator &( const bool_eq &rhs ) const {
    if( op_ == nullptr ) {
        return rhs;
    }

    return bool_eq( new bool_eq_operators::bool_eq_and( *this, rhs ) );
}

bool_eq bool_eq::operator &=( const bool_eq &rhs ) {
    return ( *this = *this & rhs );
}

bool_eq bool_eq::operator |( const bool_eq &rhs ) const {
    if( op_ == nullptr ) {
        return rhs;
    }

    return bool_eq( new bool_eq_operators::bool_eq_or( *this, rhs ) );
}

bool_eq bool_eq::operator |=( const bool_eq &rhs ) {
    return ( *this = *this | rhs );
}

bool_eq bool_eq::operator ^( const bool_eq &rhs ) const {
    if( op_ == nullptr ) {
        return rhs;
    }

    return bool_eq( new bool_eq_operators::bool_eq_xor( *this, rhs ) );
}

bool_eq bool_eq::operator ^=( const bool_eq &rhs ) {
    return ( *this = *this ^ rhs );
}

bool_eq bool_eq::operator ~() const {
    if( op_ == nullptr ) {
        throw std::runtime_error( "Cannot invert boolean equation without content" );
    }

    return bool_eq( new bool_eq_operators::bool_eq_inv( *this ) );
}

bool_eq bool_eq::in( const unsigned long &index ) {
    return bool_eq( new bool_eq_operators::bool_eq_in( index ) );
}

bool_eq_operators::bool_eq_and::bool_eq_and( const bool_eq &in0, const bool_eq &in1 ): in0_( in0 ), in1_( in1 ) {
}

bool bool_eq_operators::bool_eq_and::eval( const std::vector<bool> &input ) const {
    return in0_.eval( input ) & in1_.eval( input );
}

bool_eq_operators::bool_eq_op *bool_eq_operators::bool_eq_and::getCopy() const {
    return new bool_eq_and( *this );
}

bool_eq_operators::bool_eq_or::bool_eq_or( const bool_eq &in0, const bool_eq &in1 ): in0_( in0 ), in1_( in1 ) {
}

bool bool_eq_operators::bool_eq_or::eval( const std::vector<bool> &input ) const {
    return in0_.eval( input ) | in1_.eval( input );
}

bool_eq_operators::bool_eq_op *bool_eq_operators::bool_eq_or::getCopy() const {
    return new bool_eq_or( *this );
}

bool_eq_operators::bool_eq_xor::bool_eq_xor( const bool_eq &in0, const bool_eq &in1 ): in0_( in0 ), in1_( in1 ) {
}

bool bool_eq_operators::bool_eq_xor::eval( const std::vector<bool> &input )  const {
    return in0_.eval( input ) | in1_.eval( input );
}

bool_eq_operators::bool_eq_op *bool_eq_operators::bool_eq_xor::getCopy() const {
    return new bool_eq_xor( *this );
}

bool_eq_operators::bool_eq_inv::bool_eq_inv( const bool_eq &in0 ): in0_( in0 ) {
}

bool bool_eq_operators::bool_eq_inv::eval( const std::vector<bool> &input )  const {
    return !in0_.eval( input );
}

bool_eq_operators::bool_eq_op *bool_eq_operators::bool_eq_inv::getCopy() const {
    return new bool_eq_inv( *this );
}

bool_eq_operators::bool_eq_in::bool_eq_in( const unsigned long &index ): index_( index ) {
}

bool bool_eq_operators::bool_eq_in::eval( const std::vector<bool> &input )  const {
    if( input.size() > index_ ) {
        return input[index_];
    } else {
        return false;
    }
}

bool_eq_operators::bool_eq_op *bool_eq_operators::bool_eq_in::getCopy() const {
    return new bool_eq_in( *this );
}
