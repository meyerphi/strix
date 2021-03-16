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

  Synopsis    [Returns 1 if pDom is contained in pCut.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int Cut_CutCheckDominance( Cut_Cut_t * pDom, Cut_Cut_t * pCut )
{
    int i, k;
    for ( i = 0; i < (int)pDom->nLeaves; i++ )
    {
        for ( k = 0; k < (int)pCut->nLeaves; k++ )
            if ( pDom->pLeaves[i] == pCut->pLeaves[k] )
                break;
        if ( k == (int)pCut->nLeaves ) // node i in pDom is not contained in pCut
            return 0;
    }
    // every node in pDom is contained in pCut
    return 1;
}

/**Function*************************************************************

  Synopsis    [Checks containment for one cut.]

  Description [Returns 1 if the cut is removed.]

  SideEffects [May remove other cuts in the set.]

  SeeAlso     []

***********************************************************************/
static inline int Cut_CutFilterOne( Cut_Man_t * p, Cut_List_t * pSuperList, Cut_Cut_t * pCut )
{
    Cut_Cut_t * pTemp, * pTemp2, ** ppTail;
    int a;

    // check if this cut is filtered out by smaller cuts
    for ( a = 2; a <= (int)pCut->nLeaves; a++ )
    {
        Cut_ListForEachCut( pSuperList->pHead[a], pTemp )
        {
            // skip the non-contained cuts
            if ( (pTemp->uSign & pCut->uSign) != pTemp->uSign )
                continue;
            // check containment seriously
            if ( Cut_CutCheckDominance( pTemp, pCut ) )
            {
                p->nCutsFilter++;
                Cut_CutRecycle( p, pCut );
                return 1;
            }
        }
    }

    // filter out other cuts using this one
    for ( a = pCut->nLeaves + 1; a <= (int)pCut->nVarsMax; a++ )
    {
        ppTail = pSuperList->pHead + a;
        Cut_ListForEachCutSafe( pSuperList->pHead[a], pTemp, pTemp2 )
        {
            // skip the non-contained cuts
            if ( (pTemp->uSign & pCut->uSign) != pCut->uSign )
            {
                ppTail = &pTemp->pNext;
                continue;
            }
            // check containment seriously
            if ( Cut_CutCheckDominance( pCut, pTemp ) )
            {
                p->nCutsFilter++;
                p->nNodeCuts--;
                // move the head
                if ( pSuperList->pHead[a] == pTemp )
                    pSuperList->pHead[a] = pTemp->pNext;
                // move the tail
                if ( pSuperList->ppTail[a] == &pTemp->pNext )
                    pSuperList->ppTail[a] = ppTail;
                // skip the given cut in the list
                *ppTail = pTemp->pNext;
                // recycle pTemp
                Cut_CutRecycle( p, pTemp );
            }
            else
                ppTail = &pTemp->pNext;
        }
        assert( ppTail == pSuperList->ppTail[a] );
        assert( *ppTail == NULL );
    }

    return 0;
}

/**Function*************************************************************

  Synopsis    [Processes two cuts.]

  Description [Returns 1 if the limit has been reached.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int Cut_CutProcessTwo( Cut_Man_t * p, Cut_Cut_t * pCut0, Cut_Cut_t * pCut1, Cut_List_t * pSuperList )
{
    Cut_Cut_t * pCut;
    // merge the cuts
    if ( pCut0->nLeaves >= pCut1->nLeaves )
        pCut = Cut_CutMergeTwo( p, pCut0, pCut1 );
    else
        pCut = Cut_CutMergeTwo( p, pCut1, pCut0 );
    if ( pCut == NULL )
        return 0;
    assert( pCut->nLeaves > 1 );
    // set the signature
    pCut->uSign = pCut0->uSign | pCut1->uSign;
    // check containment
    if ( p->pParams->fFilter )
    {
        if ( Cut_CutFilterOne(p, pSuperList, pCut) )
            return 0;
    }

    // compute the truth table
    if ( p->pParams->fTruth )
        Cut_TruthCompute( p, pCut, pCut0, pCut1, p->fCompl0, p->fCompl1 );
    // add to the list
    Cut_ListAdd( pSuperList, pCut );
    // return status (0 if okay; 1 if exceeded the limit)
    return ++p->nNodeCuts == p->pParams->nKeepMax;
}

/**Function*************************************************************

  Synopsis    [Computes the cuts by merging cuts at two nodes.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Cut_Cut_t * Cut_NodeComputeCuts( Cut_Man_t * p, int Node, int Node0, int Node1, int fCompl0, int fCompl1, int TreeCode )
{
    Cut_List_t Super, * pSuper = &Super;
    Cut_Cut_t * pList;
    // start the number of cuts at the node
    p->nNodes++;
    p->nNodeCuts = 0;
    // compute the cuts
    Cut_ListStart( pSuper );
    Cut_NodeDoComputeCuts( p, pSuper, Node, fCompl0, fCompl1, Cut_NodeReadCutsNew(p, Node0), Cut_NodeReadCutsNew(p, Node1), TreeCode );
    pList = Cut_ListFinish( pSuper );
    // check if the node is over the list
    if ( p->nNodeCuts == p->pParams->nKeepMax )
        p->nCutsLimit++;
    // set the list at the node
    Vec_PtrFillExtra( p->vCutsNew, Node + 1, NULL );
    assert( Cut_NodeReadCutsNew(p, Node) == NULL );

    Cut_NodeWriteCutsNew( p, Node, pList );
    return pList;
}

/**Function*************************************************************

  Synopsis    [Computes the cuts by merging cuts at two nodes.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Cut_NodeDoComputeCuts( Cut_Man_t * p, Cut_List_t * pSuper, int Node, int fCompl0, int fCompl1, Cut_Cut_t * pList0, Cut_Cut_t * pList1, int TreeCode )
{
    Cut_Cut_t * pStop0, * pStop1, * pTemp0, * pTemp1;
    Cut_Cut_t * pStore0 = NULL, * pStore1 = NULL; // Suppress "might be used uninitialized"
    int i, Limit;

    // start with the elementary cut
    pTemp0 = Cut_CutCreateTriv( p, Node );
    Cut_ListAdd( pSuper, pTemp0 );
    p->nNodeCuts++;

    // get the cut lists of children
    if ( pList0 == NULL || pList1 == NULL )
        return;

    Limit = p->pParams->nVarsMax;
    // get the simultation bit of the node
    p->fSimul = (fCompl0 ^ pList0->fSimul) & (fCompl1 ^ pList1->fSimul);
    // set temporary variables
    p->fCompl0 = fCompl0;
    p->fCompl1 = fCompl1;
    // if tree cuts are computed, make sure only the unit cuts propagate over the DAG nodes
    if ( TreeCode & 1 )
    {
        assert( pList0->nLeaves == 1 );
        pStore0 = pList0->pNext;
        pList0->pNext = NULL;
    }
    if ( TreeCode & 2 )
    {
        assert( pList1->nLeaves == 1 );
        pStore1 = pList1->pNext;
        pList1->pNext = NULL;
    }
    // find the point in the list where the max-var cuts begin
    Cut_ListForEachCut( pList0, pStop0 )
        if ( pStop0->nLeaves == (unsigned)Limit )
            break;
    Cut_ListForEachCut( pList1, pStop1 )
        if ( pStop1->nLeaves == (unsigned)Limit )
            break;

    // small by small
    Cut_ListForEachCutStop( pList0, pTemp0, pStop0 )
    Cut_ListForEachCutStop( pList1, pTemp1, pStop1 )
    {
        if ( Cut_CutProcessTwo( p, pTemp0, pTemp1, pSuper ) )
            goto Quits;
    }
    // small by large
    Cut_ListForEachCutStop( pList0, pTemp0, pStop0 )
    Cut_ListForEachCut( pStop1, pTemp1 )
    {
        if ( (pTemp0->uSign & pTemp1->uSign) != pTemp0->uSign )
            continue;
        if ( Cut_CutProcessTwo( p, pTemp0, pTemp1, pSuper ) )
            goto Quits;
    }
    // small by large
    Cut_ListForEachCutStop( pList1, pTemp1, pStop1 )
    Cut_ListForEachCut( pStop0, pTemp0 )
    {
        if ( (pTemp0->uSign & pTemp1->uSign) != pTemp1->uSign )
            continue;
        if ( Cut_CutProcessTwo( p, pTemp0, pTemp1, pSuper ) )
            goto Quits;
    }
    // large by large
    Cut_ListForEachCut( pStop0, pTemp0 )
    Cut_ListForEachCut( pStop1, pTemp1 )
    {
        assert( pTemp0->nLeaves == (unsigned)Limit && pTemp1->nLeaves == (unsigned)Limit );
        if ( pTemp0->uSign != pTemp1->uSign )
            continue;
        for ( i = 0; i < Limit; i++ )
            if ( pTemp0->pLeaves[i] != pTemp1->pLeaves[i] )
                break;
        if ( i < Limit )
            continue;
        if ( Cut_CutProcessTwo( p, pTemp0, pTemp1, pSuper ) )
            goto Quits;
    }
    if ( p->nNodeCuts == 0 )
        p->nNodesNoCuts++;
Quits:
    if ( TreeCode & 1 )
        pList0->pNext = pStore0;
    if ( TreeCode & 2 )
        pList1->pNext = pStore1;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
