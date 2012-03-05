/***************************************************************************
******************************* 09/Jan/93 **********************************
****************************   Version 0.9   *******************************
************************* Author: Paolo Cignoni ****************************
*									   *
*    FILE:	list.c							   *
*									   *
* PURPOSE:	Implementing functions that manipulates lists globally.	   *
*									   *
*									   *
* EXPORTS:	NewList	      Creating a new instance of a list.	   *
*		NewHashList   Creating a new instance of a list, hashing it*
*									   *
*		ChangeEqualObjectList	Changing the EqualObject function. *
*		ChangeCompareObjectList	Changing the CompareObject function*
*									   *
*		EqualList     Checking if two lists are equal.		   *
*		CountList     Counting the number of elements in a list.   *
*		IsEmptyList   Checking if a list is empty.		   *
*									   *
*		EraseList     Erasing an entire List freeing all the memory*
*		CopyList      Creating a Copy of an entire List.	   *
*									   *
*		UnionList     Creating the list union of two lists.	   *
*		IntersectList Creating the list intersection of two lists. *
*		AppendList    Appending a list to another one.		   *
*		*DiffList						   *
*		*FusionList						   *
*									   *
* IMPORTS:								   *
*									   *
* GLOBALS:								   *
*									   *
*   NOTES:								   *
*									   *
*    TODO:	Controllare che ogni volta che si usa la tail, dove serve, *
*		si faccia un push-pop del CurrList.			   *
*									   *
****************************************************************************
***************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <OList/general.h>
#include <OList/olist.h>
#include <OList/error.h>
#include <math.h>



/***************************************************************************
*									   *
* FUNCTION:	NewList							   *
*									   *
*  PURPOSE:	Create a new instance of a list.			   *
*									   *
*   PARAMS:	The type of list and the size of the object		   *
*		to put in the list.					   *
*									   *
*   RETURN:	The created List if succesful,				   *
*		NULL else.						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	Before using a list you must use this function!!	   *
*		The parameter size is actually only to test the equalness  *
*		of two objects. 					   *
*		The type can be:					   *
*		FIFO, LIFO,						   *
*		CIRCULAR,						   *
*		ORDERED							   *
*									   *
***************************************************************************/

List NewList(TypeList type, int size)
{
 List l;

 l=(List)malloc(sizeof(struct Listtag));
 if(!l) ErrorNULL("InsertList, unable to allocate ListElem\n");

 l->objectsize=size;
 l->nobject=0;
 l->H=NULL;
 l->T=NULL;
 l->C=NULL;
 l->EqualObj=NULL;
 l->CompareObj=NULL;
 l->type=type;
 l->hash=NULL;
 return l;
}

/***************************************************************************
*									   *
* FUNCTION:	NewHashList						   *
*									   *
*  PURPOSE:	Create a new instance of a list hashing it and setting the *
*		EqualObjectFunction.					   *
*									   *
*   PARAMS:	The type of list and the size of the object		   *
*		to put in the list, EqualObject function, hash size, and   *
*		the hash function.					   *
*									   *
*   RETURN:	The created List if successful,				   *
*		NULL if creation goes wrong.				   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	This function simply call the NewList, ChangeEqualObject   *
*		and HashList Function.					   *
*		Its purpose is shorting the code.			   *
*		If HashCreation Fails this function says nothing.	   *
*									   *
***************************************************************************/

List NewHashList(TypeList type, int size,
		int (* comp)(void *elem1, void *elem2),
		int hashsize, int (*HashKey)(void *)   )
{
 List T=NULL_LIST;

 T=NewList(type,size);
 if(!T) return NULL_LIST;

 ChangeEqualObjectList(comp,T);
 HashList(hashsize,HashKey,T);

 return T;
}


/***************************************************************************
*									   *
* FUNCTION:	ChangeEqualObjectList					   *
*									   *
*  PURPOSE:	To change the EqualObject function.			   *
*									   *
*   PARAMS:	A list and a pointer to a boolean function accepting	   *
*		two object pointers					   *
*									   *
*   RETURN:	TRUE if Success 					   *
*		FALSE else						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	The function passed must accept two object of the same	   *
*		type of list ones.					   *
*									   *
*									   *
***************************************************************************/

boolean ChangeEqualObjectList(boolean (* eq)(void *elem1,
					       void *elem2), List l)
{
 if(!l) ErrorFALSE("ChangeEqualObjectList, not initialized List.\n");
 l->EqualObj=eq;
 return TRUE;
}



/***************************************************************************
*									   *
* FUNCTION:	ChangeCompareObjectList					   *
*									   *
*  PURPOSE:	To change the CompareObject function.			   *
*									   *
*   PARAMS:	A list and a pointer to a int function accepting	   *
*		two object pointers					   *
*									   *
*   RETURN:	TRUE if Success 					   *
*		FALSE else						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	The function passed must accept two object of the same	   *
*		type of list ones.					   *
*		It must return: 					   *
*		< 0	   <elem1> less than <elem2>			   *
*		= 0	  <elem1> equivalent to <elem2> 		   *
*		> 0	  <elem1> greater than <elem2>			   *
*									   *
*									   *
***************************************************************************/

boolean ChangeCompareObjectList(int (* comp)(void *elem1,
					       void *elem2), List l)
{
 if(!l) ErrorFALSE("ChangeCompareObjectList, not initialized List.\n");

 l->CompareObj=comp;
 return TRUE;
}



