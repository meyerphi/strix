/**CFile****************************************************************

  FileName    [abcDar.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [DAG-aware rewriting.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcDar.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "opt/dar/dar.h"
#include "aig/aig/aig.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Converts the network from the AIG manager into ABC.]

  Description [Assumes that registers are ordered after PIs/POs.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk )
{
    Vec_Ptr_t * vNodes;
    Aig_Man_t * pMan;
    Abc_Obj_t * pObj;
    int i;
    // create the manager
    pMan = Aig_ManStart( Abc_NtkNodeNum(pNtk) + 100 );
    // transfer the pointers to the basic nodes
    Abc_AigConst1(pNtk)->pCopy = (Abc_Obj_t *)Aig_ManConst1(pMan);
    Abc_NtkForEachCi( pNtk, pObj, i )
    {
        pObj->pCopy = (Abc_Obj_t *)Aig_ObjCreateCi(pMan);
        // initialize logic level of the CIs
        ((Aig_Obj_t *)pObj->pCopy)->Level = pObj->Level;
    }

    // perform the conversion of the internal nodes (assumes DFS ordering)
    vNodes = Abc_NtkDfs( pNtk, 0 );
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pObj, i )
    {
        pObj->pCopy = (Abc_Obj_t *)Aig_And( pMan, (Aig_Obj_t *)Abc_ObjChild0Copy(pObj), (Aig_Obj_t *)Abc_ObjChild1Copy(pObj) );
    }
    Vec_PtrFree( vNodes );
    // create the POs
    Abc_NtkForEachCo( pNtk, pObj, i )
        Aig_ObjCreateCo( pMan, (Aig_Obj_t *)Abc_ObjChild0Copy(pObj) );
    // remove dangling nodes
    Aig_ManCleanup( pMan );
    if ( !Aig_ManCheck( pMan ) )
    {
        Abc_Print( 1, "Abc_NtkToDar: AIG check has failed.\n" );
        Aig_ManStop( pMan );
        return NULL;
    }
    return pMan;
}

/**Function*************************************************************

  Synopsis    [Converts the network from the AIG manager into ABC.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkFromDar( Abc_Ntk_t * pNtkOld, Aig_Man_t * pMan )
{
    Vec_Ptr_t * vNodes;
    Abc_Ntk_t * pNtkNew;
    Aig_Obj_t * pObj;
    int i;
    assert( pMan->nAsserts == 0 );
    // perform strashing
    pNtkNew = Abc_NtkStartFrom( pNtkOld );
    // transfer the pointers to the basic nodes
    Aig_ManConst1(pMan)->pData = Abc_AigConst1(pNtkNew);
    Aig_ManForEachCi( pMan, pObj, i )
    {
        pObj->pData = Abc_NtkCi(pNtkNew, i);
        // initialize logic level of the CIs
        ((Abc_Obj_t *)pObj->pData)->Level = pObj->Level;
    }
    // rebuild the AIG
    vNodes = Aig_ManDfs( pMan, 1 );
    Vec_PtrForEachEntry( Aig_Obj_t *, vNodes, pObj, i )
        if ( Aig_ObjIsBuf(pObj) )
            pObj->pData = (Abc_Obj_t *)Aig_ObjChild0Copy(pObj);
        else
            pObj->pData = Abc_AigAnd( (Abc_Aig_t *)pNtkNew->pManFunc, (Abc_Obj_t *)Aig_ObjChild0Copy(pObj), (Abc_Obj_t *)Aig_ObjChild1Copy(pObj) );
    Vec_PtrFree( vNodes );
    // connect the PO nodes
    Aig_ManForEachCo( pMan, pObj, i )
    {
        if ( pMan->nAsserts && i == Aig_ManCoNum(pMan) - pMan->nAsserts )
            break;
        Abc_ObjAddFanin( Abc_NtkCo(pNtkNew, i), (Abc_Obj_t *)Aig_ObjChild0Copy(pObj) );
    }
    // if there are assertions, add them
    if ( !Abc_NtkCheck( pNtkNew ) )
        Abc_Print( 1, "Abc_NtkFromDar(): Network check has failed.\n" );
//Abc_NtkPrintCiLevels( pNtkNew );
    return pNtkNew;
}

/**Function*************************************************************

  Synopsis    [Gives the current ABC network to AIG manager for processing.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkDRewrite( Dar_Lib_t * pDarLib, Abc_Ntk_t * pNtk, Dar_RwrPar_t * pPars )
{
    Aig_Man_t * pMan, * pTemp;
    Abc_Ntk_t * pNtkAig;
    pMan = Abc_NtkToDar( pNtk );
    if ( pMan == NULL )
        return NULL;

    Dar_ManRewrite( pDarLib, pMan, pPars );

    pMan = Aig_ManDupDfs( pTemp = pMan );
    Aig_ManStop( pTemp );

    pNtkAig = Abc_NtkFromDar( pNtk, pMan );
    Aig_ManStop( pMan );
    return pNtkAig;
}

/**Function*************************************************************

  Synopsis    [Gives the current ABC network to AIG manager for processing.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Abc_NtkDRefactor( Abc_Ntk_t * pNtk, Dar_RefPar_t * pPars )
{
    Aig_Man_t * pMan, * pTemp;
    Abc_Ntk_t * pNtkAig;
    pMan = Abc_NtkToDar( pNtk );
    if ( pMan == NULL )
        return NULL;

    Dar_ManRefactor( pMan, pPars );

    pMan = Aig_ManDupDfs( pTemp = pMan );
    Aig_ManStop( pTemp );

    pNtkAig = Abc_NtkFromDar( pNtk, pMan );
    Aig_ManStop( pMan );
    return pNtkAig;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
