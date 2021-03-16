/**CFile****************************************************************

  FileName    [dar.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [DAG-aware AIG rewriting.]

  Synopsis    [External declarations.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: dar.h,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#ifndef ABC__aig__dar__dar_h
#define ABC__aig__dar__dar_h

#include "aig/aig/aig.h"
#include "opt/dar/darApi.h"

struct Dar_LibObj_t_ // library object (2 words)
{
    unsigned         Fan0    : 16;  // the first fanin
    unsigned         Fan1    : 16;  // the second fanin
    unsigned         fCompl0 :  1;  // the first compl attribute
    unsigned         fCompl1 :  1;  // the second compl attribute
    unsigned         fPhase  :  1;  // the phase of the node
    unsigned         fTerm   :  1;  // indicates a PI
    unsigned         Num     : 28;  // internal use
};

struct Dar_LibDat_t_ // library object data
{
    union {
    Aig_Obj_t *      pFunc;         // the corresponding AIG node if it exists
    int              iGunc; };      // the corresponding AIG node if it exists
    int              Level;         // level of this node after it is constructured
    int              TravId;        // traversal ID of the library object data
    float            dProb;         // probability of the node being 1
    unsigned char    fMffc;         // set to one if node is part of MFFC
    unsigned char    nLats[3];      // the number of latches on the input/output stem
};

struct Dar_Lib_t_ // library
{
    // objects
    Dar_LibObj_t *   pObjs;         // the set of library objects
    int              nObjs;         // the number of objects used
    int              iObj;          // the current object
    // structures by class
    int              nSubgr[222];   // the number of subgraphs by class
    int *            pSubgr[222];   // the subgraphs for each class
    int *            pSubgrMem;     // memory for subgraph pointers
    int              nSubgrTotal;   // the total number of subgraph
    // structure priorities
    int *            pPriosMem;     // memory for priority of structures
    int *            pPrios[222];   // pointers to the priority numbers
    // structure places in the priorities
    int *            pPlaceMem;     // memory for places of structures in the priority lists
    int *            pPlace[222];   // pointers to the places numbers
    // structure scores
    int *            pScoreMem;     // memory for scores of structures
    int *            pScore[222];   // pointers to the scores numbers
    // nodes by class
    int              nNodes[222];   // the number of nodes by class
    int *            pNodes[222];   // the nodes for each class
    int *            pNodesMem;     // memory for nodes pointers
    int              nNodesTotal;   // the total number of nodes
    // prepared library
    int              nSubgraphs;
    int              nNodes0Max;
    // nodes by class
    int              nNodes0[222];   // the number of nodes by class
    int *            pNodes0[222];   // the nodes for each class
    int *            pNodes0Mem;     // memory for nodes pointers
    int              nNodes0Total;   // the total number of nodes
    // structures by class
    int              nSubgr0[222];   // the number of subgraphs by class
    int *            pSubgr0[222];   // the subgraphs for each class
    int *            pSubgr0Mem;     // memory for subgraph pointers
    int              nSubgr0Total;   // the total number of subgraph
    // object data
    Dar_LibDat_t *   pDatas;
    int              nDatas;
    // information about NPN classes
    char **          pPerms4;
    unsigned short * puCanons;
    char *           pPhases;
    char *           pPerms;
    unsigned char *  pMap;
};

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== darLib.c ========================================================*/
extern Dar_Lib_t *     Dar_LibStart();
extern void            Dar_LibStop( Dar_Lib_t * pDarLib );
extern void            Dar_LibPrepare( Dar_Lib_t * pDarLib, int nSubgraphs );
/*=== darBalance.c ========================================================*/
extern Aig_Man_t *     Dar_ManBalance( Aig_Man_t * p );
/*=== darCore.c ========================================================*/
extern void            Dar_ManDefaultRwrParams( Dar_RwrPar_t * pPars );
extern int             Dar_ManRewrite( Dar_Lib_t * pDarLib, Aig_Man_t * pAig, Dar_RwrPar_t * pPars );
/*=== darRefact.c ========================================================*/
extern void            Dar_ManDefaultRefParams( Dar_RefPar_t * pPars );
extern int             Dar_ManRefactor( Aig_Man_t * pAig, Dar_RefPar_t * pPars );

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
