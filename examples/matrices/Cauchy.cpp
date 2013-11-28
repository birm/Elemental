/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
// NOTE: It is possible to simply include "elemental.hpp" instead
#include "elemental-lite.hpp"
#include "elemental/matrices/Cauchy.hpp"
using namespace elem;

int 
main( int argc, char* argv[] )
{
    Initialize( argc, argv );

    try
    {
        const Int m = Input("--height","height of matrix",10);
        const Int n = Input("--width","width of matrix",10);
        const bool display = Input("--display","display matrix?",true);
        const bool print = Input("--print","print matrix?",false);
        ProcessInput();
        PrintInputReport();

        std::vector<double> x( m ), y( n );
        for( Int j=0; j<m; ++j )
            x[j] = j;
        for( Int j=0; j<n; ++j )
            y[j] = j+m;

        auto A = Cauchy( DefaultGrid(), x, y );
        if( display )
            Display( A, "Cauchy");
        if( print )
            Print( A, "Cauchy:" );
    }
    catch( std::exception& e ) { ReportException(e); }

    Finalize();
    return 0;
}
