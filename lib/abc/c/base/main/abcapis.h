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
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

// procedures to start and stop the ABC framework
extern Abc_Frame_t * Abc_Start();
extern void          Abc_Stop( Abc_Frame_t * pAbc );

// procedures to load and store networks
extern Abc_Ntk_t *   Io_LoadAiger( aiger * pAiger, int fCheck );
extern aiger *       Io_StoreAiger( Abc_Ntk_t * pNtk, int fCheck );

// procedures to interact with main frame
extern void          Abc_FrameReplaceNetwork( Abc_Frame_t * p, Abc_Ntk_t * pNet );
extern Abc_Ntk_t *   Abc_FrameReadNtk( Abc_Frame_t * p );
extern Dar_Lib_t *   Abc_FrameReadDarLib( Abc_Frame_t * p );
extern void          Abc_FrameDeleteNetwork( Abc_Frame_t * p );

// commands
extern Abc_Ntk_t *   Abc_NtkBalance( Abc_Ntk_t * pNtk, int fDuplicate, int fSelective );
extern int           Abc_NtkRefactor( Abc_Ntk_t * pNtk, int nNodeSizeMax, int nConeSizeMax, int fUseZeros, int fUseDcs );
extern int           Abc_NtkRewrite( Abc_Ntk_t * pNtk, int fUseZeros, int fPrecompute );
extern int           Abc_NtkResubstitute( Abc_Ntk_t * pNtk, int nCutsMax, int nNodesMax );
extern Abc_Ntk_t *   Abc_NtkRestrashZero( Abc_Ntk_t * pNtk );
extern Abc_Ntk_t *   Abc_NtkDRewrite( Dar_Lib_t * pDarLib, Abc_Ntk_t * pNtk, Dar_RwrPar_t * pPars );
extern Abc_Ntk_t *   Abc_NtkDRefactor( Abc_Ntk_t * pNtk, Dar_RefPar_t * pPars );
extern int           Abc_NtkNetworkSize( Abc_Ntk_t * pNtk );

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