/***************************************************************************
*									   *
* FUNCTION:	EqualList						   *
*									   *
*  PURPOSE:	Checking if two list are equals.			   *
*									   *
*   PARAMS:	The two lists;						   *
*									   *
*   RETURN:	TRUE if the two lists are equal;			   *
*		FALSE else.						   *
*									   *
*  IMPORTS:	EqualObject						   *
*									   *
*    NOTES:	Two lists are equal if they contain same objects in the    *
*		same order.It is supposed that the two lists contain	   *
*		object of same type and the compare function is the same.  *
*		This function doesnt modify current element indicator	   *
*		in the lists.						   *
*  10/11/92	Changed while loop to for to manage Circular Lists.	   *
*									   *
***************************************************************************/

boolean EqualList(List l0, List l1)
{
 ListElem *e0,*e1;
 int i,n;
 if(!l0 || !l1) ErrorFALSE("EqualList, not initialized List.\n");

 if(l0->nobject!=l1->nobject) return FALSE;
 if(l0->objectsize!=l1->objectsize) return FALSE;

 e0=l0->T;
 e1=l1->T;
 n=l0->nobject;
 for(i=0;i<n;i++)
  {
   if(!EqualObject(e0->obj, e1->obj, l0)) return FALSE;
   e0=e0->next;
   e1=e1->next;
  }
 return TRUE;
 }


/***************************************************************************
*									   *
* FUNCTION:	CountList						   *
*									   *
*  PURPOSE:	Counting the number of elements of a list.		   *
*									   *
*   PARAMS:	The list;						   *
*									   *
*   RETURN:	The nubmer of elements in a list.			   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	Use the nobject field in List structure.		   *
*									   *
***************************************************************************/

int CountList(List l)
{
 if(!l) ErrorZERO("CountList, not initialized List.\n");
 return l->nobject;
}


/***************************************************************************
*									   *
* FUNCTION:	IsEmptyList						   *
*									   *
*  PURPOSE:	Checking if a list is empty.				   *
*									   *
*   PARAMS:	The list;						   *
*									   *
*   RETURN:	TRUE if the list is empty.				   *
*		FALSE else						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	Use the CountList function.				   *
*									   *
***************************************************************************/

int IsEmptyList(List l)
{
 if(!l) ErrorTRUE("IsEmptyList, not initialized List.\n");
 if (CountList(l)==0) return TRUE;
    else return FALSE;
}


/***************************************************************************
*									   *
* FUNCTION:	EraseList						   *
*									   *
*  PURPOSE:	Erase an entire List freeing all the memory.		   *
*									   *
*   PARAMS:	The list;						   *
*									   *
*   RETURN:	TRUE if successful.					   *
*									   *
*  IMPORTS:	ExtractList, EraseHash					   *
*									   *
*    NOTES:	All the elements of the list and the List structure are    *
*		freed. Because a List contains only pointers the objects   *
*		it contained retain in memory.				   *
*									   *
***************************************************************************/
boolean EraseList(List l)
{
 pointer nocare;

 if(!l) ErrorTRUE("EraseList, not initialized List.\n");

 while(ExtractList(&nocare,l));

 if(l->hash) EraseHashList(l);

 free(l);

 return TRUE;
}


/***************************************************************************
*									   *
* FUNCTION:	CopyList						   *
*									   *
*  PURPOSE:	Create a Copy of an entire List.			   *
*									   *
*   PARAMS:	The list to copy;					   *
*									   *
*   RETURN:	A new list containing the copy if successful.		   *
*		NULL else.						   *
*									   *
*  IMPORTS:	TailList, NextList, InsertList				   *
*									   *
*    NOTES:	Because Lists contains only pointers only them are	   *
*		duplicated! The objects they refers are not copied.	   *
*									   *
*		In LIFO and FIFO lists in head are the older elements, so  *
*		if we insert in NewL from head to tail the object order    *
*		is preserved.						   *
*									   *
*   10/11/92	Added  for(i=0;... to manage circular Lists		   *
*									   *
***************************************************************************/
List CopyList(List l)
{
 List NewL;
 pointer object;

 if(!l) ErrorNULL("CopyList, not initialized List.\n");

 NewL=(List)malloc(sizeof(struct Listtag));
 NewL->objectsize=l->objectsize;
 NewL->nobject=0;
 NewL->H=NULL;
 NewL->T=NULL;
 NewL->C=NULL;
 NewL->EqualObj=l->EqualObj;
 NewL->CompareObj=l->CompareObj;
 NewL->type=l->type;
 NewL->hash=NULL;

 if(l->type!=CIRCULAR)
  {
   PushCurrList(l);

   for(HeadList(l);PrevList(&object,l);)
      InsertList(object, NewL);

   PopCurrList(l);
  }
 else
  {
   int i,n;
   n=l->nobject;
   PushCurrList(l);
   HeadList(l);
   for(i=0;i<n;i++)
    {
     PrevList(&object,l);
     InsertList(object, NewL);
    }

   PopCurrList(l);
  }
 return NewL;
}


/***************************************************************************
*									   *
* FUNCTION:	UnionList						   *
*									   *
*  PURPOSE:	Create the list union of two lists.			   *
*									   *
*   PARAMS:	The two lists to unite;					   *
*									   *
*   RETURN:	A new list containing the union if successful.		   *
*		NULL else.						   *
*									   *
*  IMPORTS:	CopyList, TailList, NextList, MemberList, InsertList	   *
*									   *
*    NOTES:	This function does not modify the two lists passed,	   *
*		it create a new list. It is assumed that the two lists	   *
*		contains object of the same type.			   *
*									   *
***************************************************************************/

