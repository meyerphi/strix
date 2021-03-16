/**CFile****************************************************************

  FileName    [kitFactor.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Computation kit.]

  Synopsis    [Algebraic factoring.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - Dec 6, 2006.]

  Revision    [$Id: kitFactor.c,v 1.00 2006/12/06 00:00:00 alanmi Exp $]

***********************************************************************/

#include "kit.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

// factoring fails if intermediate memory usage exceed this limit
#define KIT_FACTOR_MEM_LIMIT  (1<<20)

static Kit_Edge_t  Kit_SopFactor_rec( Kit_Graph_t * pFForm, Kit_Sop_t * cSop, int nLits, Vec_Int_t * vMemory );
static Kit_Edge_t  Kit_SopFactorLF_rec( Kit_Graph_t * pFForm, Kit_Sop_t * cSop, Kit_Sop_t * cSimple, int nLits, Vec_Int_t * vMemory );
static Kit_Edge_t  Kit_SopFactorTrivial( Kit_Graph_t * pFForm, Kit_Sop_t * cSop, int nLits );
static Kit_Edge_t  Kit_SopFactorTrivialCube( Kit_Graph_t * pFForm, unsigned uCube, int nLits );

extern int         Kit_SopFactorVerify( Vec_Int_t * cSop, Kit_Graph_t * pFForm, int nVars );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Factors the cover.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Graph_t * Kit_SopFactor( Vec_Int_t * vCover, int fCompl, int nVars, Vec_Int_t * vMemory )
{
    Kit_Sop_t Sop, * cSop = &Sop;
    Kit_Graph_t * pFForm;
    Kit_Edge_t eRoot;
//    int nCubes;

    // works for up to 15 variables because division procedure
    // used the last bit for marking the cubes going to the remainder
    assert( nVars < 16 );

    // check for trivial functions
    if ( Vec_IntSize(vCover) == 0 )
        return Kit_GraphCreateConst0();
    if ( Vec_IntSize(vCover) == 1 && Vec_IntEntry(vCover, 0) == 0 )
        return Kit_GraphCreateConst1();

    // prepare memory manager
//    Vec_IntClear( vMemory );
    Vec_IntGrow( vMemory, KIT_FACTOR_MEM_LIMIT );

    // perform CST
    Kit_SopCreateInverse( cSop, vCover, vMemory ); // CST

    // start the factored form
    pFForm = Kit_GraphCreate( nVars );
    // factor the cover
    eRoot = Kit_SopFactor_rec( pFForm, cSop, 2 * nVars, vMemory );
    // finalize the factored form
    Kit_GraphSetRoot( pFForm, eRoot );
    if ( fCompl )
        Kit_GraphComplement( pFForm );

    // verify the factored form
//    nCubes = Vec_IntSize(vCover);
//    Vec_IntShrink( vCover, nCubes );
//    if ( !Kit_SopFactorVerify( vCover, pFForm, nVars ) )
//        printf( "Verification has failed.\n" );
    return pFForm;
}

