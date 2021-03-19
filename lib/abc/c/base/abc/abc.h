/**CFile****************************************************************

  FileName    [abc.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [External declarations.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abc.h,v 1.1 2008/05/14 22:13:11 wudenni Exp $]

***********************************************************************/

#ifndef ABC__base__abc__abc_h
#define ABC__base__abc__abc_h

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "misc/vec/vec.h"
#include "misc/st/st.h"
#include "misc/nm/nm.h"
#include "misc/mem/mem.h"
#include "misc/extra/extra.h"
#include "opt/dar/dar.h"

////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////

// object types
typedef enum {
    ABC_OBJ_NONE = 0,   //  0:  unknown
    ABC_OBJ_CONST1,     //  1:  constant 1 node (AIG only)
    ABC_OBJ_PI,         //  2:  primary input terminal
    ABC_OBJ_PO,         //  3:  primary output terminal
    ABC_OBJ_BI,         //  4:  box input terminal
    ABC_OBJ_BO,         //  5:  box output terminal
    ABC_OBJ_NODE,       //  6:  node
    ABC_OBJ_LATCH,      //  7:  latch
    ABC_OBJ_NUMBER      //  8:  unused
} Abc_ObjType_t;

// latch initial values
typedef enum {
    ABC_INIT_NONE = 0,  // 0:  unknown
    ABC_INIT_ZERO,      // 1:  zero
    ABC_INIT_ONE,       // 2:  one
    ABC_INIT_DC,        // 3:  don't-care
    ABC_INIT_OTHER      // 4:  unused
} Abc_InitType_t;

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Abc_Ntk_t_       Abc_Ntk_t;
typedef struct Abc_Obj_t_       Abc_Obj_t;
typedef struct Abc_Aig_t_       Abc_Aig_t;
typedef struct Abc_ManCut_t_    Abc_ManCut_t;

struct Abc_Obj_t_
{
    Abc_Ntk_t *       pNtk;          // the host network
    Abc_Obj_t *       pNext;         // the next pointer in the hash table
    int               Id;            // the object ID
    unsigned          Type    :  4;  // the object type
    unsigned          fMarkA  :  1;  // the multipurpose mark
    unsigned          fMarkB  :  1;  // the multipurpose mark
    unsigned          fPhase  :  1;  // the flag to mark the phase of equivalent node
    unsigned          fExor   :  1;  // marks AIG node that is a root of EXOR
    unsigned          fCompl0 :  1;  // complemented attribute of the first fanin in the AIG
    unsigned          fCompl1 :  1;  // complemented attribute of the second fanin in the AIG
    unsigned          Level   : 20;  // the level of the node
    Vec_Int_t         vFanins;       // the array of fanins
    Vec_Int_t         vFanouts;      // the array of fanouts
    void *            pData;         // the network specific data (SOP, BDD, gate, equiv class, etc)
    Abc_Obj_t *       pCopy;         // the copy of this object
};

struct Abc_Ntk_t_
{
    // general information
    Nm_Man_t *        pManName;      // name manager (stores names of objects)
    // components of the network
    Vec_Ptr_t *       vObjs;         // the array of all objects (inputs, outputs, nodes, latches, etc)
    Vec_Ptr_t *       vPis;          // the array of primary inputs
    Vec_Ptr_t *       vPos;          // the array of primary outputs
    Vec_Ptr_t *       vCis;          // the array of combinational inputs  (PIs, latches)
    Vec_Ptr_t *       vCos;          // the array of combinational outputs (POs, asserts, latches)
    Vec_Ptr_t *       vPios;         // the array of PIOs
    Vec_Ptr_t *       vBoxes;        // the array of boxes
    // the number of living objects
    int nObjCounts[ABC_OBJ_NUMBER];  // the number of objects by type
    int               nObjs;         // the number of live objs
    // miscellaneous data members
    int               nTravIds;      // the unique traversal IDs of nodes
    Vec_Int_t         vTravIds;      // trav IDs of the objects
    Mem_Fixed_t *     pMmObj;        // memory manager for objects
    Mem_Step_t *      pMmStep;       // memory manager for arrays
    void *            pManFunc;      // functionality manager (AIG manager)
    void *            pManCut;       // the cut manager (for AIGs) stores information about the cuts computed for the nodes
};

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

