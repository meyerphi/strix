/**CFile****************************************************************

  FileName    [mem.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Memory management.]

  Synopsis    [External declarations.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: mem.h,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#ifndef ABC__aig__mem__mem_h
#define ABC__aig__mem__mem_h

#include "misc/util/abc_global.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Mem_Fixed_t_    Mem_Fixed_t;
typedef struct Mem_Flex_t_     Mem_Flex_t;
typedef struct Mem_Step_t_     Mem_Step_t;

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== mem.c ===========================================================*/
// fixed-size-block memory manager
extern Mem_Fixed_t * Mem_FixedStart( int nEntrySize );
extern void          Mem_FixedStop( Mem_Fixed_t * p );
extern char *        Mem_FixedEntryFetch( Mem_Fixed_t * p );
extern void          Mem_FixedEntryRecycle( Mem_Fixed_t * p, char * pEntry );
extern int           Mem_FixedReadMemUsage( Mem_Fixed_t * p );
// hierarchical memory manager
extern Mem_Step_t *  Mem_StepStart( int nSteps );
extern void          Mem_StepStop( Mem_Step_t * p );
extern char *        Mem_StepEntryFetch( Mem_Step_t * p, int nBytes );
extern void          Mem_StepEntryRecycle( Mem_Step_t * p, char * pEntry, int nBytes );
extern int           Mem_StepReadMemUsage( Mem_Step_t * p );

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
