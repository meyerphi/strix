/*////////////////////////////////////////////////////////////////////////////

ABC: System for Sequential Synthesis and Verification

http://www.eecs.berkeley.edu/~alanmi/abc/

Copyright (c) The Regents of the University of California. All rights reserved.

Permission is hereby granted, without written agreement and without license or
royalty fees, to use, copy, modify, and distribute this software and its
documentation for any purpose, provided that the above copyright notice and
the following two paragraphs appear in all copies of this software.

IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF
THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS,
AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO PROVIDE MAINTENANCE,
SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

////////////////////////////////////////////////////////////////////////////*/

/**CFile****************************************************************

  FileName    [main.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [The main package.]

  Synopsis    [Here everything starts.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: main.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#ifndef WIN32
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#endif

#include "base/abc/abc.h"
#include "mainInt.h"

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [The main() procedure.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_RealMain()
{
    Abc_Frame_t * pAbc;
    char * sCommand;
    int  fStatus = 0;

    // start the framework
    pAbc = Abc_Start();

    // start interactive mode

    // execute commands given by the user
    while ( !feof(stdin) )
    {
        // get the command from the user
        const int BUF_LEN = 5000;
        char Prompt[BUF_LEN];
        sCommand = Abc_UtilsGetUsersInput( Prompt, BUF_LEN );

        // execute the user's command
        fStatus = Cmd_CommandExecute( pAbc, sCommand );

        // stop if the user quitted or an error occurred
        if ( fStatus == -1 || fStatus == -2 )
            break;
    }

    // if the memory should be freed, quit packages
    {
        Abc_Stop( pAbc );
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
