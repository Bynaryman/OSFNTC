#ifndef BOOL_EQ_H
#define BOOL_EQ_H

#include <vector>
#include <set>

namespace bool_eq_operators {
    class bool_eq_op;
}

class bool_eq {
    bool_eq_operators::bool_eq_op *op_;

    bool_eq_operators::bool_eq_op *getOpCopy() const;

  public:
    bool_eq();

    bool_eq( const bool_eq &rhs );

    explicit bool_eq( bool_eq_operators::bool_eq_op *op );

    ~bool_eq();

    bool eval( const std::vector<bool> &input ) const;

    void getInputs( std::set<unsigned long> &inputs ) const;

    std::vector<unsigned long> getInputs() const;

    unsigned long getInputCount() const;

    bool_eq &operator =( const bool_eq &rhs );

    // Operators
    bool_eq operator &( const bool_eq &rhs ) const;

    bool_eq operator &=( const bool_eq &rhs );


    bool_eq operator |( const bool_eq &rhs ) const;

    bool_eq operator |=( const bool_eq &rhs );

    bool_eq operator ^( const bool_eq &rhs ) const;
    bool_eq operator ^=( const bool_eq &rhs );

    bool_eq operator ~( ) const;

    static bool_eq in( const unsigned long &index );
};

namespace bool_eq_operators {
    class bool_eq_op {
      public:
        virtual ~bool_eq_op() {
        }

        virtual bool_eq_op *getCopy() const = 0;
        virtual bool eval( const std::vector<bool> &input ) const = 0;
        virtual void getInputs( std::set<unsigned long> &inputs ) const = 0;
    };

    class bool_eq_and : public bool_eq_op {
        const bool_eq in0_;
        const bool_eq in1_;
      public:
        bool_eq_and( const bool_eq &in0, const bool_eq &in1 );

        bool eval( const std::vector<bool> &input ) const;

        bool_eq_op *getCopy() const;
        void getInputs( std::set<unsigned long> &inputs ) const {
            in0_.getInputs( inputs );
            in1_.getInputs( inputs );
        }
    };

    class bool_eq_or : public bool_eq_op {
        const bool_eq in0_;
        const bool_eq in1_;
      public:
        bool_eq_or( const bool_eq &in0, const bool_eq &in1 );

        bool eval( const std::vector<bool> &input ) const;

        bool_eq_op *getCopy() const;
        void getInputs( std::set<unsigned long> &inputs ) const {
            in0_.getInputs( inputs );
            in1_.getInputs( inputs );
        }
    };

    class bool_eq_xor : public bool_eq_op {
        const bool_eq in0_;
        const bool_eq in1_;
      public:
        bool_eq_xor( const bool_eq &in0, const bool_eq &in1 );

        bool eval( const std::vector<bool> &input ) const;

        bool_eq_op *getCopy() const;
        void getInputs( std::set<unsigned long> &inputs ) const {
            in0_.getInputs( inputs );
            in1_.getInputs( inputs );
        }
    };

    class bool_eq_inv : public bool_eq_op {
        const bool_eq in0_;
      public:
        bool_eq_inv( const bool_eq &in0 );

        bool eval( const std::vector<bool> &input ) const;

        bool_eq_op *getCopy() const;
        void getInputs( std::set<unsigned long> &inputs ) const {
            in0_.getInputs( inputs );
        }
    };

    class bool_eq_in : public bool_eq_op {
        const unsigned long index_;
      public:
        bool_eq_in( const unsigned long &index );

        bool eval( const std::vector<bool> &input ) const;

        bool_eq_op *getCopy() const;
        void getInputs( std::set<unsigned long> &inputs ) const {
            inputs.insert( index_ );
        }
    };
}
#endif