// transforming floats into ints and back
static inline void        Abc_InfoClear( unsigned * p, int nWords )  { memset( p, 0, sizeof(unsigned) * nWords );   }
static inline void        Abc_InfoFill( unsigned * p, int nWords )   { memset( p, 0xff, sizeof(unsigned) * nWords );}
static inline void        Abc_InfoNot( unsigned * p, int nWords )    { int i; for ( i = nWords - 1; i >= 0; i-- ) p[i] = ~p[i];   }
static inline int         Abc_InfoIsZero( unsigned * p, int nWords ) { int i; for ( i = nWords - 1; i >= 0; i-- ) if ( p[i] )  return 0; return 1; }
static inline int         Abc_InfoIsOne( unsigned * p, int nWords )  { int i; for ( i = nWords - 1; i >= 0; i-- ) if ( ~p[i] ) return 0; return 1; }
static inline void        Abc_InfoCopy( unsigned * p, unsigned * q, int nWords )   { int i; for ( i = nWords - 1; i >= 0; i-- ) p[i]  = q[i];  }
static inline void        Abc_InfoAnd( unsigned * p, unsigned * q, int nWords )    { int i; for ( i = nWords - 1; i >= 0; i-- ) p[i] &= q[i];  }
static inline void        Abc_InfoOr( unsigned * p, unsigned * q, int nWords )     { int i; for ( i = nWords - 1; i >= 0; i-- ) p[i] |= q[i];  }
static inline void        Abc_InfoXor( unsigned * p, unsigned * q, int nWords )    { int i; for ( i = nWords - 1; i >= 0; i-- ) p[i] ^= q[i];  }
static inline int         Abc_InfoIsOrOne( unsigned * p, unsigned * q, int nWords ){ int i; for ( i = nWords - 1; i >= 0; i-- ) if ( ~(p[i] | q[i]) ) return 0; return 1; }
static inline int         Abc_InfoIsOrOne3( unsigned * p, unsigned * q, unsigned * r, int nWords ){ int i; for ( i = nWords - 1; i >= 0; i-- ) if ( ~(p[i] | q[i] | r[i]) ) return 0; return 1; }

// getting the number of objects
static inline int         Abc_NtkObjNum( Abc_Ntk_t * pNtk )          { return pNtk->nObjs;                        }
static inline int         Abc_NtkObjNumMax( Abc_Ntk_t * pNtk )       { return Vec_PtrSize(pNtk->vObjs);           }
static inline int         Abc_NtkPiNum( Abc_Ntk_t * pNtk )           { return Vec_PtrSize(pNtk->vPis);            }
static inline int         Abc_NtkPoNum( Abc_Ntk_t * pNtk )           { return Vec_PtrSize(pNtk->vPos);            }
static inline int         Abc_NtkCiNum( Abc_Ntk_t * pNtk )           { return Vec_PtrSize(pNtk->vCis);            }
static inline int         Abc_NtkCoNum( Abc_Ntk_t * pNtk )           { return Vec_PtrSize(pNtk->vCos);            }
static inline int         Abc_NtkBoxNum( Abc_Ntk_t * pNtk )          { return Vec_PtrSize(pNtk->vBoxes);          }
static inline int         Abc_NtkBiNum( Abc_Ntk_t * pNtk )           { return pNtk->nObjCounts[ABC_OBJ_BI];       }
static inline int         Abc_NtkBoNum( Abc_Ntk_t * pNtk )           { return pNtk->nObjCounts[ABC_OBJ_BO];       }
static inline int         Abc_NtkNodeNum( Abc_Ntk_t * pNtk )         { return pNtk->nObjCounts[ABC_OBJ_NODE];     }
static inline int         Abc_NtkLatchNum( Abc_Ntk_t * pNtk )        { return pNtk->nObjCounts[ABC_OBJ_LATCH];    }
static inline int         Abc_NtkIsComb( Abc_Ntk_t * pNtk )          { return Abc_NtkLatchNum(pNtk) == 0;                   }
static inline int         Abc_NtkHasOnlyLatchBoxes(Abc_Ntk_t * pNtk ){ return Abc_NtkLatchNum(pNtk) == Abc_NtkBoxNum(pNtk); }

// creating simple objects
extern Abc_Obj_t * Abc_NtkCreateObj( Abc_Ntk_t * pNtk, Abc_ObjType_t Type );
static inline Abc_Obj_t * Abc_NtkCreatePi( Abc_Ntk_t * pNtk )        { return Abc_NtkCreateObj( pNtk, ABC_OBJ_PI );         }
static inline Abc_Obj_t * Abc_NtkCreatePo( Abc_Ntk_t * pNtk )        { return Abc_NtkCreateObj( pNtk, ABC_OBJ_PO );         }
static inline Abc_Obj_t * Abc_NtkCreateBi( Abc_Ntk_t * pNtk )        { return Abc_NtkCreateObj( pNtk, ABC_OBJ_BI );         }
static inline Abc_Obj_t * Abc_NtkCreateBo( Abc_Ntk_t * pNtk )        { return Abc_NtkCreateObj( pNtk, ABC_OBJ_BO );         }
static inline Abc_Obj_t * Abc_NtkCreateNode( Abc_Ntk_t * pNtk )      { return Abc_NtkCreateObj( pNtk, ABC_OBJ_NODE );       }
static inline Abc_Obj_t * Abc_NtkCreateLatch( Abc_Ntk_t * pNtk )     { return Abc_NtkCreateObj( pNtk, ABC_OBJ_LATCH );      }