List UnionList(List L0, List L1)
{
 List UList;
 pointer object;

 if(!L0 || !L1) ErrorNULL("UnionList, not initialized List.\n");

 UList=CopyList(L0);

 TailList(L1);
 while(NextList(&object, L1))
	if(!MemberList(object, UList)) InsertList(object, UList);

 return UList;
}


/***************************************************************************
*									   *
* FUNCTION:	IntersectList						   *
*									   *
*  PURPOSE:	Create the list intersection of two lists.		   *
*									   *
*   PARAMS:	The two lists to intersecate;				   *
*									   *
*   RETURN:	A new list containing the intersection if successful.	   *
*		NULL else.						   *
*									   *
*  IMPORTS:	CopyList, TailList, NextList, MemberList, DeleteList	   *
*									   *
*    NOTES:	This function does not modify the two lists passed,	   *
*		it create a new list. It is assumed that the two lists	   *
*		contains object of the same type.			   *
*		To build intersection we make a copy of L0 and we delete   *
*		from it all the object that does not belongs to L1.	   *
*									   *
***************************************************************************/

List IntersectList(List L0, List L1)
{
 List InterList;
 pointer object;

 if(!L0 || !L1) ErrorNULL("IntersectList, not initialized List.\n");

 InterList=CopyList(L0);

 TailList(InterList);
 while(NextList(&object, InterList))
	if(!MemberList(object, L1)) DeleteList(object, InterList);

 return InterList;
}


/***************************************************************************
*									   *
* FUNCTION:	AppendList						   *
*									   *
*  PURPOSE:	Append a list to another one.				   *
*									   *
*   PARAMS:	The two lists to append.				   *
*									   *
*   RETURN:	The first list modified if successful.			   *
*		NULL else.						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	This function modify the first of lists passed,	appending  *
*		all the object of second list. It is assumed that the two  *
*		lists contain object of the same type.			   *
*		All the objects in L2 are inserted (if not presents) in L1.*
*									   *
***************************************************************************/
List AppendList(List L1, List L2)
{
 pointer object;

 if(!L1 || !L2) ErrorNULL("AppendList, not initialized List.\n");

 PushCurrList(L2);
 TailList(L2);
 while(NextList(&object, L2))
	if(!MemberList(object, L1)) InsertList(object, L2);
 PopCurrList(L2);
 return L1;
}

/*************************************************************************
listobj.c
*************************************************************************/


/***************************************************************************
*                                                                          *
* FUNCTION:     EqualObject                                                *
*                                                                          *
*  PURPOSE:     Test whether two objects are equal.                        *
*                                                                          *
*   PARAMS:     The pointers to objects and their size (all be equal).     *
*                                                                          *
*   RETURN:     TRUE if they are equal                                     *
*               FALSE else                                                 *
*                                                                          *
*  IMPORTS:     None                                                       *
*                                                                          *
*    NOTES:     If the user has not supplied a EqualObject function we     *
*               test if two objects are equals we test them word by word.  *
*                                                                          *
***************************************************************************/
boolean EqualObject(pointer h1, pointer h2, List l)
{
 int i;
 char *v1, *v2;

 if(l->EqualObj) return l->EqualObj(h1,h2);

 v1=(char *)h1;
 v2=(char *)h2;

 for(i=0;i<l->objectsize;i++)
    if(v1[i]!=v2[i]) return FALSE;

 return TRUE;
}

/***************************************************************************
*                                                                          *
* FUNCTION:	Eq_Ref							   *
*                                                                          *
*  PURPOSE:	Test whether two objects are the same pointer.		   *
*                                                                          *
*   PARAMS:	The pointers to objects;				   *
*                                                                          *
*   RETURN:     TRUE if they are equal                                     *
*               FALSE else                                                 *
*                                                                          *
*  IMPORTS:     None                                                       *
*                                                                          *
*    NOTES:	This function is one of the two for object comparing.	   *
*                                                                          *
***************************************************************************/
boolean Eq_Ref(pointer h1, pointer h2)
{
 if(h1==h2) return TRUE;
      else return FALSE;
}

/***************************************************************************
*                                                                          *
* FUNCTION:     InsertList                                                 *
*                                                                          *
*  PURPOSE:     Insert an element in a list.                               *
*                                                                          *
*   PARAMS:     The list and the pointer of the object to insert           *
*                                                                          *
*   RETURN:     TRUE if Success                                            *
*               FALSE else                                                 *
*                                                                          *
*  IMPORTS:     None                                                       *
*                                                                          *
*    NOTES:     As already stated List contains only pointers,             *
*               not real object.                                           *
*               The insert algorithm is the same for FIFO and LIFO         *
*               strategy, because in both cases we insert in Tail          *
*               In the ORDEDED case we must find where in the list         *
*               insert the new element.                                    *
*  10/Nov/92	In a CIRCULAR list we insert before l->C, that is between  *
*		l->C and l->C->prev and move l->C to the new element, so   *
*		inserting and extractig are LIFO like.			   *
*		In a CIRCULAR list Tail and Head point to two object	   *
*		such that Head->next == Tail and Tail->prev = Head.	   *
*		If we go from Tail to Head, next by next we scan the	   *
*		whole list						   *
*		If we go from Head to Tail, prev by prev we scan the	   *
*		whole list						   *
*		If nothing is deleted Head is the oldest element and Tail  *
*		the newest;						   *
*		In a CIRCULAR list l->C Must never be NULL, except when    *
*		List is empty.						   *
*                                                                          *
***************************************************************************/

