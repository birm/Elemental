/*
   Copyright 2009-2011, Jack Poulson.
   All rights reserved.

   Copyright 2011-2012, Jack Poulson, Lexing Ying, and 
   The University of Texas at Austin.
   All rights reserved.

   Copyright 2013, Jack Poulson, Lexing Ying, and Stanford University.
   All rights reserved.

   Copyright 2013-2014, Jack Poulson and The Georgia Institute of Technology.
   All rights reserved.

   Copyright 2014-2015, Jack Poulson and Stanford University.
   All rights reserved.
   
   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef EL_FACTOR_LDL_SPARSE_SYMBOLIC_HPP
#define EL_FACTOR_LDL_SPARSE_SYMBOLIC_HPP

#include "./symbolic/Separator.hpp"
#include "./symbolic/Node.hpp"
#include "./symbolic/NodeInfo.hpp"

namespace El {
namespace ldl {

Int Analysis( const Node& rootNode, NodeInfo& rootInfo, Int myOff=0 );
void Analysis
( const DistNode& rootNode, DistNodeInfo& rootInfo,
  bool storeFactRecvInds=true );

void GetChildGridDims
( const DistNodeInfo& info, vector<int>& gridHeights, vector<int>& gridWidths );

void NestedDissection
( const Graph& graph,
        vector<Int>& map,
        Separator& rootSep,
        NodeInfo& rootInfo,
  const BisectCtrl& ctrl=BisectCtrl() );
void NestedDissection
( const DistGraph& graph,
        DistMap& map,
        DistSeparator& rootSep,
        DistNodeInfo& rootInfo,
  const BisectCtrl& ctrl=BisectCtrl() );

void NaturalNestedDissection
( Int nx, Int ny, Int nz,
  const Graph& graph,
        vector<Int>& map,
        Separator& rootSep,
        NodeInfo& info,
        Int cutoff=128 );
void NaturalNestedDissection
( Int nx, Int ny, Int nz,
  const DistGraph& graph,
        DistMap& map,
        DistSeparator& rootSep,
        DistNodeInfo& info,
        Int cutoff=128,
        bool storeFactRecvInds=false );

void BuildMap( const Separator& rootSep, vector<Int>& map );
void BuildMap( const DistSeparator& rootSep, DistMap& map );

} // namespace ldl
} // namespace El

#endif // ifndef EL_FACTOR_LDL_SPARSE_SYMBOLIC_HPP
