/**CFile****************************************************************

  FileName    [darRefact.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [DAG-aware AIG rewriting.]

  Synopsis    [Refactoring.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: darRefact.c,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#include "darInt.h"
#include "aig/aig/aig.h"
#include "bool/kit/kit.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

// the refactoring manager
typedef struct Ref_Man_t_            Ref_Man_t;
struct Ref_Man_t_
{
    // input data
    Dar_RefPar_t *   pPars;          // rewriting parameters
    Aig_Man_t *      pAig;           // AIG manager
    // computed cuts
    Vec_Vec_t *      vCuts;          // the storage for cuts
    // truth table and ISOP
    Vec_Ptr_t *      vTruthElem;     // elementary truth tables
    Vec_Ptr_t *      vTruthStore;    // storage for truth tables
    Vec_Int_t *      vMemory;        // storage for ISOP
    Vec_Ptr_t *      vCutNodes;      // storage for internal nodes of the cut
    // various data members
    Vec_Ptr_t *      vLeavesBest;    // the best set of leaves
    Kit_Graph_t *    pGraphBest;     // the best factored form
    int              GainBest;       // the best gain
    int              LevelBest;      // the level of node with the best gain
    // node statistics
    int              nNodesInit;     // the initial number of nodes
    int              nNodesTried;    // the number of nodes tried
    int              nNodesBelow;    // the number of nodes below the level limit
    int              nNodesExten;    // the number of nodes with extended cut
    int              nCutsUsed;      // the number of rewriting steps
    int              nCutsTried;     // the number of cuts tries
};

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Returns the structure with default assignment of parameters.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_ManDefaultRefParams( Dar_RefPar_t * pPars )
{
    memset( pPars, 0, sizeof(Dar_RefPar_t) );
    pPars->nMffcMin     =  2;  // the min MFFC size for which refactoring is used
    pPars->nLeafMax     = 12;  // the max number of leaves of a cut
    pPars->nCutsMax     =  5;  // the max number of cuts to consider
    pPars->fExtend      =  0;
    pPars->fUseZeros    =  0;
}

/**Function*************************************************************

  Synopsis    [Starts the rewriting manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Ref_Man_t * Dar_ManRefStart( Aig_Man_t * pAig, Dar_RefPar_t * pPars )
{
    Ref_Man_t * p;
    // start the manager
    p = ABC_ALLOC( Ref_Man_t, 1 );
    memset( p, 0, sizeof(Ref_Man_t) );
    p->pAig         = pAig;
    p->pPars        = pPars;
    // other data
    p->vCuts        = Vec_VecStart( pPars->nCutsMax );
    p->vTruthElem   = Vec_PtrAllocTruthTables( pPars->nLeafMax );
    p->vTruthStore  = Vec_PtrAllocSimInfo( 1024, Kit_TruthWordNum(pPars->nLeafMax) );
    p->vMemory      = Vec_IntAlloc( 1 << 16 );
    p->vCutNodes    = Vec_PtrAlloc( 256 );
    p->vLeavesBest  = Vec_PtrAlloc( pPars->nLeafMax );
    return p;
}

/**Function*************************************************************

  Synopsis    [Stops the rewriting manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_ManRefStop( Ref_Man_t * p )
{
    Vec_VecFree( p->vCuts );
    Vec_PtrFree( p->vTruthElem );
    Vec_PtrFree( p->vTruthStore );
    Vec_PtrFree( p->vLeavesBest );
    Vec_IntFree( p->vMemory );
    Vec_PtrFree( p->vCutNodes );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Counts the number of new nodes added when using this graph.]

  Description [AIG nodes for the fanins should be assigned to pNode->pFunc
  of the leaves of the graph before calling this procedure.
  Returns -1 if the number of nodes and levels exceeded the given limit or
  the number of levels exceeded the maximum allowed level.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Dar_RefactTryGraph( Aig_Man_t * pAig, Aig_Obj_t * pRoot, Vec_Ptr_t * vCut, Kit_Graph_t * pGraph, int NodeMax )
{
    Kit_Node_t * pNode, * pNode0, * pNode1;
    Aig_Obj_t * pAnd, * pAnd0, * pAnd1;
    int i, Counter, LevelNew;
    // check for constant function or a literal
    if ( Kit_GraphIsConst(pGraph) || Kit_GraphIsVar(pGraph) )
        return 0;
    // set the levels of the leaves
    Kit_GraphForEachLeaf( pGraph, pNode, i )
    {
        pNode->pFunc = Vec_PtrEntry(vCut, i);
        pNode->Level = Aig_Regular((Aig_Obj_t *)pNode->pFunc)->Level;
        assert( Aig_Regular((Aig_Obj_t *)pNode->pFunc)->Level < (1<<24)-1 );
    }
    // compute the AIG size after adding the internal nodes
    Counter = 0;
    Kit_GraphForEachNode( pGraph, pNode, i )
    {
        // get the children of this node
        pNode0 = Kit_GraphNode( pGraph, pNode->eEdge0.Node );
        pNode1 = Kit_GraphNode( pGraph, pNode->eEdge1.Node );
        // get the AIG nodes corresponding to the children
        pAnd0 = (Aig_Obj_t *)pNode0->pFunc;
        pAnd1 = (Aig_Obj_t *)pNode1->pFunc;
        if ( pAnd0 && pAnd1 )
        {
            // if they are both present, find the resulting node
            pAnd0 = Aig_NotCond( pAnd0, pNode->eEdge0.fCompl );
            pAnd1 = Aig_NotCond( pAnd1, pNode->eEdge1.fCompl );
            pAnd  = Aig_TableLookupTwo( pAig, pAnd0, pAnd1 );
            // return -1 if the node is the same as the original root
            if ( Aig_Regular(pAnd) == pRoot )
                return -1;
        }
        else
            pAnd = NULL;
        // count the number of added nodes
        if ( pAnd == NULL || Aig_ObjIsTravIdCurrent(pAig, Aig_Regular(pAnd)) )
        {
            if ( ++Counter > NodeMax )
                return -1;
        }
        // count the number of new levels
        LevelNew = 1 + Abc_MaxInt( pNode0->Level, pNode1->Level );
        if ( pAnd )
        {
            if ( Aig_Regular(pAnd) == Aig_ManConst1(pAig) )
                LevelNew = 0;
            else if ( Aig_Regular(pAnd) == Aig_Regular(pAnd0) )
                LevelNew = (int)Aig_Regular(pAnd0)->Level;
            else if ( Aig_Regular(pAnd) == Aig_Regular(pAnd1) )
                LevelNew = (int)Aig_Regular(pAnd1)->Level;
        }
        pNode->pFunc = pAnd;
        pNode->Level = LevelNew;
    }
    return Counter;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Obj_t * Dar_RefactBuildGraph( Aig_Man_t * pAig, Vec_Ptr_t * vCut, Kit_Graph_t * pGraph )
{
    Aig_Obj_t * pAnd0, * pAnd1;
    Kit_Node_t * pNode = NULL;
    int i;
    // check for constant function
    if ( Kit_GraphIsConst(pGraph) )
        return Aig_NotCond( Aig_ManConst1(pAig), Kit_GraphIsComplement(pGraph) );
    // set the leaves
    Kit_GraphForEachLeaf( pGraph, pNode, i )
        pNode->pFunc = Vec_PtrEntry(vCut, i);
    // check for a literal
    if ( Kit_GraphIsVar(pGraph) )
        return Aig_NotCond( (Aig_Obj_t *)Kit_GraphVar(pGraph)->pFunc, Kit_GraphIsComplement(pGraph) );
    // build the AIG nodes corresponding to the AND gates of the graph
    Kit_GraphForEachNode( pGraph, pNode, i )
    {
        pAnd0 = Aig_NotCond( (Aig_Obj_t *)Kit_GraphNode(pGraph, pNode->eEdge0.Node)->pFunc, pNode->eEdge0.fCompl );
        pAnd1 = Aig_NotCond( (Aig_Obj_t *)Kit_GraphNode(pGraph, pNode->eEdge1.Node)->pFunc, pNode->eEdge1.fCompl );
        pNode->pFunc = Aig_And( pAig, pAnd0, pAnd1 );
    }
    // complement the result if necessary
    return Aig_NotCond( (Aig_Obj_t *)pNode->pFunc, Kit_GraphIsComplement(pGraph) );
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Dar_ManRefactorTryCuts( Ref_Man_t * p, Aig_Obj_t * pObj, int nNodesSaved )
{
    Vec_Ptr_t * vCut;
    Kit_Graph_t * pGraphCur;
    int k, RetValue, GainCur, nNodesAdded;
    unsigned * pTruth;

    p->GainBest = -1;
    p->pGraphBest = NULL;
    Vec_VecForEachLevel( p->vCuts, vCut, k )
    {
        if ( Vec_PtrSize(vCut) == 0 )
            continue;
//        if ( Vec_PtrSize(vCut) != 0 && Vec_PtrSize(Vec_VecEntry(p->vCuts, k+1)) != 0 )
//            continue;

        p->nCutsTried++;
        // get the cut nodes
        Aig_ObjCollectCut( pObj, vCut, p->vCutNodes );
        // get the truth table
        pTruth = Aig_ManCutTruth( pObj, vCut, p->vCutNodes, p->vTruthElem, p->vTruthStore );
        if ( Kit_TruthIsConst0(pTruth, Vec_PtrSize(vCut)) )
        {
            p->GainBest = Aig_NodeMffcSupp( p->pAig, pObj, 0, NULL );
            p->pGraphBest = Kit_GraphCreateConst0();
            Vec_PtrCopy( p->vLeavesBest, vCut );
            return p->GainBest;
        }
        if ( Kit_TruthIsConst1(pTruth, Vec_PtrSize(vCut)) )
        {
            p->GainBest = Aig_NodeMffcSupp( p->pAig, pObj, 0, NULL );
            p->pGraphBest = Kit_GraphCreateConst1();
            Vec_PtrCopy( p->vLeavesBest, vCut );
            return p->GainBest;
        }

        // try the positive phase
        RetValue = Kit_TruthIsop( pTruth, Vec_PtrSize(vCut), p->vMemory, 0 );
        if ( RetValue > -1 )
        {
            pGraphCur = Kit_SopFactor( p->vMemory, 0, Vec_PtrSize(vCut), p->vMemory );
            nNodesAdded = Dar_RefactTryGraph( p->pAig, pObj, vCut, pGraphCur, nNodesSaved - !p->pPars->fUseZeros );
            if ( nNodesAdded > -1 )
            {
                GainCur = nNodesSaved - nNodesAdded;
                if ( p->GainBest < GainCur || (p->GainBest == GainCur &&
                    (Kit_GraphIsConst(pGraphCur) || Kit_GraphRootLevel(pGraphCur) < Kit_GraphRootLevel(p->pGraphBest))) )
                {
                    p->GainBest = GainCur;
                    if ( p->pGraphBest )
                        Kit_GraphFree( p->pGraphBest );
                    p->pGraphBest = pGraphCur;
                    Vec_PtrCopy( p->vLeavesBest, vCut );
                }
                else
                    Kit_GraphFree( pGraphCur );
            }
            else
                Kit_GraphFree( pGraphCur );
        }
        // try negative phase
        Kit_TruthNot( pTruth, pTruth, Vec_PtrSize(vCut) );
        RetValue = Kit_TruthIsop( pTruth, Vec_PtrSize(vCut), p->vMemory, 0 );
//        Kit_TruthNot( pTruth, pTruth, Vec_PtrSize(vCut) );
        if ( RetValue > -1 )
        {
            pGraphCur = Kit_SopFactor( p->vMemory, 1, Vec_PtrSize(vCut), p->vMemory );
            nNodesAdded = Dar_RefactTryGraph( p->pAig, pObj, vCut, pGraphCur, nNodesSaved - !p->pPars->fUseZeros );
            if ( nNodesAdded > -1 )
            {
                GainCur = nNodesSaved - nNodesAdded;
                if ( p->GainBest < GainCur || (p->GainBest == GainCur &&
                    (Kit_GraphIsConst(pGraphCur) || Kit_GraphRootLevel(pGraphCur) < Kit_GraphRootLevel(p->pGraphBest))) )
                {
                    p->GainBest = GainCur;
                    if ( p->pGraphBest )
                        Kit_GraphFree( p->pGraphBest );
                    p->pGraphBest = pGraphCur;
                    Vec_PtrCopy( p->vLeavesBest, vCut );
                }
                else
                    Kit_GraphFree( pGraphCur );
            }
            else
                Kit_GraphFree( pGraphCur );
        }
    }

    return p->GainBest;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if a non-PI node has nLevelMin or below.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Dar_ObjCutLevelAchieved( Vec_Ptr_t * vCut, int nLevelMin )
{
    Aig_Obj_t * pObj;
    int i;
    Vec_PtrForEachEntry( Aig_Obj_t *, vCut, pObj, i )
        if ( !Aig_ObjIsCi(pObj) && (int)pObj->Level <= nLevelMin )
            return 1;
    return 0;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Dar_ManRefactor( Aig_Man_t * pAig, Dar_RefPar_t * pPars )
{
    Ref_Man_t * p;
    Vec_Ptr_t * vCut, * vCut2;
    Aig_Obj_t * pObj, * pObjNew;
    int nNodesOld, nNodeBefore, nNodeAfter, nNodesSaved, nNodesSaved2;
    int i, nLevelMin;

    // start the manager
    p = Dar_ManRefStart( pAig, pPars );
    // remove dangling nodes
    Aig_ManCleanup( pAig );
    // if updating levels is requested, start fanout and timing
    Aig_ManFanoutStart( pAig );
    // resynthesize each node once
    vCut = Vec_VecEntry( p->vCuts, 0 );
    vCut2 = Vec_VecEntry( p->vCuts, 1 );
    p->nNodesInit = Aig_ManNodeNum(pAig);
    nNodesOld = Vec_PtrSize( pAig->vObjs );
//    pProgress = Bar_ProgressStart( stdout, nNodesOld );
    Aig_ManForEachObj( pAig, pObj, i )
    {
        if ( !Aig_ObjIsNode(pObj) )
            continue;
        if ( i > nNodesOld )
            break;
        Vec_VecClear( p->vCuts );

        // get the bounded MFFC size
        nLevelMin = Abc_MaxInt( 0, Aig_ObjLevel(pObj) - 10 );
        nNodesSaved = Aig_NodeMffcSupp( pAig, pObj, nLevelMin, vCut );
        if ( nNodesSaved < p->pPars->nMffcMin ) // too small to consider
        {
            continue;
        }
        p->nNodesTried++;
        if ( Vec_PtrSize(vCut) > p->pPars->nLeafMax ) // get one reconv-driven cut
        {
            Aig_ManFindCut( pObj, vCut, p->vCutNodes, p->pPars->nLeafMax, 50 );
            nNodesSaved = Aig_NodeMffcLabelCut( p->pAig, pObj, vCut );
        }
        else if ( Vec_PtrSize(vCut) < p->pPars->nLeafMax - 2 && p->pPars->fExtend )
        {
            if ( !Dar_ObjCutLevelAchieved(vCut, nLevelMin) )
            {
                if ( Aig_NodeMffcExtendCut( pAig, pObj, vCut, vCut2 ) )
                {
                    nNodesSaved2 = Aig_NodeMffcLabelCut( p->pAig, pObj, vCut );
                    assert( nNodesSaved2 == nNodesSaved );
                }
                if ( Vec_PtrSize(vCut2) > p->pPars->nLeafMax )
                    Vec_PtrClear(vCut2);
                if ( Vec_PtrSize(vCut2) > 0 )
                {
                    p->nNodesExten++;
                }
            }
            else
                p->nNodesBelow++;
        }

        // try the cuts
        Dar_ManRefactorTryCuts( p, pObj, nNodesSaved );

        // check the best gain
        if ( !(p->GainBest > 0 || (p->GainBest == 0 && p->pPars->fUseZeros)) )
        {
            if ( p->pGraphBest )
                Kit_GraphFree( p->pGraphBest );
            continue;
        }

        // if we end up here, a rewriting step is accepted
        nNodeBefore = Aig_ManNodeNum( pAig );
        pObjNew = Dar_RefactBuildGraph( pAig, p->vLeavesBest, p->pGraphBest );
        // replace the node
        Aig_ObjReplace( pAig, pObj, pObjNew );
        // compare the gains
        nNodeAfter = Aig_ManNodeNum( pAig );
        assert( p->GainBest <= nNodeBefore - nNodeAfter );
        Kit_GraphFree( p->pGraphBest );
        p->nCutsUsed++;
    }

    // fix the levels
    Aig_ManFanoutStop( pAig );

    // remove dangling nodes (they should not be here!)
    Aig_ManCleanup( pAig );

    // stop the rewriting manager
    Dar_ManRefStop( p );
    if ( !Aig_ManCheck( pAig ) )
    {
        printf( "Dar_ManRefactor: The network check has failed.\n" );
        return 0;
    }
    return 1;

}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
