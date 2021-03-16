/**CFile****************************************************************

  FileName    [cmd.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Command processing package.]

  Synopsis    [Command file.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: cmd.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#ifdef WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "base/abc/abc.h"
#include "base/main/mainInt.h"
#include "cmdInt.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static int CmdCommandQuit          ( Abc_Frame_t * pAbc, int argc, char ** argv );
static int CmdCommandHelp          ( Abc_Frame_t * pAbc, int argc, char ** argv );
static int CmdCommandEmpty         ( Abc_Frame_t * pAbc, int argc, char ** argv );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function********************************************************************

  Synopsis    [Initializes the command package.]

  SideEffects [Commands are added to the command table.]

  SeeAlso     [Cmd_End]

******************************************************************************/
void Cmd_Init( Abc_Frame_t * pAbc )
{
    pAbc->tCommands = st__init_table(strcmp, st__strhash);

    Cmd_CommandAdd( pAbc, "Basic", "quit",          CmdCommandQuit,            0 );
    Cmd_CommandAdd( pAbc, "Basic", "help",          CmdCommandHelp,            0 );
    Cmd_CommandAdd( pAbc, "Basic", "empty",         CmdCommandEmpty,           0 );
}

/**Function********************************************************************

  Synopsis    [Ends the command package.]

  Description [Ends the command package. Tables are freed.]

  SideEffects []

  SeeAlso     []

******************************************************************************/
void Cmd_End( Abc_Frame_t * pAbc )
{
    st__generator * gen;
    char * pKey, * pValue;

    st__foreach_item( pAbc->tCommands, gen, (const char **)&pKey, (char **)&pValue )
        CmdCommandFree( (Abc_Command *)pValue );
    st__free_table( pAbc->tCommands );
}

/**Function********************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

******************************************************************************/
int CmdCommandQuit( Abc_Frame_t * pAbc, int argc, char **argv )
{
    int c;

    struct UtilOpt opt;
    Extra_UtilGetoptReset( &opt );
    while ( ( c = Extra_UtilGetopt( &opt, argc, argv, "hs" ) ) != EOF )
    {
        switch ( c )
        {
        case 'h':
            goto usage;
            break;
        case 's':
            return -2;
            break;
        default:
            goto usage;
        }
    }

    if ( argc != opt.ind )
        goto usage;

    return -1;

  usage:
    fprintf( pAbc->Err, "usage: quit [-sh]\n" );
    fprintf( pAbc->Err, "   -h  print the command usage\n" );
    fprintf( pAbc->Err, "   -s  frees all the memory before quitting\n" );
    return 1;
}

/**Function********************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

******************************************************************************/
int CmdCommandHelp( Abc_Frame_t * pAbc, int argc, char **argv )
{
    int fPrintAll, fDetails;
    int c;

    fPrintAll = 0;
    fDetails = 0;
    struct UtilOpt opt;
    Extra_UtilGetoptReset( &opt );
    while ( ( c = Extra_UtilGetopt( &opt, argc, argv, "adh" ) ) != EOF )
    {
        switch ( c )
        {
        case 'a':
            case 'v':
                fPrintAll ^= 1;
                break;
            break;
        case 'd':
            fDetails ^= 1;
            break;
        case 'h':
            goto usage;
            break;
        default:
            goto usage;
        }
    }

    if ( argc != opt.ind )
        goto usage;

    CmdCommandPrint( pAbc, fPrintAll, fDetails );
    return 0;

usage:
    fprintf( pAbc->Err, "usage: help [-a] [-d] [-h]\n" );
    fprintf( pAbc->Err, "       prints the list of available commands by group\n" );
    fprintf( pAbc->Err, " -a       toggle printing hidden commands [default = %s]\n", fPrintAll? "yes": "no" );
    fprintf( pAbc->Err, " -d       print usage details to all commands [default = %s]\n", fDetails? "yes": "no" );
    fprintf( pAbc->Err, " -h       print the command usage\n" );
    return 1;
}

/**Function********************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

******************************************************************************/
int CmdCommandEmpty( Abc_Frame_t * pAbc, int argc, char **argv )
{
    int c;

    struct UtilOpt opt;
    Extra_UtilGetoptReset( &opt );
    while ( ( c = Extra_UtilGetopt( &opt, argc, argv, "h" ) ) != EOF )
    {
        switch ( c )
        {
            case 'h':
                goto usage;
            default:
                goto usage;
        }
    }

    Abc_FrameDeleteAllNetworks( pAbc );
    return 0;
usage:

    fprintf( pAbc->Err, "usage: empty [-h]\n" );
    fprintf( pAbc->Err, "         removes all the currently stored networks\n" );
    fprintf( pAbc->Err, "   -h :  print the command usage\n");
    return 1;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
