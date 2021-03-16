/**CFile****************************************************************

  FileName    [io.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Command processing package.]

  Synopsis    [Command file.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: io.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "ioAbc.h"
#include "base/main/mainInt.h"

#ifdef WIN32
#include <process.h>
#define unlink _unlink
#else
#include <unistd.h>
#endif

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static int IoCommandReadAiger   ( Abc_Frame_t * pAbc, int argc, char **argv );
static int IoCommandWriteAiger  ( Abc_Frame_t * pAbc, int argc, char **argv );

extern void Abc_FrameCopyLTLDataBase( Abc_Frame_t *pAbc, Abc_Ntk_t * pNtk );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Io_Init( Abc_Frame_t * pAbc )
{
    Cmd_CommandAdd( pAbc, "I/O", "read_aiger",    IoCommandReadAiger,    1 );
    Cmd_CommandAdd( pAbc, "I/O", "write_aiger",   IoCommandWriteAiger,   0 );
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Io_End()
{
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int IoCommandReadAiger( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk;
    char * pFileName;
    int fCheck;
    int c;

    fCheck = 1;
    struct UtilOpt opt;
    Extra_UtilGetoptReset( &opt );
    while ( ( c = Extra_UtilGetopt( &opt, argc, argv, "ch" ) ) != EOF )
    {
        switch ( c )
        {
            case 'c':
                fCheck ^= 1;
                break;
            case 'h':
                goto usage;
            default:
                goto usage;
        }
    }
    if ( argc != opt.ind + 1 )
        goto usage;
    // get the input file name
    pFileName = argv[opt.ind];
    // read the file using the corresponding file reader
    pNtk = Io_Read( pFileName, IO_FILE_AIGER, fCheck );
    if ( pNtk == NULL )
        return 1;
    // replace the current network
    Abc_FrameReplaceCurrentNetwork( pAbc, pNtk );
    return 0;

usage:
    fprintf( pAbc->Err, "usage: read_aiger [-ch] <file>\n" );
    fprintf( pAbc->Err, "\t         reads the network in the AIGER format (http://fmv.jku.at/aiger)\n" );
    fprintf( pAbc->Err, "\t-c     : toggle network check after reading [default = %s]\n", fCheck? "yes":"no" );
    fprintf( pAbc->Err, "\t-h     : prints the command summary\n" );
    fprintf( pAbc->Err, "\tfile   : the name of a file to read\n" );
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int IoCommandWriteAiger( Abc_Frame_t * pAbc, int argc, char **argv )
{
    char * pFileName;
    int fWriteSymbols;
    int c;

    fWriteSymbols = 0;
    struct UtilOpt opt;
    Extra_UtilGetoptReset( &opt );
    while ( ( c = Extra_UtilGetopt( &opt, argc, argv, "sh" ) ) != EOF )
    {
        switch ( c )
        {
            case 's':
                fWriteSymbols ^= 1;
                break;
            case 'h':
                goto usage;
            default:
                goto usage;
        }
    }
    if ( pAbc->pNtkCur == NULL )
    {
        fprintf( pAbc->Out, "Empty network.\n" );
        return 0;
    }
    if ( argc != opt.ind + 1 )
        goto usage;
    // get the output file name
    pFileName = argv[opt.ind];
    // call the corresponding file writer
    Io_WriteAiger( pAbc->pNtkCur, pFileName, fWriteSymbols );
    return 0;

usage:
    fprintf( pAbc->Err, "usage: write_aiger [-sh] <file>\n" );
    fprintf( pAbc->Err, "\t         writes the network in the AIGER format (http://fmv.jku.at/aiger)\n" );
    fprintf( pAbc->Err, "\t-s     : toggle saving I/O names [default = %s]\n", fWriteSymbols? "yes" : "no" );
    fprintf( pAbc->Err, "\t-h     : print the help massage\n" );
    fprintf( pAbc->Err, "\tfile   : the name of the file to write (extension .aig)\n" );
    return 1;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
