/* 
简单来说，heap5.c允许我们使用不连续的内存块作为内存池 
使用方法： PortDefineHeapRegions() 必须首先被调用
vPortDefineHeapRegions()只有一个参数，参数是一个
HeapRegion_t结构的数组，HeapRegion定义如下：

typedef struct HeapRegion
{
    uint8_t *pucStartAddress; << 首地址
    uint32_t xSizeInBytes;      << 大小
} HeapRegion_t;

数组以NULL结尾，而且，数组必须的从低到高书写，如下：
HeapRegion_t xHeapRegions[] =
{
    { ( uint8_t * ) 0x80000000UL, 0x10000 }, << 定义0x10000字节 开始于0x80000000
    { ( uint8_t * ) 0x90000000UL, 0xa0000 }, << 定义0xa0000字节 开始于0x90000000
    { NULL, 0 }                << Terminates the array.
};

vPortDefineHeapRegions( xHeapRegions ); << 传递数组到函数

*/

#include <stdlib.h>
#include "heap_5.h"
/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

//#include "FreeRTOS.h"
//#include "task.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

//#if( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
//    #error This file must not be used if configSUPPORT_DYNAMIC_ALLOCATION is 0
//#endif

/* 块大小不应太小*/
#define heapMINIMUM_BLOCK_SIZE  ( ( uint32_t ) ( xHeapStructSize << 1 ) )

/* 每一字节8位 */
#define heapBITS_PER_BYTE       ( ( uint32_t ) 8 )

/* 定义链表结构体，用于根据内存地址连接空闲块 */
typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK *pxNextFreeBlock;   /*<< 下一个空闲块 */
    uint32_t xBlockSize;                      /*<< 空闲块大小 */
} BlockLink_t;

/*-----------------------------------------------------------*/

/*
 * Inserts a block of memory that is being freed into the correct position in
 * the list of free memory blocks.  The block being freed will be merged with
 * the block in front it and/or the block behind it if the memory blocks are
 * adjacent to each other.
 */
static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert );

/*-----------------------------------------------------------*/

/* The size of the structure placed at the beginning of each allocated memory
block must by correctly byte aligned. */
static const uint32_t xHeapStructSize = ( sizeof( BlockLink_t ) + ( ( uint32_t ) ( portBYTE_ALIGNMENT - 1 ) ) ) & ~( ( uint32_t ) portBYTE_ALIGNMENT_MASK );

/* Create a couple of list links to mark the start and end of the list. */
static BlockLink_t xStart, *pxEnd = NULL;

/* Keeps track of the number of free bytes remaining, but says nothing about
fragmentation. */
static uint32_t xFreeBytesRemaining = 0U;
static uint32_t xMinimumEverFreeBytesRemaining = 0U;

/* Gets set to the top bit of an uint32_t type.  When this bit in the xBlockSize
member of an BlockLink_t structure is set then the block belongs to the
application.  When the bit is free the block is still part of the free heap
space. */
static uint32_t xBlockAllocatedBit = 0;

/*-----------------------------------------------------------*/

