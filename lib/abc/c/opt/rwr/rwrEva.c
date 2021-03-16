/**CFile****************************************************************

  FileName    [rwrDec.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [DAG-aware AIG rewriting package.]

  Synopsis    [Evaluation and decomposition procedures.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: rwrDec.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "rwr.h"
#include "bool/dec/dec.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static Dec_Graph_t * Rwr_CutEvaluate( Rwr_Man_t * p, Abc_Obj_t * pRoot, Cut_Cut_t * pCut, Vec_Ptr_t * vFaninsCur, int nNodesSaved, int * pGainBest );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Performs rewriting for one node.]

  Description [This procedure considers all the cuts computed for the node
  and tries to rewrite each of them using the "forest" of different AIG
  structures precomputed and stored in the RWR manager.
  Determines the best rewriting and computes the gain in the number of AIG
  nodes in the final network. In the end, p->vFanins contains information
  about the best cut that can be used for rewriting, while p->pGraph gives
  the decomposition dag (represented using decomposition graph data structure).
  Returns gain in the number of nodes or -1 if node cannot be rewritten.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Rwr_NodeRewrite( Rwr_Man_t * p, Cut_Man_t * pManCut, Abc_Obj_t * pNode, int fUseZeros )
{
    Dec_Graph_t * pGraph;
    Cut_Cut_t * pCut;//, * pTemp;
    Abc_Obj_t * pFanin;
    unsigned uPhase;
    unsigned uTruthBest = 0; // Suppress "might be used uninitialized"
    unsigned uTruth;
    char * pPerm;
    int nNodesSaved;
    int i, GainCur = -1, GainBest = -1;

    p->nNodesConsidered++;

    // get the node's cuts
    pCut = (Cut_Cut_t *)Abc_NodeGetCutsRecursive( pManCut, pNode );
    assert( pCut != NULL );

    // go through the cuts
    for ( pCut = pCut->pNext; pCut; pCut = pCut->pNext )
    {
        // consider only 4-input cuts
        if ( pCut->nLeaves < 4 )
            continue;
//            Cut_CutPrint( pCut, 0 ), printf( "\n" );

        // get the fanin permutation
        uTruth = 0xFFFF & *Cut_CutReadTruth(pCut);
        pPerm = p->pPerms4[ (int)p->pPerms[uTruth] ];
        uPhase = p->pPhases[uTruth];
        // collect fanins with the corresponding permutation/phase
        Vec_PtrClear( p->vFaninsCur );
        Vec_PtrFill( p->vFaninsCur, (int)pCut->nLeaves, 0 );
        for ( i = 0; i < (int)pCut->nLeaves; i++ )
        {
            pFanin = Abc_NtkObj( pNode->pNtk, pCut->pLeaves[(int)pPerm[i]] );
            if ( pFanin == NULL )
                break;
            pFanin = Abc_ObjNotCond(pFanin, ((uPhase & (1<<i)) > 0) );
            Vec_PtrWriteEntry( p->vFaninsCur, i, pFanin );
        }
        if ( i != (int)pCut->nLeaves )
        {
            p->nCutsBad++;
            continue;
        }
        p->nCutsGood++;

        {
            int Counter = 0;
            Vec_PtrForEachEntry( Abc_Obj_t *, p->vFaninsCur, pFanin, i )
                if ( Abc_ObjFanoutNum(Abc_ObjRegular(pFanin)) == 1 )
                    Counter++;
            if ( Counter > 2 )
                continue;
        }

        // mark the fanin boundary
        Vec_PtrForEachEntry( Abc_Obj_t *, p->vFaninsCur, pFanin, i )
            Abc_ObjRegular(pFanin)->vFanouts.nSize++;

        // label MFFC with current ID
        Abc_NtkIncrementTravId( pNode->pNtk );
        nNodesSaved = Abc_NodeMffcLabelAig( pNode );
        // unmark the fanin boundary
        Vec_PtrForEachEntry( Abc_Obj_t *, p->vFaninsCur, pFanin, i )
            Abc_ObjRegular(pFanin)->vFanouts.nSize--;

        // evaluate the cut
        pGraph = Rwr_CutEvaluate( p, pNode, pCut, p->vFaninsCur, nNodesSaved, &GainCur );

        // check if the cut is better than the current best one
        if ( pGraph != NULL && GainBest < GainCur )
        {
            // save this form
            GainBest  = GainCur;
            p->pGraph  = pGraph;
            p->fCompl = ((uPhase & (1<<4)) > 0);
            uTruthBest = 0xFFFF & *Cut_CutReadTruth(pCut);
            // collect fanins in the
            Vec_PtrClear( p->vFanins );
            Vec_PtrForEachEntry( Abc_Obj_t *, p->vFaninsCur, pFanin, i )
                Vec_PtrPush( p->vFanins, pFanin );
        }
    }

    if ( GainBest == -1 )
        return -1;

    // copy the leaves
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vFanins, pFanin, i )
        Dec_GraphNode((Dec_Graph_t *)p->pGraph, i)->pFunc = pFanin;

    p->nScores[p->pMap[uTruthBest]]++;
    p->nNodesGained += GainBest;
    if ( fUseZeros || GainBest > 0 )
    {
        p->nNodesRewritten++;
    }

    return GainBest;
}

/**Function*************************************************************

  Synopsis    [Evaluates the cut.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Rwr_CutEvaluate( Rwr_Man_t * p, Abc_Obj_t * pRoot, Cut_Cut_t * pCut, Vec_Ptr_t * vFaninsCur, int nNodesSaved, int * pGainBest )
{
    extern int            Dec_GraphToNetworkCount( Abc_Obj_t * pRoot, Dec_Graph_t * pGraph, int NodeMax );
    Vec_Ptr_t * vSubgraphs;
    Dec_Graph_t * pGraphBest = NULL; // Suppress "might be used uninitialized"
    Dec_Graph_t * pGraphCur;
    Rwr_Node_t * pNode, * pFanin;
    int nNodesAdded, GainBest, i, k;
    unsigned uTruth;
    // find the matching class of subgraphs
    uTruth = 0xFFFF & *Cut_CutReadTruth(pCut);
    vSubgraphs = Vec_VecEntry( p->vClasses, p->pMap[uTruth] );
    p->nSubgraphs += vSubgraphs->nSize;
    // determine the best subgraph
    GainBest = -1;
    Vec_PtrForEachEntry( Rwr_Node_t *, vSubgraphs, pNode, i )
    {
        // get the current graph
        pGraphCur = (Dec_Graph_t *)pNode->pNext;
        // copy the leaves
        Vec_PtrForEachEntry( Rwr_Node_t *, vFaninsCur, pFanin, k )
            Dec_GraphNode(pGraphCur, k)->pFunc = pFanin;
        // detect how many unlabeled nodes will be reused
        nNodesAdded = Dec_GraphToNetworkCount( pRoot, pGraphCur, nNodesSaved );
        if ( nNodesAdded == -1 )
            continue;
        assert( nNodesSaved >= nNodesAdded );

        // count the gain at this node
        if ( GainBest < nNodesSaved - nNodesAdded )
        {
            GainBest   = nNodesSaved - nNodesAdded;
            pGraphBest = pGraphCur;

            // score the graph
            if ( nNodesSaved - nNodesAdded > 0 )
            {
                pNode->nScore++;
                pNode->nGain += GainBest;
                pNode->nAdded += nNodesAdded;
            }
        }
    }
    if ( GainBest == -1 )
        return NULL;
    *pGainBest = GainBest;
    return pGraphBest;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
