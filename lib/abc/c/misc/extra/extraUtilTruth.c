/**CFile****************************************************************

  FileName    [extraUtilMisc.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [extra]

  Synopsis    [Various procedures for truth table manipulation.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: extraUtilMisc.c,v 1.0 2003/09/01 00:00:00 alanmi Exp $]

***********************************************************************/

#include "extra.h"

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function*************************************************************

  Synopsis    [Swaps two adjacent variables in the truth table.]

  Description [Swaps var number Start and var number Start+1 (0-based numbers).
  The input truth table is pIn. The output truth table is pOut.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Extra_TruthSwapAdjacentVars( unsigned * pOut, unsigned * pIn, int nVars, int iVar )
{
    static const unsigned PMasks[4][3] = {
        { 0x99999999, 0x22222222, 0x44444444 },
        { 0xC3C3C3C3, 0x0C0C0C0C, 0x30303030 },
        { 0xF00FF00F, 0x00F000F0, 0x0F000F00 },
        { 0xFF0000FF, 0x0000FF00, 0x00FF0000 }
    };
    int nWords = Extra_TruthWordNum( nVars );
    int i, k, Step, Shift;

    assert( iVar < nVars - 1 );
    if ( iVar < 4 )
    {
        Shift = (1 << iVar);
        for ( i = 0; i < nWords; i++ )
            pOut[i] = (pIn[i] & PMasks[iVar][0]) | ((pIn[i] & PMasks[iVar][1]) << Shift) | ((pIn[i] & PMasks[iVar][2]) >> Shift);
    }
    else if ( iVar > 4 )
    {
        Step = (1 << (iVar - 5));
        for ( k = 0; k < nWords; k += 4*Step )
        {
            for ( i = 0; i < Step; i++ )
                pOut[i] = pIn[i];
            for ( i = 0; i < Step; i++ )
                pOut[Step+i] = pIn[2*Step+i];
            for ( i = 0; i < Step; i++ )
                pOut[2*Step+i] = pIn[Step+i];
            for ( i = 0; i < Step; i++ )
                pOut[3*Step+i] = pIn[3*Step+i];
            pIn  += 4*Step;
            pOut += 4*Step;
        }
    }
    else // if ( iVar == 4 )
    {
        for ( i = 0; i < nWords; i += 2 )
        {
            pOut[i]   = (pIn[i]   & 0x0000FFFF) | ((pIn[i+1] & 0x0000FFFF) << 16);
            pOut[i+1] = (pIn[i+1] & 0xFFFF0000) | ((pIn[i]   & 0xFFFF0000) >> 16);
        }
    }
}

/**Function*************************************************************

  Synopsis    [Expands the truth table according to the phase.]

  Description [The input and output truth tables are in pIn/pOut. The current number
  of variables is nVars. The total number of variables in nVarsAll. The last argument
  (Phase) contains shows where the variables should go.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Extra_TruthStretch( unsigned * pOut, unsigned * pIn, int nVars, int nVarsAll, unsigned Phase )
{
    unsigned * pTemp;
    int i, k, Var = nVars - 1, Counter = 0;
    for ( i = nVarsAll - 1; i >= 0; i-- )
        if ( Phase & (1 << i) )
        {
            for ( k = Var; k < i; k++ )
            {
                Extra_TruthSwapAdjacentVars( pOut, pIn, nVarsAll, k );
                pTemp = pIn; pIn = pOut; pOut = pTemp;
                Counter++;
            }
            Var--;
        }
    assert( Var == -1 );
    // swap if it was moved an even number of times
    if ( !(Counter & 1) )
        Extra_TruthCopy( pOut, pIn, nVarsAll );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
