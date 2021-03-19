/**CFile****************************************************************

  FileName    [ioWriteAiger.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Command processing package.]

  Synopsis    [Procedures to write binary AIGER format developed by
  Armin Biere, Johannes Kepler University (http://fmv.jku.at/)]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - December 16, 2006.]

  Revision    [$Id: ioWriteAiger.c,v 1.00 2006/12/16 00:00:00 alanmi Exp $]

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

static unsigned Io_ObjMakeLit( int Var, int fCompl )                 { return (Var << 1) | fCompl;                   }
static unsigned Io_ObjAigerNum( Abc_Obj_t * pObj )                   { return (unsigned)(ABC_PTRINT_T)pObj->pCopy;   }
static void     Io_ObjSetAigerNum( Abc_Obj_t * pObj, unsigned Num )  { pObj->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)Num; }

/**Function*************************************************************

  Synopsis    [Stores the AIG in the AIGER library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
aiger * Io_StoreAiger( Abc_Ntk_t * pNtk, int fCheck )
{
    int i;
    Abc_Obj_t * pObj;

    aiger * pAiger = aiger_init();
    if ( pAiger == NULL ) {
        printf( "Io_StoreAiger(): AIGER storage has failed because object could not be allocated.\n" );
        return NULL;
    }

    // set the node numbers to be used in the output file
    unsigned nNodes = 0;
    Io_ObjSetAigerNum( Abc_AigConst1(pNtk), nNodes++ );
    Abc_NtkForEachCi( pNtk, pObj, i )
        Io_ObjSetAigerNum( pObj, nNodes++ );
    Abc_AigForEachAnd( pNtk, pObj, i )
        Io_ObjSetAigerNum( pObj, nNodes++ );

    // if the driver node is a constant, we need to complement the literal below
    // because, in the AIGER format, literal 0/1 is represented as number 0/1
    // while, in ABC, constant 1 node has number 0 and so literal 0/1 will be 1/0

    // add inputs
    Abc_NtkForEachPi( pNtk, pObj, i )
    {
        unsigned lit = Io_ObjMakeLit( Io_ObjAigerNum(pObj), 0 );
        aiger_add_input( pAiger, lit, Abc_ObjName(pObj) );
    }

    // add latches
    Abc_NtkForEachLatch( pNtk, pObj, i )
    {
        Abc_Obj_t * pLatch = Abc_ObjFanin0(pObj);
        Abc_Obj_t * pNext = Abc_ObjFanin0(pLatch);
        Abc_Obj_t * pOut = Abc_ObjFanout0(pObj);
        unsigned lit = Io_ObjMakeLit( Io_ObjAigerNum(pOut), 0 );
        unsigned next = Io_ObjMakeLit( Io_ObjAigerNum(pNext), Abc_ObjFaninC0(pLatch) ^ (Io_ObjAigerNum(pNext) == 0) );
        aiger_add_latch( pAiger, lit, next, Abc_ObjName(pOut) );

        if ( Abc_LatchIsInit0(pObj) )
            aiger_add_reset( pAiger, lit, 0 );
        else if ( Abc_LatchIsInit1(pObj) )
            aiger_add_reset( pAiger, lit, 1 );
        else
        {
            assert( Abc_LatchIsInitDc(pObj) );
            aiger_add_reset( pAiger, lit, lit );
        }
    }

    // add outputs
    Abc_NtkForEachPo( pNtk, pObj, i )
    {
        Abc_Obj_t * pNext = Abc_ObjFanin0(pObj);
        unsigned lit = Io_ObjMakeLit( Io_ObjAigerNum(pNext), Abc_ObjFaninC0(pObj) ^ (Io_ObjAigerNum(pNext) == 0) );
        aiger_add_output( pAiger, lit, Abc_ObjName(pObj) );
    }

    // add and nodes
    Abc_AigForEachAnd( pNtk, pObj, i )
    {
        unsigned lhs  = Io_ObjMakeLit( Io_ObjAigerNum(pObj), 0 );
        unsigned rhs0 = Io_ObjMakeLit( Io_ObjAigerNum(Abc_ObjFanin0(pObj)), Abc_ObjFaninC0(pObj) );
        unsigned rhs1 = Io_ObjMakeLit( Io_ObjAigerNum(Abc_ObjFanin1(pObj)), Abc_ObjFaninC1(pObj) );
        aiger_add_and( pAiger, lhs, rhs0, rhs1 );
    }

    if ( fCheck && aiger_check (pAiger) != 0 ) {
        printf( "Io_StoreAiger: The network check has failed.\n" );
        return NULL;
    }

    return pAiger;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