/**Function*************************************************************

  Synopsis    [Recursive factoring procedure.]

  Description [For the pseudo-code, see Hachtel/Somenzi,
  Logic synthesis and verification algorithms, Kluwer, 1996, p. 432.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Edge_t Kit_SopFactor_rec( Kit_Graph_t * pFForm, Kit_Sop_t * cSop, int nLits, Vec_Int_t * vMemory )
{
    Kit_Sop_t Div, Quo, Rem, Com;
    Kit_Sop_t * cDiv = &Div, * cQuo = &Quo, * cRem = &Rem, * cCom = &Com;
    Kit_Edge_t eNodeDiv, eNodeQuo, eNodeRem, eNodeAnd;

    // make sure the cover contains some cubes
    assert( Kit_SopCubeNum(cSop) > 0 );

    // get the divisor
    if ( !Kit_SopDivisor(cDiv, cSop, nLits, vMemory) )
        return Kit_SopFactorTrivial( pFForm, cSop, nLits );

    // divide the cover by the divisor
    Kit_SopDivideInternal( cSop, cDiv, cQuo, cRem, vMemory );

    // check the trivial case
    assert( Kit_SopCubeNum(cQuo) > 0 );
    if ( Kit_SopCubeNum(cQuo) == 1 )
        return Kit_SopFactorLF_rec( pFForm, cSop, cQuo, nLits, vMemory );

    // make the quotient cube ABC_FREE
    Kit_SopMakeCubeFree( cQuo );

    // divide the cover by the quotient
    Kit_SopDivideInternal( cSop, cQuo, cDiv, cRem, vMemory );

    // check the trivial case
    if ( Kit_SopIsCubeFree( cDiv ) )
    {
        eNodeDiv = Kit_SopFactor_rec( pFForm, cDiv, nLits, vMemory );
        eNodeQuo = Kit_SopFactor_rec( pFForm, cQuo, nLits, vMemory );
        eNodeAnd = Kit_GraphAddNodeAnd( pFForm, eNodeDiv, eNodeQuo );
        if ( Kit_SopCubeNum(cRem) == 0 )
            return eNodeAnd;
        eNodeRem = Kit_SopFactor_rec( pFForm, cRem, nLits, vMemory );
        return Kit_GraphAddNodeOr( pFForm, eNodeAnd, eNodeRem );
    }

    // get the common cube
    Kit_SopCommonCubeCover( cCom, cDiv, vMemory );

    // solve the simple problem
    return Kit_SopFactorLF_rec( pFForm, cSop, cCom, nLits, vMemory );
}

/**Function*************************************************************

  Synopsis    [Internal recursive factoring procedure for the leaf case.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Edge_t Kit_SopFactorLF_rec( Kit_Graph_t * pFForm, Kit_Sop_t * cSop, Kit_Sop_t * cSimple, int nLits, Vec_Int_t * vMemory )
{
    Kit_Sop_t Div, Quo, Rem;
    Kit_Sop_t * cDiv = &Div, * cQuo = &Quo, * cRem = &Rem;
    Kit_Edge_t eNodeDiv, eNodeQuo, eNodeRem, eNodeAnd;
    assert( Kit_SopCubeNum(cSimple) == 1 );
    // get the most often occurring literal
    Kit_SopBestLiteralCover( cDiv, cSop, Kit_SopCube(cSimple, 0), nLits, vMemory );
    // divide the cover by the literal
    Kit_SopDivideByCube( cSop, cDiv, cQuo, cRem, vMemory );
    // get the node pointer for the literal
    eNodeDiv = Kit_SopFactorTrivialCube( pFForm, Kit_SopCube(cDiv, 0), nLits );
    // factor the quotient and remainder
    eNodeQuo = Kit_SopFactor_rec( pFForm, cQuo, nLits, vMemory );
    eNodeAnd = Kit_GraphAddNodeAnd( pFForm, eNodeDiv, eNodeQuo );
    if ( Kit_SopCubeNum(cRem) == 0 )
        return eNodeAnd;
    eNodeRem = Kit_SopFactor_rec( pFForm, cRem, nLits, vMemory );
    return Kit_GraphAddNodeOr( pFForm, eNodeAnd, eNodeRem );
}

/**Function*************************************************************

  Synopsis    [Factoring cube.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Edge_t Kit_SopFactorTrivialCube_rec( Kit_Graph_t * pFForm, unsigned uCube, int nStart, int nFinish )
{
    Kit_Edge_t eNode1, eNode2;
    int i, iLit = -1, nLits, nLits1;
    assert( uCube );
    // count the number of literals in this interval
    nLits = 0;
    for ( i = nStart; i < nFinish; i++ )
        if ( Kit_CubeHasLit(uCube, i) )
        {
            iLit = i;
            nLits++;
        }
    assert( iLit != -1 );
    // quit if there is only one literal
    if ( nLits == 1 )
        return Kit_EdgeCreate( iLit/2, iLit%2 ); // CST
    // split the literals into two parts
    nLits1 = nLits/2;
    // find the splitting point
    nLits = 0;
    for ( i = nStart; i < nFinish; i++ )
        if ( Kit_CubeHasLit(uCube, i) )
        {
            if ( nLits == nLits1 )
                break;
            nLits++;
        }
    // recursively construct the tree for the parts
    eNode1 = Kit_SopFactorTrivialCube_rec( pFForm, uCube, nStart, i  );
    eNode2 = Kit_SopFactorTrivialCube_rec( pFForm, uCube, i, nFinish );
    return Kit_GraphAddNodeAnd( pFForm, eNode1, eNode2 );
}

/**Function*************************************************************

  Synopsis    [Factoring cube.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Edge_t Kit_SopFactorTrivialCube( Kit_Graph_t * pFForm, unsigned uCube, int nLits )
{
    return Kit_SopFactorTrivialCube_rec( pFForm, uCube, 0, nLits );
}

/**Function*************************************************************

  Synopsis    [Factoring SOP.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Edge_t Kit_SopFactorTrivial_rec( Kit_Graph_t * pFForm, unsigned * pCubes, int nCubes, int nLits )
{
    Kit_Edge_t eNode1, eNode2;
    int nCubes1, nCubes2;
    if ( nCubes == 1 )
        return Kit_SopFactorTrivialCube_rec( pFForm, pCubes[0], 0, nLits );
    // split the cubes into two parts
    nCubes1 = nCubes/2;
    nCubes2 = nCubes - nCubes1;
//    nCubes2 = nCubes/2;
//    nCubes1 = nCubes - nCubes2;
    // recursively construct the tree for the parts
    eNode1 = Kit_SopFactorTrivial_rec( pFForm, pCubes,           nCubes1, nLits );
    eNode2 = Kit_SopFactorTrivial_rec( pFForm, pCubes + nCubes1, nCubes2, nLits );
    return Kit_GraphAddNodeOr( pFForm, eNode1, eNode2 );
}

/**Function*************************************************************

  Synopsis    [Factoring the cover, which has no algebraic divisors.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Kit_Edge_t Kit_SopFactorTrivial( Kit_Graph_t * pFForm, Kit_Sop_t * cSop, int nLits )
{
    return Kit_SopFactorTrivial_rec( pFForm, cSop->pCubes, cSop->nCubes, nLits );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
