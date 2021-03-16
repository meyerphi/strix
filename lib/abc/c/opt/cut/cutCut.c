/**CFile****************************************************************

  FileName    [cutNode.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [K-feasible cut computation package.]

  Synopsis    [Procedures to compute cuts for a node.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: cutNode.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "cutInt.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Allocates the cut.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Cut_Cut_t * Cut_CutAlloc( Cut_Man_t * p )
{
    Cut_Cut_t * pCut;
    // cut allocation
    pCut = (Cut_Cut_t *)Extra_MmFixedEntryFetch( p->pMmCuts );
    memset( pCut, 0, sizeof(Cut_Cut_t) );
    pCut->nVarsMax   = p->pParams->nVarsMax;
    pCut->fSimul     = p->fSimul;
    // statistics
    p->nCutsAlloc++;
    p->nCutsCur++;
    if ( p->nCutsPeak < p->nCutsAlloc - p->nCutsDealloc )
        p->nCutsPeak = p->nCutsAlloc - p->nCutsDealloc;
    return pCut;
}

/**Function*************************************************************

  Synopsis    [Recybles the cut.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Cut_CutRecycle( Cut_Man_t * p, Cut_Cut_t * pCut )
{
    p->nCutsDealloc++;
    p->nCutsCur--;
    if ( pCut->nLeaves == 1 )
        p->nCutsTriv--;
    Extra_MmFixedEntryRecycle( p->pMmCuts, (char *)pCut );
}

/**Function*************************************************************

  Synopsis    [Creates the trivial cut.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Cut_Cut_t * Cut_CutCreateTriv( Cut_Man_t * p, int Node )
{
    Cut_Cut_t * pCut;
    pCut = Cut_CutAlloc( p );
    pCut->nLeaves    = 1;
    pCut->pLeaves[0] = Node;
    pCut->uSign      = Cut_NodeSign( Node );
    if ( p->pParams->fTruth )
    {
/*
        if ( pCut->nVarsMax == 4 )
            Cut_CutWriteTruth( pCut, p->uTruthVars[0] );
        else
            Extra_BitCopy( pCut->nLeaves, p->uTruths[0], (uint8*)Cut_CutReadTruth(pCut) );
*/
        unsigned * pTruth = Cut_CutReadTruth(pCut);
        int i;
        for ( i = 0; i < p->nTruthWords; i++ )
            pTruth[i] = 0xAAAAAAAA;
    }
    p->nCutsTriv++;
    return pCut;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
