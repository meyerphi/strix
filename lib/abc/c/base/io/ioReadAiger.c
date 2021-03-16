/**CFile****************************************************************

  FileName    [ioReadAiger.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Command processing package.]

  Synopsis    [Procedures to read binary AIGER format developed by
  Armin Biere, Johannes Kepler University (http://fmv.jku.at/)]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - December 16, 2006.]

  Revision    [$Id: ioReadAiger.c,v 1.00 2006/12/16 00:00:00 alanmi Exp $]

***********************************************************************/

// The code in this file is developed in collaboration with Mark Jarvin of Toronto.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "aiger.h"
#include "ioAbc.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Extracts one unsigned AIG edge from the input buffer.]

  Description [This procedure is a slightly modified version of Armin Biere's
  procedure "unsigned decode (FILE * file)". ]

  SideEffects [Updates the current reading position.]

  SeeAlso     []

***********************************************************************/
static inline unsigned Io_ReadAigerDecode( char ** ppPos )
{
    unsigned x = 0, i = 0;
    unsigned char ch;

//    while ((ch = getnoneofch (file)) & 0x80)
    while ((ch = *(*ppPos)++) & 0x80)
        x |= (ch & 0x7f) << (7 * i++);

    return x | (ch << (7 * i));
}

/**Function*************************************************************

  Synopsis    [Reads the AIG in the binary AIGER format.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Io_ReadAiger( char * pFileName, int fCheck )
{
    FILE * pFile;
    Vec_Ptr_t * vNodes, * vTerms;
    Abc_Obj_t * pObj, * pNode0, * pNode1;
    Abc_Ntk_t * pNtkNew;
    int nTotal, nInputs, nOutputs, nLatches, nAnds;
    int nBad = 0, nConstr = 0, nJust = 0, nFair = 0;
    int nFileSize = -1, iTerm, i;
    char * pContents, * pDrivers = NULL, * pSymbols, * pCur, * pName, * pType;
    unsigned uLit0, uLit1, uLit;

    // read the file into the buffer
    nFileSize = Extra_FileSize( pFileName );
    pFile = fopen( pFileName, "rb" );
    pContents = ABC_ALLOC( char, nFileSize );
    int nRead = fread( pContents, nFileSize, 1, pFile );
    fclose( pFile );

    // check if the file has been completely read
    if ( nRead != nFileSize )
    {
        fprintf( stdout, "Error reading file.\n" );
        ABC_FREE( pContents );
        return NULL;
    }

    // check if the input file format is correct
    if ( strncmp(pContents, "aig", 3) != 0 || pContents[3] != ' ' )
    {
        fprintf( stdout, "Wrong input file format.\n" );
        ABC_FREE( pContents );
        return NULL;
    }

    // read the parameters (M I L O A + B C J F)
    pCur = pContents;         while ( *pCur != ' ' ) pCur++; pCur++;
    // read the number of objects
    nTotal = atoi( pCur );    while ( *pCur != ' ' ) pCur++; pCur++;
    // read the number of inputs
    nInputs = atoi( pCur );   while ( *pCur != ' ' ) pCur++; pCur++;
    // read the number of latches
    nLatches = atoi( pCur );  while ( *pCur != ' ' ) pCur++; pCur++;
    // read the number of outputs
    nOutputs = atoi( pCur );  while ( *pCur != ' ' ) pCur++; pCur++;
    // read the number of nodes
    nAnds = atoi( pCur );     while ( *pCur != ' ' && *pCur != '\n' ) pCur++;
    if ( *pCur == ' ' )
    {
        // read the number of properties
        pCur++;
        nBad = atoi( pCur );     while ( *pCur != ' ' && *pCur != '\n' ) pCur++;
        nOutputs += nBad;
    }
    if ( *pCur == ' ' )
    {
        // read the number of properties
        pCur++;
        nConstr = atoi( pCur );     while ( *pCur != ' ' && *pCur != '\n' ) pCur++;
        nOutputs += nConstr;
    }
    if ( *pCur == ' ' )
    {
        // read the number of properties
        pCur++;
        nJust = atoi( pCur );     while ( *pCur != ' ' && *pCur != '\n' ) pCur++;
        nOutputs += nJust;
    }
    if ( *pCur == ' ' )
    {
        // read the number of properties
        pCur++;
        nFair = atoi( pCur );     while ( *pCur != ' ' && *pCur != '\n' ) pCur++;
        nOutputs += nFair;
    }
    if ( *pCur != '\n' )
    {
        fprintf( stdout, "The parameter line is in a wrong format.\n" );
        ABC_FREE( pContents );
        return NULL;
    }
    pCur++;

    // check the parameters
    if ( nTotal != nInputs + nLatches + nAnds )
    {
        fprintf( stdout, "The number of objects does not match.\n" );
        ABC_FREE( pContents );
        return NULL;
    }
    if ( nJust || nFair )
    {
        fprintf( stdout, "Reading AIGER files with liveness properties is currently not supported.\n" );
        ABC_FREE( pContents );
        return NULL;
    }

    if ( nConstr )
    {
        if ( nConstr == 1 )
            fprintf( stdout, "Warning: The last output is interpreted as a constraint.\n" );
        else
            fprintf( stdout, "Warning: The last %d outputs are interpreted as constraints.\n", nConstr );
    }

    // allocate the empty AIG
    pNtkNew = Abc_NtkAlloc( 1 );

    // prepare the array of nodes
    vNodes = Vec_PtrAlloc( 1 + nInputs + nLatches + nAnds );
    Vec_PtrPush( vNodes, Abc_ObjNot( Abc_AigConst1(pNtkNew) ) );

    // create the PIs
    for ( i = 0; i < nInputs; i++ )
    {
        pObj = Abc_NtkCreatePi(pNtkNew);
        Vec_PtrPush( vNodes, pObj );
    }
    // create the POs
    for ( i = 0; i < nOutputs; i++ )
    {
        pObj = Abc_NtkCreatePo(pNtkNew);
    }
    // create the latches
    for ( i = 0; i < nLatches; i++ )
    {
        pObj = Abc_NtkCreateLatch(pNtkNew);
        Abc_LatchSetInit0( pObj );
        pNode0 = Abc_NtkCreateBi(pNtkNew);
        pNode1 = Abc_NtkCreateBo(pNtkNew);
        Abc_ObjAddFanin( pObj, pNode0 );
        Abc_ObjAddFanin( pNode1, pObj );
        Vec_PtrPush( vNodes, pNode1 );
    }

    // remember the beginning of latch/PO literals
    pDrivers = pCur;
    // scroll to the beginning of the binary data
    for ( i = 0; i < nLatches + nOutputs; )
        if ( *pCur++ == '\n' )
            i++;

    // create the AND gates
    for ( i = 0; i < nAnds; i++ )
    {
        uLit = ((i + 1 + nInputs + nLatches) << 1);
        uLit1 = uLit  - Io_ReadAigerDecode( &pCur );
        uLit0 = uLit1 - Io_ReadAigerDecode( &pCur );
//        assert( uLit1 > uLit0 );
        pNode0 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, uLit0 >> 1), uLit0 & 1 );
        pNode1 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, uLit1 >> 1), uLit1 & 1 );
        assert( Vec_PtrSize(vNodes) == i + 1 + nInputs + nLatches );
        Vec_PtrPush( vNodes, Abc_AigAnd((Abc_Aig_t *)pNtkNew->pManFunc, pNode0, pNode1) );
    }

    // remember the place where symbols begin
    pSymbols = pCur;

    // read the latch driver literals
    pCur = pDrivers;
    Abc_NtkForEachLatchInput( pNtkNew, pObj, i )
    {
        uLit0 = atoi( pCur );  while ( *pCur != ' ' && *pCur != '\n' ) pCur++;
        if ( *pCur == ' ' ) // read initial value
        {
            int Init;
            pCur++;
            Init = atoi( pCur );
            if ( Init == 0 )
                Abc_LatchSetInit0( Abc_NtkBox(pNtkNew, i) );
            else if ( Init == 1 )
                Abc_LatchSetInit1( Abc_NtkBox(pNtkNew, i) );
            else
            {
                assert( Init == Abc_Var2Lit(1+Abc_NtkPiNum(pNtkNew)+i, 0) );
                // unitialized value of the latch is the latch literal according to http://fmv.jku.at/hwmcc11/beyond1.pdf
                Abc_LatchSetInitDc( Abc_NtkBox(pNtkNew, i) );
            }
            while ( *pCur != ' ' && *pCur != '\n' ) pCur++;
        }
        if ( *pCur != '\n' )
        {
            fprintf( stdout, "The initial value of latch number %d is not recongnized.\n", i );
            return NULL;
        }
        pCur++;

        pNode0 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, uLit0 >> 1), (uLit0 & 1) );//^ (uLit0 < 2) );
        Abc_ObjAddFanin( pObj, pNode0 );
    }
    // read the PO driver literals
    Abc_NtkForEachPo( pNtkNew, pObj, i )
    {
        uLit0 = atoi( pCur );  while ( *pCur++ != '\n' );
        pNode0 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, uLit0 >> 1), (uLit0 & 1) );//^ (uLit0 < 2) );
        Abc_ObjAddFanin( pObj, pNode0 );
    }

    // read the names if present
    pCur = pSymbols;
    while ( pCur < pContents + nFileSize && *pCur != 'c' )
    {
        // get the terminal type
        pType = pCur;
        if ( *pCur == 'i' )
            vTerms = pNtkNew->vPis;
        else if ( *pCur == 'l' )
            vTerms = pNtkNew->vBoxes;
        else if ( *pCur == 'o' || *pCur == 'b' || *pCur == 'c' || *pCur == 'j' || *pCur == 'f' )
            vTerms = pNtkNew->vPos;
        else
        {
//                fprintf( stdout, "Wrong terminal type.\n" );
            return NULL;
        }
        // get the terminal number
        iTerm = atoi( ++pCur );  while ( *pCur++ != ' ' );
        // get the node
        if ( iTerm >= Vec_PtrSize(vTerms) )
        {
            fprintf( stdout, "The number of terminal is out of bound.\n" );
            return NULL;
        }
        pObj = (Abc_Obj_t *)Vec_PtrEntry( vTerms, iTerm );
        if ( *pType == 'l' )
            pObj = Abc_ObjFanout0(pObj);
        // assign the name
        pName = pCur;          while ( *pCur++ != '\n' );
        // assign this name
        *(pCur-1) = 0;
        Abc_ObjAssignName( pObj, pName, NULL );
        if ( *pType == 'l' )
        {
            Abc_ObjAssignName( Abc_ObjFanin0(pObj), Abc_ObjName(pObj), "L" );
            Abc_ObjAssignName( Abc_ObjFanin0(Abc_ObjFanin0(pObj)), Abc_ObjName(pObj), "_in" );
        }
        // mark the node as named
        pObj->pCopy = (Abc_Obj_t *)Abc_ObjName(pObj);
    }

    // assign the remaining names
    Abc_NtkForEachPi( pNtkNew, pObj, i )
    {
        if ( pObj->pCopy ) continue;
        Abc_ObjAssignName( pObj, Abc_ObjName(pObj), NULL );
    }
    Abc_NtkForEachLatchOutput( pNtkNew, pObj, i )
    {
        if ( pObj->pCopy ) continue;
        Abc_ObjAssignName( pObj, Abc_ObjName(pObj), NULL );
        Abc_ObjAssignName( Abc_ObjFanin0(pObj), Abc_ObjName(pObj), "L" );
        Abc_ObjAssignName( Abc_ObjFanin0(Abc_ObjFanin0(pObj)), Abc_ObjName(pObj), "_in" );
    }
    Abc_NtkForEachPo( pNtkNew, pObj, i )
    {
        if ( pObj->pCopy ) continue;
        Abc_ObjAssignName( pObj, Abc_ObjName(pObj), NULL );
    }

    // skipping the comments
    ABC_FREE( pContents );
    Vec_PtrFree( vNodes );

    // remove the extra nodes
    Abc_AigCleanup( (Abc_Aig_t *)pNtkNew->pManFunc );

    // check the result
    if ( fCheck && !Abc_NtkCheckRead( pNtkNew ) )
    {
        printf( "Io_ReadAiger: The network check has failed.\n" );
        Abc_NtkDelete( pNtkNew );
        return NULL;
    }
    return pNtkNew;
}

/**Function*************************************************************

  Synopsis    [Loads the AIG from the AIGER library.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Ntk_t * Io_LoadAiger( aiger * pAiger, int fCheck )
{
    if ( fCheck && aiger_check (pAiger) != 0 ) {
        printf( "Io_LoadAiger: The network check has failed.\n" );
        return NULL;
    }

    int i;
    Abc_Obj_t * pObj;
    Vec_Ptr_t * vNodes;
    Abc_Ntk_t * pNtkNew;

    int nInputs = pAiger->num_inputs;
    int nOutputs = pAiger->num_outputs;
    int nLatches = pAiger->num_latches;
    int nAnds = pAiger->num_ands;

    // allocate the empty AIG
    pNtkNew = Abc_NtkAlloc( 1 );

    vNodes = Vec_PtrAlloc( 1 + nInputs + nLatches + nAnds );
    Vec_PtrPush( vNodes, Abc_ObjNot( Abc_AigConst1(pNtkNew) ) );

    // create the PIs
    for ( i = 0; i < nInputs; i++ )
    {
        pObj = Abc_NtkCreatePi(pNtkNew);
        Vec_PtrPush( vNodes, pObj );
        Abc_ObjAssignName( pObj, pAiger->inputs[i].name, NULL );
    }
    // create the POs
    for ( i = 0; i < nOutputs; i++ )
    {
        pObj = Abc_NtkCreatePo(pNtkNew);
        Abc_ObjAssignName( pObj, pAiger->outputs[i].name, NULL );
    }
    // create the latches
    for ( i = 0; i < nLatches; i++ )
    {
        pObj = Abc_NtkCreateLatch(pNtkNew);
        Abc_LatchSetInit0( pObj );
        Abc_Obj_t * pNode0 = Abc_NtkCreateBi(pNtkNew);
        Abc_Obj_t * pNode1 = Abc_NtkCreateBo(pNtkNew);
        Abc_ObjAddFanin( pObj, pNode0 );
        Abc_ObjAddFanin( pNode1, pObj );
        Vec_PtrPush( vNodes, pNode1 );

        // assign latch name
        Abc_ObjAssignName( pNode1, pAiger->latches[i].name, NULL );
        Abc_ObjAssignName( pObj, Abc_ObjName(pNode1), "L" );
        Abc_ObjAssignName( pNode0, Abc_ObjName(pNode1), "_in" );
    }
    // create the AND gates
    for ( i = 0; i < nAnds; i++ )
    {
        unsigned rhs0 = pAiger->ands[i].rhs0;
        unsigned rhs1 = pAiger->ands[i].rhs1;
        Abc_Obj_t * pNode0 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, rhs0 >> 1), rhs0 & 1 );
        Abc_Obj_t * pNode1 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, rhs1 >> 1), rhs1 & 1 );
        assert( Vec_PtrSize(vNodes) == i + 1 + nInputs + nLatches );
        Vec_PtrPush( vNodes, Abc_AigAnd((Abc_Aig_t *)pNtkNew->pManFunc, pNode0, pNode1) );
    }
    // read the latch driver literals
    Abc_NtkForEachLatchInput( pNtkNew, pObj, i )
    {
        unsigned next = pAiger->latches[i].next;
        unsigned reset = pAiger->latches[i].reset;
        if ( reset == 0 )
            Abc_LatchSetInit0( Abc_NtkBox(pNtkNew, i) );
        else if ( reset == 1 )
            Abc_LatchSetInit1( Abc_NtkBox(pNtkNew, i) );
        else
        {
            assert( reset == (unsigned)Abc_Var2Lit(1+Abc_NtkPiNum(pNtkNew)+i, 0) );
            // unitialized value of the latch is the latch literal according to http://fmv.jku.at/hwmcc11/beyond1.pdf
            Abc_LatchSetInitDc( Abc_NtkBox(pNtkNew, i) );
        }

        Abc_Obj_t * pNode0 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, next >> 1), (next & 1) );
        Abc_ObjAddFanin( pObj, pNode0 );
    }
    // read the PO driver literals
    Abc_NtkForEachPo( pNtkNew, pObj, i )
    {
        unsigned lit = pAiger->outputs[i].lit;
        Abc_Obj_t * pNode0 = Abc_ObjNotCond( (Abc_Obj_t *)Vec_PtrEntry(vNodes, lit >> 1), (lit & 1) );
        Abc_ObjAddFanin( pObj, pNode0 );
    }

    // remove extra nodes
    Abc_AigCleanup( (Abc_Aig_t *)pNtkNew->pManFunc );

    // check the result
    if ( fCheck && !Abc_NtkCheckRead( pNtkNew ) )
    {
        printf( "Io_LoadAiger: The network check has failed.\n" );
        Abc_NtkDelete( pNtkNew );
        return NULL;
    }
    return pNtkNew;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