// reading objects
static inline Abc_Obj_t * Abc_NtkObj( Abc_Ntk_t * pNtk, int i )      { return (Abc_Obj_t *)Vec_PtrEntry( pNtk->vObjs, i );   }
static inline Abc_Obj_t * Abc_NtkPi( Abc_Ntk_t * pNtk, int i )       { return (Abc_Obj_t *)Vec_PtrEntry( pNtk->vPis, i );    }
static inline Abc_Obj_t * Abc_NtkPo( Abc_Ntk_t * pNtk, int i )       { return (Abc_Obj_t *)Vec_PtrEntry( pNtk->vPos, i );    }
static inline Abc_Obj_t * Abc_NtkCi( Abc_Ntk_t * pNtk, int i )       { return (Abc_Obj_t *)Vec_PtrEntry( pNtk->vCis, i );    }
static inline Abc_Obj_t * Abc_NtkCo( Abc_Ntk_t * pNtk, int i )       { return (Abc_Obj_t *)Vec_PtrEntry( pNtk->vCos, i );    }
static inline Abc_Obj_t * Abc_NtkBox( Abc_Ntk_t * pNtk, int i )      { return (Abc_Obj_t *)Vec_PtrEntry( pNtk->vBoxes, i );  }

// working with complemented attributes of objects
static inline int         Abc_ObjIsComplement( Abc_Obj_t * p )       { return (int )((ABC_PTRUINT_T)p & (ABC_PTRUINT_T)01);             }
static inline Abc_Obj_t * Abc_ObjRegular( Abc_Obj_t * p )            { return (Abc_Obj_t *)((ABC_PTRUINT_T)p & ~(ABC_PTRUINT_T)01);     }
static inline Abc_Obj_t * Abc_ObjNot( Abc_Obj_t * p )                { return (Abc_Obj_t *)((ABC_PTRUINT_T)p ^  (ABC_PTRUINT_T)01);     }
static inline Abc_Obj_t * Abc_ObjNotCond( Abc_Obj_t * p, int c )     { return (Abc_Obj_t *)((ABC_PTRUINT_T)p ^  (ABC_PTRUINT_T)(c!=0)); }

// reading data members of the object
static inline unsigned    Abc_ObjType( Abc_Obj_t * pObj )            { return pObj->Type;               }
static inline unsigned    Abc_ObjId( Abc_Obj_t * pObj )              { return pObj->Id;                 }
static inline int         Abc_ObjLevel( Abc_Obj_t * pObj )           { return pObj->Level;              }
static inline Vec_Int_t * Abc_ObjFaninVec( Abc_Obj_t * pObj )        { return &pObj->vFanins;           }
static inline Vec_Int_t * Abc_ObjFanoutVec( Abc_Obj_t * pObj )       { return &pObj->vFanouts;          }
static inline Abc_Obj_t * Abc_ObjCopy( Abc_Obj_t * pObj )            { return pObj->pCopy;              }
static inline Abc_Ntk_t * Abc_ObjNtk( Abc_Obj_t * pObj )             { return pObj->pNtk;               }
static inline void *      Abc_ObjData( Abc_Obj_t * pObj )            { return pObj->pData;              }
static inline Abc_Obj_t * Abc_ObjEquiv( Abc_Obj_t * pObj )           { return (Abc_Obj_t *)pObj->pData; }
static inline Abc_Obj_t * Abc_ObjCopyCond( Abc_Obj_t * pObj )        { return Abc_ObjRegular(pObj)->pCopy? Abc_ObjNotCond(Abc_ObjRegular(pObj)->pCopy, Abc_ObjIsComplement(pObj)) : NULL;  }

// setting data members of the network
static inline void        Abc_ObjSetLevel( Abc_Obj_t * pObj, int Level )         { pObj->Level =  Level;    }
static inline void        Abc_ObjSetCopy( Abc_Obj_t * pObj, Abc_Obj_t * pCopy )  { pObj->pCopy =  pCopy;    }

// checking the object type
static inline int         Abc_ObjIsNone( Abc_Obj_t * pObj )          { return pObj->Type == ABC_OBJ_NONE;    }
static inline int         Abc_ObjIsPi( Abc_Obj_t * pObj )            { return pObj->Type == ABC_OBJ_PI;      }
static inline int         Abc_ObjIsPo( Abc_Obj_t * pObj )            { return pObj->Type == ABC_OBJ_PO;      }
static inline int         Abc_ObjIsBi( Abc_Obj_t * pObj )            { return pObj->Type == ABC_OBJ_BI;      }
static inline int         Abc_ObjIsBo( Abc_Obj_t * pObj )            { return pObj->Type == ABC_OBJ_BO;      }
static inline int         Abc_ObjIsCi( Abc_Obj_t * pObj )            { return pObj->Type == ABC_OBJ_PI || pObj->Type == ABC_OBJ_BO; }
static inline int         Abc_ObjIsCo( Abc_Obj_t * pObj )            { return pObj->Type == ABC_OBJ_PO || pObj->Type == ABC_OBJ_BI; }
static inline int         Abc_ObjIsTerm( Abc_Obj_t * pObj )          { return Abc_ObjIsCi(pObj) || Abc_ObjIsCo(pObj); }
static inline int         Abc_ObjIsNode( Abc_Obj_t * pObj )          { return pObj->Type == ABC_OBJ_NODE;    }
static inline int         Abc_ObjIsLatch( Abc_Obj_t * pObj )         { return pObj->Type == ABC_OBJ_LATCH;   }
static inline int         Abc_ObjIsBox( Abc_Obj_t * pObj )           { return pObj->Type == ABC_OBJ_LATCH;   }

