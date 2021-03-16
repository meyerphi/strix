/**CFile****************************************************************

  FileName    [ioAbc.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Command processing package.]

  Synopsis    [External declarations.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: ioAbc.h,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#ifndef ABC__base__io__ioAbc_h
#define ABC__base__io__ioAbc_h

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include "base/abc/abc.h"
#include "misc/extra/extra.h"
#include "aiger.h"

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

// network functionality
typedef enum {
    IO_FILE_NONE = 0,
    IO_FILE_AIGER,
    IO_FILE_BAF,
    IO_FILE_BBLIF,
    IO_FILE_BLIF,
    IO_FILE_BLIFMV,
    IO_FILE_BENCH,
    IO_FILE_BOOK,
    IO_FILE_CNF,
    IO_FILE_DOT,
    IO_FILE_EDIF,
    IO_FILE_EQN,
    IO_FILE_GML,
    IO_FILE_JSON,
    IO_FILE_LIST,
    IO_FILE_PLA,
    IO_FILE_MOPLA,
    IO_FILE_SMV,
    IO_FILE_VERILOG,
    IO_FILE_UNKNOWN
} Io_FileType_t;

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

#define  IO_WRITE_LINE_LENGTH    78    // the output line length

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== abcReadAiger.c ==========================================================*/
extern Abc_Ntk_t *        Io_ReadAiger( char * pFileName, int fCheck );
extern Abc_Ntk_t *        Io_LoadAiger( aiger * pAiger, int fCheck );
/*=== abcWriteAiger.c =========================================================*/
extern void               Io_WriteAiger( Abc_Ntk_t * pNtk, char * pFileName, int fWriteSymbols );
/*=== abcUtil.c ===============================================================*/
extern Abc_Ntk_t *        Io_Read( char * pFileName, Io_FileType_t FileType, int fCheck );

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
