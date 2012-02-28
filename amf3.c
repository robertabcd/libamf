#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "amf3.h"

#define ALLOC(type, nobjs) ((type *)malloc(sizeof(type) * nobjs))

static struct amf3_value *amf3__new_value(char type) {
    struct amf3_value *v = ALLOC(struct amf3_value, 1);
    if (v) {
	memset(v, 0, sizeof(*v));
	v->retain_count = 1;
	v->type = type;
    }
    return v;
}

static void amf3__free_value(struct amf3_value *v) {
    // TODO
}

AMF3Value amf3_retain(AMF3Value v) {
    assert(v);
    v->retain_count++;
    return v;
}

void amf3_release(AMF3Value v) {
    if (!--v->retain_count)
	amf3__free_value(v);
}

AMF3Value amf3_new_undefined() {
    return amf3__new_value(AMF3_UNDEFINED);
}

AMF3Value amf3_new_false() {
    return amf3__new_value(AMF3_FALSE);
}

AMF3Value amf3_new_true() {
    return amf3__new_value(AMF3_TRUE);
}

AMF3Value amf3_new_integer(int value) {
    AMF3Value v = amf3__new_value(AMF3_INTEGER);
    if (v)
	v->v.integer = value;
    return v;
}

AMF3Value amf3_new_double(double value) {
    AMF3Value v = amf3__new_value(AMF3_DOUBLE);
    if (v)
	v->v.real = value;
    return v;
}

static struct amf3_value *amf3__new_binary(char type, const char *data, int length) {
    struct amf3_value *v = amf3__new_value(type);
    if (v) {
	v->v.binary.length = length;
	v->v.binary.data = malloc(length + 1);
	if (!v->v.binary.data) {
	    amf3__free_value(v);
	    return NULL;
	}
	memcpy(v->v.binary.data, data, length);
	v->v.binary.data[length] = '\0';
    }
    return v;
}

AMF3Value amf3_new_string(const char *utf8) {
    return amf3__new_binary(AMF3_STRING, utf8, strlen(utf8));
}

AMF3Value amf3_new_xmldoc(const char *doc, int length) {
    return amf3__new_binary(AMF3_XMLDOC, doc, length);
}

AMF3Value amf3_new_xml(const char *doc, int length) {
    return amf3__new_binary(AMF3_XML, doc, length);
}

AMF3Value amf3_new_bytearray(const char *bytes, int length) {
    return amf3__new_binary(AMF3_BYTEARRAY, bytes, length);
}

AMF3Value amf3_new_date(double date) {
    AMF3Value v = amf3__new_value(AMF3_DATE);
    if (v)
	v->v.date.value = date;
    return v;
}

int amf3_string_cmp(AMF3Value a, AMF3Value b) {
    assert(a && a->type == AMF3_STRING);
    assert(b && b->type == AMF3_STRING);
    return strcmp(a->v.binary.data, b->v.binary.data);
}

static struct amf3_value *amf3__new_traits(char externalizable, char dynamic, int nmemb) {
    struct amf3_value *v = amf3__new_value(AMF3_TRAITS);
    if (v) {
	v->v.traits.externalizable = externalizable;
	v->v.traits.dynamic = dynamic;
	if (nmemb > 0) {
	    struct amf3_value **list = ALLOC(struct amf3_value *, nmemb);
	    if (!list) {
		amf3__free_value(v);
		return NULL;
	    }
	    memset(list, 0, sizeof(*list) * nmemb);
	    v->v.traits.members = list;
	    v->v.traits.nmemb = nmemb;
	}
    }
    return v;
}

static void amf3__traits_member_set(struct amf3_value *traits, int idx, struct amf3_value *key) {
    assert(traits && traits->type == AMF3_TRAITS);
    assert(key && key->type == AMF3_STRING);
    assert(idx < traits->v.traits.nmemb);
    struct amf3_value **list = traits->v.traits.members;
    if (list[idx] == key)
	return;
    if (list[idx] != NULL)
	amf3_release(list[idx]);
    list[idx] = amf3_retain(key);
}

