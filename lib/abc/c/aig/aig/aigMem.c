/**CFile****************************************************************

  FileName    [aigMem.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [AIG package.]

  Synopsis    [Memory managers.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: aigMem.c,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#include "aig.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

struct Aig_MmFixed_t_
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

struct Aig_MmFlex_t_
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

struct Aig_MmStep_t_
{
    int               nMems;    // the number of fixed memory managers employed
    Aig_MmFixed_t **  pMems;    // memory managers: 2^1 words, 2^2 words, etc
    int               nMapSize; // the size of the memory array
    Aig_MmFixed_t **  pMap;     // maps the number of bytes into its memory manager
    // additional memory chunks
    int           nChunksAlloc;  // the maximum number of memory chunks
    int           nChunks;       // the current number of memory chunks
    char **       pChunks;       // the allocated memory
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
Aig_MmFixed_t * Aig_MmFixedStart( int nEntrySize, int nEntriesMax )
{
    Aig_MmFixed_t * p;

    p = ABC_ALLOC( Aig_MmFixed_t, 1 );
    memset( p, 0, sizeof(Aig_MmFixed_t) );

    p->nEntrySize    = nEntrySize;
    p->nEntriesAlloc = 0;
    p->nEntriesUsed  = 0;
    p->pEntriesFree  = NULL;

    p->nChunkSize = nEntriesMax / 8;
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
void Aig_MmFixedStop( Aig_MmFixed_t * p, int fVerbose )
{
    int i;
    if ( p == NULL )
        return;
    if ( fVerbose )
    {
        printf( "Fixed memory manager: Entry = %5d. Chunk = %5d. Chunks used = %5d.\n",
            p->nEntrySize, p->nChunkSize, p->nChunks );
        printf( "   Entries used = %8d. Entries peak = %8d. Memory used = %8d. Memory alloc = %8d.\n",
            p->nEntriesUsed, p->nEntriesMax, p->nEntrySize * p->nEntriesUsed, p->nMemoryAlloc );
    }
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
char * Aig_MmFixedEntryFetch( Aig_MmFixed_t * p )
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
void Aig_MmFixedEntryRecycle( Aig_MmFixed_t * p, char * pEntry )
{
    // decrement the counter of used entries
    p->nEntriesUsed--;
    // add the entry to the linked list of free entries
    *((char **)pEntry) = p->pEntriesFree;
    p->pEntriesFree = pEntry;
}

/**Function*************************************************************

  Synopsis    []

  Description [Relocates all the memory except the first chunk.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_MmFixedRestart( Aig_MmFixed_t * p )
{
    int i;
    char * pTemp;
    if ( p->nChunks == 0 )
        return;
    // deallocate all chunks except the first one
    for ( i = 1; i < p->nChunks; i++ )
        ABC_FREE( p->pChunks[i] );
    p->nChunks = 1;
    // transform these entries into a linked list
    pTemp = p->pChunks[0];
    for ( i = 1; i < p->nChunkSize; i++ )
    {
        *((char **)pTemp) = pTemp + p->nEntrySize;
        pTemp += p->nEntrySize;
    }
    // set the last link
    *((char **)pTemp) = NULL;
    // set the free entry list
    p->pEntriesFree  = p->pChunks[0];
    // set the correct statistics
    p->nMemoryAlloc  = p->nEntrySize * p->nChunkSize;
    p->nMemoryUsed   = 0;
    p->nEntriesAlloc = p->nChunkSize;
    p->nEntriesUsed  = 0;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Aig_MmFixedReadMemUsage( Aig_MmFixed_t * p )
{
    return p->nMemoryAlloc;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
