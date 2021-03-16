/**CFile****************************************************************

  FileName    [main.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [The main package.]

  Synopsis    [Here everything starts.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: main.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "mainInt.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Initialization procedure for the library project.]

  Description [Note that when Abc_Start() is run in a static library
  project, it does not load the resource file by default. As a result,
  ABC is not set up the same way, as when it is run on a command line.
  For example, some error messages while parsing files will not be
  produced, and intermediate networks will not be checked for consistancy.

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Frame_t * Abc_Start()
{
    Abc_Frame_t * pAbc;
    // start the framework
    pAbc = Abc_FrameAllocate();
    // perform initializations
    Abc_FrameInit( pAbc );

    return pAbc;
}

/**Function*************************************************************

  Synopsis    [Deallocation procedure for the library project.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_Stop( Abc_Frame_t * pAbc )
{
    // perform uninitializations
    Abc_FrameEnd( pAbc );
    // stop the framework
    Abc_FrameDeallocate( pAbc );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
