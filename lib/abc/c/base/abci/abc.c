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

#include "base/main/main.h"
#include "base/main/mainInt.h"

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
int Abc_NtkNetworkSize( Abc_Ntk_t * pNtk )
{
    return Abc_NtkNodeNum( pNtk );
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
