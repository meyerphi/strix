/**CFile****************************************************************

  FileName    [abcCut.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Interface to cut computation.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcCut.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "opt/cut/cut.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Computes the cuts for the network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void * Abc_NodeGetCutsRecursive( void * p, Abc_Obj_t * pObj )
{
    void * pList;
    if ( (pList = Abc_NodeReadCuts( p, pObj )) )
        return pList;
    Abc_NodeGetCutsRecursive( p, Abc_ObjFanin0(pObj) );
    Abc_NodeGetCutsRecursive( p, Abc_ObjFanin1(pObj) );
    return Abc_NodeGetCuts( p, pObj );
}

/**Function*************************************************************

  Synopsis    [Computes the cuts for the network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void * Abc_NodeGetCuts( void * p, Abc_Obj_t * pObj )
{
    int fDagNode, TreeCode = 0;
    assert( Abc_ObjFaninNum(pObj) == 2 );

    // check if the node is a DAG node
    fDagNode = (Abc_ObjFanoutNum(pObj) > 1 && !Abc_NodeIsMuxControlType(pObj));
    // increment the counter of DAG nodes
    if ( fDagNode )
        Cut_ManIncrementDagNodes( (Cut_Man_t *)p );

    return Cut_NodeComputeCuts( (Cut_Man_t *)p, pObj->Id, Abc_ObjFaninId0(pObj), Abc_ObjFaninId1(pObj),
        Abc_ObjFaninC0(pObj), Abc_ObjFaninC1(pObj), TreeCode );
}

/**Function*************************************************************

  Synopsis    [Computes the cuts for the network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void * Abc_NodeReadCuts( void * p, Abc_Obj_t * pObj )
{
    return Cut_NodeReadCutsNew( (Cut_Man_t *)p, pObj->Id );
}

/**Function*************************************************************

  Synopsis    [Computes the cuts for the network.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NodeFreeCuts( void * p, Abc_Obj_t * pObj )
{
    Cut_NodeFreeCuts( (Cut_Man_t *)p, pObj->Id );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
