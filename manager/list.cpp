#include "includes.h"

#include "list.h"

void List_UnAssign(PCOMMON_LIST lpItem)
{
    PASSIGNED_LIST lpAssigned=&lpItem->Assigned;
    PCONNECTION lpConnection=lpAssigned->lpAssignedTo;

    lpAssigned->lpAssignedTo=NULL;

    if (lpAssigned->lpPrev)
        ((PCOMMON_LIST)lpAssigned->lpPrev)->Assigned.lpNext=lpAssigned->lpNext;
    else
        lpConnection->lpAssigned=(PCOMMON_LIST)lpAssigned->lpNext;

    if (!lpAssigned->lpNext)
        lpConnection->lpLastAssigned=(PCOMMON_LIST)lpAssigned->lpPrev;
    else
        ((PCOMMON_LIST)lpAssigned->lpNext)->Assigned.lpPrev=lpAssigned->lpPrev;

    return;
}

void List_AssignItem(PCONNECTION lpConnection,PCOMMON_LIST lpItem)
{
    PASSIGNED_LIST lpAssigned=&lpItem->Assigned;
    lpAssigned->lpAssignedTo=lpConnection;

    do
    {
        if (!lpConnection->lpLastAssigned)
        {
            lpAssigned->lpPrev=NULL;
            lpAssigned->lpNext=NULL;

            lpConnection->lpAssigned=lpItem;
            lpConnection->lpLastAssigned=lpItem;
            break;
        }

        lpAssigned->lpNext=NULL;
        lpAssigned->lpPrev=lpConnection->lpLastAssigned;
        lpConnection->lpLastAssigned->Assigned.lpNext=lpItem;

        lpConnection->lpLastAssigned=lpItem;
    }
    while (false);
    return;
}

void List_RemoveItem(PCOMMON_LIST lpItem,PCOMMON_LIST *lppItems,PCOMMON_LIST *lppLastItem)
{
    if (lpItem->lpPrev)
        lpItem->lpPrev->lpNext=lpItem->lpNext;
    else
        *lppItems=(PCOMMON_LIST)lpItem->lpNext;

    if (!lpItem->lpNext)
        *lppLastItem=(PCOMMON_LIST)lpItem->lpPrev;
    else
        lpItem->lpNext->lpPrev=lpItem->lpPrev;

    return;
}

void List_InsertItem(PCOMMON_LIST lpItem,PCOMMON_LIST *lppItems,PCOMMON_LIST *lppLastItem)
{
    do
    {
        PCOMMON_LIST lpLastItem=*lppLastItem;
        if (!lpLastItem)
        {
            lpItem->lpPrev=NULL;
            lpItem->lpNext=NULL;

            *lppItems=lpItem;
            *lppLastItem=lpItem;
            break;
        }

        lpItem->lpNext=NULL;
        lpItem->lpPrev=lpLastItem;
        lpLastItem->lpNext=lpItem;

        *lppLastItem=lpItem;
    }
    while (false);
    return;
}

void List_Merge(PCOMMON_LIST *lppItems,PCOMMON_LIST *lppLastItem,PCOMMON_LIST lpTail,PCOMMON_LIST lpTailLastItem)
{
    do
    {
        PCOMMON_LIST lpLastItem=*lppLastItem;

        *lppLastItem=lpTailLastItem;

        if (!lpLastItem)
        {
            *lppItems=lpTail;
            break;
        }

        lpLastItem->lpNext=lpTail;
        lpTail->lpPrev=lpLastItem;
    }
    while (false);
    return;
}
