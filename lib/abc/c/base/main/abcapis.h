/**CFile****************************************************************

  FileName    [abcapis.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Include this file in the external code calling ABC.]

  Synopsis    [External declarations.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - September 29, 2012.]

  Revision    [$Id: abcapis.h,v 1.00 2012/09/29 00:00:00 alanmi Exp $]

***********************************************************************/

#ifndef MINI_AIG__abc_apis_h
#define MINI_AIG__abc_apis_h

#include "opt/dar/darApi.h"
#include "aiger.h"

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Abc_Frame_t_      Abc_Frame_t;
typedef struct Abc_Ntk_t_        Abc_Ntk_t;

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

#ifdef WIN32
  #ifdef WIN32_NO_DLL
    #define ABC_DLLEXPORT
    #define ABC_DLLIMPORT
  #else
    #define ABC_DLLEXPORT __declspec(dllexport)
    #define ABC_DLLIMPORT __declspec(dllimport)
  #endif
#else  /* defined(WIN32) */
#define ABC_DLLIMPORT
#endif /* defined(WIN32) */

#ifndef ABC_DLL
#define ABC_DLL ABC_DLLIMPORT
#endif

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

// procedures to start and stop the ABC framework
extern ABC_DLL Abc_Frame_t * Abc_Start();
extern ABC_DLL void          Abc_Stop( Abc_Frame_t * pAbc );

// procedure to execute commands in the ABC framework (pAbc)
extern ABC_DLL int   Cmd_CommandExecute( Abc_Frame_t * pAbc, const char * pCommandLine );

// procedures to load and store networks
extern Abc_Ntk_t *   Io_LoadAiger( aiger * pAiger, int fCheck );
extern aiger *       Io_StoreAiger( Abc_Ntk_t * pNtk, int fCheck );

// procedures to interact with main frame
extern ABC_DLL void            Abc_FrameReplaceCurrentNetwork( Abc_Frame_t * p, Abc_Ntk_t * pNet );
extern ABC_DLL Abc_Ntk_t *     Abc_FrameReadNtk( Abc_Frame_t * p );
extern ABC_DLL Dar_Lib_t *     Abc_FrameReadDarLib( Abc_Frame_t * p );

// commands
extern ABC_DLL Abc_Ntk_t *     Abc_NtkBalance( Abc_Ntk_t * pNtk, int fDuplicate, int fSelective );
extern ABC_DLL int             Abc_NtkRefactor( Abc_Ntk_t * pNtk, int nNodeSizeMax, int nConeSizeMax, int fUseZeros, int fUseDcs );
extern ABC_DLL int             Abc_NtkRewrite( Abc_Ntk_t * pNtk, int fUseZeros, int fPrecompute );
extern ABC_DLL int             Abc_NtkResubstitute( Abc_Ntk_t * pNtk, int nCutsMax, int nNodesMax );
extern ABC_DLL Abc_Ntk_t *     Abc_NtkRestrashZero( Abc_Ntk_t * pNtk );
extern ABC_DLL Abc_Ntk_t *     Abc_NtkDRewrite( Dar_Lib_t * pDarLib, Abc_Ntk_t * pNtk, Dar_RwrPar_t * pPars );
extern ABC_DLL Abc_Ntk_t *     Abc_NtkDRefactor( Abc_Ntk_t * pNtk, Dar_RefPar_t * pPars );
extern ABC_DLL int             Abc_NtkNetworkSize( Abc_Ntk_t * pNtk );

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
