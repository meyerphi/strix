/**CFile****************************************************************

  FileName    [mainFrame.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [The main package.]

  Synopsis    [The global framework resides in this file.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: mainFrame.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "mainInt.h"
#include "bool/dec/dec.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Frame_t * Abc_FrameAllocate()
{
    Abc_Frame_t * p;
    // allocate and clean
    p = ABC_CALLOC( Abc_Frame_t, 1 );
    // set streams
    p->Err = stderr;
    p->Out = stdout;

    return p;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_FrameDeallocate( Abc_Frame_t * p )
{
    Abc_FrameDeleteNetwork( p );

    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_FrameReadNtk( Abc_Frame_t * p )
{
    return p->pNtkCur;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dar_Lib_t * Abc_FrameReadDarLib( Abc_Frame_t * p )
{
    return p->pDarLib;
}

/**Function*************************************************************

  Synopsis    [Replaces the current network by the given one.]

  Description [This procedure does not modify the stack of saved
  networks.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_FrameReplaceNetwork( Abc_Frame_t * p, Abc_Ntk_t * pNtk )
{
    if ( pNtk == NULL )
        return;

    // delete the current network if present
    if ( p->pNtkCur ) {
        Abc_NtkDelete( p->pNtkCur );
    }
    // set the new current network
    p->pNtkCur = pNtk;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_FrameDeleteNetwork( Abc_Frame_t * p )
{
    // delete the currently saved network
    Abc_NtkDelete( p->pNtkCur );
    // set the current network empty
    p->pNtkCur = NULL;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