// working with fanin/fanout edges
static inline int         Abc_ObjFaninNum( Abc_Obj_t * pObj )        { return pObj->vFanins.nSize;     }
static inline int         Abc_ObjFanoutNum( Abc_Obj_t * pObj )       { return pObj->vFanouts.nSize;    }
static inline int         Abc_ObjFaninId( Abc_Obj_t * pObj, int i)   { return pObj->vFanins.pArray[i]; }
static inline int         Abc_ObjFaninId0( Abc_Obj_t * pObj )        { return pObj->vFanins.pArray[0]; }
static inline int         Abc_ObjFaninId1( Abc_Obj_t * pObj )        { return pObj->vFanins.pArray[1]; }
static inline int         Abc_ObjFanoutEdgeNum( Abc_Obj_t * pObj, Abc_Obj_t * pFanout )  { if ( Abc_ObjFaninId0(pFanout) == pObj->Id ) return 0; if ( Abc_ObjFaninId1(pFanout) == pObj->Id ) return 1; assert( 0 ); return -1;  }
static inline Abc_Obj_t * Abc_ObjFanout( Abc_Obj_t * pObj, int i )   { return (Abc_Obj_t *)pObj->pNtk->vObjs->pArray[ pObj->vFanouts.pArray[i] ];  }
static inline Abc_Obj_t * Abc_ObjFanout0( Abc_Obj_t * pObj )         { return (Abc_Obj_t *)pObj->pNtk->vObjs->pArray[ pObj->vFanouts.pArray[0] ];  }
static inline Abc_Obj_t * Abc_ObjFanin( Abc_Obj_t * pObj, int i )    { return (Abc_Obj_t *)pObj->pNtk->vObjs->pArray[ pObj->vFanins.pArray[i] ];   }
static inline Abc_Obj_t * Abc_ObjFanin0( Abc_Obj_t * pObj )          { return (Abc_Obj_t *)pObj->pNtk->vObjs->pArray[ pObj->vFanins.pArray[0] ];   }
static inline Abc_Obj_t * Abc_ObjFanin1( Abc_Obj_t * pObj )          { return (Abc_Obj_t *)pObj->pNtk->vObjs->pArray[ pObj->vFanins.pArray[1] ];   }
static inline Abc_Obj_t * Abc_ObjFanin0Ntk( Abc_Obj_t * pObj )       { return pObj;  }
static inline Abc_Obj_t * Abc_ObjFanout0Ntk( Abc_Obj_t * pObj )      { return pObj;  }
static inline int         Abc_ObjFaninC0( Abc_Obj_t * pObj )         { return pObj->fCompl0;                                                }
static inline int         Abc_ObjFaninC1( Abc_Obj_t * pObj )         { return pObj->fCompl1;                                                }
static inline int         Abc_ObjFaninC( Abc_Obj_t * pObj, int i )   { assert( i >=0 && i < 2 ); return i? pObj->fCompl1 : pObj->fCompl0;   }
static inline void        Abc_ObjSetFaninC( Abc_Obj_t * pObj, int i ){ assert( i >=0 && i < 2 ); if ( i ) pObj->fCompl1 = 1; else pObj->fCompl0 = 1; }
static inline void        Abc_ObjXorFaninC( Abc_Obj_t * pObj, int i ){ assert( i >=0 && i < 2 ); if ( i ) pObj->fCompl1^= 1; else pObj->fCompl0^= 1; }
static inline Abc_Obj_t * Abc_ObjChild( Abc_Obj_t * pObj, int i )    { return Abc_ObjNotCond( Abc_ObjFanin(pObj,i), Abc_ObjFaninC(pObj,i) );}
static inline Abc_Obj_t * Abc_ObjChild0( Abc_Obj_t * pObj )          { return Abc_ObjNotCond( Abc_ObjFanin0(pObj), Abc_ObjFaninC0(pObj) );  }
static inline Abc_Obj_t * Abc_ObjChild1( Abc_Obj_t * pObj )          { return Abc_ObjNotCond( Abc_ObjFanin1(pObj), Abc_ObjFaninC1(pObj) );  }
static inline Abc_Obj_t * Abc_ObjChildCopy( Abc_Obj_t * pObj, int i ){ return Abc_ObjNotCond( Abc_ObjFanin(pObj,i)->pCopy, Abc_ObjFaninC(pObj,i) );  }
static inline Abc_Obj_t * Abc_ObjChild0Copy( Abc_Obj_t * pObj )      { return Abc_ObjNotCond( Abc_ObjFanin0(pObj)->pCopy, Abc_ObjFaninC0(pObj) );    }
static inline Abc_Obj_t * Abc_ObjChild1Copy( Abc_Obj_t * pObj )      { return Abc_ObjNotCond( Abc_ObjFanin1(pObj)->pCopy, Abc_ObjFaninC1(pObj) );    }
static inline Abc_Obj_t * Abc_ObjChild0Data( Abc_Obj_t * pObj )      { return Abc_ObjNotCond( (Abc_Obj_t *)Abc_ObjFanin0(pObj)->pData, Abc_ObjFaninC0(pObj) );    }
static inline Abc_Obj_t * Abc_ObjChild1Data( Abc_Obj_t * pObj )      { return Abc_ObjNotCond( (Abc_Obj_t *)Abc_ObjFanin1(pObj)->pData, Abc_ObjFaninC1(pObj) );    }
static inline Abc_Obj_t * Abc_ObjFromLit( Abc_Ntk_t * p, int iLit )  { return Abc_ObjNotCond( Abc_NtkObj(p, Abc_Lit2Var(iLit)), Abc_LitIsCompl(iLit) );           }
static inline int         Abc_ObjToLit( Abc_Obj_t * p )              { return Abc_Var2Lit( Abc_ObjId(Abc_ObjRegular(p)), Abc_ObjIsComplement(p) );                }

