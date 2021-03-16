/**CFile****************************************************************

  FileName    [rwrLib.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [DAG-aware AIG rewriting package.]

  Synopsis    []

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: rwrLib.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "rwr.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Adds one node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Rwr_Node_t * Rwr_ManAddNode( Rwr_Man_t * p, Rwr_Node_t * p0, Rwr_Node_t * p1, int fExor, int Level, int Volume )
{
    Rwr_Node_t * pNew;
    unsigned uTruth;
    // compute truth table, leve, volume
    p->nConsidered++;
    if ( fExor )
        uTruth = (p0->uTruth ^ p1->uTruth);
    else
        uTruth = (Rwr_IsComplement(p0)? ~Rwr_Regular(p0)->uTruth : Rwr_Regular(p0)->uTruth) &
                 (Rwr_IsComplement(p1)? ~Rwr_Regular(p1)->uTruth : Rwr_Regular(p1)->uTruth) & 0xFFFF;
    // create the new node
    pNew = (Rwr_Node_t *)Extra_MmFixedEntryFetch( p->pMmNode );
    pNew->Id     = p->vForest->nSize;
    pNew->TravId = 0;
    pNew->uTruth = uTruth;
    pNew->Level  = Level;
    pNew->Volume = Volume;
    pNew->fUsed  = 0;
    pNew->fExor  = fExor;
    pNew->p0     = p0;
    pNew->p1     = p1;
    pNew->pNext  = NULL;
    Vec_PtrPush( p->vForest, pNew );
    // do not add if the node is not essential
    if ( uTruth != p->puCanons[uTruth] )
        return pNew;

    // add to the list
    p->nAdded++;
    if ( p->pTable[uTruth] == NULL )
        p->nClasses++;
    Rwr_ListAddToTail( p->pTable + uTruth, pNew );
    return pNew;
}

/**Function*************************************************************

  Synopsis    [Adds one node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Rwr_Node_t * Rwr_ManAddVar( Rwr_Man_t * p, unsigned uTruth, int fPrecompute )
{
    Rwr_Node_t * pNew;
    pNew = (Rwr_Node_t *)Extra_MmFixedEntryFetch( p->pMmNode );
    pNew->Id     = p->vForest->nSize;
    pNew->TravId = 0;
    pNew->uTruth = uTruth;
    pNew->Level  = 0;
    pNew->Volume = 0;
    pNew->fUsed  = 1;
    pNew->fExor  = 0;
    pNew->p0     = NULL;
    pNew->p1     = NULL;
    pNew->pNext  = NULL;
    Vec_PtrPush( p->vForest, pNew );
    if ( fPrecompute )
        Rwr_ListAddToTail( p->pTable + uTruth, pNew );
    return pNew;
}

/**Function*************************************************************

  Synopsis    [Adds one node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Rwr_Trav_rec( Rwr_Man_t * p, Rwr_Node_t * pNode, int * pVolume )
{
    if ( pNode->fUsed || pNode->TravId == p->nTravIds )
        return;
    pNode->TravId = p->nTravIds;
    (*pVolume)++;
    if ( pNode->fExor )
        (*pVolume)++;
    Rwr_Trav_rec( p, Rwr_Regular(pNode->p0), pVolume );
    Rwr_Trav_rec( p, Rwr_Regular(pNode->p1), pVolume );
}

/**Function*************************************************************

  Synopsis    [Adds one node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Rwr_ManNodeVolume( Rwr_Man_t * p, Rwr_Node_t * p0, Rwr_Node_t * p1 )
{
    int Volume = 0;
    Rwr_ManIncTravId( p );
    Rwr_Trav_rec( p, p0, &Volume );
    Rwr_Trav_rec( p, p1, &Volume );
    return Volume;
}

/**Function*************************************************************

  Synopsis    [Adds one node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Rwr_ManIncTravId( Rwr_Man_t * p )
{
    Rwr_Node_t * pNode;
    int i;
    if ( p->nTravIds++ >= 0 )
        return;
    Vec_PtrForEachEntry( Rwr_Node_t *, p->vForest, pNode, i )
        pNode->TravId = 0;
    p->nTravIds = 1;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
