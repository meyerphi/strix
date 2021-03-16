/**CFile****************************************************************

  FileName    [aigDup.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [AIG package.]

  Synopsis    [AIG duplication (re-strashing).]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: aigDup.c,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#include "aig/aig/aig.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Duplicates the AIG manager recursively.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Obj_t * Aig_ManDupDfs_rec( Aig_Man_t * pNew, Aig_Man_t * p, Aig_Obj_t * pObj )
{
    Aig_Obj_t * pObjNew;
    if ( pObj->pData )
        return (Aig_Obj_t *)pObj->pData;
    Aig_ManDupDfs_rec( pNew, p, Aig_ObjFanin0(pObj) );
    if ( Aig_ObjIsBuf(pObj) )
        return (Aig_Obj_t *)(pObj->pData = Aig_ObjChild0Copy(pObj));
    Aig_ManDupDfs_rec( pNew, p, Aig_ObjFanin1(pObj) );
    pObjNew = Aig_Oper( pNew, Aig_ObjChild0Copy(pObj), Aig_ObjChild1Copy(pObj), Aig_ObjType(pObj) );
    return (Aig_Obj_t *)(pObj->pData = pObjNew);
}

/**Function*************************************************************

  Synopsis    [Duplicates the AIG manager.]

  Description [This duplicator works for AIGs with choices.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Man_t * Aig_ManDupDfs( Aig_Man_t * p )
{
    Aig_Man_t * pNew;
    Aig_Obj_t * pObj, * pObjNew = NULL;
    int i;
    // create the new manager
    pNew = Aig_ManStart( Aig_ManObjNumMax(p) );
    pNew->nAsserts = p->nAsserts;
    pNew->nBarBufs = p->nBarBufs;
    if ( p->vFlopNums )
        pNew->vFlopNums = Vec_IntDup( p->vFlopNums );
    // create the PIs
    Aig_ManCleanData( p );
    // duplicate internal nodes
    Aig_ManConst1(p)->pData = Aig_ManConst1(pNew);
    Aig_ManForEachObj( p, pObj, i )
    {
        if ( Aig_ObjIsCi(pObj) )
        {
            pObjNew = Aig_ObjCreateCi( pNew );
            pObjNew->Level = pObj->Level;
            pObj->pData = pObjNew;
        }
        else if ( Aig_ObjIsCo(pObj) )
        {
            Aig_ManDupDfs_rec( pNew, p, Aig_ObjFanin0(pObj) );
            pObjNew = Aig_ObjCreateCo( pNew, Aig_ObjChild0Copy(pObj) );
            pObj->pData = pObjNew;
        }
    }
    assert( Aig_ManBufNum(p) != 0 || Aig_ManNodeNum(p) == Aig_ManNodeNum(pNew) );
    int nNodes = Aig_ManCleanup( pNew );
    if ( nNodes > 0 )
        printf( "Aig_ManDupDfs(): Cleanup after AIG duplication removed %d nodes.\n", nNodes );
    Aig_ManSetRegNum( pNew, Aig_ManRegNum(p) );
    // check the resulting network
    if ( !Aig_ManCheck(pNew) )
        printf( "Aig_ManDupDfs(): The check has failed.\n" );
    return pNew;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