// checking the AIG node types
static inline int         Abc_AigNodeIsConst( Abc_Obj_t * pNode )    { return Abc_ObjRegular(pNode)->Type == ABC_OBJ_CONST1;       }
static inline int         Abc_AigNodeIsAnd( Abc_Obj_t * pNode )      { assert(!Abc_ObjIsComplement(pNode)); return Abc_ObjFaninNum(pNode) == 2;                         }

// working with the traversal ID
static inline void        Abc_NtkIncrementTravId( Abc_Ntk_t * p )           { if (!p->vTravIds.pArray) Vec_IntFill(&p->vTravIds, Abc_NtkObjNumMax(p)+500, 0); p->nTravIds++; assert(p->nTravIds < (1<<30));  }
static inline int         Abc_NodeTravId( Abc_Obj_t * p )                   { return Vec_IntGetEntry(&Abc_ObjNtk(p)->vTravIds, Abc_ObjId(p));                       }
static inline void        Abc_NodeSetTravId( Abc_Obj_t * p, int TravId )    { Vec_IntSetEntry(&Abc_ObjNtk(p)->vTravIds, Abc_ObjId(p), TravId );                     }
static inline void        Abc_NodeSetTravIdCurrent( Abc_Obj_t * p )         { Abc_NodeSetTravId( p, Abc_ObjNtk(p)->nTravIds );                                      }
static inline void        Abc_NodeSetTravIdPrevious( Abc_Obj_t * p )        { Abc_NodeSetTravId( p, Abc_ObjNtk(p)->nTravIds-1 );                                    }
static inline int         Abc_NodeIsTravIdCurrent( Abc_Obj_t * p )          { return (Abc_NodeTravId(p) == Abc_ObjNtk(p)->nTravIds);                                }
static inline int         Abc_NodeIsTravIdPrevious( Abc_Obj_t * p )         { return (Abc_NodeTravId(p) == Abc_ObjNtk(p)->nTravIds-1);                              }
static inline void        Abc_NodeSetTravIdCurrentId( Abc_Ntk_t * p, int i) { Vec_IntSetEntry(&p->vTravIds, i, p->nTravIds );                                       }
static inline int         Abc_NodeIsTravIdCurrentId( Abc_Ntk_t * p, int i)  { return (Vec_IntGetEntry(&p->vTravIds, i) == p->nTravIds);                             }

// checking initial state of the latches
static inline void        Abc_LatchSetInitNone( Abc_Obj_t * pLatch ) { assert(Abc_ObjIsLatch(pLatch)); pLatch->pData = (void *)ABC_INIT_NONE;                       }
static inline void        Abc_LatchSetInit0( Abc_Obj_t * pLatch )    { assert(Abc_ObjIsLatch(pLatch)); pLatch->pData = (void *)ABC_INIT_ZERO;                       }
static inline void        Abc_LatchSetInit1( Abc_Obj_t * pLatch )    { assert(Abc_ObjIsLatch(pLatch)); pLatch->pData = (void *)ABC_INIT_ONE;                        }
static inline void        Abc_LatchSetInitDc( Abc_Obj_t * pLatch )   { assert(Abc_ObjIsLatch(pLatch)); pLatch->pData = (void *)ABC_INIT_DC;                         }
static inline int         Abc_LatchIsInitNone( Abc_Obj_t * pLatch )  { assert(Abc_ObjIsLatch(pLatch)); return pLatch->pData == (void *)ABC_INIT_NONE;               }
static inline int         Abc_LatchIsInit0( Abc_Obj_t * pLatch )     { assert(Abc_ObjIsLatch(pLatch)); return pLatch->pData == (void *)ABC_INIT_ZERO;               }
static inline int         Abc_LatchIsInit1( Abc_Obj_t * pLatch )     { assert(Abc_ObjIsLatch(pLatch)); return pLatch->pData == (void *)ABC_INIT_ONE;                }
static inline int         Abc_LatchIsInitDc( Abc_Obj_t * pLatch )    { assert(Abc_ObjIsLatch(pLatch)); return pLatch->pData == (void *)ABC_INIT_DC;                 }
static inline int         Abc_LatchInit( Abc_Obj_t * pLatch )        { assert(Abc_ObjIsLatch(pLatch)); return (int)(ABC_PTRINT_T)pLatch->pData;                     }

////////////////////////////////////////////////////////////////////////
///                        ITERATORS                                 ///
////////////////////////////////////////////////////////////////////////

// objects of the network
#define Abc_NtkForEachObj( pNtk, pObj, i )                                                         \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pObj) = Abc_NtkObj(pNtk, i)), 1); i++ )    \
        if ( (pObj) == NULL ) {} else