boolean InsertList(pointer object, List l)
{
 ListElem *t;
 ListElem *oldT;

 t=(ListElem *)malloc(sizeof(ListElem));
 if(!t) {
	 Error("InsertList, unable to allocate ListElem\n", NO_EXIT);
	 return FALSE;
	}
 t->obj=object;
 if(l->hash) InsertHash(t, l->hash);
 switch(l->type)
  {
   case ORDERED:
		FindLessObj(object,l);
                t->next=l->C;

                if(l->C)                  /* insert before l->C */
                  {
                    t->prev=l->C->prev;

                    if(l->C->prev)  l->C->prev->next=t;  /* normal case */
                               else l->T=t;              /* l->C was Tail */
                    l->C->prev=t;
                  }
                else                      /* insert in Head */
                 {
                   t->prev=l->H;
                   if(l->H) l->H->next=t; /* list not empty */
                       else l->T=t;       /* empty list, modify Tail */
                   l->H=t;
                  }

                t->next=l->C;


		break;
   case CIRCULAR:
		if(l->C)		  /* insert before l->C */
		 {
		   t->next=l->C;	  /* normal case */

		   t->prev=l->C->prev;
		   l->C->prev->next=t;

		   l->C->prev=t;

		   l->T=l->H->next;	  /* Adjust Head and Tail.	 */

		   l->C=t;		  /* Move l->C to Last inserted  */
		 }
		else if(l->nobject==0)	  /* List was empty */
		 {
		   l->C=t;
		   t->next=t;
		   t->prev=t;
		   l->H=t;
		   l->T=t;
		 }
		else		  /* l->C==NULL and Not Empty List */
		ErrorFALSE("InsertList, NULL CurrList in a CIRCULAR list.\n")

		break;
   case FIFO:
   case LIFO:   oldT=l->T;
                l->T=t;
                l->T->next=oldT;
                l->T->prev=NULL;
                if(oldT==NULL) l->H=t; /* If empty list init head of list */
                          else oldT->prev=t;
  }


 l->nobject++;
//	fprintf(stdout,"inserted face in the list\n");
 return TRUE;
}



/***************************************************************************
*                                                                          *
* FUNCTION:     ExtractList                                                *
*                                                                          *
*  PURPOSE:     Extract an element in a list.                              *
*                                                                          *
*   PARAMS:     The list and the pointer for the object to Extract         *
*                                                                          *
*   RETURN:     TRUE if Success                                            *
*               FALSE else                                                 *
*                                                                          *
*  IMPORTS:     None                                                       *
*                                                                          *
*    NOTES:     As already stated List contains only pointers,             *
*               not real object, so we can free everything.                *
*               FIFO we extract from Head.                                 *
*               LIFO we extract from Tail.                                 *
*               ORDERED we extract the minimum, that is from Tail.         *
*               If the list contains one element after extracting          *
*               both the Head and Tail are NULL                            *                       *
*                                                                          *
*		Remember that for each extract you must update the HASH!!! *
*   10/Nov/92	Extracting from CIRCULAR list: we extract the L->C Element.*
*		l->C, l->H and l->T are NULL only if List is empty;	   *
*                                                                          *
***************************************************************************/

boolean ExtractList(pointer Object, List l)
{
pointer *object=(pointer *)Object;
 ListElem *elemtofree;

 if(l->nobject==0) return FALSE;	/* Empty list */


 switch(l->type)
  {
   case FIFO:   *object=l->H->obj;          /* object to give back */
                elemtofree=l->H;
                l->H = l->H->prev;
                if(l->H) l->H->next = NULL; /* Was not last obj */
                    else l->T=NULL;         /* Was the last obj */
		break;
   case CIRCULAR:
		if(l->C==NULL)		  /* l->C==NULL and Not Empty List */
		ErrorFALSE("ExtractList, NULL CurrList in a CIRCULAR list.\n")

		*object=l->C->obj;	    /* object to give back */
		elemtofree=l->C;
		if(l->nobject==1)
		 {
		  l->C=NULL;
		  l->T=NULL;
		  l->H=NULL;
		 }
		else
		 {
		  l->C->prev->next=l->C->next;
		  l->C->next->prev=l->C->prev;

		  if(l->C==l->H) l->H=l->H->prev;
		  if(l->C==l->T) l->T=l->T->next;

		  l->C=l->C->next;
		 }
		break;
   case ORDERED:

   case LIFO:
   default:     *object=l->T->obj;          /* object to give back */
                elemtofree=l->T;
                l->T = l->T->next;
                if(l->T) l->T->prev = NULL; /* Was not last obj */
                    else l->H = NULL;       /* Was the last obj */
  }

 l->nobject--;

 if(l->hash) if(!DeleteHash(*object, l))
	{
	 Error("ExtractList, extracted object was not in hash!\n",NO_EXIT);
	 return FALSE;
	}
 /* the following line was modified on 10 Nov 1993; previously I freed
    elemtofree before the DeleteHash; was wrong but worked on every machine /*
 free(elemtofree);	    /* free the memory of old list element */

//fprintf(stdout,"extracted face from the list\n");
 return TRUE;
}


