/**CFile****************************************************************

  FileName    [abcBalance.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Performs global balancing of the AIG by the number of levels.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcBalance.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static void        Abc_NtkBalancePerform( Abc_Ntk_t * pNtk, Abc_Ntk_t * pNtkAig, int fDuplicate, int fSelective );
static Abc_Obj_t * Abc_NodeBalance_rec( Abc_Ntk_t * pNtkNew, Abc_Obj_t * pNode, Vec_Vec_t * vStorage, int Level, int fDuplicate, int fSelective );
static Vec_Ptr_t * Abc_NodeBalanceCone( Abc_Obj_t * pNode, Vec_Vec_t * vSuper, int Level, int fDuplicate, int fSelective );
static int         Abc_NodeBalanceCone_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vSuper, int fFirst, int fDuplicate, int fSelective );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Balances the AIG network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkBalance( Abc_Ntk_t * pNtk, int fDuplicate, int fSelective )
{
    Abc_Ntk_t * pNtkAig;
    // perform balancing
    pNtkAig = Abc_NtkStartFrom( pNtk );
    // perform balancing
    Abc_NtkBalancePerform( pNtk, pNtkAig, fDuplicate, fSelective );
    Abc_NtkFinalize( pNtk );
    Abc_AigCleanup( (Abc_Aig_t *)pNtkAig->pManFunc );
    // make sure everything is okay
    if ( !Abc_NtkCheck( pNtkAig ) )
    {
        printf( "Abc_NtkBalance: The network check has failed.\n" );
        Abc_NtkDelete( pNtkAig );
        return NULL;
    }
    return pNtkAig;
}

/**Function*************************************************************

  Synopsis    [Balances the AIG network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkBalancePerform( Abc_Ntk_t * pNtk, Abc_Ntk_t * pNtkAig, int fDuplicate, int fSelective )
{
    Vec_Vec_t * vStorage;
    Abc_Obj_t * pNode;
    int i;
    // transfer level
    Abc_NtkForEachCi( pNtk, pNode, i )
        pNode->pCopy->Level = pNode->Level;
    // allocate temporary storage for supergates
    vStorage = Vec_VecStart( 10 );
    // perform balancing of POs
    Abc_NtkForEachCo( pNtk, pNode, i )
    {
        Abc_NodeBalance_rec( pNtkAig, Abc_ObjFanin0(pNode), vStorage, 0, fDuplicate, fSelective );
    }
    Vec_VecFree( vStorage );
}

/**Function*************************************************************

  Synopsis    [Moves closer to the end the node that is best for sharing.]

  Description [If there is no node with sharing, randomly chooses one of
  the legal nodes.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NodeBalancePermute( Abc_Ntk_t * pNtkNew, Vec_Ptr_t * vSuper, int LeftBound )
{
    Abc_Obj_t * pNode1, * pNode2, * pNode3;
    int RightBound, i;
    // get the right bound
    RightBound = Vec_PtrSize(vSuper) - 2;
    assert( LeftBound <= RightBound );
    if ( LeftBound == RightBound )
        return;
    // get the two last nodes
    pNode1 = (Abc_Obj_t *)Vec_PtrEntry( vSuper, RightBound + 1 );
    pNode2 = (Abc_Obj_t *)Vec_PtrEntry( vSuper, RightBound     );
    // find the first node that can be shared
    for ( i = RightBound; i >= LeftBound; i-- )
    {
        pNode3 = (Abc_Obj_t *)Vec_PtrEntry( vSuper, i );
        if ( Abc_AigAndLookup( (Abc_Aig_t *)pNtkNew->pManFunc, pNode1, pNode3 ) )
        {
            if ( pNode3 == pNode2 )
                return;
            Vec_PtrWriteEntry( vSuper, i,          pNode2 );
            Vec_PtrWriteEntry( vSuper, RightBound, pNode3 );
            return;
        }
    }
}

/**Function*************************************************************

  Synopsis    [Rebalances the multi-input node rooted at pNodeOld.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_NodeBalance_rec( Abc_Ntk_t * pNtkNew, Abc_Obj_t * pNodeOld, Vec_Vec_t * vStorage, int Level, int fDuplicate, int fSelective )
{
    Abc_Aig_t * pMan = (Abc_Aig_t *)pNtkNew->pManFunc;
    Abc_Obj_t * pNodeNew, * pNode1, * pNode2;
    Vec_Ptr_t * vSuper;
    int i, LeftBound;
    assert( !Abc_ObjIsComplement(pNodeOld) );
    // return if the result if known
    if ( pNodeOld->pCopy )
        return pNodeOld->pCopy;
    assert( Abc_ObjIsNode(pNodeOld) );
    // get the implication supergate
    vSuper = Abc_NodeBalanceCone( pNodeOld, vStorage, Level, fDuplicate, fSelective );
    if ( vSuper->nSize == 0 )
    { // it means that the supergate contains two nodes in the opposite polarity
        pNodeOld->pCopy = Abc_ObjNot(Abc_AigConst1(pNtkNew));
        return pNodeOld->pCopy;
    }
    // for each old node, derive the new well-balanced node
    for ( i = 0; i < vSuper->nSize; i++ )
    {
        pNodeNew = Abc_NodeBalance_rec( pNtkNew, Abc_ObjRegular((Abc_Obj_t *)vSuper->pArray[i]), vStorage, Level + 1, fDuplicate, fSelective );
        vSuper->pArray[i] = Abc_ObjNotCond( pNodeNew, Abc_ObjIsComplement((Abc_Obj_t *)vSuper->pArray[i]) );
    }
    assert( vSuper->nSize > 1 );
    // sort the new nodes by level in the decreasing order
    Vec_PtrSort( vSuper, Abc_NodeCompareLevelsDecrease );
    // balance the nodes
    assert( vSuper->nSize > 1 );
    while ( vSuper->nSize > 1 )
    {
        // find the left bound on the node to be paired
        LeftBound = 0;
        // find the node that can be shared (if no such node, randomize choice)
        Abc_NodeBalancePermute( pNtkNew, vSuper, LeftBound );
        // pull out the last two nodes
        pNode1 = (Abc_Obj_t *)Vec_PtrPop(vSuper);
        pNode2 = (Abc_Obj_t *)Vec_PtrPop(vSuper);
        Abc_VecObjPushUniqueOrderByLevel( vSuper, Abc_AigAnd(pMan, pNode1, pNode2) );
    }
    // make sure the balanced node is not assigned
    assert( pNodeOld->pCopy == NULL );
    // mark the old node with the new node
    pNodeOld->pCopy = (Abc_Obj_t *)vSuper->pArray[0];
    vSuper->nSize = 0;
    return pNodeOld->pCopy;
}

/**Function*************************************************************

  Synopsis    [Collects the nodes in the cone delimited by fMarkA==1.]

  Description [Returns -1 if the AND-cone has the same node in both polarities.
  Returns 1 if the AND-cone has the same node in the same polarity. Returns 0
  if the AND-cone has no repeated nodes.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Abc_NodeBalanceCone( Abc_Obj_t * pNode, Vec_Vec_t * vStorage, int Level, int fDuplicate, int fSelective )
{
    Vec_Ptr_t * vNodes;
    int RetValue, i;
    assert( !Abc_ObjIsComplement(pNode) );
    // extend the storage
    if ( Vec_VecSize( vStorage ) <= Level )
        Vec_VecPush( vStorage, Level, 0 );
    // get the temporary array of nodes
    vNodes = Vec_VecEntry( vStorage, Level );
    Vec_PtrClear( vNodes );
    // collect the nodes in the implication supergate
    RetValue = Abc_NodeBalanceCone_rec( pNode, vNodes, 1, fDuplicate, fSelective );
    assert( vNodes->nSize > 1 );
    // unmark the visited nodes
    for ( i = 0; i < vNodes->nSize; i++ )
        Abc_ObjRegular((Abc_Obj_t *)vNodes->pArray[i])->fMarkB = 0;
    // if we found the node and its complement in the same implication supergate,
    // return empty set of nodes (meaning that we should use constant-0 node)
    if ( RetValue == -1 )
        vNodes->nSize = 0;
    return vNodes;
}

/**Function*************************************************************

  Synopsis    [Collects the nodes in the cone delimited by fMarkA==1.]

  Description [Returns -1 if the AND-cone has the same node in both polarities.
  Returns 1 if the AND-cone has the same node in the same polarity. Returns 0
  if the AND-cone has no repeated nodes.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NodeBalanceCone_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vSuper, int fFirst, int fDuplicate, int fSelective )
{
    int RetValue1, RetValue2, i;
    // check if the node is visited
    if ( Abc_ObjRegular(pNode)->fMarkB )
    {
        // check if the node occurs in the same polarity
        for ( i = 0; i < vSuper->nSize; i++ )
            if ( vSuper->pArray[i] == pNode )
                return 1;
        // check if the node is present in the opposite polarity
        for ( i = 0; i < vSuper->nSize; i++ )
            if ( vSuper->pArray[i] == Abc_ObjNot(pNode) )
                return -1;
        assert( 0 );
        return 0;
    }
    // if the new node is complemented or a PI, another gate begins
    if ( !fFirst && (Abc_ObjIsComplement(pNode) || !Abc_ObjIsNode(pNode) || (!fDuplicate && !fSelective && (Abc_ObjFanoutNum(pNode) > 1)) || Vec_PtrSize(vSuper) > 10000) )
    {
        Vec_PtrPush( vSuper, pNode );
        Abc_ObjRegular(pNode)->fMarkB = 1;
        return 0;
    }
    assert( !Abc_ObjIsComplement(pNode) );
    assert( Abc_ObjIsNode(pNode) );
    // go through the branches
    RetValue1 = Abc_NodeBalanceCone_rec( Abc_ObjChild0(pNode), vSuper, 0, fDuplicate, fSelective );
    RetValue2 = Abc_NodeBalanceCone_rec( Abc_ObjChild1(pNode), vSuper, 0, fDuplicate, fSelective );
    if ( RetValue1 == -1 || RetValue2 == -1 )
        return -1;
    // return 1 if at least one branch has a duplicate
    return RetValue1 || RetValue2;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
