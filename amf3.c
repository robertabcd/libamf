#include <assert.h>
#include <stdio.h>
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

AMF3Value amf3_new_null() {
    return amf3__new_value(AMF3_NULL);
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

static struct amf3_value *amf3__new_binary(
	char type, const char *data, int length) {
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

AMF3Value amf3_new_string(const char *string, int length) {
    return amf3__new_binary(AMF3_STRING, string, length);
}

AMF3Value amf3_new_string_utf8(const char *utf8) {
    return amf3_new_string(utf8, strlen(utf8));
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

int amf3_string_len(AMF3Value v) {
    assert(v && v->type == AMF3_STRING);
    return v->v.binary.length;
}

static struct amf3_value *amf3__new_traits(AMF3Value type,
	char externalizable, char dynamic, int nmemb) {
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
	v->v.traits.type = amf3_retain(type);
    }
    return v;
}

static void amf3__traits_member_set(
	struct amf3_value *traits, int idx, struct amf3_value *key) {
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
	    amf3_retain(repas->value);
	    amf3_release(inlist->value);
	    inlist->value = repas->value;
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

static AMF3Value amf3__new_object_direct(
	AMF3Value traits, AMF3Value *members, List dynmemb_list) {
    AMF3Value v = amf3__new_value(AMF3_OBJECT);
    if (!v)
	return NULL;
    int nmemb = traits->v.traits.nmemb;
    if (nmemb > 0 && members == NULL) {
	members = ALLOC(AMF3Value, nmemb);
	if (!members) {
	    amf3_release(v);
	    return NULL;
	}
	memset(members, 0, sizeof(AMF3Value) * nmemb);
    }
    v->v.object.traits = amf3_retain(traits);
    v->v.object.m.i.member_values = members;
    v->v.object.m.i.dynmemb_list = dynmemb_list;
    return v;
}

AMF3Value amf3_new_object(AMF3Value type, char dynamic,
	AMF3Value *member_names, int nmemb) {
    assert(!type || (type && type->type == AMF3_STRING));
    assert(nmemb >= 0);

    AMF3Value traits = amf3__new_traits(
	    type ? type : amf3_new_string_utf8(""),
	    0, dynamic, nmemb);
    if (!traits)
	return NULL;
    int i;
    for (i = 0; i < nmemb; i++)
	amf3__traits_member_set(traits, i, member_names[i]);

    AMF3Value v = amf3__new_object_direct(traits, NULL, list_new());
    amf3_release(traits);
    return v;
}

AMF3Value amf3_object_prop_get(AMF3Value o, AMF3Value key) {
    assert(o && o->type == AMF3_OBJECT);
    assert(key && key->type == AMF3_STRING);
    struct amf3_traits *traits = &o->v.object.traits->v.traits;
    if (traits->externalizable) {
	// TODO
	return NULL;
    }
    if (traits->nmemb > 0) {
	int i;
	for (i = 0; i < traits->nmemb; i++)
	    if (amf3_string_cmp(key, traits->members[i]) == 0)
		return o->v.object.m.i.member_values[i];
    }
    if (traits->dynamic)
	return list_foreach(o->v.object.m.i.dynmemb_list, amf3__kv_get_cb, key);
    return NULL;
}

void amf3_object_prop_set(AMF3Value o, AMF3Value key, AMF3Value value) {
    assert(o && o->type == AMF3_OBJECT);
    assert(key && key->type == AMF3_STRING);
    assert(value);
    struct amf3_traits *traits = &o->v.object.traits->v.traits;
    if (traits->externalizable) {
	// TODO
	return;
    }
    if (traits->nmemb > 0) {
	AMF3Value *member_values = o->v.object.m.i.member_values;
	int i;
	for (i = 0; i < traits->nmemb; i++)
	    if (amf3_string_cmp(key, traits->members[i]) == 0) {
		if (member_values[i] == NULL)
		    member_values[i] = amf3_retain(value);
		else if (member_values[i] != value) {
		    amf3_retain(value);
		    amf3_release(member_values[i]);
		    member_values[i] = value;
		}
		return;
	    }
    }
    if (traits->dynamic) {
	struct amf3_kv kv = {key, value};
	if (!list_foreach(o->v.object.m.i.dynmemb_list,
		    amf3__kv_replace_cb, &kv)) {
	    struct amf3_kv *kv = ALLOC(struct amf3_kv, 1);
	    if (!kv)
		return;
	    kv->key = amf3_retain(key);
	    kv->value = amf3_retain(value);
	    list_push(o->v.object.m.i.dynmemb_list, &kv);
	}
    }
}

static AMF3Value amf3__new_object_external_direct(
	AMF3Value traits, void *external_ctx) {
    assert(traits->v.traits.externalizable);
    AMF3Value v = amf3__new_value(AMF3_OBJECT);
    if (!v)
	return NULL;
    v->v.object.traits = amf3_retain(traits);
    v->v.object.m.external_ctx = external_ctx;
    return v;
}

AMF3Value amf3_new_object_external(AMF3Value type, void *external_ctx) {
    assert(type && type->type == AMF3_STRING);

    AMF3Value traits = amf3__new_traits(type, 1, 0, 0);
    if (!traits)
	return NULL;

    AMF3Value v = amf3__new_object_external_direct(traits, external_ctx);
    amf3_release(traits);
    return v;
}

struct amf3_ref_table *amf3_ref_table_new() {
    struct amf3_ref_table *r = ALLOC(struct amf3_ref_table, 1);
    if (r) {
	memset(r, 0, sizeof(*r));
	r->nalloc = AMF3_REF_TABLE_PREALLOC;
	r->refs = ALLOC(AMF3Value, r->nalloc);
	if (!r->refs) {
	    free(r);
	    return NULL;
	}
    }
    return r;
}

void amf3_ref_table_free(struct amf3_ref_table *r) {
    assert(r);
    if (r->refs)
	free(r->refs);
    free(r);
}

AMF3Value amf3_ref_table_push(struct amf3_ref_table *r, AMF3Value v) {
    assert(r);
    if (r->nref == r->nalloc)
	assert(r->refs = realloc(r->refs,
		    (r->nalloc <<= 1) * sizeof(AMF3Value *)));
    return (r->refs[r->nref++] = amf3_retain(v));
}

AMF3Value amf3_ref_table_get(struct amf3_ref_table *r, int idx) {
    return idx < r->nref ? r->refs[idx] : NULL;
}

int amf3_parse_u29(struct amf3_parse_context *c) {
    int j;
    int v = 0;
    char i;

    for (j = 0; j < 3; j++) {
	if (c->left < 1) return -1;
	i = *c->p++; c->left--;
	v = (v << 7) | (i & 0x7F);
	if (!(i & 0x80)) return v;
    }

    if (c->left < 1) return -1;
    i = *c->p++; c->left--;
    return (v << 8) | i;
}

AMF3Value amf3_parse_string(struct amf3_parse_context *c) {
    int len = amf3_parse_u29(c);
    if (len < 0)
	return NULL;
    if (!(len & 0x1))
	return amf3_retain(amf3_ref_table_get(c->string_refs, len >> 1));
    if ((len >>= 1) > c->left)
	return NULL;
    AMF3Value v = amf3_new_string(c->p, len);
    c->p += len;
    c->left -= len;
    if (len > 0)
	amf3_ref_table_push(c->string_refs, v);
    return v;
}

static AMF3Value amf3__parse_binary_object(
	struct amf3_parse_context *c, char type) {
    int len = amf3_parse_u29(c);
    if (len < 0)
	return NULL;
    if (!(len & 0x1))
	return amf3_retain(amf3_ref_table_get(c->object_refs, len >> 1));
    if ((len >>= 1) > c->left)
	return NULL;
    AMF3Value v = amf3__new_binary(type, c->p, len);
    c->p += len;
    c->left -= len;
    return amf3_ref_table_push(c->object_refs, v);
}

AMF3Value amf3_parse_array(struct amf3_parse_context *c) {
    int len = amf3_parse_u29(c);
    if (len < 0)
	return NULL;
    if (!(len & 0x1))
	return amf3_retain(amf3_ref_table_get(c->object_refs, len >> 1));
    len >>= 1;

    AMF3Value arr = amf3_new_array();
    if (!arr)
	return NULL;
    amf3_ref_table_push(c->object_refs, arr);

    AMF3Value key;
    while ((key = amf3_parse_string(c)) != NULL &&
	    amf3_string_len(key) > 0) {
	AMF3Value value = amf3_parse_value(c);
	if (!value) {
	    amf3_release(key);
	    amf3_release(value);
	    amf3_release(arr);
	    return NULL;
	}
	amf3_array_assoc_set(arr, key, value);
	amf3_release(key);
	amf3_release(value);
    }

    int i;
    for (i = 0; i < len; i++) {
	AMF3Value elem = amf3_parse_value(c);
	if (!elem) {
	    amf3_release(arr);
	    return NULL;
	}
	amf3_array_push(arr, elem);
	amf3_release(elem);
    }

    return arr;
}

AMF3Value amf3_parse_object(struct amf3_parse_context *c) {
    int ref = amf3_parse_u29(c);
    if (ref < 0)
	return NULL;
    if (!(ref & 0x1))
	return amf3_retain(amf3_ref_table_get(c->object_refs, ref >> 1));

    AMF3Value traits;
    AMF3Value classname;
    char external = ((ref & 0x7) == 0x7) ? 1 : 0;
    char dynamic = 0;
    int nmemb = 0;
    if ((ref & 0x3) == 0x1) {
	// traits ref
	traits = amf3_retain(amf3_ref_table_get(c->traits_refs, ref >> 2));
	classname = amf3_retain(traits->v.traits.type);
	external = traits->v.traits.externalizable;
	dynamic = traits->v.traits.dynamic;
	nmemb = traits->v.traits.nmemb;
    } else {
	classname = amf3_parse_string(c);
	if (!classname) {
	    amf3_release(traits);
	    return NULL;
	}

	if (!external) {
	    dynamic = ref >> 3;
	    nmemb = ref >> 4;
	}

	traits = amf3__new_traits(classname, external, dynamic, nmemb);
	if (!traits) {
	    amf3_release(classname);
	    return NULL;
	}

	int i;
	for (i = 0; i < ref; i++) {
	    AMF3Value key = amf3_parse_string(c);
	    if (!key) {
		amf3_release(traits);
		amf3_release(classname);
		return NULL;
	    }
	    amf3__traits_member_set(traits, i, key);
	    amf3_release(key);
	}
    }
    amf3_ref_table_push(c->traits_refs, traits);

    AMF3Value obj;
    if (external) {
	obj = amf3__new_object_external_direct(traits, NULL);
	if (!obj) {
	    amf3_release(traits);
	    return NULL;
	}
	amf3_ref_table_push(c->object_refs, obj);
	// TODO
    } else {
	obj = amf3__new_object_direct(traits, NULL, NULL);
	if (!obj) {
	    amf3_release(traits);
	    return NULL;
	}
	amf3_ref_table_push(c->object_refs, obj);

	int i, nmemb = traits->v.traits.nmemb;
	for (i = 0; i < nmemb; i++) {
	    AMF3Value value = amf3_parse_value(c);
	    if (!value) {
		amf3_release(traits);
		amf3_release(obj);
		return NULL;
	    }
	    obj->v.object.m.i.member_values[i] = value;
	}

	AMF3Value key;
	while ((key = amf3_parse_string(c)) != NULL &&
		amf3_string_len(key) > 0) {
	    AMF3Value value = amf3_parse_value(c);
	    if (!value) {
		amf3_release(traits);
		amf3_release(obj);
		amf3_release(key);
		return NULL;
	    }
	    amf3_object_prop_set(obj, key, value);
	    amf3_release(key);
	}
    }
    return obj;
}

AMF3Value amf3_parse_value(struct amf3_parse_context *c) {
    assert(c->left > 0);
    char mark = *c->p++;
    c->left--;
    switch (mark) {
	case AMF3_UNDEFINED:
	    return amf3_new_undefined();

	case AMF3_NULL:
	    return amf3_new_null();

	case AMF3_FALSE:
	    return amf3_new_false();

	case AMF3_TRUE:
	    return amf3_new_true();

	case AMF3_INTEGER:
	    {
		int integer = amf3_parse_u29(c);
		if (integer < 0)
		    return NULL;
		return amf3_new_integer(integer);
	    }

	case AMF3_DOUBLE:
	    {
		if (c->left < sizeof(uint64_t))
		    return NULL;
		uint64_t real = NTOH64(*((uint64_t *)c->p));
		c->p += sizeof(uint64_t);
		c->left -= sizeof(uint64_t);
		return amf3_new_double(*((double *)&real));
	    }

	case AMF3_STRING:
	    return amf3_parse_string(c);

	case AMF3_XMLDOC:
	case AMF3_XML:
	case AMF3_BYTEARRAY:
	    return amf3__parse_binary_object(c, mark);

	case AMF3_DATE:
	    {
		if (c->left < sizeof(uint64_t))
		    return NULL;
		uint64_t date = NTOH64(*((uint64_t *)c->p));
		c->p += sizeof(uint64_t);
		c->left -= sizeof(uint64_t);
		return amf3_new_date(*((double *)&date));
	    }

	case AMF3_ARRAY:
	    return amf3_parse_array(c);

	case AMF3_OBJECT:
	    return amf3_parse_object(c);

	default:
	    fprintf(stderr, "%s: unknown type %02X\n", __func__, mark);
	    return NULL;
    }
}