#define Abc_NtkForEachObjReverse( pNtk, pNode, i )                                                 \
    for ( i = Vec_PtrSize((pNtk)->vObjs) - 1; (i >= 0) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i-- ) \
        if ( (pNode) == NULL ) {} else
#define Abc_NtkForEachObjVec( vIds, pNtk, pObj, i )                                                \
    for ( i = 0; i < Vec_IntSize(vIds) && (((pObj) = Abc_NtkObj(pNtk, Vec_IntEntry(vIds,i))), 1); i++ ) \
        if ( (pObj) == NULL ) {} else
#define Abc_NtkForEachObjVecStart( vIds, pNtk, pObj, i, Start )                                    \
    for ( i = Start; i < Vec_IntSize(vIds) && (((pObj) = Abc_NtkObj(pNtk, Vec_IntEntry(vIds,i))), 1); i++ ) \
        if ( (pObj) == NULL ) {} else
#define Abc_NtkForEachNet( pNtk, pNet, i )                                                         \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNet) = Abc_NtkObj(pNtk, i)), 1); i++ )    \
        if ( (pNet) == NULL || !Abc_ObjIsNet(pNet) ) {} else
#define Abc_NtkForEachNode( pNtk, pNode, i )                                                       \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pNode) == NULL || !Abc_ObjIsNode(pNode) ) {} else
#define Abc_NtkForEachNodeNotBarBuf( pNtk, pNode, i )                                              \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pNode) == NULL || !Abc_ObjIsNode(pNode) ) {} else
#define Abc_NtkForEachNode1( pNtk, pNode, i )                                                      \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pNode) == NULL || !Abc_ObjIsNode(pNode) || !Abc_ObjFaninNum(pNode) ) {} else
#define Abc_NtkForEachNodeNotBarBuf1( pNtk, pNode, i )                                             \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pNode) == NULL || !Abc_ObjIsNode(pNode) || !Abc_ObjFaninNum(pNode) ) {} else
#define Abc_NtkForEachNodeReverse( pNtk, pNode, i )                                                \
    for ( i = Vec_PtrSize((pNtk)->vObjs) - 1; (i >= 0) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i-- ) \
        if ( (pNode) == NULL || !Abc_ObjIsNode(pNode) ) {} else
#define Abc_NtkForEachNodeReverse1( pNtk, pNode, i )                                               \
    for ( i = Vec_PtrSize((pNtk)->vObjs) - 1; (i >= 0) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i-- ) \
        if ( (pNode) == NULL || !Abc_ObjIsNode(pNode) || !Abc_ObjFaninNum(pNode) ) {} else
#define Abc_NtkForEachGate( pNtk, pNode, i )                                                       \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pNode) == NULL || !Abc_ObjIsGate(pNode) ) {} else
#define Abc_AigForEachAnd( pNtk, pNode, i )                                                        \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pNode) == NULL || !Abc_AigNodeIsAnd(pNode) ) {} else
#define Abc_NtkForEachNodeCi( pNtk, pNode, i )                                                       \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pNode) == NULL || (!Abc_ObjIsNode(pNode) && !Abc_ObjIsCi(pNode)) ) {} else
#define Abc_NtkForEachNodeCo( pNtk, pNode, i )                                                       \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vObjs)) && (((pNode) = Abc_NtkObj(pNtk, i)), 1); i++ )   \
        if ( (pNode) == NULL || (!Abc_ObjIsNode(pNode) && !Abc_ObjIsCo(pNode)) ) {} else
// various boxes
#define Abc_NtkForEachBox( pNtk, pObj, i )                                                         \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vBoxes)) && (((pObj) = Abc_NtkBox(pNtk, i)), 1); i++ )
#define Abc_NtkForEachLatch( pNtk, pObj, i )                                                       \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vBoxes)) && (((pObj) = Abc_NtkBox(pNtk, i)), 1); i++ )   \
        if ( !Abc_ObjIsLatch(pObj) ) {} else
#define Abc_NtkForEachLatchInput( pNtk, pObj, i )                                                  \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vBoxes)); i++ )                                          \
        if ( !(Abc_ObjIsLatch(Abc_NtkBox(pNtk, i)) && (((pObj) = Abc_ObjFanin0(Abc_NtkBox(pNtk, i))), 1)) ) {} else
#define Abc_NtkForEachLatchOutput( pNtk, pObj, i )                                                 \
    for ( i = 0; (i < Vec_PtrSize((pNtk)->vBoxes)); i++ )                                          \
        if ( !(Abc_ObjIsLatch(Abc_NtkBox(pNtk, i)) && (((pObj) = Abc_ObjFanout0(Abc_NtkBox(pNtk, i))), 1)) ) {} else
// inputs and outputs
#define Abc_NtkForEachPi( pNtk, pPi, i )                                                           \
    for ( i = 0; (i < Abc_NtkPiNum(pNtk)) && (((pPi) = Abc_NtkPi(pNtk, i)), 1); i++ )
#define Abc_NtkForEachCi( pNtk, pCi, i )                                                           \
    for ( i = 0; (i < Abc_NtkCiNum(pNtk)) && (((pCi) = Abc_NtkCi(pNtk, i)), 1); i++ )
