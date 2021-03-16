/**CFile****************************************************************

  FileName    [cutMerge.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [K-feasible cut computation package.]

  Synopsis    [Procedure to merge two cuts.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: cutMerge.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "cutInt.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Merges two cuts.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Cut_Cut_t * Cut_CutMergeTwo( Cut_Man_t * p, Cut_Cut_t * pCut0, Cut_Cut_t * pCut1 )
{
    Cut_Cut_t * pRes;
    int * pLeaves;
    int Limit, nLeaves0, nLeaves1;
    int i, k, c;

    assert( pCut0->nLeaves >= pCut1->nLeaves );

    // consider two cuts
    nLeaves0 = pCut0->nLeaves;
    nLeaves1 = pCut1->nLeaves;

    // the case of the largest cut sizes
    Limit = p->pParams->nVarsMax;
    if ( nLeaves0 == Limit && nLeaves1 == Limit )
    {
        for ( i = 0; i < nLeaves0; i++ )
            if ( pCut0->pLeaves[i] != pCut1->pLeaves[i] )
                return NULL;
        pRes = Cut_CutAlloc( p );
        for ( i = 0; i < nLeaves0; i++ )
            pRes->pLeaves[i] = pCut0->pLeaves[i];
        pRes->nLeaves = pCut0->nLeaves;
        return pRes;
    }
    // the case when one of the cuts is the largest
    if ( nLeaves0 == Limit )
    {
        for ( i = 0; i < nLeaves1; i++ )
        {
            for ( k = nLeaves0 - 1; k >= 0; k-- )
                if ( pCut0->pLeaves[k] == pCut1->pLeaves[i] )
                    break;
            if ( k == -1 ) // did not find
                return NULL;
        }
        pRes = Cut_CutAlloc( p );
        for ( i = 0; i < nLeaves0; i++ )
            pRes->pLeaves[i] = pCut0->pLeaves[i];
        pRes->nLeaves = pCut0->nLeaves;
        return pRes;
    }

    // prepare the cut
    if ( p->pReady == NULL )
        p->pReady = Cut_CutAlloc( p );
    pLeaves = p->pReady->pLeaves;

    // compare two cuts with different numbers
    i = k = 0;
    for ( c = 0; c < Limit; c++ )
    {
        if ( k == nLeaves1 )
        {
            if ( i == nLeaves0 )
            {
                p->pReady->nLeaves = c;
                pRes = p->pReady;  p->pReady = NULL;
                return pRes;
            }
            pLeaves[c] = pCut0->pLeaves[i++];
            continue;
        }
        if ( i == nLeaves0 )
        {
            if ( k == nLeaves1 )
            {
                p->pReady->nLeaves = c;
                pRes = p->pReady;  p->pReady = NULL;
                return pRes;
            }
            pLeaves[c] = pCut1->pLeaves[k++];
            continue;
        }
        if ( pCut0->pLeaves[i] < pCut1->pLeaves[k] )
        {
            pLeaves[c] = pCut0->pLeaves[i++];
            continue;
        }
        if ( pCut0->pLeaves[i] > pCut1->pLeaves[k] )
        {
            pLeaves[c] = pCut1->pLeaves[k++];
            continue;
        }
        pLeaves[c] = pCut0->pLeaves[i++];
        k++;
    }
    if ( i < nLeaves0 || k < nLeaves1 )
        return NULL;
    p->pReady->nLeaves = c;
    pRes = p->pReady;  p->pReady = NULL;
    return pRes;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
