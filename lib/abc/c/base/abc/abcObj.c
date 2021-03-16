/**CFile****************************************************************

  FileName    [abcObj.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Object creation/duplication/deletion procedures.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcObj.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "abc.h"
#include "base/main/main.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Creates a new object.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_ObjAlloc( Abc_Ntk_t * pNtk, Abc_ObjType_t Type )
{
    Abc_Obj_t * pObj;
    if ( pNtk->pMmObj )
        pObj = (Abc_Obj_t *)Mem_FixedEntryFetch( pNtk->pMmObj );
    else
        pObj = (Abc_Obj_t *)ABC_ALLOC( Abc_Obj_t, 1 );
    memset( pObj, 0, sizeof(Abc_Obj_t) );
    pObj->pNtk = pNtk;
    pObj->Type = Type;
    pObj->Id   = -1;
    return pObj;
}

/**Function*************************************************************

  Synopsis    [Recycles the object.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_ObjRecycle( Abc_Obj_t * pObj )
{
    Abc_Ntk_t * pNtk = pObj->pNtk;
    if ( pNtk->pMmStep == NULL )
    {
        ABC_FREE( pObj->vFanouts.pArray );
        ABC_FREE( pObj->vFanins.pArray );
    }
    // clean the memory to make deleted object distinct from the live one
    memset( pObj, 0, sizeof(Abc_Obj_t) );
    // recycle the object
    if ( pNtk->pMmObj )
        Mem_FixedEntryRecycle( pNtk->pMmObj, (char *)pObj );
    else
        ABC_FREE( pObj );
}

/**Function*************************************************************

  Synopsis    [Adds the node to the network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_NtkCreateObj( Abc_Ntk_t * pNtk, Abc_ObjType_t Type )
{
    Abc_Obj_t * pObj;
    // create new object, assign ID, and add to the array
    pObj = Abc_ObjAlloc( pNtk, Type );
    pObj->Id = pNtk->vObjs->nSize;
    Vec_PtrPush( pNtk->vObjs, pObj );
    pNtk->nObjCounts[Type]++;
    pNtk->nObjs++;
    // perform specialized operations depending on the object type
    switch (Type)
    {
        case ABC_OBJ_NONE:
            assert(0);
            break;
        case ABC_OBJ_CONST1:
            assert(0);
            break;
        case ABC_OBJ_PI:
            Vec_PtrPush( pNtk->vPis, pObj );
            Vec_PtrPush( pNtk->vCis, pObj );
            break;
        case ABC_OBJ_PO:
            Vec_PtrPush( pNtk->vPos, pObj );
            Vec_PtrPush( pNtk->vCos, pObj );
            break;
        case ABC_OBJ_BI:
            if ( pNtk->vCos ) Vec_PtrPush( pNtk->vCos, pObj );
            break;
        case ABC_OBJ_BO:
            if ( pNtk->vCis ) Vec_PtrPush( pNtk->vCis, pObj );
            break;
        case ABC_OBJ_NODE:
            break;
        case ABC_OBJ_LATCH:
            if ( pNtk->vBoxes ) Vec_PtrPush( pNtk->vBoxes, pObj );
            pObj->pData = (void *)ABC_INIT_NONE;
            break;
        default:
            assert(0);
            break;
    }
    return pObj;
}

/**Function*************************************************************

  Synopsis    [Deletes the object from the network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkDeleteObj( Abc_Obj_t * pObj )
{
    Abc_Ntk_t * pNtk = pObj->pNtk;
    Vec_Ptr_t * vNodes;
    int i;
    assert( !Abc_ObjIsComplement(pObj) );
    // remove from the table of names
    if ( Nm_ManFindNameById(pObj->pNtk->pManName, pObj->Id) )
        Nm_ManDeleteIdName(pObj->pNtk->pManName, pObj->Id);
    // delete fanins and fanouts
    vNodes = Vec_PtrAlloc( 100 );
    Abc_NodeCollectFanouts( pObj, vNodes );
    for ( i = 0; i < vNodes->nSize; i++ )
        Abc_ObjDeleteFanin( (Abc_Obj_t *)vNodes->pArray[i], pObj );
    Abc_NodeCollectFanins( pObj, vNodes );
    for ( i = 0; i < vNodes->nSize; i++ )
        Abc_ObjDeleteFanin( pObj, (Abc_Obj_t *)vNodes->pArray[i] );
    Vec_PtrFree( vNodes );
    // remove from the list of objects
    Vec_PtrWriteEntry( pNtk->vObjs, pObj->Id, NULL );
    pObj->Id = (1<<26)-1;
    pNtk->nObjCounts[pObj->Type]--;
    pNtk->nObjs--;
    // perform specialized operations depending on the object type
    switch (pObj->Type)
    {
        case ABC_OBJ_NONE:
            assert(0);
            break;
        case ABC_OBJ_CONST1:
            assert(0);
            break;
        case ABC_OBJ_PI:
            Vec_PtrRemove( pNtk->vPis, pObj );
            Vec_PtrRemove( pNtk->vCis, pObj );
            break;
        case ABC_OBJ_PO:
            Vec_PtrRemove( pNtk->vPos, pObj );
            Vec_PtrRemove( pNtk->vCos, pObj );
            break;
        case ABC_OBJ_BI:
            if ( pNtk->vCos ) Vec_PtrRemove( pNtk->vCos, pObj );
            break;
        case ABC_OBJ_BO:
            if ( pNtk->vCis ) Vec_PtrRemove( pNtk->vCis, pObj );
            break;
        case ABC_OBJ_NODE:
            pObj->pData = NULL;
            break;
        case ABC_OBJ_LATCH:
            if ( pNtk->vBoxes ) Vec_PtrRemove( pNtk->vBoxes, pObj );
            break;
        default:
            assert(0);
            break;
    }
    // recycle the object memory
    Abc_ObjRecycle( pObj );
}

/**Function*************************************************************

  Synopsis    [Duplicate the Obj.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_NtkDupObj( Abc_Ntk_t * pNtkNew, Abc_Obj_t * pObj, int fCopyName )
{
    Abc_Obj_t * pObjNew;
    // create the new object
    pObjNew = Abc_NtkCreateObj( pNtkNew, (Abc_ObjType_t)pObj->Type );
    // transfer names of the terminal objects
    if ( fCopyName )
    {
        if ( Abc_ObjIsCi(pObj) )
        {
            Abc_ObjAssignName( pObjNew, Abc_ObjName(Abc_ObjFanout0Ntk(pObj)), NULL );
        }
        else if ( Abc_ObjIsCo(pObj) )
        {
            if ( Abc_ObjIsPo(pObj) )
                Abc_ObjAssignName( pObjNew, Abc_ObjName(Abc_ObjFanin0Ntk(pObj)), NULL );
            else
            {
                assert( Abc_ObjIsLatch(Abc_ObjFanout0(pObj)) );
                Abc_ObjAssignName( pObjNew, Abc_ObjName(pObj), NULL );
            }
        }
    }
    if ( Abc_ObjIsLatch(pObj) ) // copy the reset value
        pObjNew->pData = pObj->pData;
    // remember the new node in the old node
    pObj->pCopy = pObjNew;
    return pObjNew;
}

/**Function*************************************************************

  Synopsis    [Duplicates the latch with its input/output terminals.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_NtkDupBox( Abc_Ntk_t * pNtkNew, Abc_Obj_t * pBox, int fCopyName )
{
    Abc_Obj_t * pTerm, * pBoxNew;
    int i;
    assert( Abc_ObjIsBox(pBox) );
    // duplicate the box
    pBoxNew = Abc_NtkDupObj( pNtkNew, pBox, fCopyName );
    // duplicate the fanins and connect them
    Abc_ObjForEachFanin( pBox, pTerm, i )
        Abc_ObjAddFanin( pBoxNew, Abc_NtkDupObj(pNtkNew, pTerm, fCopyName) );
    // duplicate the fanouts and connect them
    Abc_ObjForEachFanout( pBox, pTerm, i )
        Abc_ObjAddFanin( Abc_NtkDupObj(pNtkNew, pTerm, fCopyName), pBoxNew );
    return pBoxNew;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
