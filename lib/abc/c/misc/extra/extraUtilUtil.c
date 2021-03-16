/**CFile****************************************************************

  FileName    [extraUtilUtil.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [extra]

  Synopsis    [Old SIS utilities.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: extraUtilUtil.c,v 1.0 2003/02/01 00:00:00 alanmi Exp $]

***********************************************************************/

#include <stdio.h>
#include <string.h>
#include "extra.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

/*  File   : getopt.c
 *  Author : Henry Spencer, University of Toronto
 *  Updated: 28 April 1984
 *
 *  Changes: (R Rudell)
 *  changed index() to strchr();
 *  added getopt_reset() to reset the getopt argument parsing
 *
 *  Purpose: get option letter from argv.
 */

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [util_getopt_reset()]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Extra_UtilGetoptReset( UtilOpt_t * pOpt )
{
    pOpt->arg = 0;
    pOpt->ind = 0;
    pOpt->pScanStr = 0;
}

/**Function*************************************************************

  Synopsis    [util_getopt()]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Extra_UtilGetopt( UtilOpt_t * pOpt, int argc, char *argv[], const char *optstring )
{
    int c;
    char *place;

    pOpt->arg = NULL;

    if (pOpt->pScanStr == NULL || *pOpt->pScanStr == '\0')
    {
        if (pOpt->ind == 0)
            pOpt->ind++;
        if (pOpt->ind >= argc)
            return EOF;
        place = argv[pOpt->ind];
        if (place[0] != '-' || place[1] == '\0')
            return EOF;
        pOpt->ind++;
        if (place[1] == '-' && place[2] == '\0')
            return EOF;
        pOpt->pScanStr = place+1;
    }

    c = *pOpt->pScanStr++;
    place = strchr(optstring, c);
    if (place == NULL || c == ':') {
        (void) fprintf(stderr, "%s: unknown option %c\n", argv[0], c);
        return '?';
    }
    if (*++place == ':')
    {
        if (*pOpt->pScanStr != '\0')
        {
            pOpt->arg = pOpt->pScanStr;
            pOpt->pScanStr = NULL;
        }
        else
        {
            if (pOpt->ind >= argc)
            {
                (void) fprintf(stderr, "%s: %c requires an argument\n",
                    argv[0], c);
                return '?';
            }
            pOpt->arg = argv[pOpt->ind];
            pOpt->ind++;
        }
    }
    return c;
}

/**Function*************************************************************

  Synopsis    [Extra_UtilStrsav()]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
char * Extra_UtilStrsav( const char *s )
{
    if(s == NULL) {  /* added 7/95, for robustness */
       return NULL;
    }
    else {
       return strcpy(ABC_ALLOC(char, strlen(s)+1), s);
    }
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
