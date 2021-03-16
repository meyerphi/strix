/**CFile****************************************************************

  FileName    [main.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [The main package.]

  Synopsis    [External declarations of the main package.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: main.h,v 1.1 2008/05/14 22:13:13 wudenni Exp $]

***********************************************************************/

#ifndef ABC__base__main__main_h
#define ABC__base__main__main_h

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

// core packages
#include "base/abc/abc.h"

// data structure packages
#include "misc/vec/vec.h"
#include "misc/st/st.h"

// the framework containing all data is defined here
#include "abcapis.h"

#include "base/cmd/cmd.h"
#include "base/io/ioAbc.h"
#include "opt/dar/dar.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== main.c ===========================================================*/
extern ABC_DLL Abc_Frame_t *   Abc_Start();
extern ABC_DLL void            Abc_Stop( Abc_Frame_t * p );
/*=== mainFrame.c ===========================================================*/
extern ABC_DLL Abc_Ntk_t *     Abc_FrameReadNtk( Abc_Frame_t * p );
extern ABC_DLL Dar_Lib_t *     Abc_FrameReadDarLib( Abc_Frame_t * p );
extern ABC_DLL void            Abc_FrameReplaceCurrentNetwork( Abc_Frame_t * p, Abc_Ntk_t * pNet );
extern ABC_DLL void            Abc_FrameDeleteAllNetworks( Abc_Frame_t * p );

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
