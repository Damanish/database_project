/* rhf.c: Implementation of the Record/Heap File (RHF) layer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rhf.h"

/*
 * Helper function to initialize a new page as a slotted page
 */
static void rhf_InitPage(char *page)
{
    RHF_PageHeader *header = GET_HEADER(page);
    header->numSlots = 0;
    //* Free space starts at the END of the page */
    header->freeSpacePtr = PF_PAGE_SIZE; 
    header->nextFreeSlot = -1; /* No free slots yet */
}

/*
 * Helper function to find a page with at least 'length' bytes of free space
 * If no such page exists, it allocates a new one.
 * Returns the page number and a pointer to the *fixed* page buffer.
 */
static int rhf_GetPageWithSpace(int fd, int length, int *pageNum, char **pageBuf)
{
    int error;
    int pnum = -1;
    char *buf;

    /* Start scanning from the first page */
    while ((error = PF_GetNextPage(fd, &pnum, &buf)) == PFE_OK)
    {
        RHF_PageHeader *header = GET_HEADER(buf);
        RHF_Slot *slots = GET_SLOTS(buf);
        
        /* Calculate available space on this page */
        /* Space = (Start of Free Space) - (End of Slot Array) */
        int spaceNeededForSlot = (header->nextFreeSlot == -1) ? sizeof(RHF_Slot) : 0;
        int freeSpace = header->freeSpacePtr - (sizeof(RHF_PageHeader) + header->numSlots * sizeof(RHF_Slot));
        
        if (freeSpace >= (length + spaceNeededForSlot))
        {
            *pageNum = pnum;
            *pageBuf = buf;
            return RHF_OK; /* Found a page */
        }
        
        /* Unfix the page before getting the next one */
        if (PF_UnfixPage(fd, pnum, FALSE) != PFE_OK) {
            return PFerrno;
        }
    }

    /* If we're here, no page had enough space (or file was empty) */
    if (error != PFE_EOF) return error; /* A real error occurred */

    /* Allocate a new page */
    if ((error = PF_AllocPage(fd, pageNum, pageBuf)) != PFE_OK) {
        return error;
    }

    /* Initialize the new page */
    rhf_InitPage(*pageBuf);
    return RHF_OK;
}


/* --- Public RHF API Functions --- */

int RHF_CreateFile(char *fname) 
{
    return PF_CreateFile(fname);
}

int RHF_DestroyFile(char *fname)
{
    return PF_DestroyFile(fname);
}

int RHF_OpenFile(char *fname)
{
    return PF_OpenFile(fname);
}

int RHF_CloseFile(int fd)
{
    return PF_CloseFile(fd);
}

int RHF_InsertRecord(int fd, char *record, int length, RID *rid)
{
    int error, pageNum;
    char *pageBuf;

    /* 1. Find a page with enough space */
    if ((error = rhf_GetPageWithSpace(fd, length, &pageNum, &pageBuf)) != RHF_OK) {
        return error;
    }

    RHF_PageHeader *header = GET_HEADER(pageBuf);
    RHF_Slot *slots = GET_SLOTS(pageBuf);
    RHF_Slot *slot;

    /* 2. Find a slot for the record */
    int slotNum;
    if (header->nextFreeSlot != -1)
    {
        /* Reuse a deleted slot */
        slotNum = header->nextFreeSlot;
        slot = GET_SLOT(pageBuf, slotNum);
        header->nextFreeSlot = slot->recordOffset; /* Follow the free list chain */
    }
    else
    {
        /* Allocate a new slot */
        slotNum = header->numSlots;
        slot = GET_SLOT(pageBuf, slotNum);
        header->numSlots++;
    }

    /* 3. Write the record to the page */
    /* Records are stored from the *end* of the page, growing backwards */
    header->freeSpacePtr -= length;
    slot->recordOffset = header->freeSpacePtr;
    slot->recordLength = length;
    memcpy(GET_RECORD(pageBuf, slot), record, length);
    
    /* 4. Set the output RID */
    rid->pageNum = pageNum;
    rid->slotNum = slotNum;

    /* 5. Unfix the page, marking it dirty */
    return PF_UnfixPage(fd, pageNum, TRUE);
}

int RHF_GetRecord(int fd, RID *rid, char *recordBuf, int *length)
{
    char *pageBuf;
    int error;

    /* 1. Get the page */
    if ((error = PF_GetThisPage(fd, rid->pageNum, &pageBuf)) != PFE_OK) {
        return error;
    }

    RHF_PageHeader *header = GET_HEADER(pageBuf);
    
    /* 2. Check if RID is valid */
    if (rid->slotNum < 0 || rid->slotNum >= header->numSlots) {
        PF_UnfixPage(fd, rid->pageNum, FALSE);
        return RHF_INVALIDRID;
    }

    RHF_Slot *slot = GET_SLOT(pageBuf, rid->slotNum);

    /* 3. Check if record exists (i.e., not deleted) */
    if (slot->recordOffset == -1) {
        PF_UnfixPage(fd, rid->pageNum, FALSE);
        return RHF_NORECORD;
    }

    /* 4. Copy data to user's buffer */
    *length = slot->recordLength;
    memcpy(recordBuf, GET_RECORD(pageBuf, slot), *length);

    /* 5. Unfix the page */
    return PF_UnfixPage(fd, rid->pageNum, FALSE);
}


