#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include "Operator.hpp"
#include "utils.hpp"
#include <map>

namespace flopoco {
    ///
    /// \brief The Primitive class. Base class for all primitive implementations.
    ///
    /// Adds the needed generics to operator class.
    ///
    class Primitive : public Operator {
        std::map<std::string, std::string> generics_; //!< Map for generics
      public:
        Primitive(Operator *parentOp, Target *target );
        ~Primitive();

        ///
        /// \brief addPrimitiveLibrary adds the target specific vhdl library to the target
        /// \param op Operator in which libraries shall be added
        /// \param target Target for which libraries shall be added
        ///
        static void addPrimitiveLibrary(OperatorPtr op, Target *target);

        ///
        /// \brief checkTargetCompatibility Deprecated do not use
        /// \param target
        ///
//        static void checkTargetCompatibility( Target *target );

/*
        ///
        /// \brief setGeneric Sets the specific generic
        /// \param name Name of the generic to set
        /// \param value Value of the generic
        ///
        void setGeneric( std::string name, string value );
        ///
        /// \brief setGeneric Sets the specific generic
        /// \param name Name of the generic to set
        /// \param value Value of the generic
        ///
        void setGeneric( string name, const long value );
*/
        ///
        /// \brief getGenerics returns all set generics
        /// \return Map of all generics
        ///
        std::map<std::string, std::string> &getGenerics();

        ///
        /// \brief primitiveInstance Generate instance of the primitive.
        /// \param instanceName Instance name of the primitive.
        /// \return VHDL-code of the instanciation
        ///
        /// Code is adopted from Operator::instance, inserted parts are marked.
        /// Do not use Operator::instance as it will lose generics and doesn't set needed libraries.
        /* The new interface, similar to instance()*/
        std::string primitiveInstance(string instanceName);

        /* the old, deprecated interface */
//        std::string primitiveInstance(string instanceName , OperatorPtr parent = nullptr);

        // Operator interface
      public:
        ///
        /// \brief outputVHDL emptied version of Operator::outputVHDL as it is not needed for primitives.
        /// \param o
        /// \param name
        ///
//        virtual void outputVHDL( ostream &o, string name );
        ///
        /// \brief outputVHDLComponent emptied version of Operator::outputVHDLComponent as it is not needed for primitives.
        /// \param o
        /// \param name
        ///
//        virtual void outputVHDLComponent( ostream &o, string name );

    };
}//namespace

#endif
