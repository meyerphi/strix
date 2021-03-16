/**CFile****************************************************************

  FileName    [kitTruth.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Computation kit.]

  Synopsis    [Procedures involving truth tables.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - Dec 6, 2006.]

  Revision    [$Id: kitTruth.c,v 1.00 2006/12/06 00:00:00 alanmi Exp $]

***********************************************************************/

#include "kit.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Returns 1 if TT depends on the given variable.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Kit_TruthVarInSupport( unsigned * pTruth, int nVars, int iVar )
{
    int nWords = Kit_TruthWordNum( nVars );
    int i, k, Step;

    assert( iVar < nVars );
    switch ( iVar )
    {
    case 0:
        for ( i = 0; i < nWords; i++ )
            if ( (pTruth[i] & 0x55555555) != ((pTruth[i] & 0xAAAAAAAA) >> 1) )
                return 1;
        return 0;
    case 1:
        for ( i = 0; i < nWords; i++ )
            if ( (pTruth[i] & 0x33333333) != ((pTruth[i] & 0xCCCCCCCC) >> 2) )
                return 1;
        return 0;
    case 2:
        for ( i = 0; i < nWords; i++ )
            if ( (pTruth[i] & 0x0F0F0F0F) != ((pTruth[i] & 0xF0F0F0F0) >> 4) )
                return 1;
        return 0;
    case 3:
        for ( i = 0; i < nWords; i++ )
            if ( (pTruth[i] & 0x00FF00FF) != ((pTruth[i] & 0xFF00FF00) >> 8) )
                return 1;
        return 0;
    case 4:
        for ( i = 0; i < nWords; i++ )
            if ( (pTruth[i] & 0x0000FFFF) != ((pTruth[i] & 0xFFFF0000) >> 16) )
                return 1;
        return 0;
    default:
        Step = (1 << (iVar - 5));
        for ( k = 0; k < nWords; k += 2*Step )
        {
            for ( i = 0; i < Step; i++ )
                if ( pTruth[i] != pTruth[Step+i] )
                    return 1;
            pTruth += 2*Step;
        }
        return 0;
    }
}

/**Function*************************************************************

  Synopsis    [Computes negative cofactor of the function.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Kit_TruthCofactor0( unsigned * pTruth, int nVars, int iVar )
{
    int nWords = Kit_TruthWordNum( nVars );
    int i, k, Step;

    assert( iVar < nVars );
    switch ( iVar )
    {
    case 0:
        for ( i = 0; i < nWords; i++ )
            pTruth[i] = (pTruth[i] & 0x55555555) | ((pTruth[i] & 0x55555555) << 1);
        return;
    case 1:
        for ( i = 0; i < nWords; i++ )
            pTruth[i] = (pTruth[i] & 0x33333333) | ((pTruth[i] & 0x33333333) << 2);
        return;
    case 2:
        for ( i = 0; i < nWords; i++ )
            pTruth[i] = (pTruth[i] & 0x0F0F0F0F) | ((pTruth[i] & 0x0F0F0F0F) << 4);
        return;
    case 3:
        for ( i = 0; i < nWords; i++ )
            pTruth[i] = (pTruth[i] & 0x00FF00FF) | ((pTruth[i] & 0x00FF00FF) << 8);
        return;
    case 4:
        for ( i = 0; i < nWords; i++ )
            pTruth[i] = (pTruth[i] & 0x0000FFFF) | ((pTruth[i] & 0x0000FFFF) << 16);
        return;
    default:
        Step = (1 << (iVar - 5));
        for ( k = 0; k < nWords; k += 2*Step )
        {
            for ( i = 0; i < Step; i++ )
                pTruth[Step+i] = pTruth[i];
            pTruth += 2*Step;
        }
        return;
    }
}

/**Function*************************************************************

  Synopsis    [Computes positive cofactor of the function.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Kit_TruthCofactor1( unsigned * pTruth, int nVars, int iVar )
{
    int nWords = Kit_TruthWordNum( nVars );
    int i, k, Step;

    assert( iVar < nVars );
    switch ( iVar )
    {
    case 0:
        for ( i = 0; i < nWords; i++ )
            pTruth[i] = (pTruth[i] & 0xAAAAAAAA) | ((pTruth[i] & 0xAAAAAAAA) >> 1);
        return;
    case 1:
        for ( i = 0; i < nWords; i++ )
            pTruth[i] = (pTruth[i] & 0xCCCCCCCC) | ((pTruth[i] & 0xCCCCCCCC) >> 2);
        return;
    case 2:
        for ( i = 0; i < nWords; i++ )
            pTruth[i] = (pTruth[i] & 0xF0F0F0F0) | ((pTruth[i] & 0xF0F0F0F0) >> 4);
        return;
    case 3:
        for ( i = 0; i < nWords; i++ )
            pTruth[i] = (pTruth[i] & 0xFF00FF00) | ((pTruth[i] & 0xFF00FF00) >> 8);
        return;
    case 4:
        for ( i = 0; i < nWords; i++ )
            pTruth[i] = (pTruth[i] & 0xFFFF0000) | ((pTruth[i] & 0xFFFF0000) >> 16);
        return;
    default:
        Step = (1 << (iVar - 5));
        for ( k = 0; k < nWords; k += 2*Step )
        {
            for ( i = 0; i < Step; i++ )
                pTruth[i] = pTruth[Step+i];
            pTruth += 2*Step;
        }
        return;
    }
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
