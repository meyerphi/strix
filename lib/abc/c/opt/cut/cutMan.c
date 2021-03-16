/**CFile****************************************************************

  FileName    [cutMan.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [K-feasible cut computation package.]

  Synopsis    [Cut manager.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: cutMan.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "cutInt.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Starts the cut manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Cut_Man_t * Cut_ManStart( Cut_Params_t * pParams )
{
    Cut_Man_t * p;
    assert( pParams->nVarsMax >= 3 && pParams->nVarsMax <= CUT_SIZE_MAX );
    p = ABC_ALLOC( Cut_Man_t, 1 );
    memset( p, 0, sizeof(Cut_Man_t) );
    // set and correct parameters
    p->pParams = pParams;
    // prepare storage for cuts
    p->vCutsNew = Vec_PtrAlloc( pParams->nIdsMax );
    Vec_PtrFill( p->vCutsNew, pParams->nIdsMax, NULL );
    // entry size
    p->EntrySize = sizeof(Cut_Cut_t) + pParams->nVarsMax * sizeof(int);
    if ( pParams->fTruth )
    {
        if ( pParams->nVarsMax > 14 )
        {
            // skip computation of truth table for more than 14 inputs
            pParams->fTruth = 0;
        }
        else
        {
            p->nTruthWords = Cut_TruthWords( pParams->nVarsMax );
            p->EntrySize += p->nTruthWords * sizeof(unsigned);
        }
        p->puTemp[0] = ABC_ALLOC( unsigned, 4 * p->nTruthWords );
        p->puTemp[1] = p->puTemp[0] + p->nTruthWords;
        p->puTemp[2] = p->puTemp[1] + p->nTruthWords;
        p->puTemp[3] = p->puTemp[2] + p->nTruthWords;
    }
    // memory for cuts
    p->pMmCuts = Extra_MmFixedStart( p->EntrySize );
    p->vTemp = Vec_PtrAlloc( 100 );
    return p;
}

/**Function*************************************************************

  Synopsis    [Stops the cut manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Cut_ManStop( Cut_Man_t * p )
{
    if ( p->vCutsNew )    Vec_PtrFree( p->vCutsNew );
    if ( p->vCutsOld )    Vec_PtrFree( p->vCutsOld );
    if ( p->vCutsTemp )   Vec_PtrFree( p->vCutsTemp );
    if ( p->vFanCounts )  Vec_IntFree( p->vFanCounts );
    if ( p->vTemp )       Vec_PtrFree( p->vTemp );
    if ( p->pParams )     ABC_FREE( p->pParams );

    if ( p->vCutsMax )    Vec_PtrFree( p->vCutsMax );
    if ( p->vDelays )     Vec_IntFree( p->vDelays );
    if ( p->vDelays2 )    Vec_IntFree( p->vDelays2 );
    if ( p->vNodeCuts )   Vec_IntFree( p->vNodeCuts );
    if ( p->vNodeStarts ) Vec_IntFree( p->vNodeStarts );
    if ( p->vCutPairs )   Vec_IntFree( p->vCutPairs );
    if ( p->puTemp[0] )   ABC_FREE( p->puTemp[0] );

    Extra_MmFixedStop( p->pMmCuts );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Cut_ManIncrementDagNodes( Cut_Man_t * p )
{
    p->nNodesDag++;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
