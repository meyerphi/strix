/**CFile****************************************************************

  FileName    [abcDfs.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Procedures that use depth-first search.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcDfs.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "abc.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Performs DFS for one node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkDfs_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vNodes )
{
    Abc_Obj_t * pFanin;
    int i;
    // if this node is already visited, skip
    if ( Abc_NodeIsTravIdCurrent( pNode ) )
        return;
    // mark the node as visited
    Abc_NodeSetTravIdCurrent( pNode );
    // skip the CI
    if ( Abc_ObjIsCi(pNode) || Abc_AigNodeIsConst(pNode) )
        return;
    assert( Abc_ObjIsNode( pNode ) || Abc_ObjIsBox( pNode ) );
    // visit the transitive fanin of the node
    Abc_ObjForEachFanin( pNode, pFanin, i )
    {
//        pFanin = Abc_ObjFanin( pNode, Abc_ObjFaninNum(pNode)-1-i );
        Abc_NtkDfs_rec( Abc_ObjFanin0Ntk(pFanin), vNodes );
    }
    // add the node after the fanins have been added
    Vec_PtrPush( vNodes, pNode );
}

/**Function*************************************************************

  Synopsis    [Returns the DFS ordered array of logic nodes.]

  Description [Collects only the internal nodes, leaving out CIs and CO.
  However it marks with the current TravId both CIs and COs.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Abc_NtkDfs( Abc_Ntk_t * pNtk, int fCollectAll )
{
    Vec_Ptr_t * vNodes;
    Abc_Obj_t * pObj;
    int i;
    // set the traversal ID
    Abc_NtkIncrementTravId( pNtk );
    // start the array of nodes
    vNodes = Vec_PtrAlloc( 100 );
    Abc_NtkForEachCo( pNtk, pObj, i )
    {
        Abc_NodeSetTravIdCurrent( pObj );
        Abc_NtkDfs_rec( Abc_ObjFanin0Ntk(Abc_ObjFanin0(pObj)), vNodes );
    }
    // collect dangling nodes if asked to
    if ( fCollectAll )
    {
        Abc_NtkForEachNode( pNtk, pObj, i )
            if ( !Abc_NodeIsTravIdCurrent(pObj) )
                Abc_NtkDfs_rec( pObj, vNodes );
    }
    return vNodes;
}

/**Function*************************************************************

  Synopsis    [Performs DFS for one node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigDfs_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vNodes )
{
    Abc_Obj_t * pFanin;
    int i;
    // if this node is already visited, skip
    if ( Abc_NodeIsTravIdCurrent( pNode ) )
        return;
    // mark the node as visited
    Abc_NodeSetTravIdCurrent( pNode );
    // skip the PI
    if ( Abc_ObjIsCi(pNode) || Abc_AigNodeIsConst(pNode) )
        return;
    assert( Abc_ObjIsNode( pNode ) );
    // visit the transitive fanin of the node
    Abc_ObjForEachFanin( pNode, pFanin, i )
        Abc_AigDfs_rec( pFanin, vNodes );
    // add the node after the fanins have been added
    Vec_PtrPush( vNodes, pNode );
}

/**Function*************************************************************

  Synopsis    [Returns the DFS ordered array of logic nodes.]

  Description [Collects only the internal nodes, leaving out CIs/COs.
  However it marks both CIs and COs with the current TravId.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Abc_AigDfs( Abc_Ntk_t * pNtk, int fCollectAll, int fCollectCos )
{
    Vec_Ptr_t * vNodes;
    Abc_Obj_t * pNode;
    int i;
    // set the traversal ID
    Abc_NtkIncrementTravId( pNtk );
    // start the array of nodes
    vNodes = Vec_PtrAlloc( 100 );
    // go through the PO nodes and call for each of them
    Abc_NtkForEachCo( pNtk, pNode, i )
    {
        Abc_AigDfs_rec( Abc_ObjFanin0(pNode), vNodes );
        Abc_NodeSetTravIdCurrent( pNode );
        if ( fCollectCos )
            Vec_PtrPush( vNodes, pNode );
    }
    // collect dangling nodes if asked to
    if ( fCollectAll )
    {
        Abc_NtkForEachNode( pNtk, pNode, i )
            if ( !Abc_NodeIsTravIdCurrent(pNode) )
                Abc_AigDfs_rec( pNode, vNodes );
    }
    return vNodes;
}

/**Function*************************************************************

  Synopsis    [Recursively counts the number of logic levels of one node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkLevel_rec( Abc_Obj_t * pNode )
{
    Abc_Obj_t * pNext;
    int i, Level;
    // skip the PI
    if ( Abc_ObjIsCi(pNode) )
        return pNode->Level;
    assert( Abc_ObjIsNode( pNode ) || pNode->Type == ABC_OBJ_CONST1);
    // if this node is already visited, return
    if ( Abc_NodeIsTravIdCurrent( pNode ) )
        return pNode->Level;
    // mark the node as visited
    Abc_NodeSetTravIdCurrent( pNode );
    // visit the transitive fanin
    pNode->Level = 0;
    Abc_ObjForEachFanin( pNode, pNext, i )
    {
        Level = Abc_NtkLevel_rec( Abc_ObjFanin0Ntk(pNext) );
        if ( pNode->Level < (unsigned)Level )
            pNode->Level = Level;
    }
    if ( Abc_ObjFaninNum(pNode) > 0 )
        pNode->Level++;
    return pNode->Level;
}

/**Function*************************************************************

  Synopsis    [Computes the number of logic levels not counting PIs/POs.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkLevel( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pNode;
    int i, LevelsMax;
    // set the CI levels
    Abc_NtkForEachCi( pNtk, pNode, i )
        pNode->Level = 0;
    // perform the traversal
    LevelsMax = 0;
    Abc_NtkIncrementTravId( pNtk );
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        Abc_NtkLevel_rec( pNode );
        if ( LevelsMax < (int)pNode->Level )
            LevelsMax = (int)pNode->Level;
    }
    return LevelsMax;
}

/**Function*************************************************************

  Synopsis    [Recursively detects combinational loops.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkIsAcyclic_rec( Abc_Obj_t * pNode )
{
    Abc_Obj_t * pFanin;
    int fAcyclic, i;
    if ( Abc_ObjIsCi(pNode) || Abc_ObjIsBox(pNode) || Abc_AigNodeIsConst(pNode) )
        return 1;
    assert( Abc_ObjIsNode(pNode) );
    // make sure the node is not visited
    assert( !Abc_NodeIsTravIdPrevious(pNode) );
    // check if the node is part of the combinational loop
    if ( Abc_NodeIsTravIdCurrent(pNode) )
    {
        fprintf( stdout, "Network contains combinational loop!\n" );
        fprintf( stdout, "Node \"%s\" is encountered twice on the following path to the COs:\n", Abc_ObjName(pNode) );
        return 0;
    }
    // mark this node as a node on the current path
    Abc_NodeSetTravIdCurrent( pNode );
    // visit the transitive fanin
    Abc_ObjForEachFanin( pNode, pFanin, i )
    {
        pFanin = Abc_ObjFanin0Ntk(pFanin);
        // make sure there is no mixing of networks
        assert( pFanin->pNtk == pNode->pNtk );
        // check if the fanin is visited
        if ( Abc_NodeIsTravIdPrevious(pFanin) )
            continue;
        // traverse the fanin's cone searching for the loop
        if ( (fAcyclic = Abc_NtkIsAcyclic_rec(pFanin)) )
            continue;
        // return as soon as the loop is detected
        fprintf( stdout, " %s ->", Abc_ObjName(pFanin) );
        return 0;
    }
    // mark this node as a visited node
    Abc_NodeSetTravIdPrevious( pNode );
    return 1;
}

/**Function*************************************************************

  Synopsis    [Detects combinational loops.]

  Description [This procedure is based on the idea suggested by Donald Chai.
  As we traverse the network and visit the nodes, we need to distinquish
  three types of nodes: (1) those that are visited for the first time,
  (2) those that have been visited in this traversal but are currently not
  on the traversal path, (3) those that have been visited and are currently
  on the travesal path. When the node of type (3) is encountered, it means
  that there is a combinational loop. To mark the three types of nodes,
  two new values of the traversal IDs are used.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkIsAcyclic( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pNode;
    int fAcyclic;
    int i;
    // set the traversal ID for this DFS ordering
    Abc_NtkIncrementTravId( pNtk );
    Abc_NtkIncrementTravId( pNtk );
    // pNode->TravId == pNet->nTravIds      means "pNode is on the path"
    // pNode->TravId == pNet->nTravIds - 1  means "pNode is visited but is not on the path"
    // pNode->TravId <  pNet->nTravIds - 1  means "pNode is not visited"
    // traverse the network to detect cycles
    fAcyclic = 1;
    Abc_NtkForEachCo( pNtk, pNode, i )
    {
        pNode = Abc_ObjFanin0Ntk(Abc_ObjFanin0(pNode));
        if ( Abc_NodeIsTravIdPrevious(pNode) )
            continue;
        // traverse the output logic cone
        if ( (fAcyclic = Abc_NtkIsAcyclic_rec(pNode)) )
            continue;
        // stop as soon as the first loop is detected
        fprintf( stdout, " CO \"%s\"\n", Abc_ObjName(Abc_ObjFanout0(pNode)) );
        break;
    }
    return fAcyclic;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
