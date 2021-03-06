/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "El.hpp"

namespace El {

// Draw the spectrum from the specified half-open interval on the real line,
// then rotate with a Haar matrix

template<typename F>
void HermitianUniformSpectrum
( Matrix<F>& A, Int n, Base<F> lower, Base<F> upper )
{
    DEBUG_ONLY(CSE cse("HermitianUniformSpectrum"))
    A.Resize( n, n );
    typedef Base<F> Real;

    // Form d and D
    vector<F> d( n );
    for( Int j=0; j<n; ++j )
        d[j] = SampleUniform<Real>( lower, upper );
    Diagonal( A, d );

    // Apply a Haar matrix from both sides
    Matrix<F> Q, t;
    Matrix<Real> s;
    ImplicitHaar( Q, t, s, n );
    qr::ApplyQ( LEFT, NORMAL, Q, t, s, A );
    qr::ApplyQ( RIGHT, ADJOINT, Q, t, s, A );

    MakeDiagonalReal(A);
}

template<typename F>
void HermitianUniformSpectrum
( AbstractDistMatrix<F>& APre, Int n, Base<F> lower, Base<F> upper )
{
    DEBUG_ONLY(CSE cse("HermitianUniformSpectrum"))
    APre.Resize( n, n );
    const Grid& grid = APre.Grid();
    typedef Base<F> Real;

    // Switch to [MC,MR] so that qr::ApplyQ is fast
    auto APtr = WriteProxy<F,MC,MR>( &APre );
    auto& A = *APtr;

    // Form d and D
    vector<F> d( n );
    if( grid.Rank() == 0 )
        for( Int j=0; j<n; ++j )
            d[j] = SampleUniform<Real>( lower, upper );
    mpi::Broadcast( d.data(), n, 0, grid.Comm() );
    Diagonal( A, d );

    // Apply a Haar matrix from both sides
    DistMatrix<F> Q(grid);
    DistMatrix<F,MD,STAR> t(grid);
    DistMatrix<Real,MD,STAR> s(grid);
    ImplicitHaar( Q, t, s, n );

    // Copy the result into the correct distribution
    qr::ApplyQ( LEFT, NORMAL, Q, t, s, A );
    qr::ApplyQ( RIGHT, ADJOINT, Q, t, s, A );

    // Force the diagonal to be real-valued
    MakeDiagonalReal(A);
}

#define PROTO(F) \
  template void HermitianUniformSpectrum \
  ( Matrix<F>& A, Int n, Base<F> lower, Base<F> upper ); \
  template void HermitianUniformSpectrum \
  ( AbstractDistMatrix<F>& A, Int n, Base<F> lower, Base<F> upper );

#define EL_NO_INT_PROTO
#include "El/macros/Instantiate.h"

} // namespace El
