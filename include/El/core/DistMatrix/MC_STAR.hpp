/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef EL_DISTMATRIX_MC_STAR_DECL_HPP
#define EL_DISTMATRIX_MC_STAR_DECL_HPP

namespace El {

// Partial specialization to A[MC,* ].
//
// The rows of these distributed matrices will be replicated on all 
// processes (*), and the columns will be distributed like "Matrix Columns" 
// (MC). Thus the columns will be distributed among columns of the process
// grid.
template<typename T>
class DistMatrix<T,MC,STAR> : public AbstractDistMatrix<T>
{
public:
    // Typedefs
    // ========
    typedef AbstractDistMatrix<T> absType;
    typedef DistMatrix<T,MC,STAR> type;

    // Constructors and destructors
    // ============================

    // Create a 0 x 0 distributed matrix
    DistMatrix( const El::Grid& g=DefaultGrid(), int root=0 );
    // Create a height x width distributed matrix
    DistMatrix
    ( Int height, Int width, const El::Grid& g=DefaultGrid(), int root=0 );
    // Create a copy of distributed matrix (redistribute if necessary)
    DistMatrix( const type& A );
    DistMatrix( const absType& A );
    template<Dist U,Dist V> DistMatrix( const DistMatrix<T,U,V>& A );
    template<Dist U,Dist V> DistMatrix( const BlockDistMatrix<T,U,V>& A );
    // Move constructor
    DistMatrix( type&& A ) EL_NOEXCEPT;
    // Destructor
    ~DistMatrix();

    DistMatrix<T,MC,STAR>* Construct
    ( const El::Grid& g, int root ) const override;
    DistMatrix<T,STAR,MC>* ConstructTranspose
    ( const El::Grid& g, int root ) const override;
    DistMatrix<T,MC,STAR>* ConstructDiagonal
    ( const El::Grid& g, int root ) const override;

    // Operator overloading
    // ====================

    // Return a view
    // -------------
          type operator()( Range<Int> I, Range<Int> J );
    const type operator()( Range<Int> I, Range<Int> J ) const;

    // Make a copy
    // -----------
    type& operator=( const absType& A );
    type& operator=( const DistMatrix<T,MC,  MR  >& A );
    type& operator=( const DistMatrix<T,MC,  STAR>& A );
    type& operator=( const DistMatrix<T,STAR,MR  >& A );
    type& operator=( const DistMatrix<T,MD,  STAR>& A );
    type& operator=( const DistMatrix<T,STAR,MD  >& A );
    type& operator=( const DistMatrix<T,MR,  MC  >& A );
    type& operator=( const DistMatrix<T,MR,  STAR>& A );
    type& operator=( const DistMatrix<T,STAR,MC  >& A );
    type& operator=( const DistMatrix<T,VC,  STAR>& A );
    type& operator=( const DistMatrix<T,STAR,VC  >& A );
    type& operator=( const DistMatrix<T,VR,  STAR>& A );
    type& operator=( const DistMatrix<T,STAR,VR  >& A );
    type& operator=( const DistMatrix<T,STAR,STAR>& A );
    type& operator=( const DistMatrix<T,CIRC,CIRC>& A );
    template<Dist U,Dist V> type& operator=( const BlockDistMatrix<T,U,V>& A );

    // Move assignment
    // ---------------
    type& operator=( type&& A );

    // Rescaling
    // ---------
    const type& operator*=( T alpha );

    // Addition/subtraction
    // --------------------
    const type& operator+=( const absType& A );
    const type& operator-=( const absType& A );

    // Basic queries
    // =============
    El::DistData DistData() const override;

    Dist ColDist()             const override;
    Dist RowDist()             const override;
    Dist PartialColDist()      const override;
    Dist PartialRowDist()      const override;
    Dist PartialUnionColDist() const override;
    Dist PartialUnionRowDist() const override;
    Dist CollectedColDist()    const override;
    Dist CollectedRowDist()    const override;

    mpi::Comm DistComm()      const override;
    mpi::Comm CrossComm()     const override;
    mpi::Comm RedundantComm() const override;
    mpi::Comm ColComm()       const override;
    mpi::Comm RowComm()       const override;

    int RowStride()     const override;
    int ColStride()     const override;
    int DistSize()      const override;
    int CrossSize()     const override;
    int RedundantSize() const override;

private:
    // Friend declarations
    // ===================
    template<typename S,Dist U,Dist V> friend class DistMatrix;
    template<typename S,Dist U,Dist V> friend class BlockDistMatrix;
};

} // namespace El

#endif // ifndef EL_DISTMATRIX_MC_STAR_DECL_HPP
