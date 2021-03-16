/**CFile****************************************************************

  FileName    [abcReconv.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Computation of reconvergence-driven cuts.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcReconv.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

struct Abc_ManCut_t_
{
    // user specified parameters
    int              nNodeSizeMax;  // the limit on the size of the supernode
    int              nConeSizeMax;  // the limit on the size of the containing cone
    int              nNodeFanStop;  // the limit on the size of the supernode
    int              nConeFanStop;  // the limit on the size of the containing cone
    // internal parameters
    Vec_Ptr_t *      vNodeLeaves;   // fanins of the collapsed node (the cut)
    Vec_Ptr_t *      vConeLeaves;   // fanins of the containing cone
    Vec_Ptr_t *      vVisited;      // the visited nodes
    Vec_Vec_t *      vLevels;       // the data structure to compute TFO nodes
    Vec_Ptr_t *      vNodesTfo;     // the nodes in the TFO of the cut
};

static int   Abc_NodeBuildCutLevelOne_int( Vec_Ptr_t * vVisited, Vec_Ptr_t * vLeaves, int nSizeLimit, int nFaninLimit );
static void  Abc_NodeConeMarkCollect_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vVisited );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Unmarks the TFI cone.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void Abc_NodesMark( Vec_Ptr_t * vVisited )
{
    Abc_Obj_t * pNode;
    int i;
    Vec_PtrForEachEntry( Abc_Obj_t *, vVisited, pNode, i )
        pNode->fMarkA = 1;
}

/**Function*************************************************************

  Synopsis    [Unmarks the TFI cone.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void Abc_NodesUnmark( Vec_Ptr_t * vVisited )
{
    Abc_Obj_t * pNode;
    int i;
    Vec_PtrForEachEntry( Abc_Obj_t *, vVisited, pNode, i )
        pNode->fMarkA = 0;
}

/**Function*************************************************************

  Synopsis    [Unmarks the TFI cone.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void Abc_NodesUnmarkB( Vec_Ptr_t * vVisited )
{
    Abc_Obj_t * pNode;
    int i;
    Vec_PtrForEachEntry( Abc_Obj_t *, vVisited, pNode, i )
        pNode->fMarkB = 0;
}

/**Function*************************************************************

  Synopsis    [Evaluate the cost of removing the node from the set of leaves.]

  Description [Returns the number of new leaves that will be brought in.
  Returns large number if the node cannot be removed from the set of leaves.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int Abc_NodeGetLeafCostOne( Abc_Obj_t * pNode, int nFaninLimit )
{
    int Cost;
    // make sure the node is in the construction zone
    assert( pNode->fMarkB == 1 );
    // cannot expand over the PI node
    if ( Abc_ObjIsCi(pNode) )
        return 999;
    // get the cost of the cone
    Cost = (!Abc_ObjFanin0(pNode)->fMarkB) + (!Abc_ObjFanin1(pNode)->fMarkB);
    // always accept if the number of leaves does not increase
    if ( Cost < 2 )
        return Cost;
    // skip nodes with many fanouts
    if ( Abc_ObjFanoutNum(pNode) > nFaninLimit )
        return 999;
    // return the number of nodes that will be on the leaves if this node is removed
    return Cost;
}

/**Function*************************************************************

  Synopsis    [Finds a fanin-limited, reconvergence-driven cut for the node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Abc_NodeFindCut( Abc_ManCut_t * p, Abc_Obj_t * pRoot, int fContain )
{
    Abc_Obj_t * pNode;
    int i;

    assert( !Abc_ObjIsComplement(pRoot) );
    assert( Abc_ObjIsNode(pRoot) );

    // start the visited nodes and mark them
    Vec_PtrClear( p->vVisited );
    Vec_PtrPush( p->vVisited, pRoot );
    Vec_PtrPush( p->vVisited, Abc_ObjFanin0(pRoot) );
    Vec_PtrPush( p->vVisited, Abc_ObjFanin1(pRoot) );
    pRoot->fMarkB = 1;
    Abc_ObjFanin0(pRoot)->fMarkB = 1;
    Abc_ObjFanin1(pRoot)->fMarkB = 1;

    // start the cut
    Vec_PtrClear( p->vNodeLeaves );
    Vec_PtrPush( p->vNodeLeaves, Abc_ObjFanin0(pRoot) );
    Vec_PtrPush( p->vNodeLeaves, Abc_ObjFanin1(pRoot) );

    // compute the cut
    while ( Abc_NodeBuildCutLevelOne_int( p->vVisited, p->vNodeLeaves, p->nNodeSizeMax, p->nNodeFanStop ) );
    assert( Vec_PtrSize(p->vNodeLeaves) <= p->nNodeSizeMax );

    // return if containing cut is not requested
    if ( !fContain )
    {
        // unmark both fMarkA and fMarkB in tbe TFI
        Abc_NodesUnmarkB( p->vVisited );
        return p->vNodeLeaves;
    }

//printf( "\n\n\n" );
    // compute the containing cut
    assert( p->nNodeSizeMax < p->nConeSizeMax );
    // copy the current boundary
    Vec_PtrClear( p->vConeLeaves );
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vNodeLeaves, pNode, i )
        Vec_PtrPush( p->vConeLeaves, pNode );
    // compute the containing cut
    while ( Abc_NodeBuildCutLevelOne_int( p->vVisited, p->vConeLeaves, p->nConeSizeMax, p->nConeFanStop ) );
    assert( Vec_PtrSize(p->vConeLeaves) <= p->nConeSizeMax );
    // unmark TFI using fMarkA and fMarkB
    Abc_NodesUnmarkB( p->vVisited );
    return p->vNodeLeaves;
}

/**Function*************************************************************

  Synopsis    [Builds reconvergence-driven cut by changing one leaf at a time.]

  Description [This procedure looks at the current leaves and tries to change
  one leaf at a time in such a way that the cut grows as little as possible.
  In evaluating the fanins, this procedure looks only at their immediate
  predecessors (this is why it is called a one-level construction procedure).]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NodeBuildCutLevelOne_int( Vec_Ptr_t * vVisited, Vec_Ptr_t * vLeaves, int nSizeLimit, int nFaninLimit )
{
    Abc_Obj_t * pNode, * pFaninBest, * pNext;
    int CostBest, CostCur, i;
    // find the best fanin
    CostBest   = 100;
    pFaninBest = NULL;
//printf( "Evaluating fanins of the cut:\n" );
    Vec_PtrForEachEntry( Abc_Obj_t *, vLeaves, pNode, i )
    {
        CostCur = Abc_NodeGetLeafCostOne( pNode, nFaninLimit );
//printf( "    Fanin %s has cost %d.\n", Abc_ObjName(pNode), CostCur );
//        if ( CostBest > CostCur ) // performance improvement: expand the variable with the smallest level
        if ( CostBest > CostCur ||
             (CostBest == CostCur && pNode->Level > pFaninBest->Level) )
        {
            CostBest   = CostCur;
            pFaninBest = pNode;
        }
        if ( CostBest == 0 )
            break;
    }
    if ( pFaninBest == NULL )
        return 0;

    assert( CostBest < 3 );
    if ( vLeaves->nSize - 1 + CostBest > nSizeLimit )
        return 0;

    assert( Abc_ObjIsNode(pFaninBest) );
    // remove the node from the array
    Vec_PtrRemove( vLeaves, pFaninBest );
//printf( "Removing fanin %s.\n", Abc_ObjName(pFaninBest) );

    // add the left child to the fanins
    pNext = Abc_ObjFanin0(pFaninBest);
    if ( !pNext->fMarkB )
    {
//printf( "Adding fanin %s.\n", Abc_ObjName(pNext) );
        pNext->fMarkB = 1;
        Vec_PtrPush( vLeaves, pNext );
        Vec_PtrPush( vVisited, pNext );
    }
    // add the right child to the fanins
    pNext = Abc_ObjFanin1(pFaninBest);
    if ( !pNext->fMarkB )
    {
//printf( "Adding fanin %s.\n", Abc_ObjName(pNext) );
        pNext->fMarkB = 1;
        Vec_PtrPush( vLeaves, pNext );
        Vec_PtrPush( vVisited, pNext );
    }
    assert( vLeaves->nSize <= nSizeLimit );
    // keep doing this
    return 1;
}

/**Function*************************************************************

  Synopsis    [Get the nodes contained in the cut.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NodeConeCollect( Abc_Obj_t ** ppRoots, int nRoots, Vec_Ptr_t * vLeaves, Vec_Ptr_t * vVisited, int fIncludeFanins )
{
    Abc_Obj_t * pTemp;
    int i;
    // mark the fanins of the cone
    Abc_NodesMark( vLeaves );
    // collect the nodes in the DFS order
    Vec_PtrClear( vVisited );
    // add the fanins
    if ( fIncludeFanins )
        Vec_PtrForEachEntry( Abc_Obj_t *, vLeaves, pTemp, i )
            Vec_PtrPush( vVisited, pTemp );
    // add other nodes
    for ( i = 0; i < nRoots; i++ )
        Abc_NodeConeMarkCollect_rec( ppRoots[i], vVisited );
    // unmark both sets
    Abc_NodesUnmark( vLeaves );
    Abc_NodesUnmark( vVisited );
}

/**Function*************************************************************

  Synopsis    [Marks the TFI cone.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NodeConeMarkCollect_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vVisited )
{
    if ( pNode->fMarkA == 1 )
        return;
    // visit transitive fanin
    if ( Abc_ObjIsNode(pNode) )
    {
        Abc_NodeConeMarkCollect_rec( Abc_ObjFanin0(pNode), vVisited );
        Abc_NodeConeMarkCollect_rec( Abc_ObjFanin1(pNode), vVisited );
    }
    assert( pNode->fMarkA == 0 );
    pNode->fMarkA = 1;
    Vec_PtrPush( vVisited, pNode );
}

/**Function*************************************************************

  Synopsis    [Starts the resynthesis manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_ManCut_t * Abc_NtkManCutStart( int nNodeSizeMax, int nConeSizeMax, int nNodeFanStop, int nConeFanStop )
{
    Abc_ManCut_t * p;
    p = ABC_ALLOC( Abc_ManCut_t, 1 );
    memset( p, 0, sizeof(Abc_ManCut_t) );
    p->vNodeLeaves  = Vec_PtrAlloc( 100 );
    p->vConeLeaves  = Vec_PtrAlloc( 100 );
    p->vVisited     = Vec_PtrAlloc( 100 );
    p->vLevels      = Vec_VecAlloc( 100 );
    p->vNodesTfo    = Vec_PtrAlloc( 100 );
    p->nNodeSizeMax = nNodeSizeMax;
    p->nConeSizeMax = nConeSizeMax;
    p->nNodeFanStop = nNodeFanStop;
    p->nConeFanStop = nConeFanStop;
    return p;
}

/**Function*************************************************************

  Synopsis    [Stops the resynthesis manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkManCutStop( Abc_ManCut_t * p )
{
    Vec_PtrFree( p->vNodeLeaves );
    Vec_PtrFree( p->vConeLeaves );
    Vec_PtrFree( p->vVisited    );
    Vec_VecFree( p->vLevels );
    Vec_PtrFree( p->vNodesTfo );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Returns the leaves of the cone.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Abc_NtkManCutReadCutLarge( Abc_ManCut_t * p )
{
    return p->vConeLeaves;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
