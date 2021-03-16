/**CFile****************************************************************

  FileName    [aigUtil.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [AIG package.]

  Synopsis    [Various procedures.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: aigUtil.c,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#include "aig.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Increments the current traversal ID of the network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ManIncrementTravId( Aig_Man_t * p )
{
    if ( p->nTravIds >= (1<<30)-1 )
        Aig_ManCleanData( p );
    p->nTravIds++;
}

/**Function*************************************************************

  Synopsis    [Cleans the data pointers for the nodes.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ManCleanData( Aig_Man_t * p )
{
    Aig_Obj_t * pObj;
    int i;
    Aig_ManForEachObj( p, pObj, i )
        pObj->pData = NULL;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Obj_t * Aig_ObjReal_rec( Aig_Obj_t * pObj )
{
    Aig_Obj_t * pObjNew, * pObjR = Aig_Regular(pObj);
    if ( !Aig_ObjIsBuf(pObjR) )
        return pObj;
    pObjNew = Aig_ObjReal_rec( Aig_ObjChild0(pObjR) );
    return Aig_NotCond( pObjNew, Aig_IsComplement(pObj) );
}

/**Function*************************************************************

  Synopsis    [Sets the PI/PO numbers.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ManSetCioIds( Aig_Man_t * p )
{
    Aig_Obj_t * pObj;
    int i;
    Aig_ManForEachCi( p, pObj, i )
        pObj->CioId = i;
    Aig_ManForEachCo( p, pObj, i )
        pObj->CioId = i;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
