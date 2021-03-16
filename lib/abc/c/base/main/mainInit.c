/**CFile****************************************************************

  FileName    [mainInit.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [The main package.]

  Synopsis    [Initialization procedures.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: mainInit.c,v 1.3 2005/09/14 22:53:37 casem Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "mainInt.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

extern void Abc_Init( Abc_Frame_t * pAbc );
extern void Abc_End ( Abc_Frame_t * pAbc );
extern void Io_Init( Abc_Frame_t * pAbc );
extern void Io_End ();
extern void Cmd_Init( Abc_Frame_t * pAbc );
extern void Cmd_End ( Abc_Frame_t * pAbc );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Starts all the packages.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_FrameInit( Abc_Frame_t * pAbc )
{
    Cmd_Init( pAbc );
    Io_Init( pAbc );
    Abc_Init( pAbc );
}

/**Function*************************************************************

  Synopsis    [Stops all the packages.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_FrameEnd( Abc_Frame_t * pAbc )
{
    Abc_End( pAbc );
    Io_End();
    Cmd_End( pAbc );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
