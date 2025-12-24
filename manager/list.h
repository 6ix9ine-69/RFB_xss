#ifndef LIST_H_INCLUDED
#define LIST_H_INCLUDED

typedef struct _DOUBLY_LINKED_LIST
{
    _DOUBLY_LINKED_LIST *lpNext;
    _DOUBLY_LINKED_LIST *lpPrev;
    _DOUBLY_LINKED_LIST *lpJump;
} DOUBLY_LINKED_LIST, *PDOUBLY_LINKED_LIST;

struct _CONNECTION;
typedef struct _ASSIGNED_LIST:_DOUBLY_LINKED_LIST
{
    _CONNECTION *lpAssignedTo;
} ASSIGNED_LIST, *PASSIGNED_LIST;

typedef struct _COMMON_LIST:_DOUBLY_LINKED_LIST
{
    ASSIGNED_LIST Assigned;
} COMMON_LIST, *PCOMMON_LIST;

void List_UnAssign(PCOMMON_LIST lpItem);
void List_AssignItem(_CONNECTION *lpConnection,PCOMMON_LIST lpItem);

void List_RemoveItem(PCOMMON_LIST lpItem,PCOMMON_LIST *lppItems,PCOMMON_LIST *lppLastItem);
void List_InsertItem(PCOMMON_LIST lpItem,PCOMMON_LIST *lppItems,PCOMMON_LIST *lppLastItem);
void List_Merge(PCOMMON_LIST *lppItems,PCOMMON_LIST *lppLastItem,PCOMMON_LIST lpTail,PCOMMON_LIST lpTailLastItem);

#endif // LIST_H_INCLUDED