/***************************************************************************
*                                                                          *
* FUNCTION:     MemberList                                                 *
*                                                                          *
*  PURPOSE:     Check and find if an element belong to a list.             *
*                                                                          *
*   PARAMS:     The list and the pointer of the object to find;            *
*                                                                          *
*   RETURN:     TRUE if object belongs to list;                            *
*               FALSE else                                                 *
*                                                                          *
*  IMPORTS:     None                                                       *
*                                                                          *
*    NOTES:     This function neither alloc nor free anything, only check  *
*               for the presence of the object in a list.                  *
*               If the object is found the l->C points to the element      *
*		found, else it is not changed.				   *
*               Current implementation scan the whole list.                *
*                                                                          *
*		The newer version use scanlist functions. Note that	   *
*		NextList put in l->C the ListElem next to the one	   *
*		containing the object returned, so you must take a step	   *
*		back and if you reach end get the tail.			   *
*                                                                          *
*   28/Oct/92	Note that version before 28 oct used to modify CurrList    *
*		in any case. Last version modify it only if it find it.    *
*                                                                          *
*   10/Nov/92	Modified for tracting CIRCULAR lists.			   *
*                                                                          *
*                                                                          *
***************************************************************************/

boolean oldoldMemberList(pointer object, List l)
{
 ListElem *curr;

 if(!(l->T))
  {
   l->C=NULL;
   return FALSE;        /* Empty list */
  }
 curr=l->T;

 while(curr)
  {
   if(EqualObject(object, curr->obj, l))
    {
     l->C=curr;
     return TRUE;
    }
   curr=curr->next;
  }

 l->C=NULL;
 return FALSE;
}

boolean oldMemberList(pointer object, List l)
{
 pointer scanobj;
 ListElem *TempCurrList=l->C;

 if(l->hash) return MemberHash(object, l);

 TailList(l);

 while(NextList(&scanobj, l))
   if(EqualObject(object, scanobj, l))
   {
    if(!PrevList(&scanobj, l)) HeadList(l);
    return TRUE;
   }

 l->C=TempCurrList;
 return FALSE;
}

boolean MemberList(pointer object, List l)     /* 10/Nov/92 */
{
 pointer scanobj;
 ListElem *TempCurrList=l->C;
 int i,n;

 if(l->hash) return MemberHash(object, l);

 TailList(l);
 n=CountList(l);
 for(i=0;i<n;i++)
  {
   NextList(&scanobj, l);
   if(EqualObject(object, scanobj, l))
   {
    if(!PrevList(&scanobj, l)) HeadList(l);    /* Take a step back for */
    return TRUE;			       /* right CurrList       */
   }
  }
 l->C=TempCurrList;
 return FALSE;
}

/***************************************************************************
*                                                                          *
* FUNCTION:	DeleteCurrList						   *
*                                                                          *
*  PURPOSE:     Delete the current element from a list.                    *
*                                                                          *
*   PARAMS:     The list                                                   *
*                                                                          *
*   RETURN:     TRUE if Success                                            *
*               FALSE else                                                 *
*                                                                          *
*  IMPORTS:     None                                                       *
*                                                                          *
*    NOTES:     This function is not different for LIFO and FIFO.          *
*               As already stated List contains only pointers,             *
*               not real object, so we free only the memory of the list    *
*               element, the object that it contained remains in memory    *
*               and its pointer is still valid.                            *
*               Special cases:                                             *
*                       Empty list,                                        *
*                       object to is the only one                          *
*                                                                          *
*  10/Nov/92	Changed to manage CIRCULAR lists.			   *
*		In a CIRCULAR List CurrList after deleting point to next   *
*		element.						   *
*                                                                          *
***************************************************************************/

boolean DeleteCurrList(List l)
{
 ListElem *ElemToFree=l->C;

 if(l->nobject==0)
   {
    Error("DeleteCurrList, Trying to delete in a empty list\n",NO_EXIT);
    return FALSE;	      /* Empty list */
   }
 if(l->C==NULL)
   {
    Error("DeleteCurrList, Trying to delete a NULL current of List\n",NO_EXIT);
    return FALSE;		 /* Unsetted Current element l->C   */
   }
 if(l->nobject==1)                      /* One element List, the list beco-*/
  {                                     /* me empty and all pointers NULL  */
    l->T=NULL;
    l->H=NULL;
  }

 if(l->hash) if(!DeleteHash(l->C->obj, l))
	{
	 Error("DeleteCurrList, L->C was not in hash\n",NO_EXIT);
	 return FALSE;
	}

 if(l->H==l->C) l->H=l->C->prev;
 if(l->T==l->C) l->T=l->C->next;

 if(l->C->next) l->C->next->prev=l->C->prev;
 if(l->C->prev) l->C->prev->next=l->C->next;


 if(l->type == CIRCULAR && (l->nobject > 1)) l->C=l->C->next;
					else l->C=NULL;


 free(ElemToFree);
 l->nobject--;
 return TRUE;
}


/***************************************************************************
*                                                                          *
* FUNCTION:     DeleteList                                                 *
*                                                                          *
*  PURPOSE:     Delete an element from a list.                             *
*                                                                          *
*   PARAMS:     The list and the pointer of the object to delete           *
*                                                                          *
*   RETURN:     TRUE if Success                                            *
*               FALSE else                                                 *
*                                                                          *
*  IMPORTS:     None                                                       *
*                                                                          *
*    NOTES:     This function simply test for the presence in the list of  *
*               the element and delete it.                                 *
*                                                                          *
*   10/nov/92	Nasty Bug:						   *
*		If before delete CurrList points to deleting element we    *
*		mantain in l->C freed memory pointer.			   *
*		MemberList put in l->C the found element;		   *
*		DeleteCurrList set l->C=NULL or l->C=l->C->next for	   *
*		CIRCULAR lists						   *
*		So the only dangerous case is when DeleteCurrList set	   *
*		l->C=l->C->next.					   *
*                                                                          *
***************************************************************************/

