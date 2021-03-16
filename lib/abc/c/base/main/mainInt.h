/**CFile****************************************************************

  FileName    [mainInt.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [The main package.]

  Synopsis    [Internal declarations of the main package.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: mainInt.h,v 1.1 2008/05/14 22:13:13 wudenni Exp $]

***********************************************************************/

#ifndef ABC__base__main__mainInt_h
#define ABC__base__main__mainInt_h

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "aig/aig/aig.h"
#include "bool/dec/dec.h"
#include "opt/dar/dar.h"

////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////

// the current version
#define ABC_VERSION "UC Berkeley, ABC 1.01"

// the maximum length of an input line
#define ABC_MAX_STR     (1<<15)

////////////////////////////////////////////////////////////////////////
///                    STRUCTURE DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

typedef void (*Abc_Frame_Callback_BmcFrameDone_Func)(int frame, int po, int status);

struct Abc_Frame_t_
{
    // commands and flags
    st__table *     tCommands;     // the command table
    // the functionality
    Abc_Ntk_t *     pNtkCur;       // the current network

    // output streams
    FILE *          Out;
    FILE *          Err;

    // rewriting library
    Dar_Lib_t *     pDarLib;
};

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== mvInit.c ===================================================*/
extern ABC_DLL void            Abc_FrameInit( Abc_Frame_t * pAbc );
extern ABC_DLL void            Abc_FrameEnd( Abc_Frame_t * pAbc );
/*=== mvFrame.c =====================================================*/
extern ABC_DLL Abc_Frame_t *   Abc_FrameAllocate();
extern ABC_DLL void            Abc_FrameDeallocate( Abc_Frame_t * p );
extern ABC_DLL char *          Abc_UtilsGetUsersInput( char * buf, int bufLen );

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
