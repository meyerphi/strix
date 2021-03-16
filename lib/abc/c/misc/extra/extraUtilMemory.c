/**CFile****************************************************************

  FileName    [extraUtilMemory.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [extra]

  Synopsis    [Memory managers.]

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: extraUtilMemory.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "extra.h"

/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

struct Extra_MmFixed_t_
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

struct Extra_MmFlex_t_
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

struct Extra_MmStep_t_
{
    int                nMems;    // the number of fixed memory managers employed
    Extra_MmFixed_t ** pMems;    // memory managers: 2^1 words, 2^2 words, etc
    int                nMapSize; // the size of the memory array
    Extra_MmFixed_t ** pMap;     // maps the number of bytes into its memory manager
    int                nLargeChunksAlloc;  // the maximum number of large memory chunks
    int                nLargeChunks;       // the current number of large memory chunks
    void **            pLargeChunks;       // the allocated large memory chunks
};

/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function*************************************************************

  Synopsis    [Allocates memory pieces of fixed size.]

  Description [The size of the chunk is computed as the minimum of
  1024 entries and 64K. Can only work with entry size at least 4 byte long.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Extra_MmFixed_t * Extra_MmFixedStart( int nEntrySize )
{
    Extra_MmFixed_t * p;

    p = ABC_ALLOC( Extra_MmFixed_t, 1 );
    memset( p, 0, sizeof(Extra_MmFixed_t) );

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
void Extra_MmFixedStop( Extra_MmFixed_t * p )
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
char * Extra_MmFixedEntryFetch( Extra_MmFixed_t * p )
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
void Extra_MmFixedEntryRecycle( Extra_MmFixed_t * p, char * pEntry )
{
    // decrement the counter of used entries
    p->nEntriesUsed--;
    // add the entry to the linked list of free entries
    *((char **)pEntry) = p->pEntriesFree;
    p->pEntriesFree = pEntry;
}

/**Function*************************************************************

  Synopsis    [Allocates entries of flexible size.]

  Description [Can only work with entry size at least 4 byte long.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
Extra_MmFlex_t * Extra_MmFlexStart()
{
    Extra_MmFlex_t * p;
//printf( "allocing flex\n" );
    p = ABC_ALLOC( Extra_MmFlex_t, 1 );
    memset( p, 0, sizeof(Extra_MmFlex_t) );

    p->nEntriesUsed  = 0;
    p->pCurrent      = NULL;
    p->pEnd          = NULL;

    p->nChunkSize    = (1 << 12);
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
void Extra_MmFlexStop( Extra_MmFlex_t * p )
{
    int i;
    if ( p == NULL )
        return;
//printf( "deleting flex\n" );
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
char * Extra_MmFlexEntryFetch( Extra_MmFlex_t * p, int nBytes )
{
    char * pTemp;
    // check if there are still free entries
    if ( p->pCurrent == NULL || p->pCurrent + nBytes > p->pEnd )
    { // need to allocate more entries
        if ( p->nChunks == p->nChunksAlloc )
        {
            p->nChunksAlloc *= 2;
            p->pChunks = ABC_REALLOC( char *, p->pChunks, p->nChunksAlloc );
        }
        if ( nBytes > p->nChunkSize )
        {
            // resize the chunk size if more memory is requested than it can give
            // (ideally, this should never happen)
            p->nChunkSize = 2 * nBytes;
        }
        p->pCurrent = ABC_ALLOC( char, p->nChunkSize );
        p->pEnd     = p->pCurrent + p->nChunkSize;
        p->nMemoryAlloc += p->nChunkSize;
        // add the chunk to the chunk storage
        p->pChunks[ p->nChunks++ ] = p->pCurrent;
    }
    assert( p->pCurrent + nBytes <= p->pEnd );
    // increment the counter of used entries
    p->nEntriesUsed++;
    // keep track of the memory used
    p->nMemoryUsed += nBytes;
    // return the next entry
    pTemp = p->pCurrent;
    p->pCurrent += nBytes;
    return pTemp;
}
