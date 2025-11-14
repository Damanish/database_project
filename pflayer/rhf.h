/* rhf.h: Public interface for the Record/Heap File (RHF) layer */
#ifndef RHF_H
#define RHF_H

#include "pf.h"

/* --- Error Codes --- */
#define RHF_OK         0
#define RHF_EOF       -20  /* End of file or scan */
#define RHF_PAGEFULL  -21  /* No space on page */
#define RHF_INVALIDRID -22 /* Invalid Record ID */
#define RHF_NORECORD   -23 /* Record does not exist */
#define RHF_NOMEM      -24 /* Out of memory */

/*
 * Record ID (RID)
 * Uniquely identifies a record in the file.
 */
typedef struct {
    int pageNum;  /* Page number in the PF file */
    int slotNum;  /* Slot number on that page */
} RID;


/*
 * RHF_Scan
 * A structure to keep track of a sequential file scan.
 */
typedef struct {
    int fd;           /* File descriptor for the scan */
    int currentPage;  /* Page number of the current page */
    int currentSlot;  /* Slot number of the next record to check */
    char *pageBuf;      /* Pointer to the fixed page buffer */
    int page_is_fixed;  /* Flag: 1 if pageBuf is valid, 0 otherwise */
    
} RHF_Scan;


/* --- Slotted Page Structures --- */
/*
 * This is the metadata header stored at the START of every page.
 */
typedef struct {
    int numSlots;       /* Total number of slots on this page */
    int freeSpacePtr;   /* Offset from start of page to the beginning of free space */
    int nextFreeSlot;   /* Index of the next available (deleted) slot, or -1 if none */
} RHF_PageHeader;

/*
 * This is the structure for a single slot in the slot array.
 * The slot array starts immediately after the RHF_PageHeader.
 */
typedef struct {
    int recordOffset;  /* Offset from start of page to the record's data (-1 if empty) */
    int recordLength;  /* Length of the record */
} RHF_Slot;

/* --- Page Utility Macros --- */
/* Gets a pointer to the page's header */
#define GET_HEADER(page) ((RHF_PageHeader *)(page))

/* Gets a pointer to the slot array (which starts right after the header) */
#define GET_SLOTS(page) ((RHF_Slot *)((char *)(page) + sizeof(RHF_PageHeader)))

/* Gets a pointer to a specific slot */
#define GET_SLOT(page, slotNum) (GET_SLOTS(page) + (slotNum))

/* Gets a pointer to the record data */
#define GET_RECORD(page, slot) ((char *)(page) + (slot)->recordOffset)


/* --- Public RHF API Functions --- */

/* File Management */
extern int RHF_CreateFile(char *fname);
extern int RHF_DestroyFile(char *fname);
extern int RHF_OpenFile(char *fname);
extern int RHF_CloseFile(int fd);

/* Record Management */
extern int RHF_InsertRecord(int fd, char *record, int length, RID *rid);
extern int RHF_DeleteRecord(int fd, RID *rid);
extern int RHF_GetRecord(int fd, RID *rid, char *recordBuf, int *length);

/* Scan Management */
extern int RHF_StartScan(int fd, RHF_Scan *scan);
extern int RHF_GetNextRecord(RHF_Scan *scan, char *recordBuf, int *length, RID *rid);
extern int RHF_EndScan(RHF_Scan *scan);

/* Utility */
extern void RHF_PrintError(char *s, int err);

#endif