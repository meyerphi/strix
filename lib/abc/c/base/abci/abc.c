/**CFile****************************************************************

  FileName    [abc.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Command file.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abc.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "opt/dar/dar.h"
#include "bool/kit/kit.h"
#include "base/cmd/cmd.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static int Abc_CommandBalance                ( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Abc_CommandRewrite                ( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Abc_CommandRefactor               ( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Abc_CommandResubstitute           ( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Abc_CommandDRewrite               ( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Abc_CommandDRefactor              ( Abc_Frame_t * pAbc, int argc, char ** argv );
static int Abc_CommandZero                   ( Abc_Frame_t * pAbc, int argc, char ** argv );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_Init( Abc_Frame_t * pAbc )
{
    Cmd_CommandAdd( pAbc, "Synthesis",    "balance",       Abc_CommandBalance,          1 );
    Cmd_CommandAdd( pAbc, "Synthesis",    "rewrite",       Abc_CommandRewrite,          1 );
    Cmd_CommandAdd( pAbc, "Synthesis",    "refactor",      Abc_CommandRefactor,         1 );
    Cmd_CommandAdd( pAbc, "Synthesis",    "resub",         Abc_CommandResubstitute,     1 );
    Cmd_CommandAdd( pAbc, "Sequential",   "zero",          Abc_CommandZero,             1 );
    Cmd_CommandAdd( pAbc, "New AIG",      "drw",           Abc_CommandDRewrite,         1 );
    Cmd_CommandAdd( pAbc, "New AIG",      "drf",           Abc_CommandDRefactor,        1 );

    extern Dar_Lib_t * Dar_LibStart();
    pAbc->pDarLib = Dar_LibStart();
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_End( Abc_Frame_t * pAbc )
{
    extern void Dar_LibStop();
    Dar_LibStop( pAbc->pDarLib );
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandBalance( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkRes;
    int c;
    int fDuplicate;
    int fSelective;
    pNtk = Abc_FrameReadNtk(pAbc);

    // set defaults
    fDuplicate   = 0;
    fSelective   = 0;
    struct UtilOpt opt;
    Extra_UtilGetoptReset( &opt );
    while ( ( c = Extra_UtilGetopt( &opt, argc, argv, "dsh" ) ) != EOF )
    {
        switch ( c )
        {
        case 'd':
            fDuplicate ^= 1;
            break;
        case 's':
            fSelective ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }
    // get the new network
    pNtkRes = Abc_NtkBalance( pNtk, fDuplicate, fSelective );

    // check if balancing worked
    if ( pNtkRes == NULL )
    {
        Abc_Print( -1, "Balancing has failed.\n" );
        return 1;
    }
    // replace the current network
    Abc_FrameReplaceCurrentNetwork( pAbc, pNtkRes );
    return 0;

usage:
    Abc_Print( -2, "usage: balance [-dsh]\n" );
    Abc_Print( -2, "\t        transforms the current network into a well-balanced AIG\n" );
    Abc_Print( -2, "\t-d    : toggle duplication of logic [default = %s]\n", fDuplicate? "yes": "no" );
    Abc_Print( -2, "\t-s    : toggle duplication on the critical paths [default = %s]\n", fSelective? "yes": "no" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandRewrite( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    int fPrecompute;
    int fUseZeros;

    // set defaults
    fPrecompute  = 0;
    fUseZeros    = 0;
    struct UtilOpt opt;
    Extra_UtilGetoptReset( &opt );
    while ( ( c = Extra_UtilGetopt( &opt, argc, argv, "xzh" ) ) != EOF )
    {
        switch ( c )
        {
        case 'x':
            fPrecompute ^= 1;
            break;
        case 'z':
            fUseZeros ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }

    // modify the current network
    if ( !Abc_NtkRewrite( pNtk, fUseZeros, fPrecompute) )
    {
        Abc_Print( -1, "Rewriting has failed.\n" );
        return 1;
    }
    return 0;

usage:
    Abc_Print( -2, "usage: rewrite [-xzh]\n" );
    Abc_Print( -2, "\t         performs technology-independent rewriting of the AIG\n" );
    Abc_Print( -2, "\t-x     : precompute\n" );
    Abc_Print( -2, "\t-z     : toggle using zero-cost replacements [default = %s]\n", fUseZeros? "yes": "no" );
    Abc_Print( -2, "\t-h     : print the command usage\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandRefactor( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int c;
    int nNodeSizeMax;
    int nConeSizeMax;
    int fUseZeros;
    int fUseDcs;

    // set defaults
    nNodeSizeMax = 10;
    nConeSizeMax = 16;
    fUseZeros    =  0;
    fUseDcs      =  0;
    struct UtilOpt opt;
    Extra_UtilGetoptReset( &opt );
    while ( ( c = Extra_UtilGetopt( &opt, argc, argv, "NCzdh" ) ) != EOF )
    {
        switch ( c )
        {
        case 'N':
            if ( opt.ind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-N\" should be followed by an integer.\n" );
                goto usage;
            }
            nNodeSizeMax = atoi(argv[opt.ind]);
            opt.ind++;
            if ( nNodeSizeMax < 0 )
                goto usage;
            break;
        case 'C':
            if ( opt.ind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-C\" should be followed by an integer.\n" );
                goto usage;
            }
            nConeSizeMax = atoi(argv[opt.ind]);
            opt.ind++;
            if ( nConeSizeMax < 0 )
                goto usage;
            break;
        case 'z':
            fUseZeros ^= 1;
            break;
        case 'd':
            fUseDcs ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }
    if ( nNodeSizeMax > 15 )
    {
        Abc_Print( -1, "The cone size cannot exceed 15.\n" );
        return 1;
    }

    if ( fUseDcs && nNodeSizeMax >= nConeSizeMax )
    {
        Abc_Print( -1, "For don't-care to work, containing cone should be larger than collapsed node.\n" );
        return 1;
    }

    // modify the current network
    if ( !Abc_NtkRefactor( pNtk, nNodeSizeMax, nConeSizeMax, fUseZeros, fUseDcs ) )
    {
        Abc_Print( -1, "Refactoring has failed.\n" );
        return 1;
    }
    return 0;

usage:
    Abc_Print( -2, "usage: refactor [-N <num>] [-C <num>] [-zdh]\n" );
    Abc_Print( -2, "\t           performs technology-independent refactoring of the AIG\n" );
    Abc_Print( -2, "\t-N <num> : the max support of the collapsed node [default = %d]\n", nNodeSizeMax );
    Abc_Print( -2, "\t-C <num> : the max support of the containing cone [default = %d]\n", nConeSizeMax );
    Abc_Print( -2, "\t-z       : toggle using zero-cost replacements [default = %s]\n", fUseZeros? "yes": "no" );
    Abc_Print( -2, "\t-d       : toggle using don't-cares [default = %s]\n", fUseDcs? "yes": "no" );
    Abc_Print( -2, "\t-h       : print the command usage\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandResubstitute( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk = Abc_FrameReadNtk(pAbc);
    int RS_CUT_MIN =  4;
    int RS_CUT_MAX = 16;
    int c;
    int nCutsMax;
    int nNodesMax;

    // set defaults
    nCutsMax     =  8;
    nNodesMax    =  1;
    struct UtilOpt opt;
    Extra_UtilGetoptReset( &opt );
    while ( ( c = Extra_UtilGetopt( &opt, argc, argv, "KNh" ) ) != EOF )
    {
        switch ( c )
        {
        case 'K':
            if ( opt.ind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-K\" should be followed by an integer.\n" );
                goto usage;
            }
            nCutsMax = atoi(argv[opt.ind]);
            opt.ind++;
            if ( nCutsMax < 0 )
                goto usage;
            break;
        case 'N':
            if ( opt.ind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-N\" should be followed by an integer.\n" );
                goto usage;
            }
            nNodesMax = atoi(argv[opt.ind]);
            opt.ind++;
            if ( nNodesMax < 0 )
                goto usage;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }
    if ( nCutsMax < RS_CUT_MIN || nCutsMax > RS_CUT_MAX )
    {
        Abc_Print( -1, "Can only compute cuts for %d <= K <= %d.\n", RS_CUT_MIN, RS_CUT_MAX );
        return 1;
    }
    if ( nNodesMax < 0 || nNodesMax > 3 )
    {
        Abc_Print( -1, "Can only resubstitute at most 3 nodes.\n" );
        return 1;
    }

    // modify the current network
    if ( !Abc_NtkResubstitute( pNtk, nCutsMax, nNodesMax ) )
    {
        Abc_Print( -1, "Refactoring has failed.\n" );
        return 1;
    }
    return 0;

usage:
    Abc_Print( -2, "usage: resub [-K <num>] [-N <num>] [-h]\n" );
    Abc_Print( -2, "\t           performs technology-independent restructuring of the AIG\n" );
    Abc_Print( -2, "\t-K <num> : the max cut size (%d <= num <= %d) [default = %d]\n", RS_CUT_MIN, RS_CUT_MAX, nCutsMax );
    Abc_Print( -2, "\t-N <num> : the max number of nodes to add (0 <= num <= 3) [default = %d]\n", nNodesMax );
    Abc_Print( -2, "\t-h       : print the command usage\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandDRewrite( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkRes;
    Dar_RwrPar_t Pars, * pPars = &Pars;
    Dar_Lib_t * pDarLib;
    int c;

    pNtk = Abc_FrameReadNtk(pAbc);
    pDarLib = Abc_FrameReadDarLib(pAbc);
    // set defaults
    Dar_ManDefaultRwrParams( pPars );
    struct UtilOpt opt;
    Extra_UtilGetoptReset( &opt );
    while ( ( c = Extra_UtilGetopt( &opt, argc, argv, "CNzrh" ) ) != EOF )
    {
        switch ( c )
        {
        case 'C':
            if ( opt.ind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-C\" should be followed by an integer.\n" );
                goto usage;
            }
            pPars->nCutsMax = atoi(argv[opt.ind]);
            opt.ind++;
            if ( pPars->nCutsMax < 0 )
                goto usage;
            break;
        case 'N':
            if ( opt.ind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-N\" should be followed by an integer.\n" );
                goto usage;
            }
            pPars->nSubgMax = atoi(argv[opt.ind]);
            opt.ind++;
            if ( pPars->nSubgMax < 0 )
                goto usage;
            break;
        case 'z':
            pPars->fUseZeros ^= 1;
            break;
        case 'r':
            pPars->fRecycle ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }
    pNtkRes = Abc_NtkDRewrite( pDarLib, pNtk, pPars );
    if ( pNtkRes == NULL )
    {
        Abc_Print( -1, "Command has failed.\n" );
        return 0;
    }
    // replace the current network
    Abc_FrameReplaceCurrentNetwork( pAbc, pNtkRes );
    return 0;

usage:
    Abc_Print( -2, "usage: drw [-C num] [-N num] [-zrh]\n" );
    Abc_Print( -2, "\t         performs combinational AIG rewriting\n" );
    Abc_Print( -2, "\t-C num : the max number of cuts at a node [default = %d]\n", pPars->nCutsMax );
    Abc_Print( -2, "\t-N num : the max number of subgraphs tried [default = %d]\n", pPars->nSubgMax );
    Abc_Print( -2, "\t-z     : toggle using zero-cost replacements [default = %s]\n", pPars->fUseZeros? "yes": "no" );
    Abc_Print( -2, "\t-r     : toggle using cut recycling [default = %s]\n", pPars->fRecycle? "yes": "no" );
    Abc_Print( -2, "\t-h     : print the command usage\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandDRefactor( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkRes;
    Dar_RefPar_t Pars, * pPars = &Pars;
    int c;

    pNtk = Abc_FrameReadNtk(pAbc);
    // set defaults
    Dar_ManDefaultRefParams( pPars );
    struct UtilOpt opt;
    Extra_UtilGetoptReset( &opt );
    while ( ( c = Extra_UtilGetopt( &opt, argc, argv, "MKCezh" ) ) != EOF )
    {
        switch ( c )
        {
        case 'M':
            if ( opt.ind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-C\" should be followed by an integer.\n" );
                goto usage;
            }
            pPars->nMffcMin = atoi(argv[opt.ind]);
            opt.ind++;
            if ( pPars->nMffcMin < 0 )
                goto usage;
            break;
        case 'K':
            if ( opt.ind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-N\" should be followed by an integer.\n" );
                goto usage;
            }
            pPars->nLeafMax = atoi(argv[opt.ind]);
            opt.ind++;
            if ( pPars->nLeafMax < 0 )
                goto usage;
            break;
        case 'C':
            if ( opt.ind >= argc )
            {
                Abc_Print( -1, "Command line switch \"-C\" should be followed by an integer.\n" );
                goto usage;
            }
            pPars->nCutsMax = atoi(argv[opt.ind]);
            opt.ind++;
            if ( pPars->nCutsMax < 0 )
                goto usage;
            break;
        case 'e':
            pPars->fExtend ^= 1;
            break;
        case 'z':
            pPars->fUseZeros ^= 1;
            break;
        case 'h':
            goto usage;
        default:
            goto usage;
        }
    }
    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }
    if ( pPars->nLeafMax < 4 || pPars->nLeafMax > 15 )
    {
        Abc_Print( -1, "This command only works for cut sizes 4 <= K <= 15.\n" );
        return 1;
    }
    pNtkRes = Abc_NtkDRefactor( pNtk, pPars );
    if ( pNtkRes == NULL )
    {
        Abc_Print( -1, "Command has failed.\n" );
        return 0;
    }
    // replace the current network
    Abc_FrameReplaceCurrentNetwork( pAbc, pNtkRes );
    return 0;

usage:
    Abc_Print( -2, "usage: drf [-M num] [-K num] [-C num] [-ezh]\n" );
    Abc_Print( -2, "\t         performs combinational AIG refactoring\n" );
    Abc_Print( -2, "\t-M num : the min MFFC size to attempt refactoring [default = %d]\n", pPars->nMffcMin );
    Abc_Print( -2, "\t-K num : the max number of cuts leaves [default = %d]\n", pPars->nLeafMax );
    Abc_Print( -2, "\t-C num : the max number of cuts to try at a node [default = %d]\n", pPars->nCutsMax );
    Abc_Print( -2, "\t-e     : toggle extending the cut below MFFC [default = %s]\n", pPars->fExtend? "yes": "no" );
    Abc_Print( -2, "\t-z     : toggle using zero-cost replacements [default = %s]\n", pPars->fUseZeros? "yes": "no" );
    Abc_Print( -2, "\t-h     : print the command usage\n");
    return 1;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_CommandZero( Abc_Frame_t * pAbc, int argc, char ** argv )
{
    Abc_Ntk_t * pNtk, * pNtkRes;
    int c;
    pNtk = Abc_FrameReadNtk(pAbc);

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

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }

    if ( Abc_NtkIsComb(pNtk) )
    {
        //Abc_Print( 0, "The current network is combinational.\n" );
        return 0;
    }

    // get the new network
    pNtkRes = Abc_NtkRestrashZero( pNtk );
    if ( pNtkRes == NULL )
    {
        Abc_Print( -1, "Converting to sequential AIG has failed.\n" );
        return 1;
    }
    // replace the current network
    Abc_FrameReplaceCurrentNetwork( pAbc, pNtkRes );
    return 0;

usage:
    Abc_Print( -2, "usage: zero [-h]\n" );
    Abc_Print( -2, "\t        converts latches to have const-0 initial value\n" );
    Abc_Print( -2, "\t-h    : print the command usage\n");
    return 1;
}

int Abc_NtkNetworkSize( Abc_Ntk_t * pNtk ) {
    return Abc_NtkNodeNum( pNtk );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
