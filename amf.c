#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "amf.h"

#define ALLOC(type, nobjs) ((type *)malloc(sizeof(type) * nobjs))

static struct amf_value *amf__new_value(char type) {
    struct amf_value *v = ALLOC(struct amf_value, 1);
    if (v) {
	memset(v, 0, sizeof(*v));
	v->retain_count = 1;
	v->type = type;
    }
    return v;
}

static void amf__free_value(AMFValue v) {
    // TODO
}

AMFValue amf_retain(AMFValue v) {
    v->retain_count++;
    return v;
}

void amf_release(AMFValue v) {
    if (!--v->retain_count)
	amf__free_value(v);
}

AMFValue amf_new_number(double number) {
    struct amf_value *v = amf__new_value(AMF_NUMBER);
    if (v)
	v->v.number = number;
    return v;
}

AMFValue amf_new_boolean(char boolean) {
    struct amf_value *v = amf__new_value(AMF_BOOLEAN);
    if (v)
	v->v.boolean = boolean;
    return v;
}

AMFValue amf_new_string(const char *string, int length) {
    struct amf_value *v = amf__new_value(AMF_STRING);
    if (!v)
	return NULL;
    v->v.string.length = length;
    v->v.string.value = ALLOC(char, length + 1);
    if (!v->v.string.value) {
	free(v);
	return NULL;
    }
    memcpy(v->v.string.value, string, length);
    return v;
}

AMFValue amf_new_object() {
    return amf__new_value(AMF_OBJECT);
}

AMFValue amf_new_null() {
    return amf__new_value(AMF_NULL);
}

AMFValue amf_new_array() {
    return amf__new_value(AMF_ARRAY);
}

const char *amf_cstr(AMFValue v) {
    if (v->type != AMF_STRING)
	return NULL;
    return v->v.string.value;
}

int amf_strlen(AMFValue v) {
    if (v->type != AMF_STRING)
	return -1;
    return v->v.string.length;
}

int amf_strcmp(AMFValue a, AMFValue b) {
    assert(a->type == AMF_OBJECT);
    assert(b->type == AMF_OBJECT);
    int lena = amf_strlen(a), lenb = amf_strlen(b);
    if (lena != lenb)
	return lena - lenb;
    return strncmp(amf_cstr(a), amf_cstr(b), lena);
}

AMFValue amf_object_get(AMFValue v, AMFValue key) {
    assert(v && v->type == AMF_OBJECT);
    assert(key && key->type == AMF_STRING);

    struct amf_kvlist *kvp;
    for (kvp = v->v.object; kvp; kvp = kvp->next)
	if (strncmp(amf_cstr(key), amf_cstr(kvp->entry.key),
		    amf_strlen(kvp->entry.key)) == 0)
	    return kvp->entry.value;
    return NULL;
}

AMFValue amf_object_set(AMFValue v, AMFValue key, AMFValue value) {
    assert(v && v->type == AMF_OBJECT);
    assert(key && key->type == AMF_STRING);

    struct amf_kvlist *kvp;
    struct amf_kvlist **kvprev = &v->v.object;
    for (kvp = v->v.object; kvp; kvp = kvp->next) {
	if (strncmp(amf_cstr(key), amf_cstr(kvp->entry.key),
		    amf_strlen(kvp->entry.key)) == 0) {
	    amf_retain(value);
	    amf_release(kvp->entry.value);
	    kvp->entry.value = value;
	    return value;
	}
	kvprev = &kvp->next;
    }

    *kvprev = ALLOC(struct amf_kvlist, 1);
    if (!*kvprev)
	return NULL;
    (*kvprev)->entry.key = amf_retain(key);
    (*kvprev)->entry.value = amf_retain(value);
    (*kvprev)->next = NULL;
    return value;
}

void amf_objectiter_init(AMFObjectIter *it, AMFValue v) {
    assert(v && v->type == AMF_OBJECT);
    it->object = amf_retain(v);
    it->current = v->v.object;
}

void amf_objectiter_cleanup(AMFObjectIter *it) {
    amf_release(it->object);
    it->object = NULL;
    it->current = NULL;
}

void amf_objectiter_next(AMFObjectIter *it) {
    it->current = it->current->next;
}

AMFKeyValuePair *amf_objectiter_current(AMFObjectIter *it) {
    if (it->current)
	return &it->current->entry;
    return NULL;
}

AMFValue amf_array_push(AMFValue v, AMFValue e) {
    assert(v && v->type == AMF_ARRAY);

    struct amf_vlist *vl = ALLOC(struct amf_vlist, 1);
    if (!vl)
	return NULL;
    vl->value = amf_retain(e);
    vl->next = NULL;

    if (v->v.array.first == NULL) {
	v->v.array.first = vl;
	return e;
    }

    struct amf_vlist *p;
    for (p = v->v.array.first; p; p = p->next) {
	if (!p->next) {
	    p->next = vl;
	    return e;
	}
    }
    return NULL;
}

void amf_arrayiter_init(AMFArrayIter *it, AMFValue v) {
    assert(v && v->type == AMF_ARRAY);
    it->array = amf_retain(v);
    it->current = v->v.array.first;
}

void amf_arrayiter_cleanup(AMFArrayIter *it) {
    amf_release(it->array);
    it->array = NULL;
    it->current = NULL;
}

void amf_arrayiter_next(AMFArrayIter *it) {
    it->current = it->current->next;
}

AMFValue amf_arrayiter_current(AMFArrayIter *it) {
    if (it->current)
	return it->current->value;
    return NULL;
}

static void amf__dump_print_indent(int indent) {
    int i;
    for (i = 0; i < indent; i++)
	fprintf(stderr, "  ");
}

static void amf__dump_print(AMFValue v, int indent) {
    switch (v->type) {
	case AMF_NUMBER:
	    fprintf(stderr, "(Number) %f\n", v->v.number);
	    break;
	case AMF_BOOLEAN:
	    fprintf(stderr, "(Boolean) %s\n", v->v.boolean ? "True" : "False");
	    break;
	case AMF_STRING:
	    fprintf(stderr, "(String) \"");
	    fwrite(amf_cstr(v), amf_strlen(v), 1, stderr);
	    fprintf(stderr, "\"\n");
	    break;
	case AMF_OBJECT:
	    {
		fprintf(stderr, "(Object)\n");
		AMFObjectIter it;
		AMFKeyValuePair *kv;
		amf_objectiter_init(&it, v);
		while ((kv = amf_objectiter_current(&it)) != NULL) {
		    amf__dump_print_indent(indent + 1);
		    fprintf(stderr, "\"");
		    fwrite(amf_cstr(kv->key), amf_strlen(kv->key), 1, stderr);
		    fprintf(stderr, "\": ");
		    amf__dump_print(kv->value, indent + 2);
		    amf_objectiter_next(&it);
		}
		amf_objectiter_cleanup(&it);
	    }
	    break;
	case AMF_NULL:
	    fprintf(stderr, "(Null)\n");
	    break;
	case AMF_ARRAY:
	    {
		fprintf(stderr, "(Array)\n");
		AMFArrayIter it;
		AMFValue e;
		amf_arrayiter_init(&it, v);
		while ((e = amf_arrayiter_current(&it)) != NULL) {
		    amf__dump_print_indent(indent);
		    amf__dump_print(e, indent + 1);
		    amf_arrayiter_next(&it);
		}
		amf_arrayiter_cleanup(&it);
	    }
	    break;
	default:
	    fprintf(stderr, "(Unknown type: %02X)\n", v->type);
	    break;
    }
}

void amf_dump(AMFValue v) {
    amf__dump_print(v, 0);
}
