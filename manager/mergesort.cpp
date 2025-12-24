#include "includes.h"

#include "mergesort.h"

inline void Move2Before1(PDOUBLY_LINKED_LIST lpNode1,PDOUBLY_LINKED_LIST lpNode2)
{
    PDOUBLY_LINKED_LIST lpPrev=lpNode2->lpPrev,
                        lpNext=lpNode2->lpNext;

    lpPrev->lpNext=lpNext;
    if (lpNext)
        lpNext->lpPrev=lpPrev;

    lpPrev=lpNode1->lpPrev;
    if (lpPrev)
        lpPrev->lpNext=lpNode2;

    lpNode1->lpPrev=lpNode2;
    lpNode2->lpPrev=lpPrev;
    lpNode2->lpNext=lpNode1;
    return;
}

static void MergeSortInt(PDOUBLY_LINKED_LIST *lppList,CMP_PROC CmpProc)
{
    do
    {
        PDOUBLY_LINKED_LIST lpList=*lppList;
        if (!lpList)
            break;

        if (!lpList->lpNext)
            break;

        int iMul=1;
        while (true)
        {
            PDOUBLY_LINKED_LIST lpFirst=lpList,lpPrevBase=NULL,lpSecond;
            while (lpFirst)
            {
                if (iMul == 1)
                {
                    lpSecond=lpFirst->lpNext;
                    if (!lpSecond)
                    {
                        lpFirst->lpJump=NULL;
                        break;
                    }

                    lpFirst->lpJump=lpSecond->lpNext;
                }
                else
                {
                    lpSecond=lpFirst->lpJump;
                    if (!lpSecond)
                        break;

                    lpFirst->lpJump=lpSecond->lpJump;
                }

                PDOUBLY_LINKED_LIST lpBase=lpFirst;
                if (CmpProc(lpSecond,lpSecond->lpPrev))
                {
                    int iCnt1=iMul,iCnt2=iMul;
                    while ((iCnt1) && (iCnt2))
                    {
                        if (CmpProc(lpSecond,lpFirst))
                        {
                            if (lpFirst == lpBase)
                            {
                                if (lpPrevBase)
                                    lpPrevBase->lpJump=lpSecond;

                                lpBase=lpSecond;
                                lpBase->lpJump=lpFirst->lpJump;
                                if (lpFirst == lpList)
                                    lpList=lpSecond;
                            }

                            PDOUBLY_LINKED_LIST lpTmp=lpSecond->lpNext;
                            Move2Before1(lpFirst,lpSecond);
                            lpSecond=lpTmp;
                            if (!lpSecond)
                            {
                                lpFirst=NULL;
                                break;
                            }
                            iCnt2--;
                        }
                        else
                        {
                            lpFirst=lpFirst->lpNext;
                            iCnt1--;
                        }
                    }
                }

                lpFirst=lpBase->lpJump;
                lpPrevBase=lpBase;
            }

            if (!lpList->lpJump)
                break;

            iMul <<= 1;
        }

        *lppList=lpList;
    }
    while (false);
    return;
}

void MergeSort(PDOUBLY_LINKED_LIST *lppList,PDOUBLY_LINKED_LIST *lppLastItem,CMP_PROC CmpProc)
{
    MergeSortInt(lppList,CmpProc);

    if (lppLastItem)
    {
        PDOUBLY_LINKED_LIST lpItem=*lppList,lpPrev=NULL;
        while (lpItem)
        {
            lpPrev=lpItem;

            lpItem=lpItem->lpNext;
        }

        *lppLastItem=lpPrev;
    }
    return;
}

