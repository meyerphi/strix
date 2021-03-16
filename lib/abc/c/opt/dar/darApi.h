#ifndef ABC__aig__dar__darApi_h
#define ABC__aig__dar__darApi_h

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Dar_RwrPar_t_         Dar_RwrPar_t;
typedef struct Dar_RefPar_t_         Dar_RefPar_t;
typedef struct Dar_LibObj_t_         Dar_LibObj_t;
typedef struct Dar_LibDat_t_         Dar_LibDat_t;
typedef struct Dar_Lib_t_            Dar_Lib_t;

struct Dar_RwrPar_t_
{
    int              nCutsMax;       // the maximum number of cuts to try
    int              nSubgMax;       // the maximum number of subgraphs to try
    int              fUseZeros;      // performs zero-cost replacement
    int              fRecycle;       // enables cut recycling
};

struct Dar_RefPar_t_
{
    int              nMffcMin;       // the min MFFC size for which refactoring is used
    int              nLeafMax;       // the max number of leaves of a cut
    int              nCutsMax;       // the max number of cuts to consider
    int              fExtend;        // extends the cut below MFFC
    int              fUseZeros;      // perform zero-cost replacements
};

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== darCore.c ========================================================*/
extern void            Dar_ManDefaultRwrParams( Dar_RwrPar_t * pPars );
/*=== darRefact.c ========================================================*/
extern void            Dar_ManDefaultRefParams( Dar_RefPar_t * pPars );

#endif
