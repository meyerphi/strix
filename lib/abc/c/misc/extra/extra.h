/**CFile****************************************************************

  FileName    [extra.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [extra]

  Synopsis    [Various reusable software utilities.]

  Description [This library contains a number of operators and
  traversal routines developed to extend the functionality of
  CUDD v.2.3.x, by Fabio Somenzi (http://vlsi.colorado.edu/~fabio/)
  To compile your code with the library, #include "extra.h"
  in your source files and link your project to CUDD and this
  library. Use the library at your own risk and with caution.
  Note that debugging of some operators still continues.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: extra.h,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#ifndef ABC__misc__extra__extra_h
#define ABC__misc__extra__extra_h

#ifdef _WIN32
#define inline __inline // compatible with MS VS 6.0
#endif

/*---------------------------------------------------------------------------*/
/* Nested includes                                                           */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "misc/st/st.h"

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

typedef struct UtilOpt UtilOpt_t;
struct UtilOpt {
    char *  arg;
    int     ind;
    char *  pScanStr;
};

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;

/*===========================================================================*/
/*     Various Utilities                                                     */
/*===========================================================================*/

/*=== extraUtilBitMatrix.c ================================================================*/

typedef struct Extra_BitMat_t_ Extra_BitMat_t;
extern Extra_BitMat_t * Extra_BitMatrixStart( int nSize );
extern void         Extra_BitMatrixClean( Extra_BitMat_t * p );
extern void         Extra_BitMatrixStop( Extra_BitMat_t * p );
extern void         Extra_BitMatrixPrint( Extra_BitMat_t * p );
extern int          Extra_BitMatrixReadSize( Extra_BitMat_t * p );
extern void         Extra_BitMatrixInsert1( Extra_BitMat_t * p, int i, int k );
extern int          Extra_BitMatrixLookup1( Extra_BitMat_t * p, int i, int k );
extern void         Extra_BitMatrixDelete1( Extra_BitMat_t * p, int i, int k );
extern void         Extra_BitMatrixInsert2( Extra_BitMat_t * p, int i, int k );
extern int          Extra_BitMatrixLookup2( Extra_BitMat_t * p, int i, int k );
extern void         Extra_BitMatrixDelete2( Extra_BitMat_t * p, int i, int k );
extern void         Extra_BitMatrixOr( Extra_BitMat_t * p, int i, unsigned * pInfo );
extern void         Extra_BitMatrixOrTwo( Extra_BitMat_t * p, int i, int j );
extern int          Extra_BitMatrixCountOnesUpper( Extra_BitMat_t * p );
extern int          Extra_BitMatrixIsDisjoint( Extra_BitMat_t * p1, Extra_BitMat_t * p2 );
extern int          Extra_BitMatrixIsClique( Extra_BitMat_t * p );

/*=== extraUtilFile.c ========================================================*/

extern char *       Extra_FileGetSimilarName( char * pFileNameWrong, char * pS1, char * pS2, char * pS3, char * pS4, char * pS5 );
extern char *       Extra_FileNameAppend( char * pBase, char * pSuffix );
extern char *       Extra_FileNameWithoutPath( char * FileName );
extern int          Extra_FileSize( char * pFileName );
extern char *       Extra_TimeStamp();

/*=== extraUtilReader.c ========================================================*/

typedef struct Extra_FileReader_t_ Extra_FileReader_t;
extern Extra_FileReader_t * Extra_FileReaderAlloc( char * pFileName,
    char * pCharsComment, char * pCharsStop, char * pCharsClean );
extern void         Extra_FileReaderFree( Extra_FileReader_t * p );
extern char *       Extra_FileReaderGetFileName( Extra_FileReader_t * p );
extern int          Extra_FileReaderGetFileSize( Extra_FileReader_t * p );
extern int          Extra_FileReaderGetCurPosition( Extra_FileReader_t * p );
extern void *       Extra_FileReaderGetTokens( Extra_FileReader_t * p );
extern int          Extra_FileReaderGetLineNumber( Extra_FileReader_t * p, int iToken );

/*=== extraUtilMemory.c ========================================================*/

typedef struct Extra_MmFixed_t_    Extra_MmFixed_t;
typedef struct Extra_MmFlex_t_     Extra_MmFlex_t;
typedef struct Extra_MmStep_t_     Extra_MmStep_t;