boolean DeleteList(pointer object, List l)
{
 boolean result=FALSE;
 ListElem *OldCurr=l->C;
 ListElem *OldCurrNext=NULL;

 if(l->C) OldCurrNext = l->C->next;

 if(MemberList(object,l)) result=DeleteCurrList(l);
 else result=FALSE;

 if(OldCurrNext!=l->C) l->C=OldCurr;

 return result;
}



/***************************************************************************
*                                                                          *
* FUNCTION:     FindLessObj                                                *
*                                                                          *
*  PURPOSE:     Find the element in a list that is less than Object        *
*                                                                          *
*   PARAMS:     The list and the pointer of the object to compare          *
*                                                                          *
*   RETURN:     TRUE if Success                                            *
*               FALSE else                                                 *
*                                                                          *
*  IMPORTS:     None                                                       *
*                                                                          *
*    NOTES:     This function is used to build a list than is increasingly *
*               ordered from tail to head (tail <= head).                  *
*               The object found pointer is put in l->C;                   *
*               l->C contains an object not less than obj, l->C->prev is   *
*               less or equal than object and l->C->next, if list is       *
*               ordered, is supposed to be not less than obj.              *
*               In this version we must scan the whole list.               *
*               To mantain list ordered you must insert object before l->C.*
*               If l->C==NULL you must insert object at head.              *
*                                                                          *
***************************************************************************/

boolean FindLessObj(pointer object, List l)
{
 if(l->type!=ORDERED)
    ErrorFALSE("FindLessObj used in a not ORDERED list.\n");
 if(l->CompareObj==NULL)
    ErrorFALSE("FindLessObj, uninitialized CompareObj function.");

 l->C=l->T;
 while(l->C)
 {
  if(l->CompareObj(object,l->C->obj) <= 0) break;
  l->C=l->C->next;
 }
 return TRUE;
}
/*************************************************************************************
listhash.c
***************************************************************************************/

/***************************************************************************
*									   *
* FUNCTION:	HashList						   *
*									   *
*  PURPOSE:	Create a new instance of a hash index over a list.	   *
*									   *
*   PARAMS:	the size of the hash vector and the function HashKey.	   *
*									   *
*   RETURN:	TRUE if succesful,					   *
*		FALSE else.						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	This function is used to build the hash index over a given *
*		list; once initialized the hash struct with an index	   *
*		composed by n BuckElem we scan all the list and insert its *
*		elements in the hash index.				   *
*									   *
*									   *
***************************************************************************/

boolean HashList(int size, int (*HashKey)(void *), List l)
{
 Hash *H;
 pointer nocarepointer;

 if(l->hash) {
	      Error("HashList, Hash index already existing.\n",NO_EXIT);
	      EraseHashList(l);
	     }
 if(size<5) ErrorFALSE("HashList, Hash size is too little to be useful.\n");
 H=(Hash *)malloc(sizeof(Hash));

 if(!H) ErrorFALSE("NewHash, unable to allocate Hash struct.\n");

 H->size=size;
 H->HashKey=HashKey;
 H->H=(BuckElem **)calloc(size,sizeof(BuckElem *));
 if(!H->H) ErrorFALSE("NewHash, unable to allocate Hash vector.\n");
 l->hash=H;

 PushCurrList(l);
 for(TailList(l); l->C; NextList(&nocarepointer, l))
				InsertHash(l->C, l->hash);
 PopCurrList(l);

 return TRUE;
}

/***************************************************************************
*									   *
* FUNCTION:	InsertHash						   *
*									   *
*  PURPOSE:	Add a ListElem referring in the hash.			   *
*									   *
*   PARAMS:	The listElem to add and the hash.			   *
*									   *
*   RETURN:	TRUE if succesful,					   *
*		FALSE else.						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	The Hash index is structured as a vector of BuckElem	   *
*		pointers. The le field of each of them points to the	   *
*		ListElem of List to index. For each element to add we	   *
*		allocate a BuckElem.					   *
*									   *
***************************************************************************/

boolean InsertHash(ListElem *le, Hash *h)
{
 BuckElem *bucket;

 long ind=abs(h->HashKey(le->obj));

 ind=ind%(h->size);

 bucket=(BuckElem *)malloc(sizeof(BuckElem));
 if(!bucket) {
	 Error("InsertHash, unable to allocate bucket.\n",NO_EXIT);
	 return FALSE;
	}

 bucket->le=le;
 bucket->next=h->H[ind];
 bucket->prev=NULL;
 if(h->H[ind]) h->H[ind]->prev=bucket;
 h->H[ind]=bucket;

 return TRUE;
}

/***************************************************************************
*									   *
* FUNCTION:	MemberHash						   *
*									   *
*  PURPOSE:	Retrieve a ListElem referring in a hashed list.		   *
*									   *
*   PARAMS:	The ListElem to find and the hash.			   *
*									   *
*   RETURN:	TRUE if succesful,					   *
*		FALSE else.						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	This function use hash index for checking the presence of  *
*		an element in a list. Using the HashKey function we find   *
*		the right bucket, then we scan all its ListElem. Remember  *
*		that a ListElem of a bucket has a ListElem as object (in a *
*		bucket we contains refers to ListElems) this explain that  *
*		we check:						   *
*		EqualObject(obj, ler->obj, l)				   *
*		where ler is the ListElement Referred in bucket.	   *
*									   *
***************************************************************************/

