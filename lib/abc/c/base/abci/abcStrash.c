/**CFile****************************************************************

  FileName    [abcStrash.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Strashing of the current network.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcStrash.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "bool/dec/dec.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Reapplies structural hashing to the AIG.]

  Description [Because of the structural hashing, this procedure should not
  change the number of nodes. It is useful to detect the bugs in the original AIG.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkRestrashZero( Abc_Ntk_t * pNtk )
{
    Abc_Ntk_t * pNtkAig;
    Abc_Obj_t * pObj;
    int i;
    int Counter = 0;
    // start the new network (constants and CIs of the old network will point to the their counterparts in the new network)
    pNtkAig = Abc_NtkStartFrom( pNtk );
    // complement the 1-values registers
    Abc_NtkForEachLatch( pNtk, pObj, i )
    {
        if ( Abc_LatchIsInitDc(pObj) )
            Counter++;
        else if ( Abc_LatchIsInit1(pObj) )
            Abc_ObjFanout0(pObj)->pCopy = Abc_ObjNot(Abc_ObjFanout0(pObj)->pCopy);
    }
    if ( Counter )
    printf( "Converting %d flops from don't-care to zero initial value.\n", Counter );
    // restrash the nodes (assuming a topological order of the old network)
    Abc_NtkForEachNode( pNtk, pObj, i )
        pObj->pCopy = Abc_AigAnd( (Abc_Aig_t *)pNtkAig->pManFunc, Abc_ObjChild0Copy(pObj), Abc_ObjChild1Copy(pObj) );
    // finalize the network
    Abc_NtkFinalize( pNtk );
    // complement the 1-valued registers
    Abc_NtkForEachLatch( pNtkAig, pObj, i )
        if ( Abc_LatchIsInit1(pObj) )
        {
            Abc_ObjXorFaninC( Abc_ObjFanin0(pObj), 0 );
            // if latch has PO as one of its fanouts change latch name
            if ( Abc_NodeFindCoFanout( Abc_ObjFanout0(pObj) ) )
            {
                Nm_ManDeleteIdName( pObj->pNtk->pManName, Abc_ObjFanout0(pObj)->Id );
                Abc_ObjAssignName( Abc_ObjFanout0(pObj), Abc_ObjName(Abc_ObjFanout0(pObj)), "_inv" );
            }
        }
    // set all constant-0 values
    Abc_NtkForEachLatch( pNtkAig, pObj, i )
        Abc_LatchSetInit0( pObj );

    // make sure everything is okay
    if ( !Abc_NtkCheck( pNtkAig ) )
    {
        printf( "Abc_NtkStrash: The network check has failed.\n" );
        Abc_NtkDelete( pNtkAig );
        return NULL;
    }
    return pNtkAig;

}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
