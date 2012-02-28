#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"

List list_new() {
    List list = malloc(sizeof(struct list));
    if (list)
	memset(list, 0, sizeof(*list));
    return list;
}

void list_free(List list) {
    struct list_ent *e = list->head;
    while (e) {
	struct list_ent *t = e;
	e = e->next;
	free(t);
    }
    free(list);
}

int list_count(List list) {
    return list->count;
}

int list_push(List list, void *elem) {
    struct list_ent *e = malloc(sizeof(struct list_ent));
    if (!e)
	return -1;
    e->elem = elem;
    e->next = NULL;
    if (list->tail) {
	list->tail->next = e;
	list->tail = e;
    } else
	list->head = list->tail = e;
    return ++list->count;
}

void *list_pop(List list) {
    assert(list->tail);
    struct list_ent *e = list->tail;
    void *elem = e->elem;
    if (list->head == e)
	list->head = list->tail = NULL;
    else {
	list->tail = list->head;
	while (list->tail->next != e)
	    list->tail = list->tail->next;
	list->tail->next = NULL;
    }
    free(e);
    list->count--;
    return elem;
}

void *list_foreach(List list, list_iterfunc itfunc, void *ctx) {
    struct list_ent *e;
    int i = 0;
    for (e = list->head; e; e = e->next) {
	void *ret = itfunc(list, i++, e->elem, ctx);
	if (ret)
	    return ret;
    }
    return NULL;
}