static void *pvPortMalloc( uint32_t xWantedSize )
{
BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
void *pvReturn = NULL;

    /* The heap must be initialised before the first call to
    prvPortMalloc(). */
    configASSERT( pxEnd );

    //vTaskSuspendAll();
    {
        /* Check the requested block size is not so large that the top bit is
        set.  The top bit of the block size member of the BlockLink_t structure
        is used to determine who owns the block - the application or the
        kernel, so it must be free. */
        if( ( xWantedSize & xBlockAllocatedBit ) == 0 )
        {
            /* The wanted size is increased so it can contain a BlockLink_t
            structure in addition to the requested amount of bytes. */
            if( xWantedSize > 0 )
            {
                xWantedSize += xHeapStructSize;

                /* Ensure that blocks are always aligned to the required number
                of bytes. */
                if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
                {
                    /* Byte alignment required. */
                    xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
            {
                /* Traverse the list from the start (lowest address) block until
                one of adequate size is found. */
                pxPreviousBlock = &xStart;
                pxBlock = xStart.pxNextFreeBlock;
                while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
                {
                    pxPreviousBlock = pxBlock;
                    pxBlock = pxBlock->pxNextFreeBlock;
                }

                /* If the end marker was reached then a block of adequate size
                was not found. */
                if( pxBlock != pxEnd )
                {
                    /* Return the memory space pointed to - jumping over the
                    BlockLink_t structure at its start. */
                    pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

                    /* This block is being returned for use so must be taken out
                    of the list of free blocks. */
                    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                    /* If the block is larger than required it can be split into
                    two. */
                    if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
                    {
                        /* This block is to be split into two.  Create a new
                        block following the number of bytes requested. The void
                        cast is used to prevent byte alignment warnings from the
                        compiler. */
                        pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );

                        /* Calculate the sizes of two blocks split from the
                        single block. */
                        pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
                        pxBlock->xBlockSize = xWantedSize;

                        /* Insert the new block into the list of free blocks. */
                        prvInsertBlockIntoFreeList( ( pxNewBlockLink ) );
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    xFreeBytesRemaining -= pxBlock->xBlockSize;

                    if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
                    {
                        xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* The block is being returned - it is allocated and owned
                    by the application and has no "next" block. */
                    pxBlock->xBlockSize |= xBlockAllocatedBit;
                    pxBlock->pxNextFreeBlock = NULL;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        //traceMALLOC( pvReturn, xWantedSize );
    }
    //( void ) xTaskResumeAll();

    #if( configUSE_MALLOC_FAILED_HOOK == 1 )
    {
        if( pvReturn == NULL )
        {
            extern void vApplicationMallocFailedHook( void );
            vApplicationMallocFailedHook();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #endif

    return pvReturn;
}
/*-----------------------------------------------------------*/

static void vPortFree( void *pv )
{
uint8_t *puc = ( uint8_t * ) pv;
BlockLink_t *pxLink;

    if( pv != NULL )
    {
        /* The memory being freed will have an BlockLink_t structure immediately
        before it. */
        puc -= xHeapStructSize;

        /* This casting is to keep the compiler from issuing warnings. */
        pxLink = ( void * ) puc;

        /* Check the block is actually allocated. */
        configASSERT( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 );
        configASSERT( pxLink->pxNextFreeBlock == NULL );

        if( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 )
        {
            if( pxLink->pxNextFreeBlock == NULL )
            {
                /* The block is being returned to the heap - it is no longer
                allocated. */
                pxLink->xBlockSize &= ~xBlockAllocatedBit;

                //vTaskSuspendAll();
                {
                    /* Add this block to the list of free blocks. */
                    xFreeBytesRemaining += pxLink->xBlockSize;
                    //traceFREE( pv, pxLink->xBlockSize );
                    prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );
                }
                //( void ) xTaskResumeAll();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
}
/*-----------------------------------------------------------*/

static uint32_t xPortGetFreeHeapSize( void )
{
    return xFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

static uint32_t xPortGetMinimumEverFreeHeapSize( void )
{
    return xMinimumEverFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert )
{
BlockLink_t *pxIterator;
uint8_t *puc;

    /* Iterate through the list until a block is found that has a higher address
    than the block being inserted. */
    for( pxIterator = &xStart; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock )
    {
        /* Nothing to do here, just iterate to the right position. */
    }

    /* Do the block being inserted, and the block it is being inserted after
    make a contiguous block of memory? */
    puc = ( uint8_t * ) pxIterator;
    if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
    {
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        pxBlockToInsert = pxIterator;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    /* Do the block being inserted, and the block it is being inserted before
    make a contiguous block of memory? */
    puc = ( uint8_t * ) pxBlockToInsert;
    if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) pxIterator->pxNextFreeBlock )
    {
        if( pxIterator->pxNextFreeBlock != pxEnd )
        {
            /* Form one big block from the two blocks. */
            pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
            pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
        }
        else
        {
            pxBlockToInsert->pxNextFreeBlock = pxEnd;
        }
    }
    else
    {
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* If the block being inserted plugged a gab, so was merged with the block
    before and the block after, then it's pxNextFreeBlock pointer will have
    already been set, and should not be set here as that would make it point
    to itself. */
    if( pxIterator != pxBlockToInsert )
    {
        pxIterator->pxNextFreeBlock = pxBlockToInsert;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }
}
/*-----------------------------------------------------------*/

static void vPortDefineHeapRegions( const HeapRegion_t * const pxHeapRegions )
{
    BlockLink_t *pxFirstFreeBlockInRegion = NULL, *pxPreviousFreeBlock;
    uint32_t xAlignedHeap;
    uint32_t xTotalRegionSize, xTotalHeapSize = 0;
    uint32_t xDefinedRegions = 0;
    uint32_t xAddress;
    const HeapRegion_t *pxHeapRegion;
    
    /* Can only call once! */
    configASSERT( pxEnd == NULL );

    pxHeapRegion = &( pxHeapRegions[ xDefinedRegions ] );

    while( pxHeapRegion->xSizeInBytes > 0 )
    {
        xTotalRegionSize = pxHeapRegion->xSizeInBytes;

        /* Ensure the heap region starts on a correctly aligned boundary. */
        xAddress = ( uint32_t ) pxHeapRegion->pucStartAddress;
        if( ( xAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
        {
            xAddress += ( portBYTE_ALIGNMENT - 1 );
            xAddress &= ~portBYTE_ALIGNMENT_MASK;

            /* Adjust the size for the bytes lost to alignment. */
            xTotalRegionSize -= xAddress - ( uint32_t ) pxHeapRegion->pucStartAddress;
        }

        xAlignedHeap = xAddress;

        /* Set xStart if it has not already been set. */
        if( xDefinedRegions == 0 )
        {
            /* xStart is used to hold a pointer to the first item in the list of
            free blocks.  The void cast is used to prevent compiler warnings. */
            xStart.pxNextFreeBlock = ( BlockLink_t * ) xAlignedHeap;
            xStart.xBlockSize = ( uint32_t ) 0;
        }
        else
        {
            /* Should only get here if one region has already been added to the
            heap. */
            configASSERT( pxEnd != NULL );

            /* Check blocks are passed in with increasing start addresses. */
            configASSERT( xAddress > ( uint32_t ) pxEnd );
        }

        /* Remember the location of the end marker in the previous region, if
        any. */
        pxPreviousFreeBlock = pxEnd;

        /* pxEnd is used to mark the end of the list of free blocks and is
        inserted at the end of the region space. */
        xAddress = xAlignedHeap + xTotalRegionSize;
        xAddress -= xHeapStructSize;
        xAddress &= ~portBYTE_ALIGNMENT_MASK;
        pxEnd = ( BlockLink_t * ) xAddress;
        pxEnd->xBlockSize = 0;
        pxEnd->pxNextFreeBlock = NULL;

        /* To start with there is a single free block in this region that is
        sized to take up the entire heap region minus the space taken by the
        free block structure. */
        pxFirstFreeBlockInRegion = ( BlockLink_t * ) xAlignedHeap;
        pxFirstFreeBlockInRegion->xBlockSize = xAddress - ( uint32_t ) pxFirstFreeBlockInRegion;
        pxFirstFreeBlockInRegion->pxNextFreeBlock = pxEnd;

        /* If this is not the first region that makes up the entire heap space
        then link the previous region to this region. */
        if( pxPreviousFreeBlock != NULL )
        {
            pxPreviousFreeBlock->pxNextFreeBlock = pxFirstFreeBlockInRegion;
        }

        xTotalHeapSize += pxFirstFreeBlockInRegion->xBlockSize;

        /* Move onto the next HeapRegion_t structure. */
        xDefinedRegions++;
        pxHeapRegion = &( pxHeapRegions[ xDefinedRegions ] );
    }

    xMinimumEverFreeBytesRemaining = xTotalHeapSize;
    xFreeBytesRemaining = xTotalHeapSize;

    /* Check something was actually defined before it is accessed. */
    configASSERT( xTotalHeapSize );

    /* Work out the position of the top bit in a uint32_t variable. */
    xBlockAllocatedBit = ( ( uint32_t ) 1 ) << ( ( sizeof( uint32_t ) * heapBITS_PER_BYTE ) - 1 );
}

MM_OpsTypedef MM_Ops = 
{
    .Init       = vPortDefineHeapRegions,
    .Malloc     = pvPortMalloc,
    .Free       = vPortFree,
    .HeapSize   = xPortGetFreeHeapSize
};

