int RHF_DeleteRecord(int fd, RID *rid)
{
    char *pageBuf;
    int error;

    /* 1. Get the page */
    if ((error = PF_GetThisPage(fd, rid->pageNum, &pageBuf)) != PFE_OK) {
        return error;
    }
    
    RHF_PageHeader *header = GET_HEADER(pageBuf);
    
    /* 2. Check if RID is valid */
    if (rid->slotNum < 0 || rid->slotNum >= header->numSlots) {
        PF_UnfixPage(fd, rid->pageNum, FALSE);
        return RHF_INVALIDRID;
    }

    RHF_Slot *slot = GET_SLOT(pageBuf, rid->slotNum);
    
    /* 3. Check if already deleted */
    if (slot->recordOffset == -1) {
        PF_UnfixPage(fd, rid->pageNum, FALSE);
        return RHF_NORECORD; /* Already deleted */
    }

    /* 4. "Delete" the record by marking the slot as free */
    /* We add this slot to the front of the free list */
    int oldOffset = slot->recordOffset;
    slot->recordOffset = header->nextFreeSlot; /* Point to old head of free list */
    slot->recordLength = -1; /* Mark as empty */
    header->nextFreeSlot = rid->slotNum; /* This slot is now the head */

    /* Note: We don't reclaim the data space. Compaction is complex. */
    /* This is a simple "tombstone" implementation. */

    /* 5. Unfix the page, marking it dirty */
    return PF_UnfixPage(fd, rid->pageNum, TRUE);
}


int RHF_StartScan(int fd, RHF_Scan *scan)
{
    scan->fd = fd;
    scan->currentPage = -1; /* Start *before* the first page */
    scan->currentSlot = -1; 
    scan->pageBuf = NULL;   /* No page fixed yet */
    scan->page_is_fixed = 0; /* False */
    return RHF_OK;
}

int RHF_GetNextRecord(RHF_Scan *scan, char *recordBuf, int *length, RID *rid)
{
    int error;
    RHF_PageHeader *header;
    RHF_Slot *slot;

    while (TRUE)
    {
        /* 1. Get a page if we don't have one */
        if (!scan->page_is_fixed) {
            /* We need the next page. PF_GetNextPage returns a FIXED page */
            if ((error = PF_GetNextPage(scan->fd, &scan->currentPage, &scan->pageBuf)) != PFE_OK) {
                return (error == PFE_EOF) ? RHF_EOF : error;
            }
            scan->page_is_fixed = 1; /* TRUE */
            scan->currentSlot = 0;   /* Reset slot for new page */
        }
        
        header = GET_HEADER(scan->pageBuf);

        /* 2. Check if we're past the last slot on this page */
        if (scan->currentSlot >= header->numSlots)
        {
            /* We are. Unfix this page and loop to get the next one */
            if ((error = PF_UnfixPage(scan->fd, scan->currentPage, FALSE)) != PFE_OK) {
                return error;
            }
            scan->page_is_fixed = 0; /* FALSE */
            continue; /* Loop will call PF_GetNextPage */
        }
        
        /* 3. We have a valid slot, check if it's used */
        slot = GET_SLOT(scan->pageBuf, scan->currentSlot);
        
        /* 4. Prepare to advance to the next slot for the *next* call */
        scan->currentSlot++;

        if (slot->recordLength != -1)
        {
            /* Found a valid record! */
            *length = slot->recordLength;
            memcpy(recordBuf, GET_RECORD(scan->pageBuf, slot), *length);
            
            rid->pageNum = scan->currentPage;
            rid->slotNum = scan->currentSlot - 1; /* We just incremented it */
            
            /* We *do not* unfix the page. We leave it fixed for the next call. */
            return RHF_OK;
        }

        /* 5. This slot was empty (deleted). Loop to check the next slot. */
    }
}

int RHF_EndScan(RHF_Scan *scan)
{
    int error;

    /* Check if a page is still fixed from the scan */
    if (scan->page_is_fixed)
    {
        if ((error = PF_UnfixPage(scan->fd, scan->currentPage, FALSE)) != PFE_OK)
        {
            return error;
        }
    }
    
    /* Reset the struct */
    scan->fd = -1;
    scan->pageBuf = NULL;
    scan->page_is_fixed = 0;
    
    return RHF_OK;
}

/*
 * Helper for printing RHF-layer errors
 */
void RHF_PrintError(char *s, int err)
{
    fprintf(stderr, "%s: ", s);
    switch(err) {
        case RHF_EOF:
            fprintf(stderr, "End of scan or file.\n");
            break;
        case RHF_PAGEFULL:
            fprintf(stderr, "Page is full.\n");
            break;
        case RHF_INVALIDRID:
            fprintf(stderr, "Invalid Record ID.\n");
            break;
        case RHF_NORECORD:
            fprintf(stderr, "Record does not exist (or was deleted).\n");
            break;
        case RHF_NOMEM:
            fprintf(stderr, "Out of memory.\n");
            break;
        default:
            /* Assume it's a PF error and call its printer */
            PF_PrintError(s);
            break;
    }
}