// fixed-size-block memory manager
extern Extra_MmFixed_t *  Extra_MmFixedStart( int nEntrySize );
extern void        Extra_MmFixedStop( Extra_MmFixed_t * p );
extern char *      Extra_MmFixedEntryFetch( Extra_MmFixed_t * p );
extern void        Extra_MmFixedEntryRecycle( Extra_MmFixed_t * p, char * pEntry );
// flexible-size-block memory manager
extern Extra_MmFlex_t * Extra_MmFlexStart();
extern void        Extra_MmFlexStop( Extra_MmFlex_t * p );
extern char *      Extra_MmFlexEntryFetch( Extra_MmFlex_t * p, int nBytes );

/*=== extraUtilMisc.c ========================================================*/

/* the factorial of number */
extern int         Extra_Factorial( int n );
/* the permutation of the given number of elements */
extern char **     Extra_Permutations( int n );
/* permutation and complementation of a truth table */
unsigned           Extra_TruthPermute( unsigned Truth, char * pPerms, int nVars, int fReverse );
unsigned           Extra_TruthPolarize( unsigned uTruth, int Polarity, int nVars );
/* canonical forms of a truth table */
extern unsigned    Extra_TruthCanonP( unsigned uTruth, int nVars );
extern unsigned    Extra_TruthCanonNP( unsigned uTruth, int nVars );
extern unsigned    Extra_TruthCanonNPN( unsigned uTruth, int nVars );
/* canonical forms of 4-variable functions */
extern void        Extra_Truth4VarNPN( unsigned short ** puCanons, char ** puPhases, char ** puPerms, unsigned char ** puMap );
extern void        Extra_Truth4VarN( unsigned short ** puCanons, char *** puPhases, char ** ppCounters, int nPhasesMax );
/* precomputing tables for permutation mapping */
extern void **     Extra_ArrayAlloc( int nCols, int nRows, int Size );

/*=== extraUtilCanon.c ========================================================*/

/* fast computation of N-canoninical form up to 6 inputs */
extern int         Extra_TruthCanonFastN( int nVarsMax, int nVarsReal, unsigned * pt, unsigned ** pptRes, char ** ppfRes );

/*=== extraUtilTruth.c ================================================================*/

static inline int   Extra_BitWordNum( int nBits )    { return nBits/(8*sizeof(unsigned)) + ((nBits%(8*sizeof(unsigned))) > 0);  }
static inline int   Extra_TruthWordNum( int nVars )  { return nVars <= 5 ? 1 : (1 << (nVars - 5)); }

static inline void  Extra_TruthSetBit( unsigned * p, int Bit )   { p[Bit>>5] |= (unsigned)(1<<(Bit & 31));               }
static inline void  Extra_TruthXorBit( unsigned * p, int Bit )   { p[Bit>>5] ^= (unsigned)(1<<(Bit & 31));               }
static inline int   Extra_TruthHasBit( unsigned * p, int Bit )   { return (p[Bit>>5] & (unsigned)(1<<(Bit & 31))) > 0;   }

static inline void Extra_TruthCopy( unsigned * pOut, unsigned * pIn, int nVars )
{
    int w;
    for ( w = Extra_TruthWordNum(nVars)-1; w >= 0; w-- )
        pOut[w] = pIn[w];
}
static inline void Extra_TruthNot( unsigned * pOut, unsigned * pIn, int nVars )
{
    int w;
    for ( w = Extra_TruthWordNum(nVars)-1; w >= 0; w-- )
        pOut[w] = ~pIn[w];
}
static inline void Extra_TruthAnd( unsigned * pOut, unsigned * pIn0, unsigned * pIn1, int nVars )
{
    int w;
    for ( w = Extra_TruthWordNum(nVars)-1; w >= 0; w-- )
        pOut[w] = pIn0[w] & pIn1[w];
}
static inline void Extra_TruthNand( unsigned * pOut, unsigned * pIn0, unsigned * pIn1, int nVars )
{
    int w;
    for ( w = Extra_TruthWordNum(nVars)-1; w >= 0; w-- )
        pOut[w] = ~(pIn0[w] & pIn1[w]);
}

extern void        Extra_TruthStretch( unsigned * pOut, unsigned * pIn, int nVars, int nVarsAll, unsigned Phase );

/*=== extraUtilUtil.c ================================================================*/

extern void          Extra_UtilGetoptReset( UtilOpt_t * pOpt );
extern int           Extra_UtilGetopt(  UtilOpt_t * pOpt, int argc, char *argv[], const char *optstring );
extern char *        Extra_UtilStrsav( const char *s );

#endif /* __EXTRA_H__ */
