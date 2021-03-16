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

  Synopsis    [Returns the pointer to the linked list of cuts.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Cut_Cut_t * Cut_NodeReadCutsNew( Cut_Man_t * p, int Node )
{
    if ( Node >= p->vCutsNew->nSize )
        return NULL;
    return (Cut_Cut_t *)Vec_PtrEntry( p->vCutsNew, Node );
}

/**Function*************************************************************

  Synopsis    [Returns the pointer to the linked list of cuts.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Cut_NodeWriteCutsNew( Cut_Man_t * p, int Node, Cut_Cut_t * pList )
{
    Vec_PtrWriteEntry( p->vCutsNew, Node, pList );
}

/**Function*************************************************************

  Synopsis    [Sets the trivial cut for the node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Cut_NodeSetTriv( Cut_Man_t * p, int Node )
{
    assert( Cut_NodeReadCutsNew(p, Node) == NULL );
    Cut_NodeWriteCutsNew( p, Node, Cut_CutCreateTriv(p, Node) );
}

/**Function*************************************************************

  Synopsis    [Deallocates the cuts at the node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Cut_NodeFreeCuts( Cut_Man_t * p, int Node )
{
    Cut_Cut_t * pList, * pCut, * pCut2;
    pList = Cut_NodeReadCutsNew( p, Node );
    if ( pList == NULL )
        return;
    Cut_ListForEachCutSafe( pList, pCut, pCut2 )
        Cut_CutRecycle( p, pCut );
    Cut_NodeWriteCutsNew( p, Node, NULL );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