boolean MemberHash(pointer obj, List l)
{
 ListElem *le;
 BuckElem *b;
 long ind;
 ListElem *TempCurrList=l->C;

 if(!l->hash)
    {
     Error("MemberHash, List not hashed!\n",NO_EXIT);
     return FALSE;
    }

 ind=abs(l->hash->HashKey(obj));
 ind=ind%(l->hash->size);

 b=l->hash->H[ind];
 while(b)
  {
   le=b->le;
   if(EqualObject(obj, le->obj, l))
       {
	l->C=le;
	return TRUE;
       }
   b=b->next;
  }
 l->C=TempCurrList;
 return FALSE;
}


/***************************************************************************
*									   *
* FUNCTION:	DeleteHash						   *
*									   *
*  PURPOSE:	Delete a ListElem referring in a hashed list.		   *
*									   *
*   PARAMS:	The object in the ListElem to find and the List.	   *
*									   *
*   RETURN:	TRUE if succesful,					   *
*		FALSE else.						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:								   *
*									   *
***************************************************************************/

boolean DeleteHash(pointer obj, List l)
{
 ListElem *le;
 BuckElem *b;
 long ind;

 if(!l->hash)
    {
     Error("DeleteHash, List not hashed!\n",NO_EXIT);
     return FALSE;
    }
 ind=abs(l->hash->HashKey(obj));
 ind=ind%(l->hash->size);

 b=l->hash->H[ind];
 while(b)
  {
   le=b->le;
   if(EqualObject(obj, le->obj, l))
       {
	if(b->prev) (b->prev)->next=b->next;
	       else l->hash->H[ind]=b->next;
	if(b->next) (b->next)->prev=b->prev;
	free(b);
	return TRUE;
       }
   b=b->next;
  }

 return FALSE;
}


/***************************************************************************
*									   *
* FUNCTION:	EraseHashList						   *
*									   *
*  PURPOSE:	Destroy an hash index from a list.			   *
*									   *
*   PARAMS:	The hash to destroy.					   *
*									   *
*   RETURN:	TRUE if succesful,					   *
*		FALSE else.						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	This function frees all the memory allocated for the hash  *
*		index; for each bucket free all the BuckElem allocated in	*
*		it. Then free the whole vector of ListElem pointers.	   *
*									   *
***************************************************************************/
boolean EraseHashList(List l)
{
 int i;
 Hash *h;
 BuckElem *next, *it;
 if(!l->hash)
   {
    Error("EraseHash, trying to erase an unexistent hash.\n",NO_EXIT);
    return FALSE;
   }
 h=l->hash;
 for(i=0;i<h->size;i++)	    /* For each bucket	*/
   if(h->H[i])		    /* not empty	*/
     {
      it=h->H[i];	    /* it is the first ListElem */

      while(it)		    /* For each ListElem	*/
	{
	 next=it->next;     /* next is the next   :)	*/
	 free(it);
	 it=next;	    /* the next it is it! :)	*/
	}
     }
 free(h->H);
 free(h);
 l->hash=NULL;

 return TRUE;
}

/***************************************************************************
*									   *
* FUNCTION:	ReHashList						   *
*									   *
*  PURPOSE:	Rehash a list changing the hash size.			   *
*									   *
*   PARAMS:	The hash index size and the list to rehash.		   *
*									   *
*   RETURN:	TRUE if succesful,					   *
*		FALSE else.						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	This function erase the old hash and create a new one.	   *
*		So all the list elements must inserted in it again; this   *
*		function is time consuming. If the new hashing fails the   *
*		old one is preserved.					   *
*									   *
***************************************************************************/
boolean ReHashList(int newsize, List l)
{
 Hash *hold, *hnew;

 hold=l->hash;
 l->hash=NULL;
 if(HashList(newsize, hold->HashKey, l))
     {
      hnew=l->hash;
      l->hash=hold;
      EraseHashList(l);
      l->hash=hnew;
      return TRUE;
     }
 else
     {
      l->hash=hold;
      return TRUE;
     }
}
/**********************************************************************************
listscan.c
***********************************************************************************/

/***************************************************************************
*  GLOBAL:	CurrListStack						   *
*									   *
* PURPOSE:	This global variable is used to mantain a stack of current *
*		of list to permit scanning of list without modifying the   *
*		current of a list.					   *
*									   *
***************************************************************************/
List CurrListStack=NULL;



/***************************************************************************
*									   *
* FUNCTION:	HeadList, TailList					   *
*									   *
*  PURPOSE:	Reset the current object to the Head (Tail) of the list.   *
*									   *
*   PARAMS:	The list;						   *
*									   *
*   RETURN:	TRUE if successful.					   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	It set the Current ListElem  pointer to the value of	   *
*		l->H (T).						   *
*		This function must be used before scanning a list with the *
*		NextElemList PrevElemList function.			   *
*									   *
***************************************************************************/

boolean HeadList(List l)
{
 l->C=l->H;

 return TRUE;
}

boolean TailList(List l)
{
 l->C=l->T;

 return TRUE;
}


