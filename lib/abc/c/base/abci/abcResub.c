/**CFile****************************************************************

  FileName    [abcResub.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Resubstitution manager.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcResub.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "bool/dec/dec.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

#define ABC_RS_DIV1_MAX    150   // the max number of divisors to consider
#define ABC_RS_DIV2_MAX    500   // the max number of pair-wise divisors to consider

typedef struct Abc_ManRes_t_ Abc_ManRes_t;
struct Abc_ManRes_t_
{
    // paramers
    int                nLeavesMax; // the max number of leaves in the cone
    int                nDivsMax;   // the max number of divisors in the cone
    // representation of the cone
    Abc_Obj_t *        pRoot;      // the root of the cone
    int                nLeaves;    // the number of leaves
    int                nDivs;      // the number of all divisor (including leaves)
    int                nMffc;      // the size of MFFC
    int                nLastGain;  // the gain the number of nodes
    Vec_Ptr_t *        vDivs;      // the divisors
    // representation of the simulation info
    int                nBits;      // the number of simulation bits
    int                nWords;     // the number of unsigneds for siminfo
    Vec_Ptr_t        * vSims;      // simulation info
    unsigned         * pInfo;      // pointer to simulation info
    // observability don't-cares
    unsigned *         pCareSet;
    // internal divisor storage
    Vec_Ptr_t        * vDivs1UP;   // the single-node unate divisors
    Vec_Ptr_t        * vDivs1UN;   // the single-node unate divisors
    Vec_Ptr_t        * vDivs1B;    // the single-node binate divisors
    Vec_Ptr_t        * vDivs2UP0;  // the double-node unate divisors
    Vec_Ptr_t        * vDivs2UP1;  // the double-node unate divisors
    Vec_Ptr_t        * vDivs2UN0;  // the double-node unate divisors
    Vec_Ptr_t        * vDivs2UN1;  // the double-node unate divisors
    // other data
    Vec_Ptr_t        * vTemp;      // temporary array of nodes
    // improvement statistics
    int                nUsedNodeC;
    int                nUsedNode0;
    int                nUsedNode1Or;
    int                nUsedNode1And;
    int                nUsedNode2Or;
    int                nUsedNode2And;
    int                nUsedNode2OrAnd;
    int                nUsedNode2AndOr;
    int                nUsedNode3OrAnd;
    int                nUsedNode3AndOr;
    int                nUsedNodeTotal;
    int                nTotalDivs;
    int                nTotalLeaves;
    int                nTotalGain;
    int                nNodesBeg;
    int                nNodesEnd;
};

// external procedures
static Abc_ManRes_t* Abc_ManResubStart( int nLeavesMax, int nDivsMax );
static void          Abc_ManResubStop( Abc_ManRes_t * p );
static Dec_Graph_t * Abc_ManResubEval( Abc_ManRes_t * p, Abc_Obj_t * pRoot, Vec_Ptr_t * vLeaves, int nSteps );

// other procedures
static int           Abc_ManResubCollectDivs( Abc_ManRes_t * p, Abc_Obj_t * pRoot, Vec_Ptr_t * vLeaves );
static void          Abc_ManResubSimulate( Vec_Ptr_t * vDivs, int nLeaves, Vec_Ptr_t * vSims, int nLeavesMax, int nWords );

static void          Abc_ManResubDivsS( Abc_ManRes_t * p );
static void          Abc_ManResubDivsD( Abc_ManRes_t * p );
static Dec_Graph_t * Abc_ManResubQuit( Abc_ManRes_t * p );
static Dec_Graph_t * Abc_ManResubDivs0( Abc_ManRes_t * p );
static Dec_Graph_t * Abc_ManResubDivs1( Abc_ManRes_t * p );
static Dec_Graph_t * Abc_ManResubDivs12( Abc_ManRes_t * p );
static Dec_Graph_t * Abc_ManResubDivs2( Abc_ManRes_t * p );
static Dec_Graph_t * Abc_ManResubDivs3( Abc_ManRes_t * p );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Performs incremental resynthesis of the AIG.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkResubstitute( Abc_Ntk_t * pNtk, int nCutMax, int nStepsMax )
{
    extern void           Dec_GraphUpdateNetwork( Abc_Obj_t * pRoot, Dec_Graph_t * pGraph );
    Abc_ManRes_t * pManRes;
    Abc_ManCut_t * pManCut;
    Dec_Graph_t * pFForm;
    Vec_Ptr_t * vLeaves;
    Abc_Obj_t * pNode;
    int i, nNodes;

    // cleanup the AIG
    Abc_AigCleanup((Abc_Aig_t *)pNtk->pManFunc);
    // start the managers
    pManCut = Abc_NtkManCutStart( nCutMax, 100000, 100000, 100000 );
    pManRes = Abc_ManResubStart( nCutMax, ABC_RS_DIV1_MAX );

    if ( Abc_NtkLatchNum(pNtk) ) {
        Abc_NtkForEachLatch(pNtk, pNode, i)
            pNode->pNext = (Abc_Obj_t *)pNode->pData;
    }

    // resynthesize each node once
    pManRes->nNodesBeg = Abc_NtkNodeNum(pNtk);
    nNodes = Abc_NtkObjNumMax(pNtk);
    Abc_NtkForEachNode( pNtk, pNode, i )
    {
        // skip the nodes with many fanouts
        if ( Abc_ObjFanoutNum(pNode) > 1000 )
            continue;
        // stop if all nodes have been tried once
        if ( i >= nNodes )
            break;

        // compute a reconvergence-driven cut
        vLeaves = Abc_NodeFindCut( pManCut, pNode, 0 );

        // evaluate this cut
        pFForm = Abc_ManResubEval( pManRes, pNode, vLeaves, nStepsMax );
        if ( pFForm == NULL )
            continue;
        pManRes->nTotalGain += pManRes->nLastGain;
        // acceptable replacement found, update the graph
        Dec_GraphUpdateNetwork( pNode, pFForm );
        Dec_GraphFree( pFForm );
    }
    pManRes->nNodesEnd = Abc_NtkNodeNum(pNtk);

    // delete the managers
    Abc_ManResubStop( pManRes );
    Abc_NtkManCutStop( pManCut );

    // clean the data field
    Abc_NtkForEachObj( pNtk, pNode, i )
        pNode->pData = NULL;

    if ( Abc_NtkLatchNum(pNtk) ) {
        Abc_NtkForEachLatch(pNtk, pNode, i)
            pNode->pData = pNode->pNext, pNode->pNext = NULL;
    }

    // put the nodes into the DFS order and reassign their IDs
    Abc_NtkReassignIds( pNtk );
    // fix the levels
    Abc_NtkLevel( pNtk );
    // check
    if ( !Abc_NtkCheck( pNtk ) )
    {
        printf( "Abc_NtkRefactor: The network check has failed.\n" );
        return 0;
    }
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_ManRes_t * Abc_ManResubStart( int nLeavesMax, int nDivsMax )
{
    Abc_ManRes_t * p;
    unsigned * pData;
    int i, k;
    assert( sizeof(unsigned) == 4 );
    p = ABC_ALLOC( Abc_ManRes_t, 1 );
    memset( p, 0, sizeof(Abc_ManRes_t) );
    p->nLeavesMax = nLeavesMax;
    p->nDivsMax   = nDivsMax;
    p->vDivs      = Vec_PtrAlloc( p->nDivsMax );
    // allocate simulation info
    p->nBits      = (1 << p->nLeavesMax);
    p->nWords     = (p->nBits <= 32)? 1 : (p->nBits / 32);
    p->pInfo      = ABC_ALLOC( unsigned, p->nWords * (p->nDivsMax + 1) );
    memset( p->pInfo, 0, sizeof(unsigned) * p->nWords * p->nLeavesMax );
    p->vSims      = Vec_PtrAlloc( p->nDivsMax );
    for ( i = 0; i < p->nDivsMax; i++ )
        Vec_PtrPush( p->vSims, p->pInfo + i * p->nWords );
    // assign the care set
    p->pCareSet  = p->pInfo + p->nDivsMax * p->nWords;
    Abc_InfoFill( p->pCareSet, p->nWords );
    // set elementary truth tables
    for ( k = 0; k < p->nLeavesMax; k++ )
    {
        pData = (unsigned *)p->vSims->pArray[k];
        for ( i = 0; i < p->nBits; i++ )
            if ( i & (1 << k) )
                pData[i>>5] |= (1 << (i&31));
    }
    // create the remaining divisors
    p->vDivs1UP  = Vec_PtrAlloc( p->nDivsMax );
    p->vDivs1UN  = Vec_PtrAlloc( p->nDivsMax );
    p->vDivs1B   = Vec_PtrAlloc( p->nDivsMax );
    p->vDivs2UP0 = Vec_PtrAlloc( p->nDivsMax );
    p->vDivs2UP1 = Vec_PtrAlloc( p->nDivsMax );
    p->vDivs2UN0 = Vec_PtrAlloc( p->nDivsMax );
    p->vDivs2UN1 = Vec_PtrAlloc( p->nDivsMax );
    p->vTemp     = Vec_PtrAlloc( p->nDivsMax );
    return p;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_ManResubStop( Abc_ManRes_t * p )
{
    Vec_PtrFree( p->vDivs );
    Vec_PtrFree( p->vSims );
    Vec_PtrFree( p->vDivs1UP );
    Vec_PtrFree( p->vDivs1UN );
    Vec_PtrFree( p->vDivs1B );
    Vec_PtrFree( p->vDivs2UP0 );
    Vec_PtrFree( p->vDivs2UP1 );
    Vec_PtrFree( p->vDivs2UN0 );
    Vec_PtrFree( p->vDivs2UN1 );
    Vec_PtrFree( p->vTemp );
    ABC_FREE( p->pInfo );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_ManResubCollectDivs_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vInternal )
{
    // skip visited nodes
    if ( Abc_NodeIsTravIdCurrent(pNode) )
        return;
    Abc_NodeSetTravIdCurrent(pNode);
    // collect the fanins
    Abc_ManResubCollectDivs_rec( Abc_ObjFanin0(pNode), vInternal );
    Abc_ManResubCollectDivs_rec( Abc_ObjFanin1(pNode), vInternal );
    // collect the internal node
    if ( pNode->fMarkA == 0 )
        Vec_PtrPush( vInternal, pNode );
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_ManResubCollectDivs( Abc_ManRes_t * p, Abc_Obj_t * pRoot, Vec_Ptr_t * vLeaves )
{
    Abc_Obj_t * pNode, * pFanout;
    int i, k, Limit, Counter;

    Vec_PtrClear( p->vDivs1UP );
    Vec_PtrClear( p->vDivs1UN );
    Vec_PtrClear( p->vDivs1B );

    // add the leaves of the cuts to the divisors
    Vec_PtrClear( p->vDivs );
    Abc_NtkIncrementTravId( pRoot->pNtk );
    Vec_PtrForEachEntry( Abc_Obj_t *, vLeaves, pNode, i )
    {
        Vec_PtrPush( p->vDivs, pNode );
        Abc_NodeSetTravIdCurrent( pNode );
    }

    // mark nodes in the MFFC
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vTemp, pNode, i )
        pNode->fMarkA = 1;
    // collect the cone (without MFFC)
    Abc_ManResubCollectDivs_rec( pRoot, p->vDivs );
    // unmark the current MFFC
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vTemp, pNode, i )
        pNode->fMarkA = 0;

    // check if the number of divisors is not exceeded
    if ( Vec_PtrSize(p->vDivs) - Vec_PtrSize(vLeaves) + Vec_PtrSize(p->vTemp) >= Vec_PtrSize(p->vSims) - p->nLeavesMax )
        return 0;

    // get the number of divisors to collect
    Limit = Vec_PtrSize(p->vSims) - p->nLeavesMax - (Vec_PtrSize(p->vDivs) - Vec_PtrSize(vLeaves) + Vec_PtrSize(p->vTemp));

    // explore the fanouts, which are not in the MFFC
    Counter = 0;
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs, pNode, i )
    {
        if ( Abc_ObjFanoutNum(pNode) > 100 )
        {
//            printf( "%d ", Abc_ObjFanoutNum(pNode) );
            continue;
        }
        // if the fanout has both fanins in the set, add it
        Abc_ObjForEachFanout( pNode, pFanout, k )
        {
            if ( Abc_NodeIsTravIdCurrent(pFanout) || Abc_ObjIsCo(pFanout) )
                continue;
            if ( Abc_NodeIsTravIdCurrent(Abc_ObjFanin0(pFanout)) && Abc_NodeIsTravIdCurrent(Abc_ObjFanin1(pFanout)) )
            {
                if ( Abc_ObjFanin0(pFanout) == pRoot || Abc_ObjFanin1(pFanout) == pRoot )
                    continue;
                Vec_PtrPush( p->vDivs, pFanout );
                Abc_NodeSetTravIdCurrent( pFanout );
                // quit computing divisors if there is too many of them
                if ( ++Counter == Limit )
                    goto Quits;
            }
        }
    }

Quits :
    // get the number of divisors
    p->nDivs = Vec_PtrSize(p->vDivs);

    // add the nodes in the MFFC
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vTemp, pNode, i )
        Vec_PtrPush( p->vDivs, pNode );
    assert( pRoot == Vec_PtrEntryLast(p->vDivs) );

    assert( Vec_PtrSize(p->vDivs) - Vec_PtrSize(vLeaves) <= Vec_PtrSize(p->vSims) - p->nLeavesMax );
    return 1;
}

/**Function*************************************************************

  Synopsis    [Performs simulation.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_ManResubSimulate( Vec_Ptr_t * vDivs, int nLeaves, Vec_Ptr_t * vSims, int nLeavesMax, int nWords )
{
    Abc_Obj_t * pObj;
    unsigned * puData0, * puData1, * puData;
    int i, k;
    assert( Vec_PtrSize(vDivs) - nLeaves <= Vec_PtrSize(vSims) - nLeavesMax );
    // simulate
    Vec_PtrForEachEntry( Abc_Obj_t *, vDivs, pObj, i )
    {
        if ( i < nLeaves )
        { // initialize the leaf
            pObj->pData = Vec_PtrEntry( vSims, i );
            continue;
        }
        // set storage for the node's simulation info
        pObj->pData = Vec_PtrEntry( vSims, i - nLeaves + nLeavesMax );
        // get pointer to the simulation info
        puData  = (unsigned *)pObj->pData;
        puData0 = (unsigned *)Abc_ObjFanin0(pObj)->pData;
        puData1 = (unsigned *)Abc_ObjFanin1(pObj)->pData;
        // simulate
        if ( Abc_ObjFaninC0(pObj) && Abc_ObjFaninC1(pObj) )
            for ( k = 0; k < nWords; k++ )
                puData[k] = ~puData0[k] & ~puData1[k];
        else if ( Abc_ObjFaninC0(pObj) )
            for ( k = 0; k < nWords; k++ )
                puData[k] = ~puData0[k] & puData1[k];
        else if ( Abc_ObjFaninC1(pObj) )
            for ( k = 0; k < nWords; k++ )
                puData[k] = puData0[k] & ~puData1[k];
        else
            for ( k = 0; k < nWords; k++ )
                puData[k] = puData0[k] & puData1[k];
    }
    // normalize
    Vec_PtrForEachEntry( Abc_Obj_t *, vDivs, pObj, i )
    {
        puData = (unsigned *)pObj->pData;
        pObj->fPhase = (puData[0] & 1);
        if ( pObj->fPhase )
            for ( k = 0; k < nWords; k++ )
                puData[k] = ~puData[k];
    }
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubQuit0( Abc_Obj_t * pRoot, Abc_Obj_t * pObj )
{
    Dec_Graph_t * pGraph;
    Dec_Edge_t eRoot;
    pGraph = Dec_GraphCreate( 1 );
    Dec_GraphNode( pGraph, 0 )->pFunc = pObj;
    eRoot = Dec_EdgeCreate( 0, pObj->fPhase );
    Dec_GraphSetRoot( pGraph, eRoot );
    if ( pRoot->fPhase )
        Dec_GraphComplement( pGraph );
    return pGraph;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubQuit1( Abc_Obj_t * pRoot, Abc_Obj_t * pObj0, Abc_Obj_t * pObj1, int fOrGate )
{
    Dec_Graph_t * pGraph;
    Dec_Edge_t eRoot, eNode0, eNode1;
    assert( Abc_ObjRegular(pObj0) != Abc_ObjRegular(pObj1) );
    pGraph = Dec_GraphCreate( 2 );
    Dec_GraphNode( pGraph, 0 )->pFunc = Abc_ObjRegular(pObj0);
    Dec_GraphNode( pGraph, 1 )->pFunc = Abc_ObjRegular(pObj1);
    eNode0 = Dec_EdgeCreate( 0, Abc_ObjRegular(pObj0)->fPhase ^ Abc_ObjIsComplement(pObj0) );
    eNode1 = Dec_EdgeCreate( 1, Abc_ObjRegular(pObj1)->fPhase ^ Abc_ObjIsComplement(pObj1) );
    if ( fOrGate )
        eRoot  = Dec_GraphAddNodeOr( pGraph, eNode0, eNode1 );
    else
        eRoot  = Dec_GraphAddNodeAnd( pGraph, eNode0, eNode1 );
    Dec_GraphSetRoot( pGraph, eRoot );
    if ( pRoot->fPhase )
        Dec_GraphComplement( pGraph );
    return pGraph;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubQuit21( Abc_Obj_t * pRoot, Abc_Obj_t * pObj0, Abc_Obj_t * pObj1, Abc_Obj_t * pObj2, int fOrGate )
{
    Dec_Graph_t * pGraph;
    Dec_Edge_t eRoot, eNode0, eNode1, eNode2;
    assert( Abc_ObjRegular(pObj0) != Abc_ObjRegular(pObj1) );
    assert( Abc_ObjRegular(pObj0) != Abc_ObjRegular(pObj2) );
    assert( Abc_ObjRegular(pObj1) != Abc_ObjRegular(pObj2) );
    pGraph = Dec_GraphCreate( 3 );
    Dec_GraphNode( pGraph, 0 )->pFunc = Abc_ObjRegular(pObj0);
    Dec_GraphNode( pGraph, 1 )->pFunc = Abc_ObjRegular(pObj1);
    Dec_GraphNode( pGraph, 2 )->pFunc = Abc_ObjRegular(pObj2);
    eNode0 = Dec_EdgeCreate( 0, Abc_ObjRegular(pObj0)->fPhase ^ Abc_ObjIsComplement(pObj0) );
    eNode1 = Dec_EdgeCreate( 1, Abc_ObjRegular(pObj1)->fPhase ^ Abc_ObjIsComplement(pObj1) );
    eNode2 = Dec_EdgeCreate( 2, Abc_ObjRegular(pObj2)->fPhase ^ Abc_ObjIsComplement(pObj2) );
    if ( fOrGate )
    {
        eRoot  = Dec_GraphAddNodeOr( pGraph, eNode0, eNode1 );
        eRoot  = Dec_GraphAddNodeOr( pGraph, eNode2, eRoot );
    }
    else
    {
        eRoot  = Dec_GraphAddNodeAnd( pGraph, eNode0, eNode1 );
        eRoot  = Dec_GraphAddNodeAnd( pGraph, eNode2, eRoot );
    }
    Dec_GraphSetRoot( pGraph, eRoot );
    if ( pRoot->fPhase )
        Dec_GraphComplement( pGraph );
    return pGraph;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubQuit2( Abc_Obj_t * pRoot, Abc_Obj_t * pObj0, Abc_Obj_t * pObj1, Abc_Obj_t * pObj2, int fOrGate )
{
    Dec_Graph_t * pGraph;
    Dec_Edge_t eRoot, ePrev, eNode0, eNode1, eNode2;
    assert( Abc_ObjRegular(pObj0) != Abc_ObjRegular(pObj1) );
    assert( Abc_ObjRegular(pObj0) != Abc_ObjRegular(pObj2) );
    assert( Abc_ObjRegular(pObj1) != Abc_ObjRegular(pObj2) );
    pGraph = Dec_GraphCreate( 3 );
    Dec_GraphNode( pGraph, 0 )->pFunc = Abc_ObjRegular(pObj0);
    Dec_GraphNode( pGraph, 1 )->pFunc = Abc_ObjRegular(pObj1);
    Dec_GraphNode( pGraph, 2 )->pFunc = Abc_ObjRegular(pObj2);
    eNode0 = Dec_EdgeCreate( 0, Abc_ObjRegular(pObj0)->fPhase ^ Abc_ObjIsComplement(pObj0) );
    if ( Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
    {
        eNode1 = Dec_EdgeCreate( 1, Abc_ObjRegular(pObj1)->fPhase );
        eNode2 = Dec_EdgeCreate( 2, Abc_ObjRegular(pObj2)->fPhase );
        ePrev  = Dec_GraphAddNodeOr( pGraph, eNode1, eNode2 );
    }
    else
    {
        eNode1 = Dec_EdgeCreate( 1, Abc_ObjRegular(pObj1)->fPhase ^ Abc_ObjIsComplement(pObj1) );
        eNode2 = Dec_EdgeCreate( 2, Abc_ObjRegular(pObj2)->fPhase ^ Abc_ObjIsComplement(pObj2) );
        ePrev  = Dec_GraphAddNodeAnd( pGraph, eNode1, eNode2 );
    }
    if ( fOrGate )
        eRoot  = Dec_GraphAddNodeOr( pGraph, eNode0, ePrev );
    else
        eRoot  = Dec_GraphAddNodeAnd( pGraph, eNode0, ePrev );
    Dec_GraphSetRoot( pGraph, eRoot );
    if ( pRoot->fPhase )
        Dec_GraphComplement( pGraph );
    return pGraph;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubQuit3( Abc_Obj_t * pRoot, Abc_Obj_t * pObj0, Abc_Obj_t * pObj1, Abc_Obj_t * pObj2, Abc_Obj_t * pObj3, int fOrGate )
{
    Dec_Graph_t * pGraph;
    Dec_Edge_t eRoot, ePrev0, ePrev1, eNode0, eNode1, eNode2, eNode3;
    assert( Abc_ObjRegular(pObj0) != Abc_ObjRegular(pObj1) );
    assert( Abc_ObjRegular(pObj2) != Abc_ObjRegular(pObj3) );
    pGraph = Dec_GraphCreate( 4 );
    Dec_GraphNode( pGraph, 0 )->pFunc = Abc_ObjRegular(pObj0);
    Dec_GraphNode( pGraph, 1 )->pFunc = Abc_ObjRegular(pObj1);
    Dec_GraphNode( pGraph, 2 )->pFunc = Abc_ObjRegular(pObj2);
    Dec_GraphNode( pGraph, 3 )->pFunc = Abc_ObjRegular(pObj3);
    if ( Abc_ObjIsComplement(pObj0) && Abc_ObjIsComplement(pObj1) )
    {
        eNode0 = Dec_EdgeCreate( 0, Abc_ObjRegular(pObj0)->fPhase );
        eNode1 = Dec_EdgeCreate( 1, Abc_ObjRegular(pObj1)->fPhase );
        ePrev0 = Dec_GraphAddNodeOr( pGraph, eNode0, eNode1 );
        if ( Abc_ObjIsComplement(pObj2) && Abc_ObjIsComplement(pObj3) )
        {
            eNode2 = Dec_EdgeCreate( 2, Abc_ObjRegular(pObj2)->fPhase );
            eNode3 = Dec_EdgeCreate( 3, Abc_ObjRegular(pObj3)->fPhase );
            ePrev1 = Dec_GraphAddNodeOr( pGraph, eNode2, eNode3 );
        }
        else
        {
            eNode2 = Dec_EdgeCreate( 2, Abc_ObjRegular(pObj2)->fPhase ^ Abc_ObjIsComplement(pObj2) );
            eNode3 = Dec_EdgeCreate( 3, Abc_ObjRegular(pObj3)->fPhase ^ Abc_ObjIsComplement(pObj3) );
            ePrev1 = Dec_GraphAddNodeAnd( pGraph, eNode2, eNode3 );
        }
    }
    else
    {
        eNode0 = Dec_EdgeCreate( 0, Abc_ObjRegular(pObj0)->fPhase ^ Abc_ObjIsComplement(pObj0) );
        eNode1 = Dec_EdgeCreate( 1, Abc_ObjRegular(pObj1)->fPhase ^ Abc_ObjIsComplement(pObj1) );
        ePrev0 = Dec_GraphAddNodeAnd( pGraph, eNode0, eNode1 );
        if ( Abc_ObjIsComplement(pObj2) && Abc_ObjIsComplement(pObj3) )
        {
            eNode2 = Dec_EdgeCreate( 2, Abc_ObjRegular(pObj2)->fPhase );
            eNode3 = Dec_EdgeCreate( 3, Abc_ObjRegular(pObj3)->fPhase );
            ePrev1 = Dec_GraphAddNodeOr( pGraph, eNode2, eNode3 );
        }
        else
        {
            eNode2 = Dec_EdgeCreate( 2, Abc_ObjRegular(pObj2)->fPhase ^ Abc_ObjIsComplement(pObj2) );
            eNode3 = Dec_EdgeCreate( 3, Abc_ObjRegular(pObj3)->fPhase ^ Abc_ObjIsComplement(pObj3) );
            ePrev1 = Dec_GraphAddNodeAnd( pGraph, eNode2, eNode3 );
        }
    }
    if ( fOrGate )
        eRoot = Dec_GraphAddNodeOr( pGraph, ePrev0, ePrev1 );
    else
        eRoot = Dec_GraphAddNodeAnd( pGraph, ePrev0, ePrev1 );
    Dec_GraphSetRoot( pGraph, eRoot );
    if ( pRoot->fPhase )
        Dec_GraphComplement( pGraph );
    return pGraph;
}

/**Function*************************************************************

  Synopsis    [Derives single-node unate/binate divisors.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_ManResubDivsS( Abc_ManRes_t * p )
{
    int fMoreDivs = 1; // bug fix by Siang-Yun Lee
    Abc_Obj_t * pObj;
    unsigned * puData, * puDataR;
    int i, w;
    Vec_PtrClear( p->vDivs1UP );
    Vec_PtrClear( p->vDivs1UN );
    Vec_PtrClear( p->vDivs1B );
    puDataR = (unsigned *)p->pRoot->pData;
    Vec_PtrForEachEntryStop( Abc_Obj_t *, p->vDivs, pObj, i, p->nDivs )
    {
        puData = (unsigned *)pObj->pData;
        // check positive containment
        for ( w = 0; w < p->nWords; w++ )
//            if ( puData[w] & ~puDataR[w] )
            if ( puData[w] & ~puDataR[w] & p->pCareSet[w] ) // care set
                break;
        if ( w == p->nWords )
        {
            Vec_PtrPush( p->vDivs1UP, pObj );
            continue;
        }
        if ( fMoreDivs )
        {
            for ( w = 0; w < p->nWords; w++ )
    //            if ( ~puData[w] & ~puDataR[w] )
                if ( ~puData[w] & ~puDataR[w] & p->pCareSet[w] ) // care set
                    break;
            if ( w == p->nWords )
            {
                Vec_PtrPush( p->vDivs1UP, Abc_ObjNot(pObj) );
                continue;
            }
        }
        // check negative containment
        for ( w = 0; w < p->nWords; w++ )
//            if ( ~puData[w] & puDataR[w] )
            if ( ~puData[w] & puDataR[w] & p->pCareSet[w] ) // care set
                break;
        if ( w == p->nWords )
        {
            Vec_PtrPush( p->vDivs1UN, pObj );
            continue;
        }
        if ( fMoreDivs )
        {
            for ( w = 0; w < p->nWords; w++ )
    //            if ( puData[w] & puDataR[w] )
                if ( puData[w] & puDataR[w] & p->pCareSet[w] ) // care set
                    break;
            if ( w == p->nWords )
            {
                Vec_PtrPush( p->vDivs1UN, Abc_ObjNot(pObj) );
                continue;
            }
        }
        // add the node to binates
        Vec_PtrPush( p->vDivs1B, pObj );
    }
}

/**Function*************************************************************

  Synopsis    [Derives double-node unate/binate divisors.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_ManResubDivsD( Abc_ManRes_t * p )
{
    Abc_Obj_t * pObj0, * pObj1;
    unsigned * puData0, * puData1, * puDataR;
    int i, k, w;
    Vec_PtrClear( p->vDivs2UP0 );
    Vec_PtrClear( p->vDivs2UP1 );
    Vec_PtrClear( p->vDivs2UN0 );
    Vec_PtrClear( p->vDivs2UN1 );
    puDataR = (unsigned *)p->pRoot->pData;
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs1B, pObj0, i )
    {
        puData0 = (unsigned *)pObj0->pData;
        Vec_PtrForEachEntryStart( Abc_Obj_t *, p->vDivs1B, pObj1, k, i + 1 )
        {
            puData1 = (unsigned *)pObj1->pData;

            if ( Vec_PtrSize(p->vDivs2UP0) < ABC_RS_DIV2_MAX )
            {
                // get positive unate divisors
                for ( w = 0; w < p->nWords; w++ )
                    if ( (puData0[w] & puData1[w]) & ~puDataR[w] & p->pCareSet[w] ) // care set
                        break;
                if ( w == p->nWords )
                {
                    Vec_PtrPush( p->vDivs2UP0, pObj0 );
                    Vec_PtrPush( p->vDivs2UP1, pObj1 );
                }
                for ( w = 0; w < p->nWords; w++ )
                    if ( (~puData0[w] & puData1[w]) & ~puDataR[w] & p->pCareSet[w] ) // care set
                        break;
                if ( w == p->nWords )
                {
                    Vec_PtrPush( p->vDivs2UP0, Abc_ObjNot(pObj0) );
                    Vec_PtrPush( p->vDivs2UP1, pObj1 );
                }
                for ( w = 0; w < p->nWords; w++ )
                    if ( (puData0[w] & ~puData1[w]) & ~puDataR[w] & p->pCareSet[w] ) // care set
                        break;
                if ( w == p->nWords )
                {
                    Vec_PtrPush( p->vDivs2UP0, pObj0 );
                    Vec_PtrPush( p->vDivs2UP1, Abc_ObjNot(pObj1) );
                }
                for ( w = 0; w < p->nWords; w++ )
                    if ( (puData0[w] | puData1[w]) & ~puDataR[w] & p->pCareSet[w] ) // care set
                        break;
                if ( w == p->nWords )
                {
                    Vec_PtrPush( p->vDivs2UP0, Abc_ObjNot(pObj0) );
                    Vec_PtrPush( p->vDivs2UP1, Abc_ObjNot(pObj1) );
                }
            }

            if ( Vec_PtrSize(p->vDivs2UN0) < ABC_RS_DIV2_MAX )
            {
                // get negative unate divisors
                for ( w = 0; w < p->nWords; w++ )
                    if ( ~(puData0[w] & puData1[w]) & puDataR[w] & p->pCareSet[w] ) // care set
                        break;
                if ( w == p->nWords )
                {
                    Vec_PtrPush( p->vDivs2UN0, pObj0 );
                    Vec_PtrPush( p->vDivs2UN1, pObj1 );
                }
                for ( w = 0; w < p->nWords; w++ )
                    if ( ~(~puData0[w] & puData1[w]) & puDataR[w] & p->pCareSet[w] ) // care set
                        break;
                if ( w == p->nWords )
                {
                    Vec_PtrPush( p->vDivs2UN0, Abc_ObjNot(pObj0) );
                    Vec_PtrPush( p->vDivs2UN1, pObj1 );
                }
                for ( w = 0; w < p->nWords; w++ )
                    if ( ~(puData0[w] & ~puData1[w]) & puDataR[w] & p->pCareSet[w] ) // care set
                        break;
                if ( w == p->nWords )
                {
                    Vec_PtrPush( p->vDivs2UN0, pObj0 );
                    Vec_PtrPush( p->vDivs2UN1, Abc_ObjNot(pObj1) );
                }
                for ( w = 0; w < p->nWords; w++ )
                    if ( ~(puData0[w] | puData1[w]) & puDataR[w] & p->pCareSet[w] ) // care set
                        break;
                if ( w == p->nWords )
                {
                    Vec_PtrPush( p->vDivs2UN0, Abc_ObjNot(pObj0) );
                    Vec_PtrPush( p->vDivs2UN1, Abc_ObjNot(pObj1) );
                }
            }
        }
    }
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubQuit( Abc_ManRes_t * p )
{
    Dec_Graph_t * pGraph;
    unsigned * upData;
    int w;
    upData = (unsigned *)p->pRoot->pData;
    for ( w = 0; w < p->nWords; w++ )
//        if ( upData[w] )
        if ( upData[w] & p->pCareSet[w] ) // care set
            break;
    if ( w != p->nWords )
        return NULL;
    // get constant node graph
    if ( p->pRoot->fPhase )
        pGraph = Dec_GraphCreateConst1();
    else
        pGraph = Dec_GraphCreateConst0();
    return pGraph;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubDivs0( Abc_ManRes_t * p )
{
    Abc_Obj_t * pObj;
    unsigned * puData, * puDataR;
    int i, w;
    puDataR = (unsigned *)p->pRoot->pData;
    Vec_PtrForEachEntryStop( Abc_Obj_t *, p->vDivs, pObj, i, p->nDivs )
    {
        puData = (unsigned *)pObj->pData;
        for ( w = 0; w < p->nWords; w++ )
//            if ( puData[w] != puDataR[w] )
            if ( (puData[w] ^ puDataR[w]) & p->pCareSet[w] ) // care set
                break;
        if ( w == p->nWords )
            return Abc_ManResubQuit0( p->pRoot, pObj );
    }
    return NULL;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubDivs1( Abc_ManRes_t * p )
{
    Abc_Obj_t * pObj0, * pObj1;
    unsigned * puData0, * puData1, * puDataR;
    int i, k, w;
    puDataR = (unsigned *)p->pRoot->pData;
    // check positive unate divisors
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs1UP, pObj0, i )
    {
        puData0 = (unsigned *)Abc_ObjRegular(pObj0)->pData;
        Vec_PtrForEachEntryStart( Abc_Obj_t *, p->vDivs1UP, pObj1, k, i + 1 )
        {
            puData1 = (unsigned *)Abc_ObjRegular(pObj1)->pData;
            if ( Abc_ObjIsComplement(pObj0) && Abc_ObjIsComplement(pObj1) )
            {
                for ( w = 0; w < p->nWords; w++ )
                    if ( ((~puData0[w] | ~puData1[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
            }
            else if ( Abc_ObjIsComplement(pObj0) )
            {
                for ( w = 0; w < p->nWords; w++ )
                    if ( ((~puData0[w] | puData1[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
            }
            else if ( Abc_ObjIsComplement(pObj1) )
            {
                for ( w = 0; w < p->nWords; w++ )
                    if ( ((puData0[w] | ~puData1[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
            }
            else
            {
                for ( w = 0; w < p->nWords; w++ )
                    if ( ((puData0[w] | puData1[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
            }
            if ( w == p->nWords )
            {
                p->nUsedNode1Or++;
                return Abc_ManResubQuit1( p->pRoot, pObj0, pObj1, 1 );
            }
        }
    }
    // check negative unate divisors
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs1UN, pObj0, i )
    {
        puData0 = (unsigned *)Abc_ObjRegular(pObj0)->pData;
        Vec_PtrForEachEntryStart( Abc_Obj_t *, p->vDivs1UN, pObj1, k, i + 1 )
        {
            puData1 = (unsigned *)Abc_ObjRegular(pObj1)->pData;
            if ( Abc_ObjIsComplement(pObj0) && Abc_ObjIsComplement(pObj1) )
            {
                for ( w = 0; w < p->nWords; w++ )
                    if ( ((~puData0[w] & ~puData1[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
            }
            if ( Abc_ObjIsComplement(pObj0) )
            {
                for ( w = 0; w < p->nWords; w++ )
                    if ( ((~puData0[w] & puData1[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
            }
            if ( Abc_ObjIsComplement(pObj1) )
            {
                for ( w = 0; w < p->nWords; w++ )
                    if ( ((puData0[w] & ~puData1[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
            }
            else
            {
                for ( w = 0; w < p->nWords; w++ )
                    if ( ((puData0[w] & puData1[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
            }
            if ( w == p->nWords )
            {
                p->nUsedNode1And++;
                return Abc_ManResubQuit1( p->pRoot, pObj0, pObj1, 0 );
            }
        }
    }
    return NULL;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubDivs12( Abc_ManRes_t * p )
{
    Abc_Obj_t * pObj0, * pObj1, * pObj2, * pObjMax, * pObjMin0 = NULL, * pObjMin1 = NULL;
    unsigned * puData0, * puData1, * puData2, * puDataR;
    int i, k, j, w, LevelMax;
    puDataR = (unsigned *)p->pRoot->pData;
    // check positive unate divisors
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs1UP, pObj0, i )
    {
        puData0 = (unsigned *)Abc_ObjRegular(pObj0)->pData;
        Vec_PtrForEachEntryStart( Abc_Obj_t *, p->vDivs1UP, pObj1, k, i + 1 )
        {
            puData1 = (unsigned *)Abc_ObjRegular(pObj1)->pData;
            Vec_PtrForEachEntryStart( Abc_Obj_t *, p->vDivs1UP, pObj2, j, k + 1 )
            {
                puData2 = (unsigned *)Abc_ObjRegular(pObj2)->pData;
                if ( Abc_ObjIsComplement(pObj0) && Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] | ~puData1[w] | ~puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj0) && Abc_ObjIsComplement(pObj1) && !Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] | ~puData1[w] | puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj0) && !Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] | puData1[w] | ~puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj0) && !Abc_ObjIsComplement(pObj1) && !Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] | puData1[w] | puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( !Abc_ObjIsComplement(pObj0) && Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] | ~puData1[w] | ~puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( !Abc_ObjIsComplement(pObj0) && Abc_ObjIsComplement(pObj1) && !Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] | ~puData1[w] | puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( !Abc_ObjIsComplement(pObj0) && !Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] | puData1[w] | ~puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( !Abc_ObjIsComplement(pObj0) && !Abc_ObjIsComplement(pObj1) && !Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] | puData1[w] | puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else assert( 0 );
                if ( w == p->nWords )
                {
                    LevelMax = Abc_MaxInt( Abc_ObjRegular(pObj0)->Level, Abc_MaxInt(Abc_ObjRegular(pObj1)->Level, Abc_ObjRegular(pObj2)->Level) );

                    pObjMax = NULL;
                    if ( (int)Abc_ObjRegular(pObj0)->Level == LevelMax )
                        pObjMax = pObj0, pObjMin0 = pObj1, pObjMin1 = pObj2;
                    if ( (int)Abc_ObjRegular(pObj1)->Level == LevelMax )
                    {
                        if ( pObjMax ) continue;
                        pObjMax = pObj1, pObjMin0 = pObj0, pObjMin1 = pObj2;
                    }
                    if ( (int)Abc_ObjRegular(pObj2)->Level == LevelMax )
                    {
                        if ( pObjMax ) continue;
                        pObjMax = pObj2, pObjMin0 = pObj0, pObjMin1 = pObj1;
                    }

                    p->nUsedNode2Or++;
                    assert(pObjMin0);
                    assert(pObjMin1);
                    return Abc_ManResubQuit21( p->pRoot, pObjMin0, pObjMin1, pObjMax, 1 );
                }
            }
        }
    }
    // check negative unate divisors
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs1UN, pObj0, i )
    {
        puData0 = (unsigned *)Abc_ObjRegular(pObj0)->pData;
        Vec_PtrForEachEntryStart( Abc_Obj_t *, p->vDivs1UN, pObj1, k, i + 1 )
        {
            puData1 = (unsigned *)Abc_ObjRegular(pObj1)->pData;
            Vec_PtrForEachEntryStart( Abc_Obj_t *, p->vDivs1UN, pObj2, j, k + 1 )
            {
                puData2 = (unsigned *)Abc_ObjRegular(pObj2)->pData;
                if ( Abc_ObjIsComplement(pObj0) && Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] & ~puData1[w] & ~puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj0) && Abc_ObjIsComplement(pObj1) && !Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] & ~puData1[w] & puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj0) && !Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] & puData1[w] & ~puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj0) && !Abc_ObjIsComplement(pObj1) && !Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] & puData1[w] & puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( !Abc_ObjIsComplement(pObj0) && Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] & ~puData1[w] & ~puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( !Abc_ObjIsComplement(pObj0) && Abc_ObjIsComplement(pObj1) && !Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] & ~puData1[w] & puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( !Abc_ObjIsComplement(pObj0) && !Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] & puData1[w] & ~puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( !Abc_ObjIsComplement(pObj0) && !Abc_ObjIsComplement(pObj1) && !Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] & puData1[w] & puData2[w]) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else assert( 0 );
                if ( w == p->nWords )
                {
                    LevelMax = Abc_MaxInt( Abc_ObjRegular(pObj0)->Level, Abc_MaxInt(Abc_ObjRegular(pObj1)->Level, Abc_ObjRegular(pObj2)->Level) );

                    pObjMax = NULL;
                    if ( (int)Abc_ObjRegular(pObj0)->Level == LevelMax )
                        pObjMax = pObj0, pObjMin0 = pObj1, pObjMin1 = pObj2;
                    if ( (int)Abc_ObjRegular(pObj1)->Level == LevelMax )
                    {
                        if ( pObjMax ) continue;
                        pObjMax = pObj1, pObjMin0 = pObj0, pObjMin1 = pObj2;
                    }
                    if ( (int)Abc_ObjRegular(pObj2)->Level == LevelMax )
                    {
                        if ( pObjMax ) continue;
                        pObjMax = pObj2, pObjMin0 = pObj0, pObjMin1 = pObj1;
                    }

                    p->nUsedNode2And++;
                    assert(pObjMin0);
                    assert(pObjMin1);
                    return Abc_ManResubQuit21( p->pRoot, pObjMin0, pObjMin1, pObjMax, 0 );
                }
            }
        }
    }
    return NULL;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubDivs2( Abc_ManRes_t * p )
{
    Abc_Obj_t * pObj0, * pObj1, * pObj2;
    unsigned * puData0, * puData1, * puData2, * puDataR;
    int i, k, w;
    puDataR = (unsigned *)p->pRoot->pData;
    // check positive unate divisors
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs1UP, pObj0, i )
    {
        puData0 = (unsigned *)Abc_ObjRegular(pObj0)->pData;
        Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs2UP0, pObj1, k )
        {
            pObj2 = (Abc_Obj_t *)Vec_PtrEntry( p->vDivs2UP1, k );

            puData1 = (unsigned *)Abc_ObjRegular(pObj1)->pData;
            puData2 = (unsigned *)Abc_ObjRegular(pObj2)->pData;
            if ( Abc_ObjIsComplement(pObj0) )
            {
                if ( Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] | (puData1[w] | puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj1) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] | (~puData1[w] & puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] | (puData1[w] & ~puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] | (puData1[w] & puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
            }
            else
            {
                if ( Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] | (puData1[w] | puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj1) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] | (~puData1[w] & puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] | (puData1[w] & ~puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] | (puData1[w] & puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
            }
            if ( w == p->nWords )
            {
                p->nUsedNode2OrAnd++;
                return Abc_ManResubQuit2( p->pRoot, pObj0, pObj1, pObj2, 1 );
            }
        }
    }
    // check negative unate divisors
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs1UN, pObj0, i )
    {
        puData0 = (unsigned *)Abc_ObjRegular(pObj0)->pData;
        Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs2UN0, pObj1, k )
        {
            pObj2 = (Abc_Obj_t *)Vec_PtrEntry( p->vDivs2UN1, k );

            puData1 = (unsigned *)Abc_ObjRegular(pObj1)->pData;
            puData2 = (unsigned *)Abc_ObjRegular(pObj2)->pData;
            if ( Abc_ObjIsComplement(pObj0) )
            {
                if ( Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] & (puData1[w] | puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj1) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] & (~puData1[w] & puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] & (puData1[w] & ~puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((~puData0[w] & (puData1[w] & puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
            }
            else
            {
                if ( Abc_ObjIsComplement(pObj1) && Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] & (puData1[w] | puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj1) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] & (~puData1[w] & puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else if ( Abc_ObjIsComplement(pObj2) )
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] & (puData1[w] & ~puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
                else
                {
                    for ( w = 0; w < p->nWords; w++ )
                        if ( ((puData0[w] & (puData1[w] & puData2[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                            break;
                }
            }
            if ( w == p->nWords )
            {
                p->nUsedNode2AndOr++;
                return Abc_ManResubQuit2( p->pRoot, pObj0, pObj1, pObj2, 0 );
            }
        }
    }
    return NULL;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubDivs3( Abc_ManRes_t * p )
{
    Abc_Obj_t * pObj0, * pObj1, * pObj2, * pObj3;
    unsigned * puData0, * puData1, * puData2, * puData3, * puDataR;
    int i, k, w = 0, Flag;
    puDataR = (unsigned *)p->pRoot->pData;
    // check positive unate divisors
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs2UP0, pObj0, i )
    {
        pObj1 = (Abc_Obj_t *)Vec_PtrEntry( p->vDivs2UP1, i );
        puData0 = (unsigned *)Abc_ObjRegular(pObj0)->pData;
        puData1 = (unsigned *)Abc_ObjRegular(pObj1)->pData;
        Flag = (Abc_ObjIsComplement(pObj0) << 3) | (Abc_ObjIsComplement(pObj1) << 2);

        Vec_PtrForEachEntryStart( Abc_Obj_t *, p->vDivs2UP0, pObj2, k, i + 1 )
        {
            pObj3 = (Abc_Obj_t *)Vec_PtrEntry( p->vDivs2UP1, k );
            puData2 = (unsigned *)Abc_ObjRegular(pObj2)->pData;
            puData3 = (unsigned *)Abc_ObjRegular(pObj3)->pData;

            Flag = (Flag & 12) | ((int)Abc_ObjIsComplement(pObj2) << 1) | (int)Abc_ObjIsComplement(pObj3);
            assert( Flag < 16 );
            switch( Flag )
            {
            case 0: // 0000
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] & puData1[w]) | (puData2[w] & puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;
            case 1: // 0001
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] & puData1[w]) | (puData2[w] & ~puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;
            case 2: // 0010
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] & puData1[w]) | (~puData2[w] & puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;
            case 3: // 0011
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] & puData1[w]) | (puData2[w] | puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;

            case 4: // 0100
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] & ~puData1[w]) | (puData2[w] & puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;
            case 5: // 0101
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] & ~puData1[w]) | (puData2[w] & ~puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;
            case 6: // 0110
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] & ~puData1[w]) | (~puData2[w] & puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;
            case 7: // 0111
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] & ~puData1[w]) | (puData2[w] | puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;

            case 8: // 1000
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((~puData0[w] & puData1[w]) | (puData2[w] & puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;
            case 9: // 1001
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((~puData0[w] & puData1[w]) | (puData2[w] & ~puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;
            case 10: // 1010
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((~puData0[w] & puData1[w]) | (~puData2[w] & puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;
            case 11: // 1011
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((~puData0[w] & puData1[w]) | (puData2[w] | puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;

            case 12: // 1100
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] | puData1[w]) | (puData2[w] & puData3[w])) ^ puDataR[w]) & p->pCareSet[w] ) // care set
                        break;
                break;
            case 13: // 1101
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] | puData1[w]) | (puData2[w] & ~puData3[w])) ^ puDataR[w]) & p->pCareSet[w] )
                        break;
                break;
            case 14: // 1110
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] | puData1[w]) | (~puData2[w] & puData3[w])) ^ puDataR[w]) & p->pCareSet[w] )
                        break;
                break;
            case 15: // 1111
                for ( w = 0; w < p->nWords; w++ )
                    if ( (((puData0[w] | puData1[w]) | (puData2[w] | puData3[w])) ^ puDataR[w]) & p->pCareSet[w] )
                        break;
                break;

            }
            if ( w == p->nWords )
            {
                p->nUsedNode3OrAnd++;
                return Abc_ManResubQuit3( p->pRoot, pObj0, pObj1, pObj2, pObj3, 1 );
            }
        }
    }

    return NULL;
}

/**Function*************************************************************

  Synopsis    [Evaluates resubstution of one cut.]

  Description [Returns the graph to add if any.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_ManResubEval( Abc_ManRes_t * p, Abc_Obj_t * pRoot, Vec_Ptr_t * vLeaves, int nSteps )
{
    extern int Abc_NodeMffcInside( Abc_Obj_t * pNode, Vec_Ptr_t * vLeaves, Vec_Ptr_t * vInside );
    Dec_Graph_t * pGraph;

    assert( nSteps >= 0 );
    assert( nSteps <= 3 );
    p->pRoot = pRoot;
    p->nLeaves = Vec_PtrSize(vLeaves);
    p->nLastGain = -1;

    // collect the MFFC
    p->nMffc = Abc_NodeMffcInside( pRoot, vLeaves, p->vTemp );
    assert( p->nMffc > 0 );

    // collect the divisor nodes
    if ( !Abc_ManResubCollectDivs( p, pRoot, vLeaves ) )
        return NULL;

    p->nTotalDivs   += p->nDivs;
    p->nTotalLeaves += p->nLeaves;

    // simulate the nodes
    Abc_ManResubSimulate( p->vDivs, p->nLeaves, p->vSims, p->nLeavesMax, p->nWords );

    // consider constants
    if ( (pGraph = Abc_ManResubQuit( p )) )
    {
        p->nUsedNodeC++;
        p->nLastGain = p->nMffc;
        return pGraph;
    }

    // consider equal nodes
    if ( (pGraph = Abc_ManResubDivs0( p )) )
    {
        p->nUsedNode0++;
        p->nLastGain = p->nMffc;
        return pGraph;
    }
    if ( nSteps == 0 || p->nMffc == 1 )
    {
        return NULL;
    }

    // get the one level divisors
    Abc_ManResubDivsS( p );

    // consider one node
    if ( (pGraph = Abc_ManResubDivs1( p )) )
    {
        p->nLastGain = p->nMffc - 1;
        return pGraph;
    }
    if ( nSteps == 1 || p->nMffc == 2 )
        return NULL;

    // consider triples
    if ( (pGraph = Abc_ManResubDivs12( p )) )
    {
        p->nLastGain = p->nMffc - 2;
        return pGraph;
    }

    // get the two level divisors
    Abc_ManResubDivsD( p );

    // consider two nodes
    if ( (pGraph = Abc_ManResubDivs2( p )) )
    {
        p->nLastGain = p->nMffc - 2;
        return pGraph;
    }
    if ( nSteps == 2 || p->nMffc == 3 )
        return NULL;

    // consider two nodes
    if ( (pGraph = Abc_ManResubDivs3( p )) )
    {
        p->nLastGain = p->nMffc - 3;
        return pGraph;
    }
    if ( nSteps == 3 || p->nLeavesMax == 4 )
        return NULL;
    return NULL;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
