/**CFile****************************************************************

  FileName    [aigMan.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [AIG package.]

  Synopsis    [AIG manager.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: aigMan.c,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#include "aig.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Starts the AIG manager.]

  Description [The argument of this procedure is a soft limit on the
  the number of nodes, or 0 if the limit is unknown.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Man_t * Aig_ManStart( int nNodesMax )
{
    Aig_Man_t * p;
    if ( nNodesMax <= 0 )
        nNodesMax = 10007;
    // start the manager
    p = ABC_ALLOC( Aig_Man_t, 1 );
    memset( p, 0, sizeof(Aig_Man_t) );
    // perform initializations
    p->nTravIds = 1;
    // allocate arrays for nodes
    p->vCis  = Vec_PtrAlloc( 100 );
    p->vCos  = Vec_PtrAlloc( 100 );
    p->vObjs = Vec_PtrAlloc( 1000 );
    p->vBufs = Vec_PtrAlloc( 100 );
    // prepare the internal memory manager
    p->pMemObjs = Aig_MmFixedStart( sizeof(Aig_Obj_t), nNodesMax );
    // create the constant node
    p->pConst1 = Aig_ManFetchMemory( p );
    p->pConst1->Type = AIG_OBJ_CONST1;
    p->pConst1->fPhase = 1;
    p->nObjs[AIG_OBJ_CONST1]++;
    // start the table
    p->nTableSize = Abc_PrimeCudd( nNodesMax );
    p->pTable = ABC_ALLOC( Aig_Obj_t *, p->nTableSize );
    memset( p->pTable, 0, sizeof(Aig_Obj_t *) * p->nTableSize );
    return p;
}

/**Function*************************************************************

  Synopsis    [Stops the AIG manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ManStop( Aig_Man_t * p )
{
    Aig_Obj_t * pObj;
    int i;
    // make sure the nodes have clean marks
    Aig_ManForEachObj( p, pObj, i )
        assert( !pObj->fMarkA && !pObj->fMarkB );
    if ( p->pFanData )
        Aig_ManFanoutStop( p );
    Aig_MmFixedStop( p->pMemObjs, 0 );
    Vec_PtrFreeP( &p->vCis );
    Vec_PtrFreeP( &p->vCos );
    Vec_PtrFreeP( &p->vObjs );
    Vec_PtrFreeP( &p->vBufs );
    Vec_IntFreeP( &p->vFlopNums );
    Vec_IntFreeP( &p->vFlopReprs );
    ABC_FREE( p->pTable );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Removes combinational logic that does not feed into POs.]

  Description [Returns the number of dangling nodes removed.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Aig_ManCleanup( Aig_Man_t * p )
{
    Vec_Ptr_t * vObjs;
    Aig_Obj_t * pNode;
    int i, nNodesOld = Aig_ManNodeNum(p);
    // collect roots of dangling nodes
    vObjs = Vec_PtrAlloc( 100 );
    Aig_ManForEachObj( p, pNode, i )
        if ( Aig_ObjIsNode(pNode) && Aig_ObjRefs(pNode) == 0 )
            Vec_PtrPush( vObjs, pNode );
    // recursively remove dangling nodes
    Vec_PtrForEachEntry( Aig_Obj_t *, vObjs, pNode, i )
        Aig_ObjDelete_rec( p, pNode, 1 );
    Vec_PtrFree( vObjs );
    return nNodesOld - Aig_ManNodeNum(p);
}

/**Function*************************************************************

  Synopsis    [Sets the number of registers in the AIG manager.]

  Description [This procedure should be called after the manager is
  fully constructed.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ManSetRegNum( Aig_Man_t * p, int nRegs )
{
    p->nRegs = nRegs;
    p->nTruePis = Aig_ManCiNum(p) - nRegs;
    p->nTruePos = Aig_ManCoNum(p) - nRegs;
    Aig_ManSetCioIds( p );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
