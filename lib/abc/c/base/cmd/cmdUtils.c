/**CFile****************************************************************

  FileName    [cmdUtils.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Command processing package.]

  Synopsis    [Various utilities of the command package.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: cmdUtils.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/mainInt.h"
#include "cmdInt.h"
#include <ctype.h>

    // proper declaration of isspace

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static int CmdCommandPrintCompare( Abc_Command ** ppC1, Abc_Command ** ppC2 );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Executes one command.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int CmdCommandDispatch( Abc_Frame_t * pAbc, int * pargc, char *** pargv )
{
    int argc = *pargc;
    char ** argv = *pargv;

    int (*pFunc) ( Abc_Frame_t *, int, char ** );
    Abc_Command * pCommand;
    int fError;

    if ( argc == 0 )
        return 0;

    // get the command
    if ( ! st__lookup( pAbc->tCommands, argv[0], (char **)&pCommand ) )
    {   // the command is not in the table
        fprintf( pAbc->Err, "** cmd error: unknown command '%s'\n", argv[0] );
        fprintf( pAbc->Err, "(this is likely caused by using an alias defined in \"abc.rc\"\n" );
        fprintf( pAbc->Err, "without having this file in the current or parent directory)\n" );
        return 1;
    }

    // execute the command
    pFunc = (int (*)(Abc_Frame_t *, int, char **))pCommand->pFunc;
    fError = (*pFunc)( pAbc, argc, argv );

    return fError;
}

/**Function*************************************************************

  Synopsis    [Splits the command line string into individual commands.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
const char * CmdSplitLine( Abc_Frame_t * pAbc, const char *sCommand, int *argc, char ***argv )
{
    const char *p, *start;
    char c;
    int i, j;
    char *new_arg;
    Vec_Ptr_t * vArgs;
    int single_quote, double_quote;

    vArgs = Vec_PtrAlloc( 10 );

    p = sCommand;
    for ( ;; )
    {
        // skip leading white space
        while ( isspace( ( int ) *p ) )
        {
            p++;
        }

        // skip until end of this token
        single_quote = double_quote = 0;
        for ( start = p; ( c = *p ) != '\0'; p++ )
        {
            if ( c == ';' || c == '#' || isspace( ( int ) c ) )
            {
                if ( !single_quote && !double_quote )
                {
                    break;
                }
            }
            if ( c == '\'' )
            {
                single_quote = !single_quote;
            }
            if ( c == '"' )
            {
                double_quote = !double_quote;
            }
        }
        if ( single_quote || double_quote )
        {
            ( void ) fprintf( pAbc->Err, "** cmd warning: ignoring unbalanced quote ...\n" );
        }
        if ( start == p )
            break;

        new_arg = ABC_ALLOC( char, p - start + 1 );
        j = 0;
        for ( i = 0; i < p - start; i++ )
        {
            c = start[i];
            if ( ( c != '\'' ) && ( c != '\"' ) )
            {
                new_arg[j++] = isspace( ( int ) c ) ? ' ' : start[i];
            }
        }
        new_arg[j] = '\0';
        Vec_PtrPush( vArgs, new_arg );
    }

    *argc = vArgs->nSize;
    *argv = (char **)Vec_PtrReleaseArray( vArgs );
    Vec_PtrFree( vArgs );
    if ( *p == ';' )
    {
        p++;
    }
    else if ( *p == '#' )
    {
        for ( ; *p != 0; p++ ); // skip to end of line
    }
    return p;
}

/**Function*************************************************************

  Synopsis    [Frees the previously allocated argv array.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void CmdFreeArgv( int argc, char **argv )
{
    int i;
    for ( i = 0; i < argc; i++ )
        ABC_FREE( argv[i] );
    ABC_FREE( argv );
}

/**Function*************************************************************

  Synopsis    [Frees the previously allocated command.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void CmdCommandFree( Abc_Command * pCommand )
{
    ABC_FREE( pCommand->sGroup );
    ABC_FREE( pCommand->sName );
    ABC_FREE( pCommand );
}

/**Function*************************************************************

  Synopsis    [Prints commands alphabetically by group.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void CmdCommandPrint( Abc_Frame_t * pAbc, int fPrintAll, int fDetails )
{
    const char *key;
    char *value;
    st__generator * gen;
    Abc_Command ** ppCommands;
    Abc_Command * pCommands;
    int nCommands, iGroupStart, i, j;
    char * sGroupCur;
    int LenghtMax, nColumns, iCom = 0;
    FILE *backupErr = pAbc->Err;

    // put all commands into one array
    nCommands = st__count( pAbc->tCommands );
    ppCommands = ABC_ALLOC( Abc_Command *, nCommands );
    i = 0;
    st__foreach_item( pAbc->tCommands, gen, &key, &value )
    {
        pCommands = (Abc_Command *)value;
        if ( fPrintAll || pCommands->sName[0] != '_' )
            ppCommands[i++] = pCommands;
    }
    nCommands = i;

    // sort command by group and then by name, alphabetically
    qsort( (void *)ppCommands, (size_t)nCommands, sizeof(Abc_Command *),
            (int (*)(const void *, const void *)) CmdCommandPrintCompare );
    assert( CmdCommandPrintCompare( ppCommands, ppCommands + nCommands - 1 ) <= 0 );

    // get the longest command name
    LenghtMax = 0;
    for ( i = 0; i < nCommands; i++ )
        if ( LenghtMax < (int)strlen(ppCommands[i]->sName) )
             LenghtMax = (int)strlen(ppCommands[i]->sName);
    // get the number of columns
    nColumns = 79 / (LenghtMax + 2);

    // print the starting message
    fprintf( pAbc->Out, "      Welcome to ABC compiled on %s %s!", __DATE__, __TIME__ );

    // print the command by group
    sGroupCur = NULL;
    iGroupStart = 0;
    pAbc->Err = pAbc->Out;
    for ( i = 0; i < nCommands; i++ )
        if ( sGroupCur && strcmp( sGroupCur, ppCommands[i]->sGroup ) == 0 )
        { // this command belongs to the same group as the previous one
            if ( iCom++ % nColumns == 0 )
                fprintf( pAbc->Out, "\n" );
            // print this command
            fprintf( pAbc->Out, " %-*s", LenghtMax, ppCommands[i]->sName );
        }
        else
        { // this command starts the new group of commands
            // start the new group
            if ( fDetails && i != iGroupStart )
            { // print help messages for all commands in the previous groups
                fprintf( pAbc->Out, "\n" );
                for ( j = iGroupStart; j < i; j++ )
                {
                    char *tmp_cmd;
                    fprintf( pAbc->Out, "\n" );
                    // fprintf( pAbc->Out, "--- %s ---\n", ppCommands[j]->sName );
                    tmp_cmd = ABC_ALLOC(char, strlen(ppCommands[j]->sName)+4);
                    (void) sprintf(tmp_cmd, "%s -h", ppCommands[j]->sName);
                    (void) Cmd_CommandExecute( pAbc, tmp_cmd );
                    ABC_FREE(tmp_cmd);
                }
                fprintf( pAbc->Out, "\n" );
                fprintf( pAbc->Out, "   ----------------------------------------------------------------------" );
                iGroupStart = i;
            }
            fprintf( pAbc->Out, "\n" );
            fprintf( pAbc->Out, "\n" );
            fprintf( pAbc->Out, "%s commands:\n", ppCommands[i]->sGroup );
            // print this command
            fprintf( pAbc->Out, " %-*s", LenghtMax, ppCommands[i]->sName );
            // remember current command group
            sGroupCur = ppCommands[i]->sGroup;
            // reset the command counter
            iCom = 1;
        }
    if ( fDetails && i != iGroupStart )
    { // print help messages for all commands in the previous groups
        fprintf( pAbc->Out, "\n" );
        for ( j = iGroupStart; j < i; j++ )
        {
            char *tmp_cmd;
            fprintf( pAbc->Out, "\n" );
            // fprintf( pAbc->Out, "--- %s ---\n", ppCommands[j]->sName );
            tmp_cmd = ABC_ALLOC(char, strlen(ppCommands[j]->sName)+4);
            (void) sprintf(tmp_cmd, "%s -h", ppCommands[j]->sName);
            (void) Cmd_CommandExecute( pAbc, tmp_cmd );
            ABC_FREE(tmp_cmd);
        }
    }
    pAbc->Err = backupErr;
    fprintf( pAbc->Out, "\n" );
    ABC_FREE( ppCommands );
}

/**Function*************************************************************

  Synopsis    [Comparision function used for sorting commands.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int CmdCommandPrintCompare( Abc_Command ** ppC1, Abc_Command ** ppC2 )
{
    Abc_Command * pC1 = *ppC1;
    Abc_Command * pC2 = *ppC2;
    int RetValue;

    RetValue = strcmp( pC1->sGroup, pC2->sGroup );
    if ( RetValue < 0 )
        return -1;
    if ( RetValue > 0 )
        return 1;
    // the command belong to the same group

    // put commands with "_" at the end of the list
    if ( pC1->sName[0] != '_' && pC2->sName[0] == '_' )
        return -1;
    if ( pC1->sName[0] == '_' && pC2->sName[0] != '_' )
        return 1;

    RetValue = strcmp( pC1->sName, pC2->sName );
    if ( RetValue < 0 )
        return -1;
    if ( RetValue > 0 )
        return 1;
     // should not be two indentical commands
    assert( 0 );
    return 0;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
