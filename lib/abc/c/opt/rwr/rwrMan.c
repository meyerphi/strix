/**CFile****************************************************************

  FileName    [rwrMan.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [DAG-aware AIG rewriting package.]

  Synopsis    [Rewriting manager.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: rwrMan.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "rwr.h"
#include "base/main/main.h"
#include "bool/dec/dec.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Starts rewriting manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Rwr_Man_t * Rwr_ManStart( Dec_Man_t * pManDec, int  fPrecompute )
{
    Rwr_Man_t * p;
    p = ABC_ALLOC( Rwr_Man_t, 1 );
    memset( p, 0, sizeof(Rwr_Man_t) );
    p->nFuncs = (1<<16);
    p->puCanons = pManDec->puCanons;
    p->pPhases  = pManDec->pPhases;
    p->pPerms   = pManDec->pPerms;
    p->pMap     = pManDec->pMap;
    // initialize practical NPN classes
    p->pPractical  = Rwr_ManGetPractical( p );
    // create the table
    p->pTable = ABC_ALLOC( Rwr_Node_t *, p->nFuncs );
    memset( p->pTable, 0, sizeof(Rwr_Node_t *) * p->nFuncs );
    // create the elementary nodes
    p->pMmNode  = Extra_MmFixedStart( sizeof(Rwr_Node_t) );
    p->vForest  = Vec_PtrAlloc( 100 );
    Rwr_ManAddVar( p, 0x0000, fPrecompute ); // constant 0
    Rwr_ManAddVar( p, 0xAAAA, fPrecompute ); // var A
    Rwr_ManAddVar( p, 0xCCCC, fPrecompute ); // var B
    Rwr_ManAddVar( p, 0xF0F0, fPrecompute ); // var C
    Rwr_ManAddVar( p, 0xFF00, fPrecompute ); // var D
    p->nClasses = 5;
    // other stuff
    p->nTravIds   = 1;
    p->pPerms4    = Extra_Permutations( 4 );
    p->vLevNums   = Vec_IntAlloc( 50 );
    p->vFanins    = Vec_PtrAlloc( 50 );
    p->vFaninsCur = Vec_PtrAlloc( 50 );
    p->vNodesTemp = Vec_PtrAlloc( 50 );

    // load subgraphs
    Rwr_ManLoad( p );
    Rwr_ManPreprocess( p );

    return p;
}

/**Function*************************************************************

  Synopsis    [Stops rewriting manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Rwr_ManStop( Rwr_Man_t * p )
{
    if ( p->vClasses )
    {
        Rwr_Node_t * pNode;
        int i, k;
        Vec_VecForEachEntry( Rwr_Node_t *, p->vClasses, pNode, i, k )
            Dec_GraphFree( (Dec_Graph_t *)pNode->pNext );
    }
    if ( p->vClasses )  Vec_VecFree( p->vClasses );
    Vec_PtrFree( p->vNodesTemp );
    Vec_PtrFree( p->vForest );
    Vec_IntFree( p->vLevNums );
    Vec_PtrFree( p->vFanins );
    Vec_PtrFree( p->vFaninsCur );
    Extra_MmFixedStop( p->pMmNode );
    ABC_FREE( p->pMapInv );
    ABC_FREE( p->pTable );
    ABC_FREE( p->pPractical );
    ABC_FREE( p->pPerms4 );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Stops the resynthesis manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void * Rwr_ManReadDecs( Rwr_Man_t * p )
{
    return p->pGraph;
}

/**Function*************************************************************

  Synopsis    [Stops the resynthesis manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Rwr_ManReadCompl( Rwr_Man_t * p )
{
    return p->fCompl;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
