/**CFile****************************************************************

  FileName    [darMan.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [DAG-aware AIG rewriting.]

  Synopsis    [AIG manager.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: darMan.c,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#include "darInt.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Starts the rewriting manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dar_Man_t * Dar_ManStart( Dar_Lib_t * pDarLib, Aig_Man_t * pAig, Dar_RwrPar_t * pPars )
{
    Dar_Man_t * p;
    Aig_ManCleanData( pAig );
    p = ABC_ALLOC( Dar_Man_t, 1 );
    memset( p, 0, sizeof(Dar_Man_t) );
    p->pPars = pPars;
    p->pAig = pAig;
    p->pDarLib = pDarLib;
    p->vCutNodes = Vec_PtrAlloc( 1000 );
    p->pMemCuts = Aig_MmFixedStart( p->pPars->nCutsMax * sizeof(Dar_Cut_t), 1024 );
    p->vLeavesBest = Vec_PtrAlloc( 4 );
    return p;
}

/**Function*************************************************************

  Synopsis    [Stops the rewriting manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_ManStop( Dar_Man_t * p )
{
    if ( p->vCutNodes )
        Vec_PtrFree( p->vCutNodes );
    if ( p->pMemCuts )
        Aig_MmFixedStop( p->pMemCuts, 0 );
    if ( p->vLeavesBest )
        Vec_PtrFree( p->vLeavesBest );
    ABC_FREE( p );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