/***************************************************************************
*									   *
* FUNCTION:	CurrList						   *
*									   *
*  PURPOSE:	Get the object in the current indicator of list.	   *
*									   *
*   PARAMS:	The list and a pointer where put the pointer find;	   *
*									   *
*   RETURN:	TRUE if current indicator of list is not NULL.		   *
*		FALSE else						   *
*									   *
*  IMPORTS:	None							   *
*									   *
*    NOTES:	Note that:						   *
*									   *
*		CurrList(&obj0, l);					   *
*		NextList(&obj1, l);					   *
*									   *
*		then obj0 is equal to obj1, but:			   *
*									   *
*		NextList(&obj0, l);					   *
*		CurrList(&obj1, l);					   *
*									   *
*		then obj0 is not equal to obj1. 			   *
*		Here are two examples of using CurrList for List scanning: *
*									   *
*		TailList(l)						   *
*		while(CurrList(&obj,l))					   *
*		   {							   *
*		    MakeSomethingListElem(obj);				   *
*		    NextList(&nocare, l);				   *
*		   }							   *
*									   *
*		for(TailList(l); CurrList(&obj,l); NextList(&nocare, l))   *
*		    MakeSomethingListElem(obj);				   *
*									   *
*		The user is suggested to use only NextList for scanning.   *
*									   *
***************************************************************************/
boolean CurrList(pointer Object, List l)
/* boolean CurrList(pointer *object, List l) Modified to avoid a warning */
{
 pointer *object=(pointer *)Object;
 if(l->C)
     {
      *object = l->C->obj;
      return TRUE;
     }
 else return FALSE;
}


/***************************************************************************
*                                                                          *
* FUNCTION:	NextList, PrevList					   *
*                                                                          *
*  PURPOSE:     Scan all the elements of a list without destroying them.   *
*                                                                          *
*   PARAMS:     The list, and a pointer where put the pointer find;        *
*                                                                          *
*   RETURN:     TRUE if successful.                                        *
*               FALSE if list ends.                                        *
*                                                                          *
*  IMPORTS:     None                                                       *
*                                                                          *
*    NOTES:     It use the ListElem pointer C to mantain the position      *
*               inside a list.                                             *
*               Its name is a little uncorrect because it returns the      *
*               current element instead of the next (prev) element, but    *
*               also increment that pointer, from that the name.           *
*               To remember                                                *
*               Next move from tail to head                                *
*               Prev move from head to tail                                *
*                                                                          *
*               Examples:                                                  *
*                                                                          *
*               TailList(l);                                               *
*		while(NextList(&obj,l)) MakeSomething(obj);		   *
*                                                                          *
*		for(TailList(l);NextList(&obj,l);) MakeSomething(obj);	   *
*                                                                          *
*               HeadList(l);                                               *
*		while(PrevList(&obj,l)) MakeSomething(obj);		   *
*                                                                          *
*		for(HeadList(l);PrevList(&obj,l);) MakeSomething(obj);	   *
*                                                                          *
*		If you want to use instead of an object the current	   *
*		indicator l->C of the list (as we do in hash functions)    *
*		you must remember that after NextList(&obj,l) l->C points  *
*		to the ListElem after the one containing obj so you must   *
*		do NextList as last thing.				   *
*                                                                          *
*		TailList(l)						   *
*		while(CurrList(&obj,l))					   *
*		   {							   *
*		    MakeSomethingListElem(obj);				   *
*		    NextList(nocare, l);				   *
*		   }							   *
*									   *
*		for(TailList(l); CurrList(&obj,l); NextList(nocare, l))	   *
*		    MakeSomethingListElem(obj);				   *
*									   *
***************************************************************************/

boolean NextList(pointer Object, List l)
{
 pointer *object=(pointer *)Object;
 if(l->C==NULL) return FALSE;

 *object=l->C->obj;
 l->C=l->C->next;

 return TRUE;
}

boolean PrevList(pointer Object, List l)
{
 pointer *object=(pointer *)Object;
 if(l->C==NULL) return FALSE;

 *object=l->C->obj;
 l->C=l->C->prev;

 return TRUE;
}


/***************************************************************************
*                                                                          *
* FUNCTION:	PushCurrList, PopCurrList				   *
*                                                                          *
*  PURPOSE:	Store and retrieve in a stack the Current of a list	   *
*                                                                          *
*   PARAMS:	The list						   *
*                                                                          *
*   RETURN:     TRUE if Success                                            *
*               FALSE else                                                 *
*                                                                          *
*  IMPORTS:	The global CurrListStack				   *
*                                                                          *
*    NOTES:	This function is used to leave unchanged the current	   *
*		indicator of a list. Infact many functions explicitely	   *
*		modify that indicator. This function provide a tool for    *
*		overiding this problem. 				   *
*		For a safe use any function before exiting must pop all it *
*		pushed!!						   *
*		E.g. for analyzing a list without changing the current of  *
*		the list:						   *
*									   *
*		PushCurrList(l);					   *
*		TailList(l);						   *
*		while(NextList(l,&obj)) MakeSomething(obj);		   *
*		PopCurrList(l);						   *
*									   *
***************************************************************************/
boolean PushCurrList(List l)
{
 if(!CurrListStack) CurrListStack=NewList(LIFO,sizeof(ListElem *));

 return InsertList(l->C, CurrListStack);
}

boolean PopCurrList(List l)
{
 if(!CurrListStack) Error("PopCurrList, pop without push!\n", NO_EXIT);

 ExtractList((pointer)&(l->C), CurrListStack);
 return TRUE;
}


