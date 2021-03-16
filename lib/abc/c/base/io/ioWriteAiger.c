/**CFile****************************************************************

  FileName    [ioWriteAiger.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Command processing package.]

  Synopsis    [Procedures to write binary AIGER format developed by
  Armin Biere, Johannes Kepler University (http://fmv.jku.at/)]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - December 16, 2006.]

  Revision    [$Id: ioWriteAiger.c,v 1.00 2006/12/16 00:00:00 alanmi Exp $]

***********************************************************************/

// The code in this file is developed in collaboration with Mark Jarvin of Toronto.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "aig/aig/aig.h"
#include "ioAbc.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

/*
    The following is taken from the AIGER format description,
    which can be found at http://fmv.jku.at/aiger
*/

/*
         The AIGER And-Inverter Graph (AIG) Format Version 20061129
         ----------------------------------------------------------
              Armin Biere, Johannes Kepler University, 2006

  This report describes the AIG file format as used by the AIGER library.
  The purpose of this report is not only to motivate and document the
  format, but also to allow independent implementations of writers and
  readers by giving precise and unambiguous definitions.

  ...

Introduction

  The name AIGER contains as one part the acronym AIG of And-Inverter
  Graphs and also if pronounced in German sounds like the name of the
  'Eiger', a mountain in the Swiss alps.  This choice should emphasize the
  origin of this format. It was first openly discussed at the Alpine
  Verification Meeting 2006 in Ascona as a way to provide a simple, compact
  file format for a model checking competition affiliated to CAV 2007.

  ...

Binary Format Definition

  The binary format is semantically a subset of the ASCII format with a
  slightly different syntax.  The binary format may need to reencode
  literals, but translating a file in binary format into ASCII format and
  then back in to binary format will result in the same file.

  The main differences of the binary format to the ASCII format are as
  follows.  After the header the list of input literals and all the
  current state literals of a latch can be omitted.  Furthermore the
  definitions of the AND gates are binary encoded.  However, the symbol
  table and the comment section are as in the ASCII format.

  The header of an AIGER file in binary format has 'aig' as format
  identifier, but otherwise is identical to the ASCII header.  The standard
  file extension for the binary format is therefore '.aig'.

  A header for the binary format is still in ASCII encoding:

    aig M I L O A

  Constants, variables and literals are handled in the same way as in the
  ASCII format.  The first simplifying restriction is on the variable
  indices of inputs and latches.  The variable indices of inputs come first,
  followed by the pseudo-primary inputs of the latches and then the variable
  indices of all LHS of AND gates:

    input variable indices        1,          2,  ... ,  I
    latch variable indices      I+1,        I+2,  ... ,  (I+L)
    AND variable indices      I+L+1,      I+L+2,  ... ,  (I+L+A) == M

  The corresponding unsigned literals are

    input literals                2,          4,  ... ,  2*I
    latch literals            2*I+2,      2*I+4,  ... ,  2*(I+L)
    AND literals          2*(I+L)+2,  2*(I+L)+4,  ... ,  2*(I+L+A) == 2*M

  All literals have to be defined, and therefore 'M = I + L + A'.  With this
  restriction it becomes possible that the inputs and the current state
  literals of the latches do not have to be listed explicitly.  Therefore,
  after the header only the list of 'L' next state literals follows, one per
  latch on a single line, and then the 'O' outputs, again one per line.

  In the binary format we assume that the AND gates are ordered and respect
  the child parent relation.  AND gates with smaller literals on the LHS
  come first.  Therefore we can assume that the literals on the right-hand
  side of a definition of an AND gate are smaller than the LHS literal.
  Furthermore we can sort the literals on the RHS, such that the larger
  literal comes first.  A definition thus consists of three literals

      lhs rhs0 rhs1

  with 'lhs' even and 'lhs > rhs0 >= rhs1'.  Also the variable indices are
  pairwise different to avoid combinational self loops.  Since the LHS
  indices of the definitions are all consecutive (as even integers),
  the binary format does not have to keep 'lhs'.  In addition, we can use
  the order restriction and only write the differences 'delta0' and 'delta1'
  instead of 'rhs0' and 'rhs1', with

      delta0 = lhs - rhs0,  delta1 = rhs0 - rhs1

  The differences will all be strictly positive, and in practice often very
  small.  We can take advantage of this fact by the simple little-endian
  encoding of unsigned integers of the next section.  After the binary delta
  encoding of the RHSs of all AND gates, the optional symbol table and
  optional comment section start in the same format as in the ASCII case.

  ...

*/

