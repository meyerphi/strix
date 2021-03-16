/**CFile****************************************************************

  FileName    [abcUtil.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Various utilities.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcUtil.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "abc.h"
#include "base/main/main.h"
#include "bool/dec/dec.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Cleans the copy field of all objects.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkCleanCopy( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pObj;
    int i;
    Abc_NtkForEachObj( pNtk, pObj, i )
        pObj->pCopy = NULL;
}

/**Function*************************************************************

  Synopsis    [Checks if the internal node has CO fanout.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_NodeFindCoFanout( Abc_Obj_t * pNode )
{
    Abc_Obj_t * pFanout;
    int i;
    Abc_ObjForEachFanout( pNode, pFanout, i )
        if ( Abc_ObjIsCo(pFanout) )
            return pFanout;
    return NULL;
}

/**Function*************************************************************

  Synopsis    [Inserts a new node in the order by levels.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_VecObjPushUniqueOrderByLevel( Vec_Ptr_t * p, Abc_Obj_t * pNode )
{
    Abc_Obj_t * pNode1, * pNode2;
    int i;
    if ( Vec_PtrPushUnique(p, pNode) )
        return;
    // find the p of the node
    for ( i = p->nSize-1; i > 0; i-- )
    {
        pNode1 = (Abc_Obj_t *)p->pArray[i  ];
        pNode2 = (Abc_Obj_t *)p->pArray[i-1];
        if ( Abc_ObjRegular(pNode1)->Level <= Abc_ObjRegular(pNode2)->Level )
            break;
        p->pArray[i  ] = pNode2;
        p->pArray[i-1] = pNode1;
    }
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the node is the root of EXOR/NEXOR.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NodeIsExorType( Abc_Obj_t * pNode )
{
    Abc_Obj_t * pNode0, * pNode1;
    // check that the node is regular
    assert( !Abc_ObjIsComplement(pNode) );
    // if the node is not AND, this is not EXOR
    if ( !Abc_AigNodeIsAnd(pNode) )
        return 0;
    // if the children are not complemented, this is not EXOR
    if ( !Abc_ObjFaninC0(pNode) || !Abc_ObjFaninC1(pNode) )
        return 0;
    // get children
    pNode0 = Abc_ObjFanin0(pNode);
    pNode1 = Abc_ObjFanin1(pNode);
    // if the children are not ANDs, this is not EXOR
    if ( Abc_ObjFaninNum(pNode0) != 2 || Abc_ObjFaninNum(pNode1) != 2 )
        return 0;
    // this is AIG, which means the fanins should be ordered
    assert( Abc_ObjFaninId0(pNode0) != Abc_ObjFaninId1(pNode1) ||
            Abc_ObjFaninId0(pNode1) != Abc_ObjFaninId1(pNode0) );
    // if grand children are not the same, this is not EXOR
    if ( Abc_ObjFaninId0(pNode0) != Abc_ObjFaninId0(pNode1) ||
         Abc_ObjFaninId1(pNode0) != Abc_ObjFaninId1(pNode1) )
         return 0;
    // finally, if the complemented edges are matched, this is not EXOR
    if ( Abc_ObjFaninC0(pNode0) == Abc_ObjFaninC0(pNode1) ||
         Abc_ObjFaninC1(pNode0) == Abc_ObjFaninC1(pNode1) )
         return 0;
    return 1;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the node is the control type of the MUX.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NodeIsMuxControlType( Abc_Obj_t * pNode )
{
    Abc_Obj_t * pNode0, * pNode1;
    // check that the node is regular
    assert( !Abc_ObjIsComplement(pNode) );
    // skip the node that do not have two fanouts
    if ( Abc_ObjFanoutNum(pNode) != 2 )
        return 0;
    // get the fanouts
    pNode0 = Abc_ObjFanout( pNode, 0 );
    pNode1 = Abc_ObjFanout( pNode, 1 );
    // if they have more than one fanout, we are not interested
    if ( Abc_ObjFanoutNum(pNode0) != 1 ||  Abc_ObjFanoutNum(pNode1) != 1 )
        return 0;
    // if the fanouts have the same fanout, this is MUX or EXOR (or a redundant gate (CA)(CB))
    return Abc_ObjFanout0(pNode0) == Abc_ObjFanout0(pNode1);
}

/**Function*************************************************************

  Synopsis    [Returns 1 if it is an AIG with choice nodes.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NodeCollectFanins( Abc_Obj_t * pNode, Vec_Ptr_t * vNodes )
{
    Abc_Obj_t * pFanin;
    int i;
    Vec_PtrClear(vNodes);
    Abc_ObjForEachFanin( pNode, pFanin, i )
        Vec_PtrPush( vNodes, pFanin );
}

/**Function*************************************************************

  Synopsis    [Returns 1 if it is an AIG with choice nodes.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NodeCollectFanouts( Abc_Obj_t * pNode, Vec_Ptr_t * vNodes )
{
    Abc_Obj_t * pFanout;
    int i;
    Vec_PtrClear(vNodes);
    Abc_ObjForEachFanout( pNode, pFanout, i )
        Vec_PtrPush( vNodes, pFanout );
}

/**Function*************************************************************

  Synopsis    [Procedure used for sorting the nodes in decreasing order of levels.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NodeCompareLevelsDecrease( Abc_Obj_t ** pp1, Abc_Obj_t ** pp2 )
{
    int Diff = Abc_ObjRegular(*pp1)->Level - Abc_ObjRegular(*pp2)->Level;
    if ( Diff > 0 )
        return -1;
    if ( Diff < 0 )
        return 1;
    Diff = Abc_ObjRegular(*pp1)->Id - Abc_ObjRegular(*pp2)->Id;
    if ( Diff > 0 )
        return -1;
    if ( Diff < 0 )
        return 1;
    return 0;
}

/**Function*************************************************************

  Synopsis    [Puts the nodes into the DFS order and reassign their IDs.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkReassignIds( Abc_Ntk_t * pNtk )
{
    Vec_Ptr_t * vNodes;
    Vec_Ptr_t * vObjsNew;
    Abc_Obj_t * pNode, * pTemp, * pConst1;
    int i, k;
    // start the array of objects with new IDs
    vObjsNew = Vec_PtrAlloc( pNtk->nObjs );
    // put constant node first
    pConst1 = Abc_AigConst1(pNtk);
    assert( pConst1->Id == 0 );
    Vec_PtrPush( vObjsNew, pConst1 );
    // put PI nodes next
    Abc_NtkForEachPi( pNtk, pNode, i )
    {
        pNode->Id = Vec_PtrSize( vObjsNew );
        Vec_PtrPush( vObjsNew, pNode );
    }
    // put PO nodes next
    Abc_NtkForEachPo( pNtk, pNode, i )
    {
        pNode->Id = Vec_PtrSize( vObjsNew );
        Vec_PtrPush( vObjsNew, pNode );
    }
    // put latches and their inputs/outputs next
    Abc_NtkForEachBox( pNtk, pNode, i )
    {
        pNode->Id = Vec_PtrSize( vObjsNew );
        Vec_PtrPush( vObjsNew, pNode );
        Abc_ObjForEachFanin( pNode, pTemp, k )
        {
            pTemp->Id = Vec_PtrSize( vObjsNew );
            Vec_PtrPush( vObjsNew, pTemp );
        }
        Abc_ObjForEachFanout( pNode, pTemp, k )
        {
            pTemp->Id = Vec_PtrSize( vObjsNew );
            Vec_PtrPush( vObjsNew, pTemp );
        }
    }
    // finally, internal nodes in the DFS order
    vNodes = Abc_AigDfs( pNtk, 1, 0 );
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i )
    {
        if ( pNode == pConst1 )
            continue;
        pNode->Id = Vec_PtrSize( vObjsNew );
        Vec_PtrPush( vObjsNew, pNode );
    }
    Vec_PtrFree( vNodes );
    assert( Vec_PtrSize(vObjsNew) == pNtk->nObjs );

    // update the fanin/fanout arrays
    Abc_NtkForEachObj( pNtk, pNode, i )
    {
        Abc_ObjForEachFanin( pNode, pTemp, k )
            pNode->vFanins.pArray[k] = pTemp->Id;
        Abc_ObjForEachFanout( pNode, pTemp, k )
            pNode->vFanouts.pArray[k] = pTemp->Id;
    }

    // replace the array of objs
    Vec_PtrFree( pNtk->vObjs );
    pNtk->vObjs = vObjsNew;

    // rehash the AIG
    Abc_AigRehash( (Abc_Aig_t *)pNtk->pManFunc );

    // update the name manager!!!
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
