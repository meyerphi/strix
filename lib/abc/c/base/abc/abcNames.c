/**CFile****************************************************************

  FileName    [abcNames.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Procedures working with net and node names.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcNames.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "abc.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Returns the unique name for the object.]

  Description [If the name previously did not exist, creates a new unique
  name but does not assign this name to the object. The temporary unique
  name is stored in a static buffer inside this procedure. It is important
  that the name is used before the function is called again!]

  SideEffects []

  SeeAlso     []

***********************************************************************/
char * Abc_ObjName( Abc_Obj_t * pObj )
{
    return Nm_ManCreateUniqueName( pObj->pNtk->pManName, pObj->Id );
}

/**Function*************************************************************

  Synopsis    [Assigns the given name to the object.]

  Description [The object should not have a name assigned. The same
  name may be used for several objects, which they share the same net
  in the original netlist. (For example, latch output and primary output
  may have the same name.) This procedure returns the pointer to the
  internally stored representation of the given name.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
char * Abc_ObjAssignName( Abc_Obj_t * pObj, char * pName, char * pSuffix )
{
    assert( pName != NULL );
    return Nm_ManStoreIdName( pObj->pNtk->pManName, pObj->Id, pObj->Type, pName, pSuffix );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
