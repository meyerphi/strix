/**CFile****************************************************************

  FileName    [abcNtk.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Network creation/duplication/deletion procedures.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcNtk.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "abc.h"
#include "base/main/main.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static const int ABC_NUM_STEPS = 10;

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Creates a new Ntk.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkAlloc( int fUseMemMan )
{
    Abc_Ntk_t * pNtk;
    pNtk = ABC_ALLOC( Abc_Ntk_t, 1 );
    memset( pNtk, 0, sizeof(Abc_Ntk_t) );
    // start the object storage
    pNtk->vObjs       = Vec_PtrAlloc( 100 );
    pNtk->vPios       = Vec_PtrAlloc( 100 );
    pNtk->vPis        = Vec_PtrAlloc( 100 );
    pNtk->vPos        = Vec_PtrAlloc( 100 );
    pNtk->vCis        = Vec_PtrAlloc( 100 );
    pNtk->vCos        = Vec_PtrAlloc( 100 );
    pNtk->vBoxes      = Vec_PtrAlloc( 100 );
    // start the memory managers
    pNtk->pMmObj      = fUseMemMan? Mem_FixedStart( sizeof(Abc_Obj_t) ) : NULL;
    pNtk->pMmStep     = fUseMemMan? Mem_StepStart( ABC_NUM_STEPS ) : NULL;
    // get ready to assign the first Obj ID
    pNtk->nTravIds    = 1;
    // start the functionality manager
    pNtk->pManFunc = Abc_AigAlloc( pNtk );
    // name manager
    pNtk->pManName = Nm_ManCreate( 200 );
    return pNtk;
}

/**Function*************************************************************

  Synopsis    [Starts a new network using existing network as a model.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkStartFrom( Abc_Ntk_t * pNtk )
{
    Abc_Ntk_t * pNtkNew;
    Abc_Obj_t * pObj;
    int fCopyNames, i;
    if ( pNtk == NULL )
        return NULL;
    // copy names
    fCopyNames = 1;
    // start the network
    pNtkNew = Abc_NtkAlloc( 1 );
    // clean the node copy fields
    Abc_NtkCleanCopy( pNtk );
    // map the constant nodes
    Abc_AigConst1(pNtk)->pCopy = Abc_AigConst1(pNtkNew);
    // clone CIs/CIs/boxes
    Abc_NtkForEachPi( pNtk, pObj, i )
        Abc_NtkDupObj( pNtkNew, pObj, fCopyNames );
    Abc_NtkForEachPo( pNtk, pObj, i )
        Abc_NtkDupObj( pNtkNew, pObj, fCopyNames );
    Abc_NtkForEachBox( pNtk, pObj, i )
        Abc_NtkDupBox( pNtkNew, pObj, fCopyNames );
    // transfer logic level
    Abc_NtkForEachCi( pNtk, pObj, i )
        pObj->pCopy->Level = pObj->Level;
    // check that the CI/CO/latches are copied correctly
    assert( Abc_NtkCiNum(pNtk)    == Abc_NtkCiNum(pNtkNew) );
    assert( Abc_NtkCoNum(pNtk)    == Abc_NtkCoNum(pNtkNew) );
    assert( Abc_NtkLatchNum(pNtk) == Abc_NtkLatchNum(pNtkNew) );
    return pNtkNew;
}

/**Function*************************************************************

  Synopsis    [Finalizes the network using the existing network as a model.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkFinalize( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pObj, * pDriver, * pDriverNew;
    int i;
    // set the COs of the strashed network
    Abc_NtkForEachCo( pNtk, pObj, i )
    {
        pDriver    = Abc_ObjFanin0Ntk( Abc_ObjFanin0(pObj) );
        pDriverNew = Abc_ObjNotCond(pDriver->pCopy, Abc_ObjFaninC0(pObj));
        Abc_ObjAddFanin( pObj->pCopy, pDriverNew );
    }
}

/**Function*************************************************************

  Synopsis    [Deletes the Ntk.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkDelete( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pObj;
    int TotalMemory, i;
    if ( pNtk == NULL )
        return;
    // make sure all the marks are clean
    Abc_NtkForEachObj( pNtk, pObj, i )
    {
        // these flags should be always zero
        // if this is not true, something is wrong somewhere
        assert( pObj->fMarkA == 0 );
        assert( pObj->fMarkB == 0 );
    }
    // free the nodes
    if ( pNtk->pMmStep == NULL )
    {
        Abc_NtkForEachObj( pNtk, pObj, i )
        {
            ABC_FREE( pObj->vFanouts.pArray );
            ABC_FREE( pObj->vFanins.pArray );
        }
    }
    if ( pNtk->pMmObj == NULL )
    {
        Abc_NtkForEachObj( pNtk, pObj, i )
            ABC_FREE( pObj );
    }

    // free the arrays
    Vec_PtrFree( pNtk->vPios );
    Vec_PtrFree( pNtk->vPis );
    Vec_PtrFree( pNtk->vPos );
    Vec_PtrFree( pNtk->vCis );
    Vec_PtrFree( pNtk->vCos );
    Vec_PtrFree( pNtk->vObjs );
    Vec_PtrFree( pNtk->vBoxes );
    ABC_FREE( pNtk->vTravIds.pArray );
    TotalMemory  = 0;
    TotalMemory += pNtk->pMmObj? Mem_FixedReadMemUsage(pNtk->pMmObj)  : 0;
    TotalMemory += pNtk->pMmStep? Mem_StepReadMemUsage(pNtk->pMmStep) : 0;
//    fprintf( stdout, "The total memory allocated internally by the network = %0.2f MB.\n", ((double)TotalMemory)/(1<<20) );
    // free the storage
    if ( pNtk->pMmObj )
        Mem_FixedStop( pNtk->pMmObj );
    if ( pNtk->pMmStep )
        Mem_StepStop ( pNtk->pMmStep );
    // name manager
    Nm_ManFree( pNtk->pManName );
    // stop the functionality manager
    Abc_AigFree( (Abc_Aig_t *)pNtk->pManFunc );

    ABC_FREE( pNtk );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