static unsigned Io_ObjMakeLit( int Var, int fCompl )                 { return (Var << 1) | fCompl;                   }
static unsigned Io_ObjAigerNum( Abc_Obj_t * pObj )                   { return (unsigned)(ABC_PTRINT_T)pObj->pCopy;  }
static void     Io_ObjSetAigerNum( Abc_Obj_t * pObj, unsigned Num )  { pObj->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)Num;     }

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Adds one unsigned AIG edge to the output buffer.]

  Description [This procedure is a slightly modified version of Armin Biere's
  procedure "void encode (FILE * file, unsigned x)" ]

  SideEffects [Returns the current writing position.]

  SeeAlso     []

***********************************************************************/
int Io_WriteAigerEncode( unsigned char * pBuffer, int Pos, unsigned x )
{
    unsigned char ch;
    while (x & ~0x7f)
    {
        ch = (x & 0x7f) | 0x80;
//        putc (ch, file);
        pBuffer[Pos++] = ch;
        x >>= 7;
    }
    ch = x;
//    putc (ch, file);
    pBuffer[Pos++] = ch;
    return Pos;
}

/**Function*************************************************************

  Synopsis    [Procedure to write data into BZ2 file.]

  Description [Based on the vsnprintf() man page.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
typedef struct bz2file {
  FILE   * f;
  char   * buf;
  int      nBytes;
  int      nBytesMax;
} bz2file;

int fprintfBz2Aig( bz2file * b, char * fmt, ... ) {
    int n;
    va_list ap;
    va_start(ap,fmt);
    n = vfprintf( b->f, fmt, ap);
    va_end(ap);
    return n;
}

/**Function*************************************************************

  Synopsis    [Writes the AIG in the binary AIGER format.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Io_WriteAiger( Abc_Ntk_t * pNtk, char * pFileName, int fWriteSymbols )
{
//    FILE * pFile;
    Abc_Obj_t * pObj, * pDriver, * pLatch;
    int i, nNodes, nBufferSize, Pos;
    unsigned char * pBuffer;
    unsigned uLit0, uLit1, uLit;
    bz2file b;

    memset(&b,0,sizeof(b));
    b.nBytesMax = (1<<12);
    b.buf = ABC_ALLOC( char,b.nBytesMax );

    // start the output stream
    b.f = fopen( pFileName, "wb" );
    if ( b.f == NULL )
    {
        fprintf( stdout, "Ioa_WriteBlif(): Cannot open the output file \"%s\".\n", pFileName );
        ABC_FREE(b.buf);
        return;
    }

    // set the node numbers to be used in the output file
    nNodes = 0;
    Io_ObjSetAigerNum( Abc_AigConst1(pNtk), nNodes++ );
    Abc_NtkForEachCi( pNtk, pObj, i )
        Io_ObjSetAigerNum( pObj, nNodes++ );
    Abc_AigForEachAnd( pNtk, pObj, i )
        Io_ObjSetAigerNum( pObj, nNodes++ );

    // write the header "M I L O A" where M = I + L + A
    fprintfBz2Aig( &b, "aig %u %u %u %u %u",
        Abc_NtkPiNum(pNtk) + Abc_NtkLatchNum(pNtk) + Abc_NtkNodeNum(pNtk),
        Abc_NtkPiNum(pNtk),
        Abc_NtkLatchNum(pNtk),
        Abc_NtkPoNum(pNtk),
        Abc_NtkNodeNum(pNtk) );
    fprintfBz2Aig( &b, "\n" );

    // if the driver node is a constant, we need to complement the literal below
    // because, in the AIGER format, literal 0/1 is represented as number 0/1
    // while, in ABC, constant 1 node has number 0 and so literal 0/1 will be 1/0

    // write latch drivers
    Abc_NtkForEachLatch( pNtk, pLatch, i )
    {
        pObj = Abc_ObjFanin0(pLatch);
        pDriver = Abc_ObjFanin0(pObj);
        uLit = Io_ObjMakeLit( Io_ObjAigerNum(pDriver), Abc_ObjFaninC0(pObj) ^ (Io_ObjAigerNum(pDriver) == 0) );
        if ( Abc_LatchIsInit0(pLatch) )
            fprintfBz2Aig( &b, "%u\n", uLit );
        else if ( Abc_LatchIsInit1(pLatch) )
            fprintfBz2Aig( &b, "%u 1\n", uLit );
        else
        {
            assert( Abc_LatchIsInitDc(pLatch) );
            fprintfBz2Aig( &b, "%u %u\n", uLit, Io_ObjMakeLit( Io_ObjAigerNum(Abc_ObjFanout0(pLatch)), 0 ) );
        }
    }
    // write PO drivers
    Abc_NtkForEachPo( pNtk, pObj, i )
    {
        pDriver = Abc_ObjFanin0(pObj);
        fprintfBz2Aig( &b, "%u\n", Io_ObjMakeLit( Io_ObjAigerNum(pDriver), Abc_ObjFaninC0(pObj) ^ (Io_ObjAigerNum(pDriver) == 0) ) );
    }

    // write the nodes into the buffer
    Pos = 0;
    nBufferSize = 6 * Abc_NtkNodeNum(pNtk) + 100; // skeptically assuming 3 chars per one AIG edge
    pBuffer = ABC_ALLOC( unsigned char, nBufferSize );
    Abc_AigForEachAnd( pNtk, pObj, i )
    {
        uLit  = Io_ObjMakeLit( Io_ObjAigerNum(pObj), 0 );
        uLit0 = Io_ObjMakeLit( Io_ObjAigerNum(Abc_ObjFanin0(pObj)), Abc_ObjFaninC0(pObj) );
        uLit1 = Io_ObjMakeLit( Io_ObjAigerNum(Abc_ObjFanin1(pObj)), Abc_ObjFaninC1(pObj) );
        if ( uLit0 > uLit1 )
        {
            unsigned Temp = uLit0;
            uLit0 = uLit1;
            uLit1 = Temp;
        }
        assert( uLit1 < uLit );
        Pos = Io_WriteAigerEncode( pBuffer, Pos, (unsigned)(uLit  - uLit1) );
        Pos = Io_WriteAigerEncode( pBuffer, Pos, (unsigned)(uLit1 - uLit0) );
        if ( Pos > nBufferSize - 10 )
        {
            printf( "Io_WriteAiger(): AIGER generation has failed because the allocated buffer is too small.\n" );
            fclose( b.f );
            ABC_FREE(b.buf);
            return;
        }
    }
    assert( Pos < nBufferSize );

    // write the buffer
    fwrite( pBuffer, 1, Pos, b.f );
    ABC_FREE( pBuffer );

    // write the symbol table
    if ( fWriteSymbols )
    {
        // write PIs
        Abc_NtkForEachPi( pNtk, pObj, i )
            fprintfBz2Aig( &b, "i%d %s\n", i, Abc_ObjName(pObj) );
        // write latches
        Abc_NtkForEachLatch( pNtk, pObj, i )
            fprintfBz2Aig( &b, "l%d %s\n", i, Abc_ObjName(Abc_ObjFanout0(pObj)) );
        // write POs
        Abc_NtkForEachPo( pNtk, pObj, i )
            fprintfBz2Aig( &b, "o%d %s\n", i, Abc_ObjName(pObj) );
    }

    // close the file
    fclose( b.f );
    ABC_FREE(b.buf);
}

/**Function*************************************************************

  Synopsis    [Stores the AIG in the AIGER library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
aiger * Io_StoreAiger( Abc_Ntk_t * pNtk, int fCheck )
{
    int i;
    Abc_Obj_t * pObj;

    aiger * pAiger = aiger_init();
    if ( pAiger == NULL ) {
        printf( "Io_StoreAiger(): AIGER storage has failed because object could not be allocated.\n" );
        return NULL;
    }

    // set the node numbers to be used in the output file
    unsigned nNodes = 0;
    Io_ObjSetAigerNum( Abc_AigConst1(pNtk), nNodes++ );
    Abc_NtkForEachCi( pNtk, pObj, i )
        Io_ObjSetAigerNum( pObj, nNodes++ );
    Abc_AigForEachAnd( pNtk, pObj, i )
        Io_ObjSetAigerNum( pObj, nNodes++ );

    // if the driver node is a constant, we need to complement the literal below
    // because, in the AIGER format, literal 0/1 is represented as number 0/1
    // while, in ABC, constant 1 node has number 0 and so literal 0/1 will be 1/0

    // add inputs
    Abc_NtkForEachPi( pNtk, pObj, i )
    {
        unsigned lit = Io_ObjMakeLit( Io_ObjAigerNum(pObj), 0 );
        aiger_add_input( pAiger, lit, Abc_ObjName(pObj) );
    }

    // add latches
    Abc_NtkForEachLatch( pNtk, pObj, i )
    {
        Abc_Obj_t * pLatch = Abc_ObjFanin0(pObj);
        Abc_Obj_t * pNext = Abc_ObjFanin0(pLatch);
        Abc_Obj_t * pOut = Abc_ObjFanout0(pObj);
        unsigned lit = Io_ObjMakeLit( Io_ObjAigerNum(pOut), 0 );
        unsigned next = Io_ObjMakeLit( Io_ObjAigerNum(pNext), Abc_ObjFaninC0(pLatch) ^ (Io_ObjAigerNum(pNext) == 0) );
        aiger_add_latch( pAiger, lit, next, Abc_ObjName(pOut) );

        if ( Abc_LatchIsInit0(pObj) )
            aiger_add_reset( pAiger, lit, 0 );
        else if ( Abc_LatchIsInit1(pObj) )
            aiger_add_reset( pAiger, lit, 1 );
        else
        {
            assert( Abc_LatchIsInitDc(pObj) );
            aiger_add_reset( pAiger, lit, lit );
        }
    }

    // add outputs
    Abc_NtkForEachPo( pNtk, pObj, i )
    {
        Abc_Obj_t * pNext = Abc_ObjFanin0(pObj);
        unsigned lit = Io_ObjMakeLit( Io_ObjAigerNum(pNext), Abc_ObjFaninC0(pObj) ^ (Io_ObjAigerNum(pNext) == 0) );
        aiger_add_output( pAiger, lit, Abc_ObjName(pObj) );
    }

    // add and nodes
    Abc_AigForEachAnd( pNtk, pObj, i )
    {
        unsigned lhs  = Io_ObjMakeLit( Io_ObjAigerNum(pObj), 0 );
        unsigned rhs0 = Io_ObjMakeLit( Io_ObjAigerNum(Abc_ObjFanin0(pObj)), Abc_ObjFaninC0(pObj) );
        unsigned rhs1 = Io_ObjMakeLit( Io_ObjAigerNum(Abc_ObjFanin1(pObj)), Abc_ObjFaninC1(pObj) );
        aiger_add_and( pAiger, lhs, rhs0, rhs1 );
    }

    if ( fCheck && aiger_check (pAiger) != 0 ) {
        printf( "Io_StoreAiger: The network check has failed.\n" );
        return NULL;
    }

    return pAiger;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
