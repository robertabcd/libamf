#ifndef _LIST_H
#   define _LIST_H

struct list_ent {
    void *elem;
    struct list_ent *next;
};

struct list {
    int count;
    struct list_ent *head;
    struct list_ent *tail;
};


typedef struct list *List;
/* return 0 to continue; otherwise stop. */
typedef void * (* list_iterfunc) (List list, int idx, void *elem, void *ctx);

List list_new();
void list_free(List list);
int list_count(List list);
int list_push(List list, void *elem);
void *list_pop(List list);
/* if the list is fully traversed, returns NULL; otherwise, returns the value
 * that `itfunc` returns. */
void *list_foreach(List list, list_iterfunc itfunc, void *ctx);

#endif
