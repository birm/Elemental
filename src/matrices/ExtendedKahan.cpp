/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "El.hpp"

// Generate a 3(2^k) x 3(2^k) Extended Kahan matrix, which has the form
// A = S R, where S = diag(1,zeta,...,zeta^(3 2^k - 1)), 
// 
//         | I -phi H_k    0     |
//     R = | 0    I      phi H_k |,
//         | 0    0        I     |
//
// where H_k is the 2^k x 2^k Hadamard matrix, 0 < mu << 1, and 
// phi^2 + zeta^2 = 1.
//

namespace El {

template<typename F> 
inline void MakeExtendedKahan
( Matrix<F>& A, Base<F> phi, Base<F> mu )
{
    DEBUG_ONLY(CSE cse("MakeExtendedKahan"))
    typedef Base<F> Real;

    if( A.Height() != A.Width() )
        LogicError("Extended Kahan matrices must be square");
    const Int n = A.Height();
    if( n % 3 != 0 )
        LogicError("Dimension must be an integer multiple of 3");
    const Int l = n / 3;
    if( !l || (l & (l-1)) )
        LogicError("n/3 is not a power of two");
    Int k=0;
    while( Int(1u<<k) < l )
        ++k;

    if( phi <= Real(0) || phi >= Real(1) )
        LogicError("phi must be in (0,1)");
    if( mu <= Real(0) || mu >= Real(1) )
        LogicError("mu must be in (0,1)");

    // Start by setting A to the identity, and then modify the necessary 
    // l x l blocks of its 3 x 3 partitioning.
    MakeIdentity( A );
    auto ABlock = A( IR(2*l,3*l), IR(2*l,3*l) );
    ABlock *= mu;
    ABlock = A( IR(0,l), IR(l,2*l) );
    Walsh( ABlock, k );
    ABlock *= -phi;
    ABlock = A( IR(l,2*l), IR(2*l,3*l) );
    Walsh( ABlock, k );
    ABlock *= phi;

    // Now scale A by S
    const Real zeta = Sqrt(Real(1)-phi*phi);
    for( Int i=0; i<n; ++i )
    {
        const Real gamma = Pow(zeta,Real(i));
        for( Int j=0; j<n; ++j )
            A.Set( i, j, gamma*A.Get(i,j) );
    }
}

template<typename F>
inline void MakeExtendedKahan
( AbstractDistMatrix<F>& A, Base<F> phi, Base<F> mu )
{
    DEBUG_ONLY(CSE cse("MakeExtendedKahan"))
    typedef Base<F> Real;

    if( A.Height() != A.Width() )
        LogicError("Extended Kahan matrices must be square");
    const Int n = A.Height();
    if( n % 3 != 0 )
        LogicError("Dimension must be an integer multiple of 3");
    const Int l = n / 3;
    if( !l || (l & (l-1)) )
        LogicError("n/3 is not a power of two");
    Int k=0;
    while( Int(1u<<k) < l )
        ++k;

    if( phi <= Real(0) || phi >= Real(1) )
        LogicError("phi must be in (0,1)");
    if( mu <= Real(0) || mu >= Real(1) )
        LogicError("mu must be in (0,1)");

    // Start by setting A to the identity, and then modify the necessary 
    // l x l blocks of its 3 x 3 partitioning.
    MakeIdentity( A );
    unique_ptr<AbstractDistMatrix<F>> 
        ABlock( A.Construct(A.Grid(),A.Root()) );
    View( *ABlock, A, IR(2*l,3*l), IR(2*l,3*l) );
    *ABlock *= mu;
    View( *ABlock, A, IR(0,l), IR(l,2*l) );
    Walsh( *ABlock, k );
    *ABlock *= -phi;
    View( *ABlock, A, IR(l,2*l), IR(2*l,3*l) );
    Walsh( *ABlock, k );
    *ABlock *= phi;

    // Now scale A by S
    const Real zeta = Sqrt(Real(1)-phi*phi);
    for( Int iLoc=0; iLoc<A.LocalHeight(); ++iLoc )
    {
        const Int i = A.GlobalRow(iLoc);
        const Real gamma = Pow(zeta,Real(i));
        for( Int jLoc=0; jLoc<A.LocalWidth(); ++jLoc )
            A.SetLocal( iLoc, jLoc, gamma*A.GetLocal(iLoc,jLoc) );
    }
}

template<typename F>
void ExtendedKahan( Matrix<F>& A, Int k, Base<F> phi, Base<F> mu )
{
    DEBUG_ONLY(CSE cse("ExtendedKahan"))
    const Int n = 3*(1u<<k);
    A.Resize( n, n );
    MakeExtendedKahan( A, phi, mu );
}

template<typename F>
void ExtendedKahan( AbstractDistMatrix<F>& A, Int k, Base<F> phi, Base<F> mu )
{
    DEBUG_ONLY(CSE cse("ExtendedKahan"))
    const Int n = 3*(1u<<k);
    A.Resize( n, n );
    MakeExtendedKahan( A, phi, mu );
}

#define PROTO(F) \
  template void ExtendedKahan \
  ( Matrix<F>& A, Int k, Base<F> phi, Base<F> mu ); \
  template void ExtendedKahan \
  ( AbstractDistMatrix<F>& A, Int k, Base<F> phi, Base<F> mu );

#define EL_NO_INT_PROTO
#define EL_ENABLE_QUAD
#include "El/macros/Instantiate.h"

} // namespace El
