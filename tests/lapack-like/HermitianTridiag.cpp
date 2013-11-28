/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
// NOTE: It is possible to simply include "elemental.hpp" instead
#include "elemental-lite.hpp"
#include "elemental/blas-like/level1/MakeTriangular.hpp"
#include "elemental/blas-like/level1/UpdateDiagonal.hpp"
#include "elemental/lapack-like/HermitianTridiag.hpp"
#include "elemental/lapack-like/Norm/Infinity.hpp"
#include "elemental/lapack-like/Norm/Frobenius.hpp"
#include "elemental/matrices/HermitianUniformSpectrum.hpp"
#include "elemental/matrices/Wigner.hpp"
using namespace std;
using namespace elem;

template<typename F> 
void TestCorrectness
( UpperOrLower uplo, 
  const DistMatrix<F>& A, 
  const DistMatrix<F,STAR,STAR>& t,
        DistMatrix<F>& AOrig,
  bool print, bool display )
{
    typedef Base<F> Real;
    const Grid& g = A.Grid();
    const Int m = AOrig.Height();
    const Real infNormAOrig = HermitianInfinityNorm( uplo, AOrig );
    const Real frobNormAOrig = HermitianFrobeniusNorm( uplo, AOrig );
    if( g.Rank() == 0 )
        cout << "Testing error..." << endl;

    // Grab the diagonal and subdiagonal of the symmetric tridiagonal matrix
    Int subdiagonal = ( uplo==LOWER ? -1 : +1 );
    auto d = A.GetRealPartOfDiagonal();
    auto e = A.GetRealPartOfDiagonal( subdiagonal );
     
    // Grab a full copy of e so that we may fill the opposite subdiagonal 
    DistMatrix<Real,STAR,STAR> e_STAR_STAR( e );
    DistMatrix<Real,MD,STAR> eOpposite(g);
    eOpposite.AlignWithDiagonal( A.DistData(), -subdiagonal );
    eOpposite = e_STAR_STAR;
    
    // Zero B and then fill its tridiagonal
    DistMatrix<F> B(g);
    B.AlignWith( A );
    Zeros( B, m, m );
    B.SetRealPartOfDiagonal( d );
    B.SetRealPartOfDiagonal( e, subdiagonal );
    B.SetRealPartOfDiagonal( eOpposite, -subdiagonal );
    if( print )
        Print( B, "Tridiagonal" );
    if( display )
        Display( B, "Tridiagonal" );

    // Reverse the accumulated Householder transforms, ignoring symmetry
    hermitian_tridiag::ApplyQ( LEFT, uplo, NORMAL, A, t, B );
    hermitian_tridiag::ApplyQ( RIGHT, uplo, ADJOINT, A, t, B );
    if( print )
        Print( B, "Rotated tridiagonal" );
    if( display )
        Display( B, "Rotated tridiagonal" );

    // Compare the appropriate triangle of AOrig and B
    MakeTriangular( uplo, AOrig );
    MakeTriangular( uplo, B );
    Axpy( F(-1), AOrig, B );
    if( print )
        Print( B, "Error in rotated tridiagonal" );
    if( display )
        Display( B, "Error in rotated tridiagonal" );
    const Real infNormError = HermitianInfinityNorm( uplo, B );
    const Real frobNormError = HermitianFrobeniusNorm( uplo, B );

    // Compute || I - Q Q^H ||
    MakeIdentity( B );
    hermitian_tridiag::ApplyQ( RIGHT, uplo, ADJOINT, A, t, B );
    DistMatrix<F> QHAdj( g );
    Adjoint( B, QHAdj );
    MakeIdentity( B );
    hermitian_tridiag::ApplyQ( LEFT, uplo, NORMAL, A, t, B );
    Axpy( F(-1), B, QHAdj );
    hermitian_tridiag::ApplyQ( RIGHT, uplo, ADJOINT, A, t, B );
    UpdateDiagonal( B, F(-1) );
    const Real infNormQError = InfinityNorm( B );
    const Real frobNormQError = FrobeniusNorm( B ); 

    if( g.Rank() == 0 )
    {
        cout << "    ||AOrig||_1 = ||AOrig||_oo = " << infNormAOrig << "\n"
             << "    ||AOrig||_F                = " << frobNormAOrig << "\n"
             << "    || I - Q^H Q ||_oo         = " << infNormQError << "\n"
             << "    || I - Q^H Q ||_F          = " << frobNormQError << "\n"
             << "    ||AOrig - Q^H A Q||_oo     = " << infNormError << "\n"
             << "    ||AOrig - Q^H A Q||_F      = " << frobNormError << endl;
    }
}