#define Abc_NtkForEachPo( pNtk, pPo, i )                                                           \
    for ( i = 0; (i < Abc_NtkPoNum(pNtk)) && (((pPo) = Abc_NtkPo(pNtk, i)), 1); i++ )
#define Abc_NtkForEachCo( pNtk, pCo, i )                                                           \
    for ( i = 0; (i < Abc_NtkCoNum(pNtk)) && (((pCo) = Abc_NtkCo(pNtk, i)), 1); i++ )
// fanin and fanouts
#define Abc_ObjForEachFanin( pObj, pFanin, i )                                                     \
    for ( i = 0; (i < Abc_ObjFaninNum(pObj)) && (((pFanin) = Abc_ObjFanin(pObj, i)), 1); i++ )
#define Abc_ObjForEachFanout( pObj, pFanout, i )                                                   \
    for ( i = 0; (i < Abc_ObjFanoutNum(pObj)) && (((pFanout) = Abc_ObjFanout(pObj, i)), 1); i++ )
#define Abc_ObjForEachFaninId( pObj, iFanin, i )                                                   \
    for ( i = 0; (i < Abc_ObjFaninNum(pObj)) && (((iFanin) = Abc_ObjFaninId(pObj, i)), 1); i++ )
#define Abc_ObjForEachFanoutId( pObj, iFanout, i )                                                 \
    for ( i = 0; (i < Abc_ObjFanoutNum(pObj)) && (((iFanout) = Abc_ObjFanoutId(pObj, i)), 1); i++ )
// cubes and literals
#define Abc_CubeForEachVar( pCube, Value, i )                                                      \
    for ( i = 0; (pCube[i] != ' ') && (Value = pCube[i]); i++ )
#define Abc_SopForEachCube( pSop, nFanins, pCube )                                                 \
    for ( pCube = (pSop); *pCube; pCube += (nFanins) + 3 )
