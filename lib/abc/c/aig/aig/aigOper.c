/**CFile****************************************************************

  FileName    [aigOper.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [AIG package.]

  Synopsis    [AIG operations.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: aigOper.c,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#include "aig.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Perform one operation.]

  Description [The argument nodes can be complemented.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Obj_t * Aig_Oper( Aig_Man_t * p, Aig_Obj_t * p0, Aig_Obj_t * p1, Aig_Type_t Type )
{
    if ( Type == AIG_OBJ_AND )
        return Aig_And( p, p0, p1 );
    assert( 0 );
    return NULL;
}

/**Function*************************************************************

  Synopsis    [Performs canonicization step.]

  Description [The argument nodes can be complemented.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Obj_t * Aig_And( Aig_Man_t * p, Aig_Obj_t * p0, Aig_Obj_t * p1 )
{
    Aig_Obj_t * pGhost, * pResult;
    // check trivial cases
    if ( p0 == p1 )
        return p0;
    if ( p0 == Aig_Not(p1) )
        return Aig_Not(p->pConst1);
    if ( Aig_Regular(p0) == p->pConst1 )
        return p0 == p->pConst1 ? p1 : Aig_Not(p->pConst1);
    if ( Aig_Regular(p1) == p->pConst1 )
        return p1 == p->pConst1 ? p0 : Aig_Not(p->pConst1);
    pGhost = Aig_ObjCreateGhost( p, p0, p1, AIG_OBJ_AND );
    if ( (pResult = Aig_TableLookup( p, pGhost )) )
        return pResult;
    return Aig_ObjCreate( p, pGhost );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
