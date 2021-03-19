/**CFile****************************************************************

  FileName    [ioReadAiger.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Command processing package.]

  Synopsis    [Procedures to read binary AIGER format developed by
  Armin Biere, Johannes Kepler University (http://fmv.jku.at/)]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - December 16, 2006.]

  Revision    [$Id: ioReadAiger.c,v 1.00 2006/12/16 00:00:00 alanmi Exp $]

***********************************************************************/

// The code in this file is developed in collaboration with Mark Jarvin of Toronto.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "aiger.h"

#include "base/main/main.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Loads the AIG from the AIGER library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Io_LoadAiger( aiger * pAiger, int fCheck )
{
    if ( fCheck && aiger_check (pAiger) != 0 ) {
        printf( "Io_LoadAiger: The network check has failed.\n" );
        return NULL;
    }

    int i;
    Abc_Obj_t * pObj;
    Vec_Ptr_t * vNodes;
    Abc_Ntk_t * pNtkNew;

    int nInputs = pAiger->num_inputs;
    int nOutputs = pAiger->num_outputs;
    int nLatches = pAiger->num_latches;
    int nAnds = pAiger->num_ands;

    // allocate the empty AIG
    pNtkNew = Abc_NtkAlloc( 1 );

    vNodes = Vec_PtrAlloc( 1 + nInputs + nLatches + nAnds );
    Vec_PtrPush( vNodes, Abc_ObjNot( Abc_AigConst1(pNtkNew) ) );

    // create the PIs
    for ( i = 0; i < nInputs; i++ )
    {
        pObj = Abc_NtkCreatePi(pNtkNew);
        Vec_PtrPush( vNodes, pObj );
        Abc_ObjAssignName( pObj, pAiger->inputs[i].name, NULL );
    }
    // create the POs
    for ( i = 0; i < nOutputs; i++ )
    {
        pObj = Abc_NtkCreatePo(pNtkNew);
        Abc_ObjAssignName( pObj, pAiger->outputs[i].name, NULL );
    }
    // create the latches
    for ( i = 0; i < nLatches; i++ )
    {
        pObj = Abc_NtkCreateLatch(pNtkNew);
        Abc_LatchSetInit0( pObj );
        Abc_Obj_t * pNode0 = Abc_NtkCreateBi(pNtkNew);
        Abc_Obj_t * pNode1 = Abc_NtkCreateBo(pNtkNew);
        Abc_ObjAddFanin( pObj, pNode0 );
        Abc_ObjAddFanin( pNode1, pObj );
        Vec_PtrPush( vNodes, pNode1 );

        // assign latch name
        Abc_ObjAssignName( pNode1, pAiger->latches[i].name, NULL );
        Abc_ObjAssignName( pObj, Abc_ObjName(pNode1), "L" );
        Abc_ObjAssignName( pNode0, Abc_ObjName(pNode1), "_in" );
    }
    // create the AND gates
    for ( i = 0; i < nAnds; i++ )
    {
        unsigned rhs0 = pAiger->ands[i].rhs0;
        unsigned rhs1 = pAiger->ands[i].rhs1;
        Abc_Obj_t * pNode0 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, rhs0 >> 1), rhs0 & 1 );
        Abc_Obj_t * pNode1 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, rhs1 >> 1), rhs1 & 1 );
        assert( Vec_PtrSize(vNodes) == i + 1 + nInputs + nLatches );
        Vec_PtrPush( vNodes, Abc_AigAnd((Abc_Aig_t *)pNtkNew->pManFunc, pNode0, pNode1) );
    }
    // read the latch driver literals
    Abc_NtkForEachLatchInput( pNtkNew, pObj, i )
    {
        unsigned next = pAiger->latches[i].next;
        unsigned reset = pAiger->latches[i].reset;
        if ( reset == 0 )
            Abc_LatchSetInit0( Abc_NtkBox(pNtkNew, i) );
        else if ( reset == 1 )
            Abc_LatchSetInit1( Abc_NtkBox(pNtkNew, i) );
        else
        {
            assert( reset == (unsigned)Abc_Var2Lit(1+Abc_NtkPiNum(pNtkNew)+i, 0) );
            // unitialized value of the latch is the latch literal according to http://fmv.jku.at/hwmcc11/beyond1.pdf
            Abc_LatchSetInitDc( Abc_NtkBox(pNtkNew, i) );
        }

        Abc_Obj_t * pNode0 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, next >> 1), (next & 1) );
        Abc_ObjAddFanin( pObj, pNode0 );
    }
    // read the PO driver literals
    Abc_NtkForEachPo( pNtkNew, pObj, i )
    {
        unsigned lit = pAiger->outputs[i].lit;
        Abc_Obj_t * pNode0 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, lit >> 1), (lit & 1) );
        Abc_ObjAddFanin( pObj, pNode0 );
    }

    // remove extra nodes
    Abc_AigCleanup( (Abc_Aig_t *)pNtkNew->pManFunc );

    // check the result
    if ( fCheck && !Abc_NtkCheckRead( pNtkNew ) )
    {
        printf( "Io_LoadAiger: The network check has failed.\n" );
        Abc_NtkDelete( pNtkNew );
        return NULL;
    }
    return pNtkNew;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
