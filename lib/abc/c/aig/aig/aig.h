/**CFile****************************************************************

  FileName    [aig.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [AIG package.]

  Synopsis    [External declarations.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: aig.h,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#ifndef ABC__aig__aig__aig_h
#define ABC__aig__aig__aig_h

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "misc/vec/vec.h"

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Aig_Man_t_            Aig_Man_t;
typedef struct Aig_Obj_t_            Aig_Obj_t;
typedef struct Aig_MmFixed_t_        Aig_MmFixed_t;
typedef struct Aig_MmFlex_t_         Aig_MmFlex_t;
typedef struct Aig_MmStep_t_         Aig_MmStep_t;

// object types
typedef enum {
    AIG_OBJ_NONE,                    // 0: non-existent object
    AIG_OBJ_CONST1,                  // 1: constant 1
    AIG_OBJ_CI,                      // 2: combinational input
    AIG_OBJ_CO,                      // 3: combinational output
    AIG_OBJ_BUF,                     // 4: buffer node
    AIG_OBJ_AND,                     // 5: AND node
    AIG_OBJ_VOID                     // 6: unused object
} Aig_Type_t;

// the AIG node
struct Aig_Obj_t_  // 8 words
{
    union {
        Aig_Obj_t *  pNext;          // strashing table
        int          CioId;          // 0-based number of CI/CO
    };
    Aig_Obj_t *      pFanin0;        // fanin
    Aig_Obj_t *      pFanin1;        // fanin
    unsigned int     Type    :  3;   // object type
    unsigned int     fPhase  :  1;   // value under 000...0 pattern
    unsigned int     fMarkA  :  1;   // multipurpose mask
    unsigned int     fMarkB  :  1;   // multipurpose mask
    unsigned int     nRefs   : 26;   // reference count
    unsigned         Level   : 24;   // the level of this node
    unsigned         nCuts   :  8;   // the number of cuts
    int              TravId;         // unique ID of last traversal involving the node
    int              Id;             // unique ID of the node
    union {                          // temporary store for user's data
        void *       pData;
        int          iData;
        float        dData;
    };
};

// the AIG manager
struct Aig_Man_t_
{
    // AIG nodes
    Vec_Ptr_t *      vCis;           // the array of PIs
    Vec_Ptr_t *      vCos;           // the array of POs
    Vec_Ptr_t *      vObjs;          // the array of all nodes (optional)
    Vec_Ptr_t *      vBufs;          // the array of buffers
    Aig_Obj_t *      pConst1;        // the constant 1 node
    Aig_Obj_t        Ghost;          // the ghost node
    int              nRegs;          // the number of registers (registers are last POs)
    int              nTruePis;       // the number of true primary inputs
    int              nTruePos;       // the number of true primary outputs
    int              nAsserts;       // the number of asserts among POs (asserts are first POs)
    int              nBarBufs;       // the number of barrier buffers
    // AIG node counters
    int              nObjs[AIG_OBJ_VOID];// the number of objects by type
    int              nDeleted;       // the number of deleted objects
    // structural hash table
    Aig_Obj_t **     pTable;         // structural hash table
    int              nTableSize;     // structural hash table size
    // representation of fanouts
    int *            pFanData;       // the database to store fanout information
    int              nFansAlloc;     // the size of fanout representation
    int              nBufReplaces;   // the number of times replacement led to a buffer
    int              nBufFixes;      // the number of times buffers were propagated
    int              nBufMax;        // the maximum number of buffers during computation
    // various data members
    Aig_MmFixed_t *  pMemObjs;       // memory manager for objects
    int              nTravIds;       // the current traversal ID
    Vec_Int_t *      vFlopNums;
    Vec_Int_t *      vFlopReprs;
};

// cut computation
typedef struct Aig_ManCut_t_         Aig_ManCut_t;
typedef struct Aig_Cut_t_            Aig_Cut_t;

// the cut used to represent node in the AIG
struct Aig_Cut_t_
{
    Aig_Cut_t *     pNext;           // the next cut in the table
    int             Cost;            // the cost of the cut
    unsigned        uSign;           // cut signature
    int             iNode;           // the node, for which it is the cut
    short           nCutSize;        // the number of bytes in the cut
    char            nLeafMax;        // the maximum number of fanins
    char            nFanins;         // the current number of fanins
    int             pFanins[0];      // the fanins (followed by the truth table)
};

// the CNF computation manager
struct Aig_ManCut_t_
{
    // AIG manager
    Aig_Man_t *     pAig;            // the input AIG manager
    Aig_Cut_t **    pCuts;           // the cuts for each node in the output manager
    // parameters
    int             nCutsMax;        // the max number of cuts at the node
    int             nLeafMax;        // the max number of leaves of a cut
    int             fTruth;          // enables truth table computation
    int             fVerbose;        // enables verbose output
    // internal variables
    int             nCutSize;        // the number of bytes needed to store one cut
    int             nTruthWords;     // the number of truth table words
    Aig_MmFixed_t * pMemCuts;        // memory manager for cuts
    unsigned *      puTemp[4];       // used for the truth table computation
};

static inline Aig_Cut_t *  Aig_ObjCuts( Aig_ManCut_t * p, Aig_Obj_t * pObj )                         { return p->pCuts[pObj->Id];  }
static inline void         Aig_ObjSetCuts( Aig_ManCut_t * p, Aig_Obj_t * pObj, Aig_Cut_t * pCuts )   { p->pCuts[pObj->Id] = pCuts; }

static inline int          Aig_CutLeaveNum( Aig_Cut_t * pCut )          { return pCut->nFanins;                                    }
static inline int *        Aig_CutLeaves( Aig_Cut_t * pCut )            { return pCut->pFanins;                                    }
static inline unsigned *   Aig_CutTruth( Aig_Cut_t * pCut )             { return (unsigned *)(pCut->pFanins + pCut->nLeafMax);     }
static inline Aig_Cut_t *  Aig_CutNext( Aig_Cut_t * pCut )              { return (Aig_Cut_t *)(((char *)pCut) + pCut->nCutSize);   }

// iterator over cuts of the node
#define Aig_ObjForEachCut( p, pObj, pCut, i )                           \
    for ( i = 0, pCut = Aig_ObjCuts(p, pObj); i < p->nCutsMax; i++, pCut = Aig_CutNext(pCut) )
// iterator over leaves of the cut
#define Aig_CutForEachLeaf( p, pCut, pLeaf, i )                         \
    for ( i = 0; (i < (int)(pCut)->nFanins) && ((pLeaf) = Aig_ManObj(p, (pCut)->pFanins[i])); i++ )

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

static inline unsigned     Aig_ObjCutSign( unsigned ObjId )       { return (1 << (ObjId & 31));                            }
static inline int          Aig_WordCountOnes( unsigned uWord )
{
    uWord = (uWord & 0x55555555) + ((uWord>>1) & 0x55555555);
    uWord = (uWord & 0x33333333) + ((uWord>>2) & 0x33333333);
    uWord = (uWord & 0x0F0F0F0F) + ((uWord>>4) & 0x0F0F0F0F);
    uWord = (uWord & 0x00FF00FF) + ((uWord>>8) & 0x00FF00FF);
    return  (uWord & 0x0000FFFF) + (uWord>>16);
}
static inline int          Aig_WordFindFirstBit( unsigned uWord )
{
    int i;
    for ( i = 0; i < 32; i++ )
        if ( uWord & (1 << i) )
            return i;
    return -1;
}

static inline Aig_Obj_t *  Aig_Regular( Aig_Obj_t * p )           { return (Aig_Obj_t *)((ABC_PTRUINT_T)(p) & ~01);  }
static inline Aig_Obj_t *  Aig_Not( Aig_Obj_t * p )               { return (Aig_Obj_t *)((ABC_PTRUINT_T)(p) ^  01);  }
static inline Aig_Obj_t *  Aig_NotCond( Aig_Obj_t * p, int c )    { return (Aig_Obj_t *)((ABC_PTRUINT_T)(p) ^ (c));  }
static inline int          Aig_IsComplement( Aig_Obj_t * p )      { return (int)((ABC_PTRUINT_T)(p) & 01);           }

static inline int          Aig_ManCiNum( Aig_Man_t * p )          { return p->nObjs[AIG_OBJ_CI];                     }
static inline int          Aig_ManCoNum( Aig_Man_t * p )          { return p->nObjs[AIG_OBJ_CO];                     }
static inline int          Aig_ManBufNum( Aig_Man_t * p )         { return p->nObjs[AIG_OBJ_BUF];                    }
static inline int          Aig_ManAndNum( Aig_Man_t * p )         { return p->nObjs[AIG_OBJ_AND];                    }
static inline int          Aig_ManNodeNum( Aig_Man_t * p )        { return p->nObjs[AIG_OBJ_AND]; }
static inline int          Aig_ManGetCost( Aig_Man_t * p )        { return p->nObjs[AIG_OBJ_AND]; }
static inline int          Aig_ManObjNum( Aig_Man_t * p )         { return Vec_PtrSize(p->vObjs) - p->nDeleted;      }
static inline int          Aig_ManObjNumMax( Aig_Man_t * p )      { return Vec_PtrSize(p->vObjs);                    }
static inline int          Aig_ManRegNum( Aig_Man_t * p )         { return p->nRegs;                                 }

static inline Aig_Obj_t *  Aig_ManConst0( Aig_Man_t * p )         { return Aig_Not(p->pConst1);                      }
static inline Aig_Obj_t *  Aig_ManConst1( Aig_Man_t * p )         { return p->pConst1;                               }
static inline Aig_Obj_t *  Aig_ManGhost( Aig_Man_t * p )          { return &p->Ghost;                                }
static inline Aig_Obj_t *  Aig_ManCi( Aig_Man_t * p, int i )      { return (Aig_Obj_t *)Vec_PtrEntry(p->vCis, i);    }
static inline Aig_Obj_t *  Aig_ManCo( Aig_Man_t * p, int i )      { return (Aig_Obj_t *)Vec_PtrEntry(p->vCos, i);    }
static inline Aig_Obj_t *  Aig_ManLo( Aig_Man_t * p, int i )      { return (Aig_Obj_t *)Vec_PtrEntry(p->vCis, Aig_ManCiNum(p)-Aig_ManRegNum(p)+i);   }
static inline Aig_Obj_t *  Aig_ManLi( Aig_Man_t * p, int i )      { return (Aig_Obj_t *)Vec_PtrEntry(p->vCos, Aig_ManCoNum(p)-Aig_ManRegNum(p)+i);   }
static inline Aig_Obj_t *  Aig_ManObj( Aig_Man_t * p, int i )     { return p->vObjs ? (Aig_Obj_t *)Vec_PtrEntry(p->vObjs, i) : NULL;  }

static inline Aig_Type_t   Aig_ObjType( Aig_Obj_t * pObj )        { return (Aig_Type_t)pObj->Type;       }
static inline int          Aig_ObjIsNone( Aig_Obj_t * pObj )      { return pObj->Type == AIG_OBJ_NONE;   }
static inline int          Aig_ObjIsConst1( Aig_Obj_t * pObj )    { assert(!Aig_IsComplement(pObj)); return pObj->Type == AIG_OBJ_CONST1; }
static inline int          Aig_ObjIsCi( Aig_Obj_t * pObj )        { return pObj->Type == AIG_OBJ_CI;     }
static inline int          Aig_ObjIsCo( Aig_Obj_t * pObj )        { return pObj->Type == AIG_OBJ_CO;     }
static inline int          Aig_ObjIsBuf( Aig_Obj_t * pObj )       { return pObj->Type == AIG_OBJ_BUF;    }
static inline int          Aig_ObjIsAnd( Aig_Obj_t * pObj )       { return pObj->Type == AIG_OBJ_AND;    }
static inline int          Aig_ObjIsNode( Aig_Obj_t * pObj )      { return pObj->Type == AIG_OBJ_AND;    }
static inline int          Aig_ObjIsTerm( Aig_Obj_t * pObj )      { return pObj->Type == AIG_OBJ_CI  || pObj->Type == AIG_OBJ_CO || pObj->Type == AIG_OBJ_CONST1;   }
static inline int          Aig_ObjIsHash( Aig_Obj_t * pObj )      { return pObj->Type == AIG_OBJ_AND;    }
static inline int          Aig_ObjIsCand( Aig_Obj_t * pObj )      { return pObj->Type == AIG_OBJ_CI || pObj->Type == AIG_OBJ_AND; }
static inline int          Aig_ObjCioId( Aig_Obj_t * pObj )       { assert( !Aig_ObjIsNode(pObj) ); return pObj->CioId;                                            }
static inline int          Aig_ObjId( Aig_Obj_t * pObj )          { return pObj->Id;                     }

static inline int          Aig_ObjIsMarkA( Aig_Obj_t * pObj )     { return pObj->fMarkA;  }
static inline void         Aig_ObjSetMarkA( Aig_Obj_t * pObj )    { pObj->fMarkA = 1;     }
static inline void         Aig_ObjClearMarkA( Aig_Obj_t * pObj )  { pObj->fMarkA = 0;     }

static inline void         Aig_ObjSetTravId( Aig_Obj_t * pObj, int TravId )                { pObj->TravId = TravId;                         }
static inline void         Aig_ObjSetTravIdCurrent( Aig_Man_t * p, Aig_Obj_t * pObj )      { pObj->TravId = p->nTravIds;                    }
static inline void         Aig_ObjSetTravIdPrevious( Aig_Man_t * p, Aig_Obj_t * pObj )     { pObj->TravId = p->nTravIds - 1;                }
static inline int          Aig_ObjIsTravIdCurrent( Aig_Man_t * p, Aig_Obj_t * pObj )       { return (int)(pObj->TravId == p->nTravIds);     }
static inline int          Aig_ObjIsTravIdPrevious( Aig_Man_t * p, Aig_Obj_t * pObj )      { return (int)(pObj->TravId == p->nTravIds - 1); }

static inline int          Aig_ObjPhase( Aig_Obj_t * pObj )       { return pObj->fPhase;                           }
static inline int          Aig_ObjPhaseReal( Aig_Obj_t * pObj )   { return pObj? Aig_Regular(pObj)->fPhase ^ Aig_IsComplement(pObj) : 1;                              }
static inline int          Aig_ObjRefs( Aig_Obj_t * pObj )        { return pObj->nRefs;                            }
static inline void         Aig_ObjRef( Aig_Obj_t * pObj )         { pObj->nRefs++;                                 }
static inline void         Aig_ObjDeref( Aig_Obj_t * pObj )       { assert( pObj->nRefs > 0 ); pObj->nRefs--;      }
static inline void         Aig_ObjClearRef( Aig_Obj_t * pObj )    { pObj->nRefs = 0;                               }
static inline int          Aig_ObjFaninId0( Aig_Obj_t * pObj )    { return pObj->pFanin0? Aig_Regular(pObj->pFanin0)->Id : -1; }
static inline int          Aig_ObjFaninId1( Aig_Obj_t * pObj )    { return pObj->pFanin1? Aig_Regular(pObj->pFanin1)->Id : -1; }
static inline int          Aig_ObjFaninC0( Aig_Obj_t * pObj )     { return Aig_IsComplement(pObj->pFanin0);        }
static inline int          Aig_ObjFaninC1( Aig_Obj_t * pObj )     { return Aig_IsComplement(pObj->pFanin1);        }
static inline Aig_Obj_t *  Aig_ObjFanin0( Aig_Obj_t * pObj )      { return Aig_Regular(pObj->pFanin0);             }
static inline Aig_Obj_t *  Aig_ObjFanin1( Aig_Obj_t * pObj )      { return Aig_Regular(pObj->pFanin1);             }
static inline Aig_Obj_t *  Aig_ObjChild0( Aig_Obj_t * pObj )      { return pObj->pFanin0;                          }
static inline Aig_Obj_t *  Aig_ObjChild1( Aig_Obj_t * pObj )      { return pObj->pFanin1;                          }
static inline Aig_Obj_t *  Aig_ObjChild0Copy( Aig_Obj_t * pObj )  { assert( !Aig_IsComplement(pObj) ); return Aig_ObjFanin0(pObj)? Aig_NotCond((Aig_Obj_t *)Aig_ObjFanin0(pObj)->pData, Aig_ObjFaninC0(pObj)) : NULL;  }
static inline Aig_Obj_t *  Aig_ObjChild1Copy( Aig_Obj_t * pObj )  { assert( !Aig_IsComplement(pObj) ); return Aig_ObjFanin1(pObj)? Aig_NotCond((Aig_Obj_t *)Aig_ObjFanin1(pObj)->pData, Aig_ObjFaninC1(pObj)) : NULL;  }
static inline Aig_Obj_t *  Aig_ObjChild0Next( Aig_Obj_t * pObj )  { assert( !Aig_IsComplement(pObj) ); return Aig_ObjFanin0(pObj)? Aig_NotCond((Aig_Obj_t *)Aig_ObjFanin0(pObj)->pNext, Aig_ObjFaninC0(pObj)) : NULL;  }
static inline Aig_Obj_t *  Aig_ObjChild1Next( Aig_Obj_t * pObj )  { assert( !Aig_IsComplement(pObj) ); return Aig_ObjFanin1(pObj)? Aig_NotCond((Aig_Obj_t *)Aig_ObjFanin1(pObj)->pNext, Aig_ObjFaninC1(pObj)) : NULL;  }
static inline void         Aig_ObjChild0Flip( Aig_Obj_t * pObj )  { assert( !Aig_IsComplement(pObj) ); pObj->pFanin0 = Aig_Not(pObj->pFanin0);        }
static inline void         Aig_ObjChild1Flip( Aig_Obj_t * pObj )  { assert( !Aig_IsComplement(pObj) ); pObj->pFanin1 = Aig_Not(pObj->pFanin1);        }
static inline Aig_Obj_t *  Aig_ObjCopy( Aig_Obj_t * pObj )        { assert( !Aig_IsComplement(pObj) ); return (Aig_Obj_t *)pObj->pData;               }
static inline void         Aig_ObjSetCopy( Aig_Obj_t * pObj, Aig_Obj_t * pCopy )     {  assert( !Aig_IsComplement(pObj) ); pObj->pData = pCopy;       }
static inline Aig_Obj_t *  Aig_ObjRealCopy( Aig_Obj_t * pObj )    { return Aig_NotCond((Aig_Obj_t *)Aig_Regular(pObj)->pData, Aig_IsComplement(pObj));}
static inline int          Aig_ObjToLit( Aig_Obj_t * pObj )       { return Abc_Var2Lit( Aig_ObjId(Aig_Regular(pObj)), Aig_IsComplement(pObj) );       }
static inline Aig_Obj_t *  Aig_ObjFromLit( Aig_Man_t * p,int iLit){ return Aig_NotCond( Aig_ManObj(p, Abc_Lit2Var(iLit)), Abc_LitIsCompl(iLit) );     }
static inline int          Aig_ObjLevel( Aig_Obj_t * pObj )       { assert( !Aig_IsComplement(pObj) ); return pObj->Level;                            }
static inline int          Aig_ObjLevelNew( Aig_Obj_t * pObj )    { assert( !Aig_IsComplement(pObj) ); return Aig_ObjFanin1(pObj)? 1 + Abc_MaxInt(Aig_ObjFanin0(pObj)->Level, Aig_ObjFanin1(pObj)->Level) : Aig_ObjFanin0(pObj)->Level; }
static inline int          Aig_ObjSetLevel( Aig_Obj_t * pObj, int i ) { assert( !Aig_IsComplement(pObj) ); return pObj->Level = i;                    }
static inline void         Aig_ObjClean( Aig_Obj_t * pObj )       { memset( pObj, 0, sizeof(Aig_Obj_t) );                                                             }
static inline Aig_Obj_t *  Aig_ObjFanout0( Aig_Man_t * p, Aig_Obj_t * pObj )  { assert(p->pFanData && pObj->Id < p->nFansAlloc); return Aig_ManObj(p, p->pFanData[5*pObj->Id] >> 1); }
static inline int          Aig_ObjWhatFanin( Aig_Obj_t * pObj, Aig_Obj_t * pFanin )
{
    if ( Aig_ObjFanin0(pObj) == pFanin ) return 0;
    if ( Aig_ObjFanin1(pObj) == pFanin ) return 1;
    assert(0); return -1;
}
static inline int          Aig_ObjFanoutC( Aig_Obj_t * pObj, Aig_Obj_t * pFanout )
{
    if ( Aig_ObjFanin0(pFanout) == pObj ) return Aig_ObjFaninC0(pObj);
    if ( Aig_ObjFanin1(pFanout) == pObj ) return Aig_ObjFaninC1(pObj);
    assert(0); return -1;
}

// create the ghost of the new node
static inline Aig_Obj_t *  Aig_ObjCreateGhost( Aig_Man_t * p, Aig_Obj_t * p0, Aig_Obj_t * p1, Aig_Type_t Type )
{
    Aig_Obj_t * pGhost;
    assert( Type != AIG_OBJ_AND || !Aig_ObjIsConst1(Aig_Regular(p0)) );
    assert( p1 == NULL || !Aig_ObjIsConst1(Aig_Regular(p1)) );
    assert( Type == AIG_OBJ_CI || Aig_Regular(p0) != Aig_Regular(p1) );
    pGhost = Aig_ManGhost(p);
    pGhost->Type = Type;
    if ( p1 == NULL || Aig_Regular(p0)->Id < Aig_Regular(p1)->Id )
    {
        pGhost->pFanin0 = p0;
        pGhost->pFanin1 = p1;
    }
    else
    {
        pGhost->pFanin0 = p1;
        pGhost->pFanin1 = p0;
    }
    return pGhost;
}

// internal memory manager
static inline Aig_Obj_t * Aig_ManFetchMemory( Aig_Man_t * p )
{
    extern char * Aig_MmFixedEntryFetch( Aig_MmFixed_t * p );
    Aig_Obj_t * pTemp;
    pTemp = (Aig_Obj_t *)Aig_MmFixedEntryFetch( p->pMemObjs );
    memset( pTemp, 0, sizeof(Aig_Obj_t) );
    pTemp->Id = Vec_PtrSize(p->vObjs);
    Vec_PtrPush( p->vObjs, pTemp );
    return pTemp;
}
static inline void Aig_ManRecycleMemory( Aig_Man_t * p, Aig_Obj_t * pEntry )
{
    extern void Aig_MmFixedEntryRecycle( Aig_MmFixed_t * p, char * pEntry );
    assert( pEntry->nRefs == 0 );
    pEntry->Type = AIG_OBJ_NONE; // distinquishes a dead node from a live node
    Aig_MmFixedEntryRecycle( p->pMemObjs, (char *)pEntry );
    p->nDeleted++;
}

////////////////////////////////////////////////////////////////////////
///                             ITERATORS                            ///
////////////////////////////////////////////////////////////////////////

// iterator over the combinational inputs
#define Aig_ManForEachCi( p, pObj, i )                                          \
    Vec_PtrForEachEntry( Aig_Obj_t *, p->vCis, pObj, i )
#define Aig_ManForEachCiReverse( p, pObj, i )                                   \
    Vec_PtrForEachEntryReverse( Aig_Obj_t *, p->vCis, pObj, i )
// iterator over the combinational outputs
#define Aig_ManForEachCo( p, pObj, i )                                          \
    Vec_PtrForEachEntry( Aig_Obj_t *, p->vCos, pObj, i )
#define Aig_ManForEachCoReverse( p, pObj, i )                                   \
    Vec_PtrForEachEntryReverse( Aig_Obj_t *, p->vCos, pObj, i )
// iterators over all objects, including those currently not used
#define Aig_ManForEachObj( p, pObj, i )                                         \
    Vec_PtrForEachEntry( Aig_Obj_t *, p->vObjs, pObj, i ) if ( (pObj) == NULL ) {} else
#define Aig_ManForEachObjReverse( p, pObj, i )                                  \
    Vec_PtrForEachEntryReverse( Aig_Obj_t *, p->vObjs, pObj, i ) if ( (pObj) == NULL ) {} else
// iterators over the objects whose IDs are stored in an array
#define Aig_ManForEachObjVec( vIds, p, pObj, i )                                \
    for ( i = 0; i < Vec_IntSize(vIds) && (((pObj) = Aig_ManObj(p, Vec_IntEntry(vIds,i))), 1); i++ )
#define Aig_ManForEachObjVecReverse( vIds, p, pObj, i )                         \
    for ( i = Vec_IntSize(vIds) - 1; i >= 0 && (((pObj) = Aig_ManObj(p, Vec_IntEntry(vIds,i))), 1); i-- )
// iterators over all nodes
#define Aig_ManForEachNode( p, pObj, i )                                        \
    Vec_PtrForEachEntry( Aig_Obj_t *, p->vObjs, pObj, i ) if ( (pObj) == NULL || !Aig_ObjIsNode(pObj) ) {} else
#define Aig_ManForEachNodeReverse( p, pObj, i )                                 \
    Vec_PtrForEachEntryReverse( Aig_Obj_t *, p->vObjs, pObj, i ) if ( (pObj) == NULL || !Aig_ObjIsNode(pObj) ) {} else

// these two procedures are only here for the use inside the iterator
static inline int     Aig_ObjFanout0Int( Aig_Man_t * p, int ObjId )  { assert(ObjId < p->nFansAlloc);  return p->pFanData[5*ObjId];                         }
static inline int     Aig_ObjFanoutNext( Aig_Man_t * p, int iFan )   { assert(iFan/2 < p->nFansAlloc); return p->pFanData[5*(iFan >> 1) + 3 + (iFan & 1)];  }
// iterator over the fanouts
#define Aig_ObjForEachFanout( p, pObj, pFanout, iFan, i )                       \
    for ( assert(p->pFanData), i = 0; (i < (int)(pObj)->nRefs) &&               \
          (((iFan) = i? Aig_ObjFanoutNext(p, iFan) : Aig_ObjFanout0Int(p, pObj->Id)), 1) && \
          (((pFanout) = Aig_ManObj(p, iFan>>1)), 1); i++ )

////////////////////////////////////////////////////////////////////////
///                     SEQUENTIAL ITERATORS                         ///
////////////////////////////////////////////////////////////////////////

// iterator over the primary inputs
#define Aig_ManForEachPiSeq( p, pObj, i )                                       \
    Vec_PtrForEachEntryStop( Aig_Obj_t *, p->vCis, pObj, i, Aig_ManCiNum(p)-Aig_ManRegNum(p) )
// iterator over the latch outputs
#define Aig_ManForEachLoSeq( p, pObj, i )                                       \
    Vec_PtrForEachEntryStart( Aig_Obj_t *, p->vCis, pObj, i, Aig_ManCiNum(p)-Aig_ManRegNum(p) )
// iterator over the primary outputs
#define Aig_ManForEachPoSeq( p, pObj, i )                                       \
    Vec_PtrForEachEntryStop( Aig_Obj_t *, p->vCos, pObj, i, Aig_ManCoNum(p)-Aig_ManRegNum(p) )
// iterator over the latch inputs
#define Aig_ManForEachLiSeq( p, pObj, i )                                       \
    Vec_PtrForEachEntryStart( Aig_Obj_t *, p->vCos, pObj, i, Aig_ManCoNum(p)-Aig_ManRegNum(p) )
// iterator over the latch input and outputs
#define Aig_ManForEachLiLoSeq( p, pObjLi, pObjLo, k )                           \
    for ( k = 0; (k < Aig_ManRegNum(p)) && (((pObjLi) = Aig_ManLi(p, k)), 1)    \
        && (((pObjLo)=Aig_ManLo(p, k)), 1); k++ )

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== aigCheck.c ========================================================*/
extern int             Aig_ManCheck( Aig_Man_t * p );
extern void            Aig_ManCheckPhase( Aig_Man_t * p );
/*=== aigCuts.c ========================================================*/
extern Aig_ManCut_t *  Aig_ComputeCuts( Aig_Man_t * pAig, int nCutsMax, int nLeafMax, int fTruth, int fVerbose );
extern void            Aig_ManCutStop( Aig_ManCut_t * p );
/*=== aigDfs.c ==========================================================*/
extern Vec_Ptr_t *     Aig_ManDfs( Aig_Man_t * p, int fNodesOnly );
extern int             Aig_DagSize( Aig_Obj_t * pObj );
extern void            Aig_ConeUnmark_rec( Aig_Obj_t * pObj );
extern void            Aig_ObjCollectCut( Aig_Obj_t * pRoot, Vec_Ptr_t * vLeaves, Vec_Ptr_t * vNodes );
/*=== aigDup.c ==========================================================*/
extern Aig_Man_t *     Aig_ManDupDfs( Aig_Man_t * p );
/*=== aigFanout.c ==========================================================*/
extern void            Aig_ObjAddFanout( Aig_Man_t * p, Aig_Obj_t * pObj, Aig_Obj_t * pFanout );
extern void            Aig_ObjRemoveFanout( Aig_Man_t * p, Aig_Obj_t * pObj, Aig_Obj_t * pFanout );
extern void            Aig_ManFanoutStart( Aig_Man_t * p );
extern void            Aig_ManFanoutStop( Aig_Man_t * p );
/*=== aigMan.c ==========================================================*/
extern Aig_Man_t *     Aig_ManStart( int nNodesMax );
extern void            Aig_ManStop( Aig_Man_t * p );
extern int             Aig_ManCleanup( Aig_Man_t * p );
extern void            Aig_ManSetRegNum( Aig_Man_t * p, int nRegs );
/*=== aigMem.c ==========================================================*/
extern void            Aig_ManStartMemory( Aig_Man_t * p );
extern void            Aig_ManStopMemory( Aig_Man_t * p );
/*=== aigMffc.c ==========================================================*/
extern int             Aig_NodeMffcSupp( Aig_Man_t * p, Aig_Obj_t * pNode, int LevelMin, Vec_Ptr_t * vSupp );
extern int             Aig_NodeMffcLabel( Aig_Man_t * p, Aig_Obj_t * pNode );
extern int             Aig_NodeMffcLabelCut( Aig_Man_t * p, Aig_Obj_t * pNode, Vec_Ptr_t * vLeaves );
extern int             Aig_NodeMffcExtendCut( Aig_Man_t * p, Aig_Obj_t * pNode, Vec_Ptr_t * vLeaves, Vec_Ptr_t * vResult );
/*=== aigObj.c ==========================================================*/
extern Aig_Obj_t *     Aig_ObjCreateCi( Aig_Man_t * p );
extern Aig_Obj_t *     Aig_ObjCreateCo( Aig_Man_t * p, Aig_Obj_t * pDriver );
extern Aig_Obj_t *     Aig_ObjCreate( Aig_Man_t * p, Aig_Obj_t * pGhost );
extern void            Aig_ObjConnect( Aig_Man_t * p, Aig_Obj_t * pObj, Aig_Obj_t * pFan0, Aig_Obj_t * pFan1 );
extern void            Aig_ObjDisconnect( Aig_Man_t * p, Aig_Obj_t * pObj );
extern void            Aig_ObjDelete( Aig_Man_t * p, Aig_Obj_t * pObj );
extern void            Aig_ObjDelete_rec( Aig_Man_t * p, Aig_Obj_t * pObj, int fFreeTop );
extern void            Aig_ObjPatchFanin0( Aig_Man_t * p, Aig_Obj_t * pObj, Aig_Obj_t * pFaninNew );
extern void            Aig_ObjReplace( Aig_Man_t * p, Aig_Obj_t * pObjOld, Aig_Obj_t * pObjNew );
/*=== aigOper.c =========================================================*/
extern Aig_Obj_t *     Aig_Oper( Aig_Man_t * p, Aig_Obj_t * p0, Aig_Obj_t * p1, Aig_Type_t Type );
extern Aig_Obj_t *     Aig_And( Aig_Man_t * p, Aig_Obj_t * p0, Aig_Obj_t * p1 );
/*=== aigTable.c ========================================================*/
extern Aig_Obj_t *     Aig_TableLookup( Aig_Man_t * p, Aig_Obj_t * pGhost );
extern Aig_Obj_t *     Aig_TableLookupTwo( Aig_Man_t * p, Aig_Obj_t * pFanin0, Aig_Obj_t * pFanin1 );
extern void            Aig_TableInsert( Aig_Man_t * p, Aig_Obj_t * pObj );
extern void            Aig_TableDelete( Aig_Man_t * p, Aig_Obj_t * pObj );
extern int             Aig_TableCountEntries( Aig_Man_t * p );
/*=== aigTruth.c ========================================================*/
extern unsigned *      Aig_ManCutTruth( Aig_Obj_t * pRoot, Vec_Ptr_t * vLeaves, Vec_Ptr_t * vNodes, Vec_Ptr_t * vTruthElem, Vec_Ptr_t * vTruthStore );
/*=== aigUtil.c =========================================================*/
extern void            Aig_ManIncrementTravId( Aig_Man_t * p );
extern void            Aig_ManCleanData( Aig_Man_t * p );
extern Aig_Obj_t *     Aig_ObjReal_rec( Aig_Obj_t * pObj );
extern void            Aig_ManSetCioIds( Aig_Man_t * p );
/*=== aigWin.c =========================================================*/
extern void            Aig_ManFindCut( Aig_Obj_t * pRoot, Vec_Ptr_t * vFront, Vec_Ptr_t * vVisited, int nSizeLimit, int nFanoutLimit );
/*=== aigMem.c ===========================================================*/
// fixed-size-block memory manager
extern Aig_MmFixed_t * Aig_MmFixedStart( int nEntrySize, int nEntriesMax );
extern void            Aig_MmFixedStop( Aig_MmFixed_t * p, int fVerbose );
extern char *          Aig_MmFixedEntryFetch( Aig_MmFixed_t * p );
extern void            Aig_MmFixedEntryRecycle( Aig_MmFixed_t * p, char * pEntry );
extern void            Aig_MmFixedRestart( Aig_MmFixed_t * p );
extern int             Aig_MmFixedReadMemUsage( Aig_MmFixed_t * p );

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
