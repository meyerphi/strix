/**CFile****************************************************************

  FileName    [cutTruth.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [K-feasible cut computation package.]

  Synopsis    [Incremental truth table computation.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: cutTruth.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "cutInt.h"

/*
    Truth tables computed in this package are represented as bit-strings
    stored in the cut data structure. Cuts of any number of inputs have
    the truth table with 2^k bits, where k is the max number of cut inputs.
*/

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Computes the stretching phase of the cut w.r.t. the merged cut.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline unsigned Cut_TruthPhase( Cut_Cut_t * pCut, Cut_Cut_t * pCut1 )
{
    unsigned uPhase = 0;
    int i, k;
    for ( i = k = 0; i < (int)pCut->nLeaves; i++ )
    {
        if ( k == (int)pCut1->nLeaves )
            break;
        if ( pCut->pLeaves[i] < pCut1->pLeaves[k] )
            continue;
        assert( pCut->pLeaves[i] == pCut1->pLeaves[k] );
        uPhase |= (1 << i);
        k++;
    }
    return uPhase;
}

/**Function*************************************************************

  Synopsis    [Performs truth table computation.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Cut_TruthCompute( Cut_Man_t * p, Cut_Cut_t * pCut, Cut_Cut_t * pCut0, Cut_Cut_t * pCut1, int fCompl0, int fCompl1 )
{
    // permute the first table
    if ( fCompl0 )
        Extra_TruthNot( p->puTemp[0], Cut_CutReadTruth(pCut0), pCut->nVarsMax );
    else
        Extra_TruthCopy( p->puTemp[0], Cut_CutReadTruth(pCut0), pCut->nVarsMax );
    Extra_TruthStretch( p->puTemp[2], p->puTemp[0], pCut0->nLeaves, pCut->nVarsMax, Cut_TruthPhase(pCut, pCut0) );
    // permute the second table
    if ( fCompl1 )
        Extra_TruthNot( p->puTemp[1], Cut_CutReadTruth(pCut1), pCut->nVarsMax );
    else
        Extra_TruthCopy( p->puTemp[1], Cut_CutReadTruth(pCut1), pCut->nVarsMax );
    Extra_TruthStretch( p->puTemp[3], p->puTemp[1], pCut1->nLeaves, pCut->nVarsMax, Cut_TruthPhase(pCut, pCut1) );
    // produce the resulting table
    if ( pCut->fCompl )
        Extra_TruthNand( Cut_CutReadTruth(pCut), p->puTemp[2], p->puTemp[3], pCut->nVarsMax );
    else
        Extra_TruthAnd( Cut_CutReadTruth(pCut), p->puTemp[2], p->puTemp[3], pCut->nVarsMax );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
