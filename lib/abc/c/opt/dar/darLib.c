/**CFile****************************************************************

  FileName    [darLib.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [DAG-aware AIG rewriting.]

  Synopsis    [Library of AIG subgraphs used for rewriting.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: darLib.c,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#include "darInt.h"
#include "dar.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static inline Dar_LibObj_t * Dar_LibObj( Dar_Lib_t * p, int Id )    { return p->pObjs + Id; }
static inline int            Dar_LibObjTruth( Dar_LibObj_t * pObj ) { return pObj->Num < (0xFFFF & ~pObj->Num) ? pObj->Num : (0xFFFF & ~pObj->Num); }

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Starts the library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dar_Lib_t * Dar_LibAlloc( int nObjs )
{
    unsigned uTruths[4] = { 0xAAAA, 0xCCCC, 0xF0F0, 0xFF00 };
    Dar_Lib_t * p;
    int i;
    p = ABC_ALLOC( Dar_Lib_t, 1 );
    memset( p, 0, sizeof(Dar_Lib_t) );
    // allocate objects
    p->nObjs = nObjs;
    p->pObjs = ABC_ALLOC( Dar_LibObj_t, nObjs );
    memset( p->pObjs, 0, sizeof(Dar_LibObj_t) * nObjs );
    // allocate canonical data
    p->pPerms4 = Dar_Permutations( 4 );
    Dar_Truth4VarNPN( &p->puCanons, &p->pPhases, &p->pPerms, &p->pMap );
    // start the elementary objects
    p->iObj = 4;
    for ( i = 0; i < 4; i++ )
    {
        p->pObjs[i].fTerm = 1;
        p->pObjs[i].Num = uTruths[i];
    }
    return p;
}

/**Function*************************************************************

  Synopsis    [Frees the library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_LibFree( Dar_Lib_t * p )
{
    ABC_FREE( p->pObjs );
    ABC_FREE( p->pDatas );
    ABC_FREE( p->pNodesMem );
    ABC_FREE( p->pNodes0Mem );
    ABC_FREE( p->pSubgrMem );
    ABC_FREE( p->pSubgr0Mem );
    ABC_FREE( p->pPriosMem );
    ABC_FREE( p->pPlaceMem );
    ABC_FREE( p->pScoreMem );
    ABC_FREE( p->pPerms4 );
    ABC_FREE( p->puCanons );
    ABC_FREE( p->pPhases );
    ABC_FREE( p->pPerms );
    ABC_FREE( p->pMap );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Adds one AND to the library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_LibAddNode( Dar_Lib_t * p, int Id0, int Id1, int fCompl0, int fCompl1 )
{
    Dar_LibObj_t * pFan0 = Dar_LibObj( p, Id0 );
    Dar_LibObj_t * pFan1 = Dar_LibObj( p, Id1 );
    Dar_LibObj_t * pObj  = p->pObjs + p->iObj++;
    pObj->Fan0 = Id0;
    pObj->Fan1 = Id1;
    pObj->fCompl0 = fCompl0;
    pObj->fCompl1 = fCompl1;
    pObj->fPhase = (fCompl0 ^ pFan0->fPhase) & (fCompl1 ^ pFan1->fPhase);
    pObj->Num = 0xFFFF & (fCompl0? ~pFan0->Num : pFan0->Num) & (fCompl1? ~pFan1->Num : pFan1->Num);
}

/**Function*************************************************************

  Synopsis    [Adds one AND to the library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_LibSetup_rec( Dar_Lib_t * p, Dar_LibObj_t * pObj, int Class, int fCollect )
{
    if ( pObj->fTerm || (int)pObj->Num == Class )
        return;
    pObj->Num = Class;
    Dar_LibSetup_rec( p, Dar_LibObj(p, pObj->Fan0), Class, fCollect );
    Dar_LibSetup_rec( p, Dar_LibObj(p, pObj->Fan1), Class, fCollect );
    if ( fCollect )
        p->pNodes[Class][ p->nNodes[Class]++ ] = pObj-p->pObjs;
    else
        p->nNodes[Class]++;
}

/**Function*************************************************************

  Synopsis    [Adds one AND to the library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_LibSetup( Dar_Lib_t * p, Vec_Int_t * vOuts, Vec_Int_t * vPrios )
{
    Dar_LibObj_t * pObj;
    int nNodesTotal, uTruth, Class, Out, i, k;
    assert( p->iObj == p->nObjs );

    // count the number of representatives of each class
    for ( i = 0; i < 222; i++ )
        p->nSubgr[i] = p->nNodes[i] = 0;
    Vec_IntForEachEntry( vOuts, Out, i )
    {
        pObj = Dar_LibObj( p, Out );
        uTruth = Dar_LibObjTruth( pObj );
        Class = p->pMap[uTruth];
        p->nSubgr[Class]++;
    }
    // allocate memory for the roots of each class
    p->pSubgrMem = ABC_ALLOC( int, Vec_IntSize(vOuts) );
    p->pSubgr0Mem = ABC_ALLOC( int, Vec_IntSize(vOuts) );
    p->nSubgrTotal = 0;
    for ( i = 0; i < 222; i++ )
    {
        p->pSubgr[i] = p->pSubgrMem + p->nSubgrTotal;
        p->pSubgr0[i] = p->pSubgr0Mem + p->nSubgrTotal;
        p->nSubgrTotal += p->nSubgr[i];
        p->nSubgr[i] = 0;
    }
    assert( p->nSubgrTotal == Vec_IntSize(vOuts) );
    // add the outputs to storage
    Vec_IntForEachEntry( vOuts, Out, i )
    {
        pObj = Dar_LibObj( p, Out );
        uTruth = Dar_LibObjTruth( pObj );
        Class = p->pMap[uTruth];
        p->pSubgr[Class][ p->nSubgr[Class]++ ] = Out;
    }

    int Counter = 0;
    // allocate memory for the priority of roots of each class
    p->pPriosMem = ABC_ALLOC( int, Vec_IntSize(vOuts) );
    p->nSubgrTotal = 0;
    for ( i = 0; i < 222; i++ )
    {
        p->pPrios[i] = p->pPriosMem + p->nSubgrTotal;
        p->nSubgrTotal += p->nSubgr[i];
        for ( k = 0; k < p->nSubgr[i]; k++ )
            p->pPrios[i][k] = Vec_IntEntry(vPrios, Counter++);

    }
    assert( p->nSubgrTotal == Vec_IntSize(vOuts) );
    assert( Counter == Vec_IntSize(vPrios) );

    // create traversal IDs
    for ( i = 0; i < p->iObj; i++ )
        Dar_LibObj(p, i)->Num = 0xff;
    // count nodes in each class
    for ( i = 0; i < 222; i++ )
        for ( k = 0; k < p->nSubgr[i]; k++ )
            Dar_LibSetup_rec( p, Dar_LibObj(p, p->pSubgr[i][k]), i, 0 );
    // count the total number of nodes
    p->nNodesTotal = 0;
    for ( i = 0; i < 222; i++ )
        p->nNodesTotal += p->nNodes[i];
    // allocate memory for the nodes of each class
    p->pNodesMem = ABC_ALLOC( int, p->nNodesTotal );
    p->pNodes0Mem = ABC_ALLOC( int, p->nNodesTotal );
    p->nNodesTotal = 0;
    for ( i = 0; i < 222; i++ )
    {
        p->pNodes[i] = p->pNodesMem + p->nNodesTotal;
        p->pNodes0[i] = p->pNodes0Mem + p->nNodesTotal;
        p->nNodesTotal += p->nNodes[i];
        p->nNodes[i] = 0;
    }
    // create traversal IDs
    for ( i = 0; i < p->iObj; i++ )
        Dar_LibObj(p, i)->Num = 0xff;
    // add the nodes to storage
    nNodesTotal = 0;
    for ( i = 0; i < 222; i++ )
    {
         for ( k = 0; k < p->nSubgr[i]; k++ )
            Dar_LibSetup_rec( p, Dar_LibObj(p, p->pSubgr[i][k]), i, 1 );
         nNodesTotal += p->nNodes[i];
//printf( "Class %3d : Subgraphs = %4d. Nodes = %5d.\n", i, p->nSubgr[i], p->nNodes[i] );
    }
    assert( nNodesTotal == p->nNodesTotal );
     // prepare the number of the PI nodes
    for ( i = 0; i < 4; i++ )
        Dar_LibObj(p, i)->Num = i;
}

/**Function*************************************************************

  Synopsis    [Starts the library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_LibCreateData( Dar_Lib_t * p, int nDatas )
{
    if ( p->nDatas == nDatas )
        return;
    ABC_FREE( p->pDatas );
    // allocate datas
    p->nDatas = nDatas;
    p->pDatas = ABC_ALLOC( Dar_LibDat_t, nDatas );
    memset( p->pDatas, 0, sizeof(Dar_LibDat_t) * nDatas );
}

/**Function*************************************************************

  Synopsis    [Adds one AND to the library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_LibSetup0_rec( Dar_Lib_t * p, Dar_LibObj_t * pObj, int Class, int fCollect )
{
    if ( pObj->fTerm || (int)pObj->Num == Class )
        return;
    pObj->Num = Class;
    Dar_LibSetup0_rec( p, Dar_LibObj(p, pObj->Fan0), Class, fCollect );
    Dar_LibSetup0_rec( p, Dar_LibObj(p, pObj->Fan1), Class, fCollect );
    if ( fCollect )
        p->pNodes0[Class][ p->nNodes0[Class]++ ] = pObj-p->pObjs;
    else
        p->nNodes0[Class]++;
}

/**Function*************************************************************

  Synopsis    [Starts the library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_LibPrepare( Dar_Lib_t * p, int nSubgraphs )
{
    int i, k, nNodes0Total;
    if ( p->nSubgraphs == nSubgraphs )
        return;

    // favor special classes:
    //  1 : F = (!d*!c*!b*!a)
    //  4 : F = (!d*!c*!(b*a))
    // 12 : F = (!d*!(c*!(!b*!a)))
    // 20 : F = (!d*!(c*b*a))

    // set the subgraph counters
    p->nSubgr0Total = 0;
    for ( i = 0; i < 222; i++ )
    {
//        if ( i == 1 || i == 4 || i == 12 || i == 20 ) // special classes
        if ( i == 1 ) // special classes
            p->nSubgr0[i] = p->nSubgr[i];
        else
            p->nSubgr0[i] = Abc_MinInt( p->nSubgr[i], nSubgraphs );
        p->nSubgr0Total += p->nSubgr0[i];
        for ( k = 0; k < p->nSubgr0[i]; k++ )
            p->pSubgr0[i][k] = p->pSubgr[i][ p->pPrios[i][k] ];
    }

    // count the number of nodes
    // clean node counters
    for ( i = 0; i < 222; i++ )
        p->nNodes0[i] = 0;
    // create traversal IDs
    for ( i = 0; i < p->iObj; i++ )
        Dar_LibObj(p, i)->Num = 0xff;
    // count nodes in each class
    // count the total number of nodes and the largest class
    p->nNodes0Total = 0;
    p->nNodes0Max = 0;
    for ( i = 0; i < 222; i++ )
    {
        for ( k = 0; k < p->nSubgr0[i]; k++ )
            Dar_LibSetup0_rec( p, Dar_LibObj(p, p->pSubgr0[i][k]), i, 0 );
        p->nNodes0Total += p->nNodes0[i];
        p->nNodes0Max = Abc_MaxInt( p->nNodes0Max, p->nNodes0[i] );
    }

    // clean node counters
    for ( i = 0; i < 222; i++ )
        p->nNodes0[i] = 0;
    // create traversal IDs
    for ( i = 0; i < p->iObj; i++ )
        Dar_LibObj(p, i)->Num = 0xff;
    // add the nodes to storage
    nNodes0Total = 0;
    for ( i = 0; i < 222; i++ )
    {
        for ( k = 0; k < p->nSubgr0[i]; k++ )
            Dar_LibSetup0_rec( p, Dar_LibObj(p, p->pSubgr0[i][k]), i, 1 );
         nNodes0Total += p->nNodes0[i];
    }
    assert( nNodes0Total == p->nNodes0Total );
     // prepare the number of the PI nodes
    for ( i = 0; i < 4; i++ )
        Dar_LibObj(p, i)->Num = i;

    // realloc the datas
    Dar_LibCreateData( p, p->nNodes0Max + 32 );
    // allocated more because Dar_LibBuildBest() sometimes requires more entries
}

/**Function*************************************************************

  Synopsis    [Reads library from array.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dar_Lib_t * Dar_LibRead()
{
    Vec_Int_t * vObjs, * vOuts, * vPrios;
    Dar_Lib_t * p;
    int i;
    // read nodes and outputs
    vObjs  = Dar_LibReadNodes();
    vOuts  = Dar_LibReadOuts();
    vPrios = Dar_LibReadPrios();
    // create library
    p = Dar_LibAlloc( Vec_IntSize(vObjs)/2 + 4 );
    // create nodes
    for ( i = 0; i < vObjs->nSize; i += 2 )
        Dar_LibAddNode( p, vObjs->pArray[i] >> 1, vObjs->pArray[i+1] >> 1,
            vObjs->pArray[i] & 1, vObjs->pArray[i+1] & 1 );
    // create outputs
    Dar_LibSetup( p, vOuts, vPrios );
    Vec_IntFree( vObjs );
    Vec_IntFree( vOuts );
    Vec_IntFree( vPrios );
    return p;
}

/**Function*************************************************************

  Synopsis    [Starts the library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dar_Lib_t * Dar_LibStart()
{
    Dar_Lib_t * pDarLib = Dar_LibRead();
    return pDarLib;
}

/**Function*************************************************************

  Synopsis    [Stops the library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_LibStop( Dar_Lib_t * pDarLib )
{
    assert( pDarLib != NULL );
    Dar_LibFree( pDarLib );
}

/**Function*************************************************************

  Synopsis    [Matches the cut with its canonical form.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Dar_LibCutMatch( Dar_Man_t * p, Dar_Cut_t * pCut )
{
    Aig_Obj_t * pFanin;
    unsigned uPhase;
    char * pPerm;
    int i;
    assert( pCut->nLeaves == 4 );
    // get the fanin permutation
    uPhase = p->pDarLib->pPhases[pCut->uTruth];
    pPerm = p->pDarLib->pPerms4[ (int)p->pDarLib->pPerms[pCut->uTruth] ];
    // collect fanins with the corresponding permutation/phase
    for ( i = 0; i < (int)pCut->nLeaves; i++ )
    {
        pFanin = Aig_ManObj( p->pAig, pCut->pLeaves[ (int)pPerm[i] ] );
        if ( pFanin == NULL )
        {
            p->nCutsBad++;
            return 0;
        }
        pFanin = Aig_NotCond(pFanin, ((uPhase >> i) & 1) );
        p->pDarLib->pDatas[i].pFunc = pFanin;
        p->pDarLib->pDatas[i].Level = Aig_Regular(pFanin)->Level;
    }
    p->nCutsGood++;
    return 1;
}

/**Function*************************************************************

  Synopsis    [Marks the MFFC of the node.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Dar_LibCutMarkMffc( Dar_Lib_t * pDarLib, Aig_Man_t * p, Aig_Obj_t * pRoot, int nLeaves )
{
    int i, nNodes;
    // mark the cut leaves
    for ( i = 0; i < nLeaves; i++ )
        Aig_Regular(pDarLib->pDatas[i].pFunc)->nRefs++;
    // label MFFC with current ID
    nNodes = Aig_NodeMffcLabel( p, pRoot );
    // unmark the cut leaves
    for ( i = 0; i < nLeaves; i++ )
        Aig_Regular(pDarLib->pDatas[i].pFunc)->nRefs--;
    return nNodes;
}

/**Function*************************************************************

  Synopsis    [Assigns numbers to the nodes of one class.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_LibEvalAssignNums( Dar_Man_t * p, int Class, Aig_Obj_t * pRoot )
{
    Dar_LibObj_t * pObj;
    Dar_LibDat_t * pData, * pData0, * pData1;
    Aig_Obj_t * pFanin0, * pFanin1;
    int i;
    for ( i = 0; i < p->pDarLib->nNodes0[Class]; i++ )
    {
        // get one class node, assign its temporary number and set its data
        pObj = Dar_LibObj(p->pDarLib, p->pDarLib->pNodes0[Class][i]);
        pObj->Num = 4 + i;
        assert( (int)pObj->Num < p->pDarLib->nNodes0Max + 4 );
        pData = p->pDarLib->pDatas + pObj->Num;
        pData->fMffc = 0;
        pData->pFunc = NULL;
        pData->TravId = 0xFFFF;

        // explore the fanins
        assert( (int)Dar_LibObj(p->pDarLib, pObj->Fan0)->Num < p->pDarLib->nNodes0Max + 4 );
        assert( (int)Dar_LibObj(p->pDarLib, pObj->Fan1)->Num < p->pDarLib->nNodes0Max + 4 );
        pData0 = p->pDarLib->pDatas + Dar_LibObj(p->pDarLib, pObj->Fan0)->Num;
        pData1 = p->pDarLib->pDatas + Dar_LibObj(p->pDarLib, pObj->Fan1)->Num;
        pData->Level = 1 + Abc_MaxInt(pData0->Level, pData1->Level);
        if ( pData0->pFunc == NULL || pData1->pFunc == NULL )
            continue;
        pFanin0 = Aig_NotCond( pData0->pFunc, pObj->fCompl0 );
        pFanin1 = Aig_NotCond( pData1->pFunc, pObj->fCompl1 );
        if ( Aig_Regular(pFanin0) == pRoot || Aig_Regular(pFanin1) == pRoot )
            continue;
        pData->pFunc = Aig_TableLookupTwo( p->pAig, pFanin0, pFanin1 );
        if ( pData->pFunc )
        {
            // update the level to be more accurate
            pData->Level = Aig_Regular(pData->pFunc)->Level;
            // mark the node if it is part of MFFC
            pData->fMffc = Aig_ObjIsTravIdCurrent(p->pAig, Aig_Regular(pData->pFunc));
        }
    }
}

/**Function*************************************************************

  Synopsis    [Evaluates one cut.]

  Description [Returns the best gain.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Dar_LibEval_rec( Dar_Lib_t * pDarLib, Dar_LibObj_t * pObj, int Out, int nNodesSaved )
{
    Dar_LibDat_t * pData;
    int Area;
    pData = pDarLib->pDatas + pObj->Num;
    if ( pData->TravId == Out )
        return 0;
    pData->TravId = Out;
    if ( pObj->fTerm )
    {
        return 0;
    }
    assert( pObj->Num > 3 );
    if ( pData->pFunc && !pData->fMffc )
    {
        return 0;
    }
    // this is a new node - get a bound on the area of its branches
    nNodesSaved--;
    Area = Dar_LibEval_rec( pDarLib, Dar_LibObj(pDarLib, pObj->Fan0), Out, nNodesSaved );
    if ( Area > nNodesSaved )
        return 0xff;
    Area += Dar_LibEval_rec( pDarLib, Dar_LibObj(pDarLib, pObj->Fan1), Out, nNodesSaved );
    if ( Area > nNodesSaved )
        return 0xff;
    return Area + 1;
}

/**Function*************************************************************

  Synopsis    [Evaluates one cut.]

  Description [Returns the best gain.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_LibEval( Dar_Man_t * p, Aig_Obj_t * pRoot, Dar_Cut_t * pCut, int * pnMffcSize )
{
    Dar_LibObj_t * pObj;
    int Out, k, Class, nNodesSaved, nNodesAdded, nNodesGained;
    if ( pCut->nLeaves != 4 )
        return;
    // check if the cut exits and assigns leaves and their levels
    if ( !Dar_LibCutMatch(p, pCut) )
        return;
    // mark MFFC of the node
    nNodesSaved = Dar_LibCutMarkMffc( p->pDarLib, p->pAig, pRoot, pCut->nLeaves );
    // evaluate the cut
    Class = p->pDarLib->pMap[pCut->uTruth];
    Dar_LibEvalAssignNums( p, Class, pRoot );
    // profile outputs by their savings
    p->nTotalSubgs += p->pDarLib->nSubgr0[Class];
    p->ClassSubgs[Class] += p->pDarLib->nSubgr0[Class];
    for ( Out = 0; Out < p->pDarLib->nSubgr0[Class]; Out++ )
    {
        pObj = Dar_LibObj(p->pDarLib, p->pDarLib->pSubgr0[Class][Out]);
        if ( Aig_Regular(p->pDarLib->pDatas[pObj->Num].pFunc) == pRoot )
            continue;
        nNodesAdded = Dar_LibEval_rec( p->pDarLib, pObj, Out, nNodesSaved - !p->pPars->fUseZeros );
        nNodesGained = nNodesSaved - nNodesAdded;
        if ( nNodesGained < 0 || (nNodesGained == 0 && !p->pPars->fUseZeros) )
            continue;
        if ( nNodesGained <  p->GainBest ||
            (nNodesGained == p->GainBest && p->pDarLib->pDatas[pObj->Num].Level >= p->LevelBest) )
            continue;
        // remember this possibility
        Vec_PtrClear( p->vLeavesBest );
        for ( k = 0; k < (int)pCut->nLeaves; k++ )
            Vec_PtrPush( p->vLeavesBest, p->pDarLib->pDatas[k].pFunc );
        p->OutBest    = p->pDarLib->pSubgr0[Class][Out];
        p->OutNumBest = Out;
        p->LevelBest  = p->pDarLib->pDatas[pObj->Num].Level;
        p->GainBest   = nNodesGained;
        p->ClassBest  = Class;
        *pnMffcSize   = nNodesSaved;
    }
}

/**Function*************************************************************

  Synopsis    [Clears the fields of the nodes used in this cut.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dar_LibBuildClear_rec( Dar_Lib_t * pDarLib, Dar_LibObj_t * pObj, int * pCounter )
{
    if ( pObj->fTerm )
        return;
    pObj->Num = (*pCounter)++;
    pDarLib->pDatas[ pObj->Num ].pFunc = NULL;
    Dar_LibBuildClear_rec( pDarLib, Dar_LibObj(pDarLib, pObj->Fan0), pCounter );
    Dar_LibBuildClear_rec( pDarLib, Dar_LibObj(pDarLib, pObj->Fan1), pCounter );
}

/**Function*************************************************************

  Synopsis    [Reconstructs the best cut.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Obj_t * Dar_LibBuildBest_rec( Dar_Man_t * p, Dar_LibObj_t * pObj )
{
    Aig_Obj_t * pFanin0, * pFanin1;
    Dar_LibDat_t * pData = p->pDarLib->pDatas + pObj->Num;
    if ( pData->pFunc )
        return pData->pFunc;
    pFanin0 = Dar_LibBuildBest_rec( p, Dar_LibObj(p->pDarLib, pObj->Fan0) );
    pFanin1 = Dar_LibBuildBest_rec( p, Dar_LibObj(p->pDarLib, pObj->Fan1) );
    pFanin0 = Aig_NotCond( pFanin0, pObj->fCompl0 );
    pFanin1 = Aig_NotCond( pFanin1, pObj->fCompl1 );
    pData->pFunc = Aig_And( p->pAig, pFanin0, pFanin1 );
//    assert( pData->Level == (int)Aig_Regular(pData->pFunc)->Level );
    return pData->pFunc;
}

/**Function*************************************************************

  Synopsis    [Reconstructs the best cut.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Obj_t * Dar_LibBuildBest( Dar_Man_t * p )
{
    int i, Counter = 4;
    for ( i = 0; i < Vec_PtrSize(p->vLeavesBest); i++ )
        p->pDarLib->pDatas[i].pFunc = (Aig_Obj_t *)Vec_PtrEntry( p->vLeavesBest, i );
    Dar_LibBuildClear_rec( p->pDarLib, Dar_LibObj(p->pDarLib, p->OutBest), &Counter );
    return Dar_LibBuildBest_rec( p, Dar_LibObj(p->pDarLib, p->OutBest) );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
