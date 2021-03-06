/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "El.hpp"

#include "../util.hpp"

namespace El {
namespace qp {
namespace direct {

//
// Despite the fact that the CVXOPT documentation [1] suggests a single-stage
// procedure for initializing (x,y,z,s), a post-processed two-stage procedure 
// is currently used by the code [2], which, in the case that G = -I and h = 0,
//
// 1) Minimize || x ||^2, s.t. A x = b  by solving
//
//    | Q A^T -I | |  x |   | 0 |
//    | A  0   0 | |  u | = | b |,
//    |-I  0  -I | | -s |   | 0 |
//
//   where 'u' is an unused dummy variable. A Schur-complement manipulation
//   yields
//
//    | Q+I A^T | | x |   | 0 |
//    | A    0  | | u | = | b |.
//
// 2) Minimize || z ||^2, s.t. A^T y - z + c in range(Q) by solving
//
//    | Q A^T -I | | u |   | -c |
//    | A  0   0 | | y | = |  0 |,
//    |-I  0  -I | | z |   |  0 |
//
//    where 'u = -z' is an unused dummy variable. A Schur-complement 
//    manipulation yields
//
//    | Q+I A^T | | -z |   | -c |
//    | A    0  | |  y | = |  0 |.
//
// 3) Set 
//
//      alpha_p := -min(x), and
//      alpha_d := -min(z).
//
//    Then shift x and z according to the rules:
//
//      x := ( alpha_p > -sqrt(eps)*Max(1,||x||_2) ? x + (1+alpha_p)e : x )
//      z := ( alpha_d > -sqrt(eps)*Max(1,||z||_2) ? z + (1+alpha_d)e : z ),
//
//    where 'eps' is the machine precision, 'e' is a vector of all ones 
//    (for more general conic optimization problems, it is the product of 
//    identity elements from the Jordan algebras whose squares yield the 
//    relevant cone.
//
//    Since the post-processing in step (3) has a large discontinuity as the 
//    minimum entry approaches sqrt(eps)*Max(1,||q||_2), we also provide
//    the ability to instead use an entrywise lower clip.
//
// [1] L. Vandenberghe
//     "The CVXOPT linear and quadratic cone program solvers"
//     <http://www.seas.ucla.edu/~vandenbe/publications/coneprog.pdf>
//
// [2] L. Vandenberghe
//     CVXOPT's source file, "src/python/coneprog.py"
//     <https://github.com/cvxopt/cvxopt/blob/f3ca94fb997979a54b913f95b816132f7fd44820/src/python/coneprog.py>
//

template<typename Real>
void Initialize
( const Matrix<Real>& Q, const Matrix<Real>& A, 
  const Matrix<Real>& b, const Matrix<Real>& c,
        Matrix<Real>& x,       Matrix<Real>& y,
        Matrix<Real>& z,
  bool primalInit, bool dualInit, bool standardShift )
{
    DEBUG_ONLY(CSE cse("qp::direct::Initialize"))
    const Int m = A.Height();
    const Int n = A.Width();
    if( primalInit )
        if( x.Height() != n || x.Width() != 1 )
            LogicError("x was of the wrong size");
    if( dualInit )
    {
        if( y.Height() != m || y.Width() != 1 )
            LogicError("y was of the wrong size");
        if( z.Height() != n || z.Width() != 1 )
            LogicError("z was of the wrong size");
    }
    if( primalInit && dualInit )
    {
        // TODO: Perform a consistency check
        return;
    }

    // Form the KKT matrix
    // ===================
    Matrix<Real> J, ones;
    Ones( ones, n, 1 );
    AugmentedKKT( Q, A, ones, ones, J );

    // Factor the KKT matrix
    // =====================
    Matrix<Real> dSub;
    Matrix<Int> p;
    LDL( J, dSub, p, false );

    Matrix<Real> rc, rb, rmu, u, v, d;
    Zeros( rmu, n, 1 );
    if( !primalInit )
    {
        // Minimize || x ||^2, s.t. A x = b  by solving
        //
        //    | Q+I A^T | | x |   | 0 |
        //    | A    0  | | u | = | b |,
        //
        // where 'u' is an unused dummy variable.
        Zeros( rc, n, 1 );
        rb = b;
        rb *= -1;
        Zeros( rmu, n, 1 );
        AugmentedKKTRHS( ones, rc, rb, rmu, d );
        ldl::SolveAfter( J, dSub, p, d, false );
        ExpandAugmentedSolution( ones, ones, rmu, d, x, u, v );
    }
    if( !dualInit ) 
    {
        // Minimize || z ||^2, s.t. A^T y - z + c in range(Q) by solving
        //
        //    | Q+I A^T | | -z |   | -c |
        //    | A    0  | |  y | = |  0 |.
        rc = c;
        Zeros( rb, m, 1 );
        AugmentedKKTRHS( ones, rc, rb, rmu, d );
        ldl::SolveAfter( J, dSub, p, d, false );
        ExpandAugmentedSolution( ones, ones, rmu, d, z, y, u );
        z *= -1;
    }

    const Real epsilon = Epsilon<Real>();
    const Real xNorm = Nrm2( x );
    const Real zNorm = Nrm2( z );
    const Real gammaPrimal = Sqrt(epsilon)*Max(xNorm,Real(1));
    const Real gammaDual   = Sqrt(epsilon)*Max(zNorm,Real(1));
    if( standardShift )
    {
        // alpha_p := min { alpha : x + alpha*e >= 0 }
        // -------------------------------------------
        const auto xMinPair = VectorMin( x );
        const Real alphaPrimal = -xMinPair.value;
        if( alphaPrimal >= Real(0) && primalInit )
            RuntimeError("initialized x was non-positive");

        // alpha_d := min { alpha : z + alpha*e >= 0 }
        // -------------------------------------------
        const auto zMinPair = VectorMin( z );
        const Real alphaDual = -zMinPair.value;
        if( alphaDual >= Real(0) && dualInit )
            RuntimeError("initialized z was non-positive");

        if( alphaPrimal >= -gammaPrimal )
            Shift( x, alphaPrimal+1 );
        if( alphaDual >= -gammaDual )
            Shift( z, alphaDual+1 );
    }
    else
    {
        LowerClip( x, gammaPrimal );
        LowerClip( z, gammaDual   );
    }
}

template<typename Real>
void Initialize
( const AbstractDistMatrix<Real>& Q, const AbstractDistMatrix<Real>& A, 
  const AbstractDistMatrix<Real>& b, const AbstractDistMatrix<Real>& c,
        AbstractDistMatrix<Real>& x,       AbstractDistMatrix<Real>& y,
        AbstractDistMatrix<Real>& z,
  bool primalInit, bool dualInit, bool standardShift )
{
    DEBUG_ONLY(CSE cse("qp::direct::Initialize"))
    const Int m = A.Height();
    const Int n = A.Width();
    const Grid& g = A.Grid();
    if( primalInit )
        if( x.Height() != n || x.Width() != 1 )
            LogicError("x was of the wrong size");
    if( dualInit )
    {
        if( y.Height() != m || y.Width() != 1 )
            LogicError("y was of the wrong size");
        if( z.Height() != n || z.Width() != 1 )
            LogicError("z was of the wrong size");
    }
    if( primalInit && dualInit )
    {
        // TODO: Perform a consistency check
        return;
    }

    // Form the KKT matrix
    // ===================
    DistMatrix<Real> J(g), ones(g);
    Ones( ones, n, 1 );
    AugmentedKKT( Q, A, ones, ones, J );

    // Factor the KKT matrix
    // =====================
    DistMatrix<Real> dSub(g);
    DistMatrix<Int> p(g);
    LDL( J, dSub, p, false );

    DistMatrix<Real> rc(g), rb(g), rmu(g), u(g), v(g), d(g);
    Zeros( rmu, n, 1 );
    if( !primalInit )
    {
        // Minimize || x ||^2, s.t. A x = b  by solving
        //
        //    | Q+I A^T | | x |   | 0 |
        //    | A    0  | | u | = | b |,
        //
        // where 'u' is an unused dummy variable.
        Zeros( rc, n, 1 );
        rb = b;
        rb *= -1;
        Zeros( rmu, n, 1 );
        AugmentedKKTRHS( ones, rc, rb, rmu, d );
        ldl::SolveAfter( J, dSub, p, d, false );
        ExpandAugmentedSolution( ones, ones, rmu, d, x, u, v );
    }
    if( !dualInit ) 
    {
        // Minimize || z ||^2, s.t. A^T y - z + c in range(Q) by solving
        //
        //    | Q+I A^T | | -z |   | -c |
        //    | A    0  | |  y | = |  0 |.
        rc = c;
        Zeros( rb, m, 1 );
        AugmentedKKTRHS( ones, rc, rb, rmu, d );
        ldl::SolveAfter( J, dSub, p, d, false );
        ExpandAugmentedSolution( ones, ones, rmu, d, z, y, u );
        z *= -1;
    }

    const Real epsilon = Epsilon<Real>();
    const Real xNorm = Nrm2( x );
    const Real zNorm = Nrm2( z );
    const Real gammaPrimal = Sqrt(epsilon)*Max(xNorm,Real(1));
    const Real gammaDual   = Sqrt(epsilon)*Max(zNorm,Real(1));
    if( standardShift )
    {
        // alpha_p := min { alpha : x + alpha*e >= 0 }
        // -------------------------------------------
        const auto xMinPair = VectorMin( x );
        const Real alphaPrimal = -xMinPair.value;
        if( alphaPrimal >= Real(0) && primalInit )
            RuntimeError("initialized x was non-positive");

        // alpha_d := min { alpha : z + alpha*e >= 0 }
        // -------------------------------------------
        const auto zMinPair = VectorMin( z );
        const Real alphaDual = -zMinPair.value;
        if( alphaDual >= Real(0) && dualInit )
            RuntimeError("initialized z was non-positive");

        if( alphaPrimal >= -gammaPrimal )
            Shift( x, alphaPrimal+1 );
        if( alphaDual >= -gammaDual )
            Shift( z, alphaDual+1 );
    }
    else
    {
        LowerClip( x, gammaPrimal );
        LowerClip( z, gammaDual   );
    }
}

template<typename Real>
void Initialize
( const SparseMatrix<Real>& Q,  const SparseMatrix<Real>& A, 
  const Matrix<Real>& b,        const Matrix<Real>& c,
        Matrix<Real>& x,              Matrix<Real>& y,
        Matrix<Real>& z,
        vector<Int>& map,             vector<Int>& invMap, 
        ldl::Separator& rootSep,           ldl::NodeInfo& info,
  bool primalInit, bool dualInit, bool standardShift, 
  const RegQSDCtrl<Real>& qsdCtrl )
{
    DEBUG_ONLY(CSE cse("lp::direct::Initialize"))
    const Int m = A.Height();
    const Int n = A.Width();
    if( primalInit )
        if( x.Height() != n || x.Width() != 1 )
            LogicError("x was of the wrong size");
    if( dualInit )
    {
        if( y.Height() != m || y.Width() != 1 )
            LogicError("y was of the wrong size");
        if( z.Height() != n || z.Width() != 1 )
            LogicError("z was of the wrong size");
    }
    if( primalInit && dualInit )
    {
        // TODO: Perform a consistency check
        return;
    }

    // Form the KKT matrix
    // ===================
    SparseMatrix<Real> J, JOrig;
    Matrix<Real> ones;
    Ones( ones, n, 1 );
    AugmentedKKT( Q, A, ones, ones, JOrig, false );
    J = JOrig;

    // (Approximately) factor the KKT matrix
    // =====================================
    Matrix<Real> reg;
    reg.Resize( n+m, 1 );
    for( Int i=0; i<n+m; ++i )
    {
        if( i < n )
            reg.Set( i, 0, qsdCtrl.regPrimal );
        else
            reg.Set( i, 0, -qsdCtrl.regDual );
    }
    UpdateRealPartOfDiagonal( J, Real(1), reg );

    NestedDissection( J.LockedGraph(), map, rootSep, info );
    InvertMap( map, invMap );

    ldl::Front<Real> JFront;
    JFront.Pull( J, map, info );
    LDL( info, JFront, LDL_2D );

    // Compute the proposed step from the KKT system
    // ---------------------------------------------
    Matrix<Real> rc, rb, rmu, d, u, v;
    Zeros( rmu, n, 1 );
    if( !primalInit )
    {
        // Minimize || x ||^2, s.t. A x = b  by solving
        //
        //    | Q+I A^T | | x |   | 0 |
        //    | A    0  | | u | = | b |,
        //
        // where 'u' is an unused dummy variable.
        Zeros( rc, n, 1 );
        rb = b;
        rb *= -1;
        Zeros( rmu, n, 1 );
        AugmentedKKTRHS( ones, rc, rb, rmu, d );

        reg_qsd_ldl::SolveAfter( JOrig, reg, invMap, info, JFront, d, qsdCtrl );
        ExpandAugmentedSolution( ones, ones, rmu, d, x, u, v );
    }
    if( !dualInit ) 
    {
        // Minimize || z ||^2, s.t. A^T y - z + c in range(Q) by solving
        //
        //    | Q+I A^T | | -z |   | -c |
        //    | A    0  | |  y | = |  0 |.
        rc = c;
        Zeros( rb, m, 1 );
        AugmentedKKTRHS( ones, rc, rb, rmu, d );

        reg_qsd_ldl::SolveAfter( JOrig, reg, invMap, info, JFront, d, qsdCtrl );
        ExpandAugmentedSolution( ones, ones, rmu, d, z, y, u );
        z *= -1;
    }

    const Real epsilon = Epsilon<Real>();
    const Real xNorm = Nrm2( x );
    const Real zNorm = Nrm2( z );
    const Real gammaPrimal = Sqrt(epsilon)*Max(xNorm,Real(1));
    const Real gammaDual   = Sqrt(epsilon)*Max(zNorm,Real(1));
    if( standardShift )
    {
        // alpha_p := min { alpha : x + alpha*e >= 0 }
        // -------------------------------------------
        const auto xMinPair = VectorMin( x );
        const Real alphaPrimal = -xMinPair.value;
        if( alphaPrimal >= Real(0) && primalInit )
            RuntimeError("initialized x was non-positive");

        // alpha_d := min { alpha : z + alpha*e >= 0 }
        // -------------------------------------------
        const auto zMinPair = VectorMin( z );
        const Real alphaDual = -zMinPair.value;
        if( alphaDual >= Real(0) && dualInit )
            RuntimeError("initialized z was non-positive");

        if( alphaPrimal >= -gammaPrimal )
            Shift( x, alphaPrimal+1 );
        if( alphaDual >= -gammaDual )
            Shift( z, alphaDual+1 );
    }
    else
    {
        LowerClip( x, gammaPrimal );
        LowerClip( z, gammaDual   );
    }
}

template<typename Real>
void Initialize
( const DistSparseMatrix<Real>& Q,  const DistSparseMatrix<Real>& A, 
  const DistMultiVec<Real>& b,      const DistMultiVec<Real>& c,
        DistMultiVec<Real>& x,            DistMultiVec<Real>& y,
        DistMultiVec<Real>& z,
        DistMap& map,                     DistMap& invMap, 
        ldl::DistSeparator& rootSep,           ldl::DistNodeInfo& info,
  bool primalInit, bool dualInit, bool standardShift, 
  const RegQSDCtrl<Real>& qsdCtrl )
{
    DEBUG_ONLY(CSE cse("lp::direct::Initialize"))
    const Int m = A.Height();
    const Int n = A.Width();
    mpi::Comm comm = A.Comm();
    if( primalInit )
        if( x.Height() != n || x.Width() != 1 )
            LogicError("x was of the wrong size");
    if( dualInit )
    {
        if( y.Height() != m || y.Width() != 1 )
            LogicError("y was of the wrong size");
        if( z.Height() != n || z.Width() != 1 )
            LogicError("z was of the wrong size");
    }
    if( primalInit && dualInit )
    {
        // TODO: Perform a consistency check
        return;
    }

    // Form the KKT matrix
    // ===================
    DistSparseMatrix<Real> J(comm), JOrig(comm);
    DistMultiVec<Real> ones(comm);
    Ones( ones, n, 1 );
    AugmentedKKT( Q, A, ones, ones, JOrig, false );
    J = JOrig;

    // (Approximately) factor the KKT matrix
    // =====================================
    DistMultiVec<Real> reg(comm);
    reg.Resize( n+m, 1 );
    for( Int iLoc=0; iLoc<reg.LocalHeight(); ++iLoc )
    {
        const Int i = reg.GlobalRow(iLoc);
        if( i < n )
            reg.SetLocal( iLoc, 0, qsdCtrl.regPrimal );
        else
            reg.SetLocal( iLoc, 0, -qsdCtrl.regDual );
    }
    UpdateRealPartOfDiagonal( J, Real(1), reg );

    NestedDissection( J.LockedDistGraph(), map, rootSep, info );
    InvertMap( map, invMap );

    ldl::DistFront<Real> JFront;
    JFront.Pull( J, map, rootSep, info );
    LDL( info, JFront, LDL_2D );

    // Compute the proposed step from the KKT system
    // ---------------------------------------------
    DistMultiVec<Real> rc(comm), rb(comm), rmu(comm), d(comm), u(comm), v(comm);
    Zeros( rmu, n, 1 );
    if( !primalInit )
    {
        // Minimize || x ||^2, s.t. A x = b  by solving
        //
        //    | Q+I A^T | | x |   | 0 |
        //    | A    0  | | u | = | b |,
        //
        // where 'u' is an unused dummy variable.
        Zeros( rc, n, 1 );
        rb = b;
        rb *= -1;
        Zeros( rmu, n, 1 );
        AugmentedKKTRHS( ones, rc, rb, rmu, d );

        reg_qsd_ldl::SolveAfter( JOrig, reg, invMap, info, JFront, d, qsdCtrl );
        ExpandAugmentedSolution( ones, ones, rmu, d, x, u, v );
    }
    if( !dualInit ) 
    {
        // Minimize || z ||^2, s.t. A^T y - z + c in range(Q) by solving
        //
        //    | Q+I A^T | | -z |   | -c |
        //    | A    0  | |  y | = |  0 |.
        rc = c;
        Zeros( rb, m, 1 );
        AugmentedKKTRHS( ones, rc, rb, rmu, d );

        reg_qsd_ldl::SolveAfter( JOrig, reg, invMap, info, JFront, d, qsdCtrl );
        ExpandAugmentedSolution( ones, ones, rmu, d, z, y, u );
        z *= -1;
    }

    const Real epsilon = Epsilon<Real>();
    const Real xNorm = Nrm2( x );
    const Real zNorm = Nrm2( z );
    const Real gammaPrimal = Sqrt(epsilon)*Max(xNorm,Real(1));
    const Real gammaDual   = Sqrt(epsilon)*Max(zNorm,Real(1));
    if( standardShift )
    {
        // alpha_p := min { alpha : x + alpha*e >= 0 }
        // -------------------------------------------
        const auto xMinPair = VectorMin( x );
        const Real alphaPrimal = -xMinPair.value;
        if( alphaPrimal >= Real(0) && primalInit )
            RuntimeError("initialized x was non-positive");

        // alpha_d := min { alpha : z + alpha*e >= 0 }
        // -------------------------------------------
        const auto zMinPair = VectorMin( z );
        const Real alphaDual = -zMinPair.value;
        if( alphaDual >= Real(0) && dualInit )
            RuntimeError("initialized z was non-positive");

        if( alphaPrimal >= -gammaPrimal )
            Shift( x, alphaPrimal+1 );
        if( alphaDual >= -gammaDual )
            Shift( z, alphaDual+1 );
    }
    else
    {
        LowerClip( x, gammaPrimal );
        LowerClip( z, gammaDual   );
    }
}

#define PROTO(Real) \
  template void Initialize \
  ( const Matrix<Real>& Q, const Matrix<Real>& A, \
    const Matrix<Real>& b, const Matrix<Real>& c, \
          Matrix<Real>& x,       Matrix<Real>& y, \
          Matrix<Real>& z, \
    bool primalInit, bool dualInit, bool standardShift ); \
  template void Initialize \
  ( const AbstractDistMatrix<Real>& Q, const AbstractDistMatrix<Real>& A, \
    const AbstractDistMatrix<Real>& b, const AbstractDistMatrix<Real>& c, \
          AbstractDistMatrix<Real>& x,       AbstractDistMatrix<Real>& y, \
          AbstractDistMatrix<Real>& z, \
    bool primalInit, bool dualInit, bool standardShift ); \
  template void Initialize \
  ( const SparseMatrix<Real>& Q, const SparseMatrix<Real>& A, \
    const Matrix<Real>& b,       const Matrix<Real>& c, \
          Matrix<Real>& x,             Matrix<Real>& y, \
          Matrix<Real>& z, \
          vector<Int>& map,            vector<Int>& invMap, \
          ldl::Separator& rootSep,          ldl::NodeInfo& info, \
    bool primalInit, bool dualInit, bool standardShift, \
    const RegQSDCtrl<Real>& qsdCtrl ); \
  template void Initialize \
  ( const DistSparseMatrix<Real>& Q, const DistSparseMatrix<Real>& A, \
    const DistMultiVec<Real>& b,     const DistMultiVec<Real>& c, \
          DistMultiVec<Real>& x,            DistMultiVec<Real>& y, \
          DistMultiVec<Real>& z, \
          DistMap& map,                     DistMap& invMap, \
          ldl::DistSeparator& rootSep,           ldl::DistNodeInfo& info, \
    bool primalInit, bool dualInit, bool standardShift, \
    const RegQSDCtrl<Real>& qsdCtrl );

#define EL_NO_INT_PROTO
#define EL_NO_COMPLEX_PROTO
#include "El/macros/Instantiate.h"

} // namespace direct
} // namespace qp
} // namespace El
