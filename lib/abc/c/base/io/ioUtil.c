/**CFile****************************************************************

  FileName    [ioUtil.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Command processing package.]

  Synopsis    [Procedures to write the network in BENCH format.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: ioUtil.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "ioAbc.h"
#include "base/main/main.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Read the network from a file.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Io_ReadNetlist( char * pFileName, Io_FileType_t FileType, int fCheck )
{
    FILE * pFile;
    Abc_Ntk_t * pNtk;
    if ( FileType == IO_FILE_NONE || FileType == IO_FILE_UNKNOWN )
    {
        fprintf( stdout, "Generic file reader requires a known file extension to open \"%s\".\n", pFileName );
        return NULL;
    }
    // check if the file exists
    pFile = fopen( pFileName, "r" );
    if ( pFile == NULL )
    {
       fprintf( stdout, "Cannot open input file \"%s\".\n", pFileName );
       return NULL;
    }
    fclose( pFile );
    // read the AIG
    assert( FileType == IO_FILE_AIGER);
    pNtk = Io_ReadAiger( pFileName, fCheck );
    if ( pNtk == NULL )
    {
        fprintf( stdout, "Reading AIG from file has failed.\n" );
        return NULL;
    }
    return pNtk;
}

/**Function*************************************************************

  Synopsis    [Read the network from a file.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Io_Read( char * pFileName, Io_FileType_t FileType, int fCheck )
{
    Abc_Ntk_t * pNtk;
    // get the netlist
    pNtk = Io_ReadNetlist( pFileName, FileType, fCheck );
    if ( pNtk == NULL )
        return NULL;
    return pNtk;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