#define Abc_SopForEachCubePair( pSop, nFanins, pCube, pCube2 )                                     \
    Abc_SopForEachCube( pSop, nFanins, pCube )                                                     \
    Abc_SopForEachCube( pCube + (nFanins) + 3, nFanins, pCube2 )

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== abcAig.c ==========================================================*/
extern Abc_Aig_t *        Abc_AigAlloc( Abc_Ntk_t * pNtk );
extern void               Abc_AigFree( Abc_Aig_t * pMan );
extern int                Abc_AigCleanup( Abc_Aig_t * pMan );
extern int                Abc_AigCheck( Abc_Aig_t * pMan );
extern Abc_Obj_t *        Abc_AigConst1( Abc_Ntk_t * pNtk );
extern Abc_Obj_t *        Abc_AigAnd( Abc_Aig_t * pMan, Abc_Obj_t * p0, Abc_Obj_t * p1 );
extern Abc_Obj_t *        Abc_AigAndLookup( Abc_Aig_t * pMan, Abc_Obj_t * p0, Abc_Obj_t * p1 );
extern void               Abc_AigReplace( Abc_Aig_t * pMan, Abc_Obj_t * pOld, Abc_Obj_t * pNew );
extern void               Abc_AigDeleteNode( Abc_Aig_t * pMan, Abc_Obj_t * pOld );
extern void               Abc_AigRehash( Abc_Aig_t * pMan );
extern int                Abc_AigNodeIsAcyclic( Abc_Obj_t * pNode, Abc_Obj_t * pRoot );
/*=== abcBalance.c ==========================================================*/
extern Abc_Ntk_t *        Abc_NtkBalance( Abc_Ntk_t * pNtk, int fDuplicate, int fSelective );
/*=== abcCheck.c ==========================================================*/
extern int                Abc_NtkCheck( Abc_Ntk_t * pNtk );
extern int                Abc_NtkCheckRead( Abc_Ntk_t * pNtk );
extern int                Abc_NtkDoCheck( Abc_Ntk_t * pNtk );
extern int                Abc_NtkCheckObj( Abc_Ntk_t * pNtk, Abc_Obj_t * pObj );
extern int                Abc_NtkCheckUniqueCiNames( Abc_Ntk_t * pNtk );
extern int                Abc_NtkCheckUniqueCoNames( Abc_Ntk_t * pNtk );
extern int                Abc_NtkCheckUniqueCioNames( Abc_Ntk_t * pNtk );
/*=== abcCut.c ==========================================================*/
extern void *             Abc_NodeGetCutsRecursive( void * p, Abc_Obj_t * pObj );
extern void *             Abc_NodeGetCuts( void * p, Abc_Obj_t * pObj );
extern void *             Abc_NodeReadCuts( void * p, Abc_Obj_t * pObj );
extern void               Abc_NodeFreeCuts( void * p, Abc_Obj_t * pObj );
/*=== abcDfs.c ==========================================================*/
extern Vec_Ptr_t *        Abc_NtkDfs( Abc_Ntk_t * pNtk, int fCollectAll );
extern int                Abc_NtkIsDfsOrdered( Abc_Ntk_t * pNtk );
extern Vec_Ptr_t *        Abc_AigDfs( Abc_Ntk_t * pNtk, int fCollectAll, int fCollectCos );
extern int                Abc_NtkLevel( Abc_Ntk_t * pNtk );
extern int                Abc_NtkIsAcyclic( Abc_Ntk_t * pNtk );
/*=== abcFanio.c ==========================================================*/
extern void               Abc_ObjAddFanin( Abc_Obj_t * pObj, Abc_Obj_t * pFanin );
extern void               Abc_ObjDeleteFanin( Abc_Obj_t * pObj, Abc_Obj_t * pFanin );
extern void               Abc_ObjRemoveFanins( Abc_Obj_t * pObj );
extern void               Abc_ObjPatchFanin( Abc_Obj_t * pObj, Abc_Obj_t * pFaninOld, Abc_Obj_t * pFaninNew );
/*=== abcNames.c ====================================================*/
extern char *             Abc_ObjName( Abc_Obj_t * pNode );
extern char *             Abc_ObjAssignName( Abc_Obj_t * pObj, char * pName, char * pSuffix );
extern void               Abc_NtkTransferNameIds( Abc_Ntk_t * p, Abc_Ntk_t * pNew );
/*=== abcNtk.c ==========================================================*/
extern Abc_Ntk_t *        Abc_NtkAlloc( int fUseMemMan );
extern Abc_Ntk_t *        Abc_NtkStartFrom( Abc_Ntk_t * pNtk );
extern void               Abc_NtkFinalize( Abc_Ntk_t * pNtk );
extern void               Abc_NtkDelete( Abc_Ntk_t * pNtk );
/*=== abcObj.c ==========================================================*/
extern Abc_Obj_t *        Abc_ObjAlloc( Abc_Ntk_t * pNtk, Abc_ObjType_t Type );
extern void               Abc_ObjRecycle( Abc_Obj_t * pObj );
extern Abc_Obj_t *        Abc_NtkCreateObj( Abc_Ntk_t * pNtk, Abc_ObjType_t Type );
extern void               Abc_NtkDeleteObj( Abc_Obj_t * pObj );
extern Abc_Obj_t *        Abc_NtkDupObj( Abc_Ntk_t * pNtkNew, Abc_Obj_t * pObj, int fCopyName );
extern Abc_Obj_t *        Abc_NtkDupBox( Abc_Ntk_t * pNtkNew, Abc_Obj_t * pBox, int fCopyName );
/*=== abcReconv.c ==========================================================*/
extern Abc_ManCut_t *     Abc_NtkManCutStart( int nNodeSizeMax, int nConeSizeMax, int nNodeFanStop, int nConeFanStop );
extern void               Abc_NtkManCutStop( Abc_ManCut_t * p );
extern Vec_Ptr_t *        Abc_NtkManCutReadCutLarge( Abc_ManCut_t * p );
extern Vec_Ptr_t *        Abc_NodeFindCut( Abc_ManCut_t * p, Abc_Obj_t * pRoot, int  fContain );
extern void               Abc_NodeConeCollect( Abc_Obj_t ** ppRoots, int nRoots, Vec_Ptr_t * vFanins, Vec_Ptr_t * vVisited, int fIncludeFanins );
/*=== abcRefs.c ==========================================================*/
extern int                Abc_NodeMffcLabelAig( Abc_Obj_t * pNode );
/*=== abcRefactor.c ==========================================================*/
extern int                Abc_NtkRefactor( Abc_Ntk_t * pNtk, int nNodeSizeMax, int nConeSizeMax, int fUseZeros, int fUseDcs );
/*=== abcRewrite.c ==========================================================*/
extern int                Abc_NtkRewrite( Abc_Ntk_t * pNtk, int fUseZeros, int fPrecompute );
/*=== abcStrash.c ==========================================================*/
extern Abc_Ntk_t *        Abc_NtkRestrashZero( Abc_Ntk_t * pNtk );
/*=== abcDar.c ==========================================================*/
extern Abc_Ntk_t *        Abc_NtkDRewrite( Dar_Lib_t * pDarLib, Abc_Ntk_t * pNtk, Dar_RwrPar_t * pPars );
extern Abc_Ntk_t *        Abc_NtkDRefactor( Abc_Ntk_t * pNtk, Dar_RefPar_t * pPars );
/*=== abcUtil.c ==========================================================*/
extern void               Abc_NtkCleanCopy( Abc_Ntk_t * pNtk );
extern void               Abc_NtkCleanNext( Abc_Ntk_t * pNtk );
extern Abc_Obj_t *        Abc_NodeFindCoFanout( Abc_Obj_t * pNode );
extern void               Abc_VecObjPushUniqueOrderByLevel( Vec_Ptr_t * p, Abc_Obj_t * pNode );
extern int                Abc_NodeIsExorType( Abc_Obj_t * pNode );
extern int                Abc_NodeIsMuxControlType( Abc_Obj_t * pNode );
extern void               Abc_NodeCollectFanins( Abc_Obj_t * pNode, Vec_Ptr_t * vNodes );
extern void               Abc_NodeCollectFanouts( Abc_Obj_t * pNode, Vec_Ptr_t * vNodes );
extern int                Abc_NodeCompareLevelsDecrease( Abc_Obj_t ** pp1, Abc_Obj_t ** pp2 );
extern void               Abc_NtkReassignIds( Abc_Ntk_t * pNtk );

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
