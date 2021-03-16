/**CFile****************************************************************

  FileName    [cmd.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Command processing package.]

  Synopsis    [External declarations of the command package.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: cmd.h,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#ifndef ABC__base__cmd__cmd_h
#define ABC__base__cmd__cmd_h

////////////////////////////////////////////////////////////////////////
///                    STRUCTURE DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

typedef struct MvCommand    Abc_Command;  // one command

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== cmdApi.c ========================================================*/
typedef int (*Cmd_CommandFuncType)(Abc_Frame_t*, int, char**);
extern void        Cmd_CommandAdd( Abc_Frame_t * pAbc, const char * sGroup, const char * sName, Cmd_CommandFuncType pFunc, int fChanges );

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
