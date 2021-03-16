/**CFile****************************************************************

  FileName    [darCore.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [DAG-aware AIG rewriting.]

  Synopsis    [Core of the rewriting package.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: darCore.c,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#include "darInt.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

// iterator over the nodes in the topological order
#define Aig_ManForEachNodeInOrder( p, pObj )                                    \
    for ( assert(p->pOrderData), p->iPrev = 0, p->iNext = p->pOrderData[1];     \
          p->iNext && (((pObj) = Aig_ManObj(p, p->iNext)), 1);                  \
          p->iNext = p->pOrderData[2*p->iPrev+1] )

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Returns the structure with default assignment of parameters.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_ManDefaultRwrParams( Dar_RwrPar_t * pPars )
{
    memset( pPars, 0, sizeof(Dar_RwrPar_t) );
    pPars->nCutsMax     =  8; // 8
    pPars->nSubgMax     =  5; // 5 is a "magic number"
    pPars->fUseZeros    =  0;
    pPars->fRecycle     =  1;
}

#define MAX_VAL 10

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Dar_ManRewrite( Dar_Lib_t * pDarLib, Aig_Man_t * pAig, Dar_RwrPar_t * pPars )
{
    Dar_Man_t * p;
    Dar_Cut_t * pCut;
    Aig_Obj_t * pObj, * pObjNew;
    int i, k, nNodesOld, nNodeBefore, nNodeAfter;
    int Counter = 0;
    int nMffcSize;//, nMffcGains[MAX_VAL+1][MAX_VAL+1] = {{0}};
    // prepare the library
    Dar_LibPrepare( pDarLib, pPars->nSubgMax );
    // create rewriting manager
    p = Dar_ManStart( pDarLib, pAig, pPars );
    // remove dangling nodes
    Aig_ManCleanup( pAig );
    // start fanout
    Aig_ManFanoutStart( pAig );
    // resynthesize each node once
    p->nNodesInit = Aig_ManNodeNum(pAig);
    nNodesOld = Vec_PtrSize( pAig->vObjs );

    Aig_ManForEachObj( pAig, pObj, i )
    {
        if ( !Aig_ObjIsNode(pObj) )
            continue;
        if ( i > nNodesOld )
            break;
        if ( pPars->fRecycle && ++Counter % 50000 == 0 && Aig_DagSize(pObj) < Vec_PtrSize(p->vCutNodes)/100 )
        {
            Dar_ManCutsRestart( p );
        }

        // compute cuts for the node
        p->nNodesTried++;
        Dar_ObjSetCuts( pObj, NULL );
        Dar_ObjComputeCuts_rec( p, pObj );

        // check if there is a trivial cut
        Dar_ObjForEachCut( pObj, pCut, k )
            if ( pCut->nLeaves == 0 || (pCut->nLeaves == 1 && pCut->pLeaves[0] != pObj->Id && Aig_ManObj(p->pAig, pCut->pLeaves[0])) )
                break;
        if ( k < (int)pObj->nCuts )
        {
            assert( pCut->nLeaves < 2 );
            if ( pCut->nLeaves == 0 ) // replace by constant
            {
                assert( pCut->uTruth == 0 || pCut->uTruth == 0xFFFF );
                pObjNew = Aig_NotCond( Aig_ManConst1(p->pAig), pCut->uTruth==0 );
            }
            else
            {
                assert( pCut->uTruth == 0xAAAA || pCut->uTruth == 0x5555 );
                pObjNew = Aig_NotCond( Aig_ManObj(p->pAig, pCut->pLeaves[0]), pCut->uTruth==0x5555 );
            }
            // remove the old cuts
            Dar_ObjSetCuts( pObj, NULL );
            // replace the node
            Aig_ObjReplace( pAig, pObj, pObjNew );
            continue;
        }

        // evaluate the cuts
        p->GainBest = -1;
        nMffcSize   = -1;
        Dar_ObjForEachCut( pObj, pCut, k )
        {
            int nLeavesOld = pCut->nLeaves;
            if ( pCut->nLeaves == 3 )
                pCut->pLeaves[pCut->nLeaves++] = 0;
            Dar_LibEval( p, pObj, pCut, &nMffcSize );
            pCut->nLeaves = nLeavesOld;
        }
        // check the best gain
        if ( !(p->GainBest > 0 || (p->GainBest == 0 && p->pPars->fUseZeros)) )
        {
            continue;
        }
        // remove the old cuts
        Dar_ObjSetCuts( pObj, NULL );
        // if we end up here, a rewriting step is accepted
        nNodeBefore = Aig_ManNodeNum( pAig );
        pObjNew = Dar_LibBuildBest( p ); // pObjNew can be complemented!
        pObjNew = Aig_NotCond( pObjNew, Aig_ObjPhaseReal(pObjNew) ^ pObj->fPhase );
        // replace the node
        Aig_ObjReplace( pAig, pObj, pObjNew );
        // compare the gains
        nNodeAfter = Aig_ManNodeNum( pAig );
        assert( p->GainBest <= nNodeBefore - nNodeAfter );
        // count gains of this class
        p->ClassGains[p->ClassBest] += nNodeBefore - nNodeAfter;
    }

    Dar_ManCutsFree( p );
    // stop fanout
    Aig_ManFanoutStop( pAig );
    // stop the rewriting manager
    Dar_ManStop( p );
    Aig_ManCheckPhase( pAig );
    // check
    if ( !Aig_ManCheck( pAig ) )
    {
        printf( "Aig_ManRewrite: The network check has failed.\n" );
        return 0;
    }
    return 1;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