template<typename F>
void TestHermitianTridiag
( UpperOrLower uplo, Int m, const Grid& g, 
  bool testCorrectness, bool print, bool display )
{
    DistMatrix<F> A(g), AOrig(g);
    DistMatrix<F,STAR,STAR> t(g);

    //HermitianUniformSpectrum( A, m, -10, 10 );
    Wigner( A, m );
    if( testCorrectness )
    {
        if( g.Rank() == 0 )
        {
            cout << "  Making copy of original matrix...";
            cout.flush();
        }
        AOrig = A;
        if( g.Rank() == 0 )
            cout << "DONE" << endl;
    }
    if( print )
        Print( A, "A" );
    if( display )
        Display( A, "A" );

    if( g.Rank() == 0 )
    {
        cout << "  Starting tridiagonalization...";
        cout.flush();
    }
    mpi::Barrier( g.Comm() );
    const double startTime = mpi::Time();
    HermitianTridiag( uplo, A, t );
    mpi::Barrier( g.Comm() );
    const double runTime = mpi::Time() - startTime;
    const double realGFlops = 16./3.*Pow(double(m),3.)/(1.e9*runTime);
    const double gFlops = ( IsComplex<F>::val ? 4*realGFlops : realGFlops );
    if( g.Rank() == 0 )
    {
        cout << "DONE. " << endl
             << "  Time = " << runTime << " seconds. GFlops = " 
             << gFlops << endl;
    }
    if( print )
    {
        Print( A, "A after HermitianTridiag" );
        Print( t, "t after HermitianTridiag" );
    }
    if( display )
    {
        Display( A, "A after HermitianTridiag" );
        Display( t, "t after HermitianTridiag" );
    }
    if( testCorrectness )
        TestCorrectness( uplo, A, t, AOrig, print, display );
}

int 
main( int argc, char* argv[] )
{
    Initialize( argc, argv );
    mpi::Comm comm = mpi::COMM_WORLD;
    const Int commRank = mpi::CommRank( comm );
    const Int commSize = mpi::CommSize( comm );

    try
    {
        Int r = Input("--gridHeight","height of process grid",0);
        const char uploChar = Input("--uplo","upper or lower storage: L/U",'L');
        const Int m = Input("--height","height of matrix",100);
        const Int nb = Input("--nb","algorithmic blocksize",96);
        const Int nbLocal = Input("--nbLocal","local blocksize",32);
        const bool testCorrectness = Input
            ("--correctness","test correctness?",true);
        const bool print = Input("--print","print matrices?",false);
        const bool display = Input("--display","display matrices?",false);
        ProcessInput();
        PrintInputReport();

        if( r == 0 )
            r = Grid::FindFactor( commSize );
        const Grid g( comm, r );
        const UpperOrLower uplo = CharToUpperOrLower( uploChar );
        SetBlocksize( nb );
        SetLocalSymvBlocksize<double>( nbLocal );
        SetLocalSymvBlocksize<Complex<double>>( nbLocal );
        ComplainIfDebug();
        if( commRank == 0 )
            cout << "Will test HermitianTridiag" << uploChar << endl;

        if( commRank == 0 )
        {
            cout << "----------------------------------\n"
                 << "Double-precision normal algorithm:\n"
                 << "----------------------------------" << endl;
        }
        SetHermitianTridiagApproach( HERMITIAN_TRIDIAG_NORMAL );
        TestHermitianTridiag<double>
        ( uplo, m, g, testCorrectness, print, display );

        if( commRank == 0 )
        {
            cout << "--------------------------------------------------\n"
                 << "Double-precision square algorithm, row-major grid:\n"
                 << "--------------------------------------------------" 
                 << endl;
        }
        SetHermitianTridiagApproach( HERMITIAN_TRIDIAG_SQUARE );
        SetHermitianTridiagGridOrder( ROW_MAJOR );
        TestHermitianTridiag<double>
        ( uplo, m, g, testCorrectness, print, display );


        if( commRank == 0 )
        {
            cout << "--------------------------------------------------\n"
                 << "Double-precision square algorithm, col-major grid:\n"
                 << "--------------------------------------------------" 
                 << endl;
        }
        SetHermitianTridiagApproach( HERMITIAN_TRIDIAG_SQUARE );
        SetHermitianTridiagGridOrder( COLUMN_MAJOR );
        TestHermitianTridiag<double>
        ( uplo, m, g, testCorrectness, print, display );

        if( commRank == 0 )
        {
            cout << "------------------------------------------\n"
                 << "Double-precision complex normal algorithm:\n"
                 << "------------------------------------------" << endl;
        }
        SetHermitianTridiagApproach( HERMITIAN_TRIDIAG_NORMAL );
        TestHermitianTridiag<Complex<double>>
        ( uplo, m, g, testCorrectness, print, display );

        if( commRank == 0 )
        {
            cout << "-------------------------------------------\n"
                 << "Double-precision complex square algorithm, \n"
                 << "row-major grid:\n"
                 << "-------------------------------------------" 
                 << endl;
        }
        SetHermitianTridiagApproach( HERMITIAN_TRIDIAG_SQUARE );
        SetHermitianTridiagGridOrder( ROW_MAJOR );
        TestHermitianTridiag<Complex<double>>
        ( uplo, m, g, testCorrectness, print, display );

        if( commRank == 0 )
        {
            cout << "-------------------------------------------\n"
                 << "Double-precision complex square algorithm, \n"
                 << "col-major grid:\n"
                 << "-------------------------------------------" 
                 << endl;
        }
        SetHermitianTridiagApproach( HERMITIAN_TRIDIAG_SQUARE );
        SetHermitianTridiagGridOrder( COLUMN_MAJOR );
        TestHermitianTridiag<Complex<double>>
        ( uplo, m, g, testCorrectness, print, display );
    }
    catch( exception& e ) { ReportException(e); }

    Finalize();
    return 0;
}