AMF3Value amf3_new_array() {
    AMF3Value v = amf3__new_value(AMF3_ARRAY);
    if (v) {
	v->v.array.assoc_list = list_new();
	v->v.array.dense_list = list_new();
    }
    return v;
}

void amf3_array_push(AMF3Value a, AMF3Value v) {
    assert(a->type == AMF3_ARRAY);
    list_push(a->v.array.dense_list, amf3_retain(v));
}

static void *amf3__kv_replace_cb(List list, int idx, struct amf3_kv *inlist, struct amf3_kv *repas) {
    if (amf3_string_cmp(inlist->key, repas->key) == 0) {
	if (inlist->value != repas->value) {
	    amf3_release(inlist->value);
	    inlist->value = amf3_retain(repas->value);
	}
	return repas; // a non-null pointer will do
    }
    return NULL;
}

static void *amf3__kv_get_cb(List list, int idx, struct amf3_kv *inlist, AMF3Value key) {
    if (amf3_string_cmp(inlist->key, key) == 0)
	return inlist->value;
    return NULL;
}

void amf3_array_assoc_set(AMF3Value a, AMF3Value key, AMF3Value value) {
    assert(a && a->type == AMF3_ARRAY);
    assert(key && key->type == AMF3_STRING);
    struct amf3_kv kv = {key, value};
    list_foreach(a->v.array.assoc_list, amf3__kv_replace_cb, &kv);
}

AMF3Value amf3_array_assoc_get(AMF3Value a, AMF3Value key) {
    assert(a && a->type == AMF3_ARRAY);
    assert(key && key->type == AMF3_STRING);
    return list_foreach(a->v.array.assoc_list, amf3__kv_get_cb, key);
}

AMF3Value amf3_new_object(AMF3Value type, char dynamic, AMF3Value *member_names, int nmemb) {
    assert(!type || type && type->type == AMF3_STRING);
    assert(nmemb >= 0);

    AMF3Value traits = amf3__new_traits(0, dynamic, nmemb);
    if (!traits)
	return NULL;
    int i;
    for (i = 0; i < nmemb; i++)
	amf3__traits_member_set(traits, i, member_names[i]);

    AMF3Value v = amf3__new_value(AMF3_OBJECT);
    if (!v) {
	amf3_release(traits);
	return NULL;
    }
    if (type)
	v->v.object.type = amf3_retain(type);
    v->v.object.traits = traits;
    if (nmemb > 0) {
	v->v.object.m.i.member_values = ALLOC(AMF3Value *, nmemb);
	memset(v->v.object.m.i.member_values, 0, sizeof(AMF3Value *) * nmemb);
    }
    if (dynamic)
	v->v.object.m.i.dynmemb_list = list_new();
    return v;
}

AMF3Value amf3_object_prop_get(AMF3Value o, AMF3Value key) {
    assert(o && o->type == AMF3_OBJECT);
    assert(key && key->type == AMF3_STRING);
    if (o->v.object.traits->externalizable) {
	// TODO
	return NULL;
    }
    if (o->v.object.traits->nmemb > 0) {
	AMF3Value *member_names = o->v.object.traits->members;
	int i;
	for (i = 0; i < o->v.object.traits->nmemb; i++)
	    if (amf3_string_cmp(key, members_names[i]) == 0)
		return o->v.object.m.i.member_values[i];
    }
    if (o->v.object.traits->dynamic)
	return list_foreach(o->v.object.m.i.dynmemb_list, amf3__kv_get_cb, key);
    return NULL;
}

AMF3Value amf3_new_object_external(AMF3Value type, void *external_ctx) {
    assert(type && type->type == AMF3_STRING);

    AMF3Value traits = amf3__new_traits(1, 0, 0);
    if (!traits)
	return NULL;

    AMF3Value v = amf3__new_value(AMF3_OBJECT);
    if (!v) {
	amf3_release(traits);
	return NULL;
    }
    v->v.object.type = amf3_retain(type);
    v->v.object.traits = traits;
    v->v.object.m.external_ctx = external_ctx;
    return v;
}
