/**CFile****************************************************************

  FileName    [aigDfs.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [AIG package.]

  Synopsis    [DFS traversal procedures.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: aigDfs.c,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#include "aig.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Collects internal nodes in the DFS order.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ManDfs_rec( Aig_Man_t * p, Aig_Obj_t * pObj, Vec_Ptr_t * vNodes )
{
    if ( pObj == NULL )
        return;
    assert( !Aig_IsComplement(pObj) );
    if ( Aig_ObjIsTravIdCurrent(p, pObj) )
        return;
    Aig_ObjSetTravIdCurrent(p, pObj);
    Aig_ManDfs_rec( p, Aig_ObjFanin0(pObj), vNodes );
    Aig_ManDfs_rec( p, Aig_ObjFanin1(pObj), vNodes );
    Vec_PtrPush( vNodes, pObj );
}

/**Function*************************************************************

  Synopsis    [Collects objects of the AIG in the DFS order.]

  Description [Works with choice nodes.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Aig_ManDfs( Aig_Man_t * p, int fNodesOnly )
{
    Vec_Ptr_t * vNodes;
    Aig_Obj_t * pObj;
    int i;
    Aig_ManIncrementTravId( p );
    Aig_ObjSetTravIdCurrent( p, Aig_ManConst1(p) );
    // start the array of nodes
    vNodes = Vec_PtrAlloc( Aig_ManObjNumMax(p) );
    // mark PIs if they should not be collected
    if ( fNodesOnly )
        Aig_ManForEachCi( p, pObj, i )
            Aig_ObjSetTravIdCurrent( p, pObj );
    else
        Vec_PtrPush( vNodes, Aig_ManConst1(p) );
    // collect nodes reachable in the DFS order
    Aig_ManForEachCo( p, pObj, i )
        Aig_ManDfs_rec( p, fNodesOnly? Aig_ObjFanin0(pObj): pObj, vNodes );
    if ( fNodesOnly )
        assert( Vec_PtrSize(vNodes) == Aig_ManNodeNum(p) );
    else
        assert( Vec_PtrSize(vNodes) == Aig_ManObjNum(p) );
    return vNodes;
}

/**Function*************************************************************

  Synopsis    [Counts the number of AIG nodes rooted at this cone.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Aig_ConeCountAndMark_rec( Aig_Obj_t * pObj )
{
    int Counter;
    assert( !Aig_IsComplement(pObj) );
    if ( !Aig_ObjIsNode(pObj) || Aig_ObjIsMarkA(pObj) )
        return 0;
    Counter = 1 + Aig_ConeCountAndMark_rec( Aig_ObjFanin0(pObj) ) +
        Aig_ConeCountAndMark_rec( Aig_ObjFanin1(pObj) );
    assert( !Aig_ObjIsMarkA(pObj) ); // loop detection
    Aig_ObjSetMarkA( pObj );
    return Counter;
}

/**Function*************************************************************

  Synopsis    [Counts the number of AIG nodes rooted at this cone.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ConeUnmark_rec( Aig_Obj_t * pObj )
{
    assert( !Aig_IsComplement(pObj) );
    if ( !Aig_ObjIsNode(pObj) || !Aig_ObjIsMarkA(pObj) )
        return;
    Aig_ConeUnmark_rec( Aig_ObjFanin0(pObj) );
    Aig_ConeUnmark_rec( Aig_ObjFanin1(pObj) );
    assert( Aig_ObjIsMarkA(pObj) ); // loop detection
    Aig_ObjClearMarkA( pObj );
}

/**Function*************************************************************

  Synopsis    [Counts the number of AIG nodes rooted at this cone.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Aig_DagSize( Aig_Obj_t * pObj )
{
    int Counter;
    Counter = Aig_ConeCountAndMark_rec( Aig_Regular(pObj) );
    Aig_ConeUnmark_rec( Aig_Regular(pObj) );
    return Counter;
}

/**Function*************************************************************

  Synopsis    [Computes the internal nodes of the cut.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ObjCollectCut_rec( Aig_Obj_t * pNode, Vec_Ptr_t * vNodes )
{
    if ( pNode->fMarkA )
        return;
    pNode->fMarkA = 1;
    assert( Aig_ObjIsNode(pNode) );
    Aig_ObjCollectCut_rec( Aig_ObjFanin0(pNode), vNodes );
    Aig_ObjCollectCut_rec( Aig_ObjFanin1(pNode), vNodes );
    Vec_PtrPush( vNodes, pNode );
}

/**Function*************************************************************

  Synopsis    [Computes the internal nodes of the cut.]

  Description [Does not include the leaves of the cut.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ObjCollectCut( Aig_Obj_t * pRoot, Vec_Ptr_t * vLeaves, Vec_Ptr_t * vNodes )
{
    Aig_Obj_t * pObj;
    int i;
    // collect and mark the leaves
    Vec_PtrClear( vNodes );
    Vec_PtrForEachEntry( Aig_Obj_t *, vLeaves, pObj, i )
    {
        assert( pObj->fMarkA == 0 );
        pObj->fMarkA = 1;
    }
    // collect and mark the nodes
    Aig_ObjCollectCut_rec( pRoot, vNodes );
    // clean the nodes
    Vec_PtrForEachEntry( Aig_Obj_t *, vNodes, pObj, i )
        pObj->fMarkA = 0;
    Vec_PtrForEachEntry( Aig_Obj_t *, vLeaves, pObj, i )
        pObj->fMarkA = 0;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
