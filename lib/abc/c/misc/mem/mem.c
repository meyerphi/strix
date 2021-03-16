/**CFile****************************************************************

  FileName    [esopMem.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Cover manipulation package.]

  Synopsis    [Memory managers.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: esopMem.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mem.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

struct Mem_Fixed_t_
{
    // information about individual entries
    int           nEntrySize;    // the size of one entry
    int           nEntriesAlloc; // the total number of entries allocated
    int           nEntriesUsed;  // the number of entries in use
    int           nEntriesMax;   // the max number of entries in use
    char *        pEntriesFree;  // the linked list of free entries

    // this is where the memory is stored
    int           nChunkSize;    // the size of one chunk
    int           nChunksAlloc;  // the maximum number of memory chunks
    int           nChunks;       // the current number of memory chunks
    char **       pChunks;       // the allocated memory

    // statistics
    int           nMemoryUsed;   // memory used in the allocated entries
    int           nMemoryAlloc;  // memory allocated
};

struct Mem_Flex_t_
{
    // information about individual entries
    int           nEntriesUsed;  // the number of entries allocated
    char *        pCurrent;      // the current pointer to free memory
    char *        pEnd;          // the first entry outside the free memory

    // this is where the memory is stored
    int           nChunkSize;    // the size of one chunk
    int           nChunksAlloc;  // the maximum number of memory chunks
    int           nChunks;       // the current number of memory chunks
    char **       pChunks;       // the allocated memory

    // statistics
    int           nMemoryUsed;   // memory used in the allocated entries
    int           nMemoryAlloc;  // memory allocated
};

struct Mem_Step_t_
{
    int             nMems;              // the number of fixed memory managers employed
    Mem_Fixed_t **  pMems;              // memory managers: 2^1 words, 2^2 words, etc
    int             nMapSize;           // the size of the memory array
    Mem_Fixed_t **  pMap;               // maps the number of bytes into its memory manager
    int             nLargeChunksAlloc;  // the maximum number of large memory chunks
    int             nLargeChunks;       // the current number of large memory chunks
    void **         pLargeChunks;       // the allocated large memory chunks
};

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Allocates memory pieces of fixed size.]

  Description [The size of the chunk is computed as the minimum of
  1024 entries and 64K. Can only work with entry size at least 4 byte long.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Mem_Fixed_t * Mem_FixedStart( int nEntrySize )
{
    Mem_Fixed_t * p;

    p = ABC_ALLOC( Mem_Fixed_t, 1 );
    memset( p, 0, sizeof(Mem_Fixed_t) );

    p->nEntrySize    = nEntrySize;
    p->nEntriesAlloc = 0;
    p->nEntriesUsed  = 0;
    p->pEntriesFree  = NULL;

    if ( nEntrySize * (1 << 10) < (1<<16) )
        p->nChunkSize = (1 << 10);
    else
        p->nChunkSize = (1<<16) / nEntrySize;
    if ( p->nChunkSize < 8 )
        p->nChunkSize = 8;

    p->nChunksAlloc  = 64;
    p->nChunks       = 0;
    p->pChunks       = ABC_ALLOC( char *, p->nChunksAlloc );

    p->nMemoryUsed   = 0;
    p->nMemoryAlloc  = 0;
    return p;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Mem_FixedStop( Mem_Fixed_t * p )
{
    int i;
    if ( p == NULL )
        return;
    for ( i = 0; i < p->nChunks; i++ )
        ABC_FREE( p->pChunks[i] );
    ABC_FREE( p->pChunks );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
char * Mem_FixedEntryFetch( Mem_Fixed_t * p )
{
    char * pTemp;
    int i;

    // check if there are still free entries
    if ( p->nEntriesUsed == p->nEntriesAlloc )
    { // need to allocate more entries
        assert( p->pEntriesFree == NULL );
        if ( p->nChunks == p->nChunksAlloc )
        {
            p->nChunksAlloc *= 2;
            p->pChunks = ABC_REALLOC( char *, p->pChunks, p->nChunksAlloc );
        }
        p->pEntriesFree = ABC_ALLOC( char, p->nEntrySize * p->nChunkSize );
        p->nMemoryAlloc += p->nEntrySize * p->nChunkSize;
        // transform these entries into a linked list
        pTemp = p->pEntriesFree;
        for ( i = 1; i < p->nChunkSize; i++ )
        {
            *((char **)pTemp) = pTemp + p->nEntrySize;
            pTemp += p->nEntrySize;
        }
        // set the last link
        *((char **)pTemp) = NULL;
        // add the chunk to the chunk storage
        p->pChunks[ p->nChunks++ ] = p->pEntriesFree;
        // add to the number of entries allocated
        p->nEntriesAlloc += p->nChunkSize;
    }
    // incrememt the counter of used entries
    p->nEntriesUsed++;
    if ( p->nEntriesMax < p->nEntriesUsed )
        p->nEntriesMax = p->nEntriesUsed;
    // return the first entry in the free entry list
    pTemp = p->pEntriesFree;
    p->pEntriesFree = *((char **)pTemp);
    return pTemp;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Mem_FixedEntryRecycle( Mem_Fixed_t * p, char * pEntry )
{
    // decrement the counter of used entries
    p->nEntriesUsed--;
    // add the entry to the linked list of free entries
    *((char **)pEntry) = p->pEntriesFree;
    p->pEntriesFree = pEntry;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Mem_FixedReadMemUsage( Mem_Fixed_t * p )
{
    return p->nMemoryAlloc;
}

/**Function*************************************************************

  Synopsis    [Starts the hierarchical memory manager.]

  Description [This manager can allocate entries of any size.
  Iternally they are mapped into the entries with the number of bytes
  equal to the power of 2. The smallest entry size is 8 bytes. The
  next one is 16 bytes etc. So, if the user requests 6 bytes, he gets
  8 byte entry. If we asks for 25 bytes, he gets 32 byte entry etc.
  The input parameters "nSteps" says how many fixed memory managers
  are employed internally. Calling this procedure with nSteps equal
  to 10 results in 10 hierarchically arranged internal memory managers,
  which can allocate up to 4096 (1Kb) entries. Requests for larger
  entries are handed over to malloc() and then ABC_FREE()ed.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Mem_Step_t * Mem_StepStart( int nSteps )
{
    Mem_Step_t * p;
    int i, k;
    p = ABC_ALLOC( Mem_Step_t, 1 );
    memset( p, 0, sizeof(Mem_Step_t) );
    p->nMems = nSteps;
    // start the fixed memory managers
    p->pMems = ABC_ALLOC( Mem_Fixed_t *, p->nMems );
    for ( i = 0; i < p->nMems; i++ )
        p->pMems[i] = Mem_FixedStart( (8<<i) );
    // set up the mapping of the required memory size into the corresponding manager
    p->nMapSize = (4<<p->nMems);
    p->pMap = ABC_ALLOC( Mem_Fixed_t *, p->nMapSize+1 );
    p->pMap[0] = NULL;
    for ( k = 1; k <= 4; k++ )
        p->pMap[k] = p->pMems[0];
    for ( i = 0; i < p->nMems; i++ )
        for ( k = (4<<i)+1; k <= (8<<i); k++ )
            p->pMap[k] = p->pMems[i];
//for ( i = 1; i < 100; i ++ )
//printf( "%10d: size = %10d\n", i, p->pMap[i]->nEntrySize );
    return p;
}

/**Function*************************************************************

  Synopsis    [Stops the memory manager.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Mem_StepStop( Mem_Step_t * p )
{
    int i;
    for ( i = 0; i < p->nMems; i++ )
        Mem_FixedStop( p->pMems[i] );
    if ( p->pLargeChunks )
    {
        for ( i = 0; i < p->nLargeChunks; i++ )
            ABC_FREE( p->pLargeChunks[i] );
        ABC_FREE( p->pLargeChunks );
    }
    ABC_FREE( p->pMems );
    ABC_FREE( p->pMap );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Creates the entry.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
char * Mem_StepEntryFetch( Mem_Step_t * p, int nBytes )
{
    if ( nBytes == 0 )
        return NULL;
    if ( nBytes > p->nMapSize )
    {
//        printf( "Allocating %d bytes.\n", nBytes );
//        return ABC_ALLOC( char, nBytes );
        if ( p->nLargeChunks == p->nLargeChunksAlloc )
        {
            if ( p->nLargeChunksAlloc == 0 )
                p->nLargeChunksAlloc = 32;
            p->nLargeChunksAlloc *= 2;
            p->pLargeChunks = (void **)ABC_REALLOC( char *, p->pLargeChunks, p->nLargeChunksAlloc );
        }
        p->pLargeChunks[ p->nLargeChunks++ ] = ABC_ALLOC( char, nBytes );
        return (char *)p->pLargeChunks[ p->nLargeChunks - 1 ];
    }
    return Mem_FixedEntryFetch( p->pMap[nBytes] );
}

/**Function*************************************************************

  Synopsis    [Recycles the entry.]

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Mem_StepEntryRecycle( Mem_Step_t * p, char * pEntry, int nBytes )
{
    if ( nBytes == 0 )
        return;
    if ( nBytes > p->nMapSize )
    {
//        ABC_FREE( pEntry );
        return;
    }
    Mem_FixedEntryRecycle( p->pMap[nBytes], pEntry );
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Mem_StepReadMemUsage( Mem_Step_t * p )
{
    int i, nMemTotal = 0;
    for ( i = 0; i < p->nMems; i++ )
        nMemTotal += p->pMems[i]->nMemoryAlloc;
    return nMemTotal;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
