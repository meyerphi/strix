/**CFile****************************************************************

  FileName    [abcRewrite.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Technology-independent resynthesis of the AIG based on DAG aware rewriting.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcRewrite.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "opt/rwr/rwr.h"
#include "bool/dec/dec.h"

/*
    The ideas realized in this package are inspired by the paper:
    Per Bjesse, Arne Boralv, "DAG-aware circuit compression for
    formal verification", Proc. ICCAD 2004, pp. 42-49.
*/

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static Cut_Man_t * Abc_NtkStartCutManForRewrite( Abc_Ntk_t * pNtk );

extern void  Abc_PlaceBegin( Abc_Ntk_t * pNtk );
extern void  Abc_PlaceEnd( Abc_Ntk_t * pNtk );
extern void  Abc_PlaceUpdate( Vec_Ptr_t * vAddedCells, Vec_Ptr_t * vUpdatedNets );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Performs incremental rewriting of the AIG.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkRewrite( Abc_Ntk_t * pNtk, int fUseZeros, int fPrecompute )
{
    extern void           Dec_GraphUpdateNetwork( Abc_Obj_t * pRoot, Dec_Graph_t * pGraph );
    Dec_Man_t * pManDec;
    Cut_Man_t * pManCut;
    Rwr_Man_t * pManRwr;
    Abc_Obj_t * pNode;
    Dec_Graph_t * pGraph;
    int i, nNodes, nGain, fCompl;

    // cleanup the AIG
    Abc_AigCleanup((Abc_Aig_t *)pNtk->pManFunc);

    // start the decomposition manager
    pManDec   = Dec_ManStart();
    // start the rewriting manager
    pManRwr = Rwr_ManStart( pManDec, fPrecompute );
    if ( pManRwr == NULL )
        return 0;
    // start the cut manager
    pManCut = Abc_NtkStartCutManForRewrite( pNtk );
    pNtk->pManCut = pManCut;

    // resynthesize each node once
    pManRwr->nNodesBeg = Abc_NtkNodeNum(pNtk);
    nNodes = Abc_NtkObjNumMax(pNtk);
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        // stop if all nodes have been tried once
        if ( i >= nNodes )
            break;
        // skip the nodes with many fanouts
        if ( Abc_ObjFanoutNum(pNode) > 1000 )
            continue;

        // for each cut, try to resynthesize it
        nGain = Rwr_NodeRewrite( pManRwr, pManCut, pNode, fUseZeros );
        if ( !(nGain > 0 || (nGain == 0 && fUseZeros)) )
            continue;
        // if we end up here, a rewriting step is accepted

        // get hold of the new subgraph to be added to the AIG
        pGraph = (Dec_Graph_t *)Rwr_ManReadDecs(pManRwr);
        fCompl = Rwr_ManReadCompl(pManRwr);

        // complement the FF if needed
        if ( fCompl ) Dec_GraphComplement( pGraph );
        Dec_GraphUpdateNetwork( pNode, pGraph );
        if ( fCompl ) Dec_GraphComplement( pGraph );
    }
    // print stats
    pManRwr->nNodesEnd = Abc_NtkNodeNum(pNtk);
    // delete the managers
    Rwr_ManStop( pManRwr );
    Cut_ManStop( pManCut );
    Dec_ManStop( pManDec );
    pNtk->pManCut = NULL;

    // put the nodes into the DFS order and reassign their IDs
    Abc_NtkReassignIds( pNtk );
    Abc_NtkLevel( pNtk );
    // check
    if ( !Abc_NtkCheck( pNtk ) )
    {
        printf( "Abc_NtkRewrite: The network check has failed.\n" );
        return 0;
    }
    return 1;
}

/**Function*************************************************************

  Synopsis    [Starts the cut manager for rewriting.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Cut_Man_t * Abc_NtkStartCutManForRewrite( Abc_Ntk_t * pNtk )
{
    Cut_Params_t * pParams = ABC_ALLOC( Cut_Params_t, 1 );
    Cut_Man_t * pManCut;
    Abc_Obj_t * pObj;
    int i;
    // start the cut manager
    memset( pParams, 0, sizeof(Cut_Params_t) );
    pParams->nVarsMax  = 4;     // the max cut size ("k" of the k-feasible cuts)
    pParams->nKeepMax  = 250;   // the max number of cuts kept at a node
    pParams->fTruth    = 1;     // compute truth tables
    pParams->fFilter   = 1;     // filter dominated cuts
    pParams->nIdsMax   = Abc_NtkObjNumMax( pNtk );
    pManCut = Cut_ManStart( pParams );
    // set cuts for PIs
    Abc_NtkForEachCi( pNtk, pObj, i )
        if ( Abc_ObjFanoutNum(pObj) > 0 )
            Cut_NodeSetTriv( pManCut, pObj->Id );
    return pManCut;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
