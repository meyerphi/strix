/**CFile****************************************************************

  FileName    [kitGraph.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Computation kit.]

  Synopsis    [Decomposition graph representation.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - Dec 6, 2006.]

  Revision    [$Id: kitGraph.c,v 1.00 2006/12/06 00:00:00 alanmi Exp $]

***********************************************************************/

#include "kit.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Creates a graph with the given number of leaves.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Graph_t * Kit_GraphCreate( int nLeaves )
{
    Kit_Graph_t * pGraph;
    pGraph = ABC_ALLOC( Kit_Graph_t, 1 );
    memset( pGraph, 0, sizeof(Kit_Graph_t) );
    pGraph->nLeaves = nLeaves;
    pGraph->nSize = nLeaves;
    pGraph->nCap = 2 * nLeaves + 50;
    pGraph->pNodes = ABC_ALLOC( Kit_Node_t, pGraph->nCap );
    memset( pGraph->pNodes, 0, sizeof(Kit_Node_t) * pGraph->nSize );
    return pGraph;
}

/**Function*************************************************************

  Synopsis    [Creates constant 0 graph.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Graph_t * Kit_GraphCreateConst0()
{
    Kit_Graph_t * pGraph;
    pGraph = ABC_ALLOC( Kit_Graph_t, 1 );
    memset( pGraph, 0, sizeof(Kit_Graph_t) );
    pGraph->fConst = 1;
    pGraph->eRoot.fCompl = 1;
    return pGraph;
}

/**Function*************************************************************

  Synopsis    [Creates constant 1 graph.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Graph_t * Kit_GraphCreateConst1()
{
    Kit_Graph_t * pGraph;
    pGraph = ABC_ALLOC( Kit_Graph_t, 1 );
    memset( pGraph, 0, sizeof(Kit_Graph_t) );
    pGraph->fConst = 1;
    return pGraph;
}

/**Function*************************************************************

  Synopsis    [Creates a graph with the given number of leaves.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Kit_GraphFree( Kit_Graph_t * pGraph )
{
    ABC_FREE( pGraph->pNodes );
    ABC_FREE( pGraph );
}

/**Function*************************************************************

  Synopsis    [Appends a new node to the graph.]

  Description [This procedure is meant for internal use.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Node_t * Kit_GraphAppendNode( Kit_Graph_t * pGraph )
{
    Kit_Node_t * pNode;
    if ( pGraph->nSize == pGraph->nCap )
    {
        pGraph->pNodes = ABC_REALLOC( Kit_Node_t, pGraph->pNodes, 2 * pGraph->nCap );
        pGraph->nCap   = 2 * pGraph->nCap;
    }
    pNode = pGraph->pNodes + pGraph->nSize++;
    memset( pNode, 0, sizeof(Kit_Node_t) );
    return pNode;
}

/**Function*************************************************************

  Synopsis    [Creates an AND node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Edge_t Kit_GraphAddNodeAnd( Kit_Graph_t * pGraph, Kit_Edge_t eEdge0, Kit_Edge_t eEdge1 )
{
    Kit_Node_t * pNode;
    // get the new node
    pNode = Kit_GraphAppendNode( pGraph );
    // set the inputs and other info
    pNode->eEdge0 = eEdge0;
    pNode->eEdge1 = eEdge1;
    pNode->fCompl0 = eEdge0.fCompl;
    pNode->fCompl1 = eEdge1.fCompl;
    return Kit_EdgeCreate( pGraph->nSize - 1, 0 );
}

/**Function*************************************************************

  Synopsis    [Creates an OR node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Edge_t Kit_GraphAddNodeOr( Kit_Graph_t * pGraph, Kit_Edge_t eEdge0, Kit_Edge_t eEdge1 )
{
    Kit_Node_t * pNode;
    // get the new node
    pNode = Kit_GraphAppendNode( pGraph );
    // set the inputs and other info
    pNode->eEdge0 = eEdge0;
    pNode->eEdge1 = eEdge1;
    pNode->fCompl0 = eEdge0.fCompl;
    pNode->fCompl1 = eEdge1.fCompl;
    // make adjustments for the OR gate
    pNode->fNodeOr = 1;
    pNode->eEdge0.fCompl = !pNode->eEdge0.fCompl;
    pNode->eEdge1.fCompl = !pNode->eEdge1.fCompl;
    return Kit_EdgeCreate( pGraph->nSize - 1, 1 );
}

/**Function*************************************************************

  Synopsis    [Derives the factored form from the truth table.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Graph_t * Kit_TruthToGraph( unsigned * pTruth, int nVars, Vec_Int_t * vMemory )
{
    Kit_Graph_t * pGraph;
    int RetValue;
    // derive SOP
    RetValue = Kit_TruthIsop( pTruth, nVars, vMemory, 1 ); // tried 1 and found not useful in "renode"
    if ( RetValue == -1 )
        return NULL;
    if ( Vec_IntSize(vMemory) > (1<<16) )
        return NULL;
//    printf( "Isop size = %d.\n", Vec_IntSize(vMemory) );
    assert( RetValue == 0 || RetValue == 1 );
    // derive factored form
    pGraph = Kit_SopFactor( vMemory, RetValue, nVars, vMemory );
    return pGraph;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
