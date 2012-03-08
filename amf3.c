#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "amf3.h"

#ifdef HAVE_FLEX_COMMON_OBJECTS
#   include "flex.h"
#endif

static const struct amf3_plugin_parser g_plugin_parsers[] = {
#ifdef HAVE_FLEX_COMMON_OBJECTS
    {
	"DSK",
	flex_parse_acknowledgemessageext,
	flex_free_acknowledgemessageext,
	flex_dump_acknowledgemessageext,
	flex_serialize_acknowledgemessageext
    },
    {
	"flex.messaging.messages.AcknowledgeMessageExt",
	flex_parse_acknowledgemessageext,
	flex_free_acknowledgemessageext,
	flex_dump_acknowledgemessageext,
	flex_serialize_acknowledgemessageext
    },
    {
	"DSA",
	flex_parse_asyncmessageext,
	flex_free_asyncmessageext,
	flex_dump_asyncmessageext,
	flex_serialize_asyncmessageext
    },
    {
	"flex.messaging.messages.AsyncMessageExt",
	flex_parse_asyncmessageext,
	flex_free_asyncmessageext,
	flex_dump_asyncmessageext,
	flex_serialize_asyncmessageext
    },
    {
	"DSC",
	flex_parse_commandmessageext,
	flex_free_commandmessageext,
	flex_dump_commandmessageext,
	flex_serialize_commandmessageext
    },
    {
	"flex.messaging.messages.CommandMessageExt",
	flex_parse_commandmessageext,
	flex_free_commandmessageext,
	flex_dump_commandmessageext,
	flex_serialize_commandmessageext
    },
    {
	"flex.messaging.io.ArrayCollection",
	flex_parse_arraycollection,
	flex_free_arraycollection,
	flex_dump_arraycollection,
	flex_serialize_arraycollection
    },
#endif
    {NULL, NULL, NULL, NULL, NULL}
};

#define ALLOC(type, nobjs) ((type *)malloc(sizeof(type) * nobjs))
#define CALLOC(nobjs, type) ((type *)calloc(nobjs, sizeof(type)))

#define LOG_ERROR (1)
#define LOG_DEBUG (7)
#ifndef DEBUG_LEVEL
#   define DEBUG_LEVEL LOG_ERROR
#endif

static void LOG(int level, const char *fmt, ...) {
    if (level <= DEBUG_LEVEL) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
    }
}

static struct amf3_value *amf3__new_value(char type) {
    struct amf3_value *v = ALLOC(struct amf3_value, 1);
    if (v) {
	memset(v, 0, sizeof(*v));
	v->retain_count = 1;
	v->type = type;
    }
    return v;
}

static void *amf3__kv_release_cb(
	List list, int idx, void *INLIST, void *unused) {
    struct amf3_kv *inlist = (struct amf3_kv *)INLIST;
    amf3_release(inlist->key);
    amf3_release(inlist->value);
    free(inlist);
    return NULL;
}

static void *amf3__release_cb(
	List list, int idx, void *INLIST, void *unused) {
    amf3_release((AMF3Value)INLIST);
    return NULL;
}

static const struct amf3_plugin_parser *
amf3__find_plugin_parser(AMF3Value classname) {
    int i;
    for (i = 0; g_plugin_parsers[i].classname; i++)
	if (strcmp(g_plugin_parsers[i].classname,
		    amf3_string_cstr(classname)) == 0)
	    return &g_plugin_parsers[i];
    return NULL;
}

static void amf3__free_value(struct amf3_value *v) {
    switch (v->type) {
	case AMF3_UNDEFINED:
	case AMF3_NULL:
	case AMF3_FALSE:
	case AMF3_TRUE:
	case AMF3_INTEGER:
	case AMF3_DOUBLE:
	case AMF3_DATE:
	    break;

	case AMF3_STRING:
	case AMF3_XMLDOC:
	case AMF3_XML:
	case AMF3_BYTEARRAY:
	    free(v->v.binary.data);
	    break;

	case AMF3_ARRAY:
	    list_foreach(v->v.array.assoc_list, amf3__kv_release_cb, NULL);
	    list_foreach(v->v.array.dense_list, amf3__release_cb, NULL);
	    list_free(v->v.array.assoc_list);
	    list_free(v->v.array.dense_list);
	    break;

	case AMF3_OBJECT:
	    if (v->v.object.traits->v.traits.externalizable) {
		const struct amf3_plugin_parser *pp = amf3__find_plugin_parser(
			v->v.object.traits->v.traits.type);
		if (pp)
		    pp->freefunc(v->v.object.m.external_ctx);
		else {
		    LOG(LOG_ERROR, "%s: cannot free external object of type '%s'\n",
			    amf3_string_cstr(v->v.object.traits->v.traits.type));
		    return;
		}
	    } else {
		int i;
		for (i = 0; i < v->v.object.traits->v.traits.nmemb; i++)
		    if (v->v.object.m.i.member_values[i])
			amf3_release(v->v.object.m.i.member_values[i]);
		free(v->v.object.m.i.member_values);

		list_foreach(v->v.object.m.i.dynmemb_list,
			amf3__kv_release_cb, NULL);
		list_free(v->v.object.m.i.dynmemb_list);
	    }
	    amf3_release(v->v.object.traits);
	    break;

	case AMF3_TRAITS:
	    amf3_release(v->v.traits.type);
	    if (v->v.traits.nmemb > 0) {
		amf3_release(v);
		int i;
		for (i = 0; i < v->v.traits.nmemb; i++)
		    amf3_release(v->v.traits.members[i]);
		free(v->v.traits.members);
	    }
	    break;

	default:
	    LOG(LOG_ERROR, "%s: unknown type %02X\n", __func__, v->type);
	    return;
    }
    free(v);
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

const char *amf3_string_cstr(AMF3Value v) {
    assert(v && v->type == AMF3_STRING);
    return v->v.binary.data;
}

int amf3_binary_len(AMF3Value v) {
    assert(v && (v->type == AMF3_BYTEARRAY || v->type == AMF3_XML
		|| v->type == AMF3_XMLDOC || v->type == AMF3_STRING));
    return v->v.binary.length;
}

const char *amf3_binary_data(AMF3Value v) {
    assert(v && (v->type == AMF3_BYTEARRAY || v->type == AMF3_XML
		|| v->type == AMF3_XMLDOC || v->type == AMF3_STRING));
    return v->v.binary.data;
}

static struct amf3_value *amf3__new_traits(AMF3Value type,
	char externalizable, char dynamic, int nmemb) {
    LOG(LOG_DEBUG, "[TRAITS][%s%s](%d)%s\n",
	    externalizable ? "E" : " ",
	    dynamic ? "D" : " ",
	    nmemb, amf3_string_cstr(type));
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

static void *amf3__kv_replace_cb(List list, int idx, void *INL, void *REP) {
    struct amf3_kv *inlist = (struct amf3_kv *)INL;
    struct amf3_kv *repas = (struct amf3_kv *)REP;
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

static void *amf3__kv_get_cb(List list, int idx, void *INLIST, void *KEY) {
    struct amf3_kv *inlist = (struct amf3_kv *)INLIST;
    AMF3Value key = (AMF3Value)KEY;
    if (amf3_string_cmp(inlist->key, key) == 0)
	return inlist->value;
    return NULL;
}

void amf3_array_assoc_set(AMF3Value a, AMF3Value key, AMF3Value value) {
    assert(a && a->type == AMF3_ARRAY);
    assert(key && key->type == AMF3_STRING);
    struct amf3_kv kvfind = {key, value};
    if (!list_foreach(a->v.array.assoc_list, amf3__kv_replace_cb, &kvfind)) {
	struct amf3_kv *kv = ALLOC(struct amf3_kv, 1);
	if (!kv)
	    return;
	kv->key = amf3_retain(key);
	kv->value = amf3_retain(value);
	list_push(a->v.array.assoc_list, kv);
    }
}

AMF3Value amf3_array_assoc_get(AMF3Value a, AMF3Value key) {
    assert(a && a->type == AMF3_ARRAY);
    assert(key && key->type == AMF3_STRING);
    return list_foreach(a->v.array.assoc_list, amf3__kv_get_cb, key);
}

static AMF3Value amf3__new_object_direct(
	AMF3Value traits, AMF3Value *members, List dynmemb_list) {
    assert(traits->type == AMF3_TRAITS);
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
	struct amf3_kv kvfind = {key, value};
	if (!list_foreach(o->v.object.m.i.dynmemb_list,
		    amf3__kv_replace_cb, &kvfind)) {
	    struct amf3_kv *kv = ALLOC(struct amf3_kv, 1);
	    if (!kv)
		return;
	    kv->key = amf3_retain(key);
	    kv->value = amf3_retain(value);
	    list_push(o->v.object.m.i.dynmemb_list, kv);
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
    if (r->refs) {
	int i;
	for (i = 0; i < r->nref; i++)
	    amf3_release(r->refs[i]);
	free(r->refs);
    }
    free(r);
}

AMF3Value amf3_ref_table_push(struct amf3_ref_table *r, AMF3Value v) {
    assert(r);
    if (r->nref == r->nalloc) {
	r->refs = realloc(r->refs,
		(r->nalloc <<= 1) * sizeof(AMF3Value));
	assert(r->refs);
    }
    return (r->refs[r->nref++] = amf3_retain(v));
}

AMF3Value amf3_ref_table_get(struct amf3_ref_table *r, int idx) {
    return idx < r->nref ? r->refs[idx] : NULL;
}

static void *amf3__find_double(struct amf3_ref_table *r,
	int idx, AMF3Value v, void *ctx) {
    struct amf3_valfind *vf = ctx;
    if (v->type != AMF3_DOUBLE)
	return NULL;
    if (v->v.real == vf->value->v.real) {
	vf->idx = idx;
	return v;
    }
    return NULL;
}

static void *amf3__find_string(struct amf3_ref_table *r,
	int idx, AMF3Value v, void *ctx) {
    struct amf3_valfind *vf = ctx;
    if (v->type != AMF3_STRING)
	return NULL;
    if (amf3_string_cmp(v, vf->value) == 0) {
	vf->idx = idx;
	return v;
    }
    return NULL;
}

static void *amf3__find_binary(struct amf3_ref_table *r,
	int idx, AMF3Value v, void *ctx) {
    struct amf3_valfind *vf = ctx;
    if (v->type != vf->value->type ||
	    v->v.binary.length != vf->value->v.binary.length)
	return NULL;
    if (memcmp(v->v.binary.data, vf->value->v.binary.data, v->v.binary.length) == 0) {
	vf->idx = idx;
	return v;
    }
    return NULL;
}

static void *amf3__find_date(struct amf3_ref_table *r,
	int idx, AMF3Value v, void *ctx) {
    struct amf3_valfind *vf = ctx;
    if (v->type != AMF3_DATE)
	return NULL;
    if (v->v.date.value == vf->value->v.date.value) {
	vf->idx = idx;
	return v;
    }
    return NULL;
}

static void *amf3__find_by_pointer(struct amf3_ref_table *r,
	int idx, AMF3Value v, void *ctx) {
    struct amf3_valfind *vf = ctx;
    if (v == vf->value) {
	vf->idx = idx;
	return v;
    }
    return NULL;
}

static void *amf3__find_traits(struct amf3_ref_table *r,
	int idx, AMF3Value v, void *ctx) {
    struct amf3_valfind *vf = ctx;
    if (v->type != AMF3_TRAITS)
	return NULL;
    if (amf3_string_cmp(v->v.traits.type, vf->value->v.traits.type) == 0) {
	vf->idx = idx;
	return v;
    }
    return NULL;
}

void *amf3_ref_table_foreach(struct amf3_ref_table *r,
	void *(* callback)(struct amf3_ref_table *r, int idx, AMF3Value elem, void *ctx),
	void *ctx) {
    void *ret;
    int i;
    for (i = 0; i < r->nref; i++)
	if ((ret = callback(r, i, r->refs[i], ctx)) != NULL)
	    return ret;
    return NULL;
}

int amf3_ref_table_find(struct amf3_ref_table *r, AMF3Value v) {
    struct amf3_valfind vf = {v, -1};
    switch (v->type) {
	case AMF3_UNDEFINED:
	case AMF3_NULL:
	case AMF3_FALSE:
	case AMF3_TRUE:
	case AMF3_INTEGER:
	    // never caches
	    break;

	case AMF3_DOUBLE:
	    amf3_ref_table_foreach(r, amf3__find_double, &vf);
	    break;

	case AMF3_STRING:
	    amf3_ref_table_foreach(r, amf3__find_string, &vf);
	    break;

	case AMF3_XMLDOC:
	case AMF3_XML:
	case AMF3_BYTEARRAY:
	    amf3_ref_table_foreach(r, amf3__find_binary, &vf);
	    break;

	case AMF3_OBJECT:
	case AMF3_ARRAY:
	    amf3_ref_table_foreach(r, amf3__find_by_pointer, &vf);
	    break;

	case AMF3_DATE:
	    amf3_ref_table_foreach(r, amf3__find_date, &vf);
	    break;

	case AMF3_TRAITS:
	    amf3_ref_table_foreach(r, amf3__find_traits, &vf);
	    break;

	default:
	    LOG(LOG_ERROR, "%s: unknown type %02X\n", __func__, v->type);
	    break;
    }
    return vf.idx;
}

int amf3_parse_u29(struct amf3_parse_context *c) {
    int j;
    int v = 0;
    unsigned char i;

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
    LOG(LOG_DEBUG, "[ARRAY] length = %d\n", len);

    AMF3Value arr = amf3_new_array();
    if (!arr)
	return NULL;
    amf3_ref_table_push(c->object_refs, arr);

    LOG(LOG_DEBUG, "[ARRAY] parsing assoc part\n");
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
    if (key)
	amf3_release(key);

    int i;
    for (i = 0; i < len; i++) {
	LOG(LOG_DEBUG, "[ARRAY] parsing dense part #%d of %d\n", i, len);
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

	LOG(LOG_DEBUG, "[*TRAITS]{%d} %s\n", ref >> 2,
		amf3_string_cstr(classname));
    } else {
	classname = amf3_parse_string(c);
	if (!classname)
	    return NULL;

	if (!external) {
	    dynamic = (ref >> 3) & 1;
	    nmemb = ref >> 4;
	}

	traits = amf3__new_traits(classname, external, dynamic, nmemb);
	if (!traits) {
	    amf3_release(classname);
	    return NULL;
	}

	int i;
	for (i = 0; i < nmemb; i++) {
	    AMF3Value key = amf3_parse_string(c);
	    if (!key) {
		amf3_release(traits);
		amf3_release(classname);
		return NULL;
	    }
	    amf3__traits_member_set(traits, i, key);
	    amf3_release(key);
	}

	amf3_ref_table_push(c->traits_refs, traits);

	LOG(LOG_DEBUG, "[TRAITS]{%d}[%s%s] %s\n",
		c->traits_refs->nref - 1,
		external ? "E" : " ",
		dynamic ? "D" : " ",
		amf3_string_cstr(classname));
    }

    AMF3Value obj;
    if (external) {
	obj = amf3__new_object_external_direct(traits, NULL);
	if (!obj) {
	    amf3_release(traits);
	    amf3_release(classname);
	    return NULL;
	}
	amf3_ref_table_push(c->object_refs, obj);

	const struct amf3_plugin_parser *pp;
	if ((pp = amf3__find_plugin_parser(classname)) != NULL) {
	    if (pp->handler(c, classname,
			&obj->v.object.m.external_ctx) != 0) {
		LOG(LOG_ERROR, "%s: external parser of type '%s' returns error\n",
			__func__, amf3_string_cstr(classname));
		amf3_release(traits);
		amf3_release(classname);
		amf3_release(obj);
		return NULL;
	    }
	} else {
	    LOG(LOG_ERROR, "%s: cannot parse type '%s'\n",
		    __func__, amf3_string_cstr(classname));
	    amf3_release(traits);
	    amf3_release(classname);
	    amf3_release(obj);
	    return NULL;
	}
    } else {
	obj = amf3__new_object_direct(traits, NULL, list_new());
	if (!obj) {
	    amf3_release(traits);
	    amf3_release(classname);
	    return NULL;
	}
	amf3_ref_table_push(c->object_refs, obj);

	int i, nmemb = traits->v.traits.nmemb;
	for (i = 0; i < nmemb; i++) {
	    LOG(LOG_DEBUG, "%s::%s\n",
		    amf3_string_cstr(traits->v.traits.type),
		    amf3_string_cstr(traits->v.traits.members[i]));

	    AMF3Value value = amf3_parse_value(c);
	    if (!value) {
		amf3_release(traits);
		amf3_release(classname);
		amf3_release(obj);
		return NULL;
	    }
	    obj->v.object.m.i.member_values[i] = value;

#if DEBUG_LEVEL >= LOG_DEBUG
	    if (value->type != AMF3_OBJECT && value->type != AMF3_ARRAY) {
		LOG(LOG_DEBUG, "=> ");
		amf3_dump_value(value, 0);
	    }
#endif
	}

	if (dynamic) {
	    AMF3Value key;
	    while ((key = amf3_parse_string(c)) != NULL &&
		    amf3_string_len(key) > 0) {
		AMF3Value value = amf3_parse_value(c);
		if (!value) {
		    amf3_release(traits);
		    amf3_release(classname);
		    amf3_release(obj);
		    amf3_release(key);
		    return NULL;
		}
		amf3_object_prop_set(obj, key, value);
		amf3_release(key);
		amf3_release(value);
	    }
	    if (key)
		amf3_release(key);
	}
    }
    amf3_release(traits);
    amf3_release(classname);
    return obj;
}

static double amf3__read_double(struct amf3_parse_context *c) {
    union {
	uint64_t i;
	double d;
    } conv;
    if (c->left < sizeof(uint64_t))
	return 0.0;
    conv.i = NTOH64(*((uint64_t *)c->p));
    c->p += sizeof(uint64_t);
    c->left -= sizeof(uint64_t);
    return conv.d;
}

AMF3Value amf3_parse_date(struct amf3_parse_context *c) {
    int ref = amf3_parse_u29(c);
    if (ref < 0)
	return NULL;
    if (!(ref & 0x1))
	return amf3_retain(amf3_ref_table_get(c->object_refs, ref >> 1));
    return amf3_ref_table_push(c->object_refs,
	    amf3_new_date(amf3__read_double(c)));
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
		return amf3_new_integer((integer << 3) >> 3);
	    }

	case AMF3_DOUBLE:
	    return amf3_new_double(amf3__read_double(c));

	case AMF3_STRING:
	    return amf3_parse_string(c);

	case AMF3_XMLDOC:
	case AMF3_XML:
	case AMF3_BYTEARRAY:
	    return amf3__parse_binary_object(c, mark);

	case AMF3_DATE:
	    return amf3_parse_date(c);

	case AMF3_ARRAY:
	    return amf3_parse_array(c);

	case AMF3_OBJECT:
	    return amf3_parse_object(c);

	default:
	    LOG(LOG_ERROR, "%s: unknown type %02X\n", __func__, mark);
	    return NULL;
    }
}

AMF3ParseContext amf3_parse_context_new(const char *data, int length) {
    AMF3ParseContext c = CALLOC(1, struct amf3_parse_context);
    if (c) {
	c->data = c->p = data;
	c->length = c->left = length;
	c->object_refs = amf3_ref_table_new();
	c->string_refs = amf3_ref_table_new();
	c->traits_refs = amf3_ref_table_new();
    }
    return c;
}

void amf3_parse_context_free(AMF3ParseContext c) {
    assert(c);
    if (c->object_refs)
	amf3_ref_table_free(c->object_refs);
    if (c->string_refs)
	amf3_ref_table_free(c->string_refs);
    if (c->traits_refs)
	amf3_ref_table_free(c->traits_refs);
    free(c);
}

void amf3__print_indent(int indent) {
    int i;
    for (i = 0; i < indent; i++)
	fprintf(stderr, "  ");
}

void amf3_dump_value(AMF3Value v, int depth);

static void *amf3__dump_kv(
	List list, int idx, void *INLIST, void *DEPTH) {
    struct amf3_kv *kv = (struct amf3_kv *)INLIST;
    int depth = *((int *)DEPTH);
    amf3__print_indent(depth);
    fprintf(stderr, "\"%s\": ", amf3_string_cstr(kv->key));
    amf3_dump_value(kv->value, depth + 1);
    return NULL;
}

static void *amf3__dump_v(
	List list, int idx, void *value, void *DEPTH) {
    int depth = *((int *)DEPTH);
    amf3__print_indent(depth);
    fprintf(stderr, "[%d] ", idx);
    amf3_dump_value((AMF3Value)value, depth + 1);
    return NULL;
}

void amf3_dump_value(AMF3Value v, int depth) {
    FILE *fp = stderr;
    static const char *typenames[] = {
	"Undefined", "Null", "False", "True", "Integer",
	"Double", "String", "XmlDoc", "Date", "Array",
	"Object", "Xml", "ByteArray"
    };
    if (v->type <= AMF3_BYTEARRAY)
	fprintf(fp, "(%s)", typenames[(int)v->type]);
    switch (v->type) {
	case AMF3_UNDEFINED:
	case AMF3_NULL:
	case AMF3_FALSE:
	case AMF3_TRUE:
	    fprintf(fp, "\n");
	    break;

	case AMF3_INTEGER:
	    fprintf(fp, " %d\n", v->v.integer);
	    break;

	case AMF3_DOUBLE:
	    fprintf(fp, " %f\n", v->v.real);
	    break;

	case AMF3_STRING:
	case AMF3_XMLDOC:
	case AMF3_XML:
	case AMF3_BYTEARRAY:
	    fprintf(fp, " %d \"", v->v.binary.length);
	    fwrite(v->v.binary.data, v->v.binary.length, 1, fp);
	    fprintf(fp, "\"\n");
	    break;

	case AMF3_DATE:
	    fprintf(fp, " %f\n", v->v.real);
	    break;

	case AMF3_ARRAY:
	    {
		int nxdepth = depth + 1;
		fprintf(fp, "\n");
		amf3__print_indent(depth);
		fprintf(fp, "(assoc)\n");
		list_foreach(v->v.array.assoc_list, amf3__dump_kv, &nxdepth);
		list_foreach(v->v.array.dense_list, amf3__dump_v, &depth);
	    }
	    break;

	case AMF3_OBJECT:
	    {
		AMF3Value traits = v->v.object.traits;
		fprintf(fp, "\n");
		amf3__print_indent(depth);
		amf3_dump_value(traits, depth);

		if (traits->v.traits.externalizable) {
		    const struct amf3_plugin_parser *pp =
			amf3__find_plugin_parser(traits->v.traits.type);
		    if (pp && pp->dumpfunc)
			pp->dumpfunc(v->v.object.m.external_ctx, depth);
		    else {
			amf3__print_indent(depth);
			fprintf(fp, "(No dump handler defined)\n");
		    }
		} else {
		    int i;
		    for (i = 0; i < traits->v.traits.nmemb; i++) {
			amf3__print_indent(depth);
			fprintf(fp, "\"%s\": ",
				amf3_string_cstr(traits->v.traits.members[i]));
			amf3_dump_value(
				v->v.object.m.i.member_values[i], depth + 1);
		    }

		    if (traits->v.traits.dynamic) {
			amf3__print_indent(depth);
			fprintf(fp, "(dynmember)\n");
			int nxdepth = depth + 1;
			list_foreach(v->v.object.m.i.dynmemb_list,
				amf3__dump_kv, &nxdepth);
		    }
		}
	    }
	    break;

	case AMF3_TRAITS:
	    fprintf(fp, "<traits> [%s%s] %s\n",
		    v->v.traits.externalizable ? "E" : " ",
		    v->v.traits.dynamic ? "D" : " ",
		    amf3_string_cstr(v->v.traits.type));
	    break;

	default:
	    LOG(LOG_ERROR, "(Unknown %02X)\n", v->type);
    }
}

AMF3SerializeContext amf3_serialize_context_new() {
    AMF3SerializeContext c = CALLOC(1, struct amf3_serialize_context);
    if (c) {
	c->object_refs = amf3_ref_table_new();
	c->string_refs = amf3_ref_table_new();
	c->traits_refs = amf3_ref_table_new();
	if (!c->object_refs || !c->string_refs || !c->traits_refs) {
	    amf3_serialize_context_free(c);
	    return NULL;
	}

	c->allocated = 1024;
	c->buffer = ALLOC(char, c->allocated);
	if (!c->buffer) {
	    amf3_serialize_context_free(c);
	    return NULL;
	}
	c->length = 0;
    }
    return c;
}

void amf3_serialize_context_free(AMF3SerializeContext c) {
    assert(c);
    if (c->buffer)
	free(c->buffer);
    if (c->object_refs)
	amf3_ref_table_free(c->object_refs);
    if (c->string_refs)
	amf3_ref_table_free(c->string_refs);
    if (c->traits_refs)
	amf3_ref_table_free(c->traits_refs);
    free(c);
}

int amf3_serialize_write_func(AMF3SerializeContext c, const void *data, int len) {
    if (c->length + len > c->allocated) {
	while (c->allocated >= 0 && c->length + len > c->allocated)
	    c->allocated <<= 1;
	char *p = realloc(c->buffer, c->allocated);
	if (!p)
	    return -1;
	c->buffer = p;
    }
    memcpy(c->buffer + c->length, data, len);
    c->length += len;
    return len;
}

const char *amf3_serialize_context_get_buffer(AMF3SerializeContext c, int *len) {
    if (len)
	*len = c->length;
    return c->buffer;
}

int amf3_serialize_u29(AMF3SerializeContext c, int integer) {
    unsigned char b[4];
    int offset = 0, len = 4;
    integer &= 0x3FFFFFFF;
    if (integer >> 21) {
	b[0] = 0x80 | ((integer >> 22) & 0x7F);
	b[1] = 0x80 | ((integer >> 15) & 0x7F);
	b[2] = 0x80 | ((integer >>  8) & 0x7F);
	b[3] = integer & 0xFF;
    } else {
	b[0] = (integer >> 14) & 0x7F;
	b[1] = (integer >>  7) & 0x7F;
	b[2] = integer & 0x7F;
	len--;
	if (!b[0]) {
	    len--;
	    offset++;
	    if (!b[1]) {
		len--;
		offset++;
	    }
	}
	b[0] |= 0x80;
	b[1] |= 0x80;
    }
    return amf3_serialize_write_func(c, b + offset, len);
}

static int amf3__serialize_object_ref(AMF3SerializeContext c, int refidx) {
    return amf3_serialize_u29(c, refidx << 1);
}

static int amf3__serialize_double(AMF3SerializeContext c, double real) {
    union {
	uint64_t i;
	double d;
    } conv;
    conv.d = real;
    conv.i = HTON64(conv.i);
    return amf3_serialize_write_func(c, &conv.i, sizeof(conv.i));
}

static int amf3__serialize_string(AMF3SerializeContext c, AMF3Value v) {
    // empty string, never sent by ref
    if (amf3_string_len(v) == 0)
	return amf3_serialize_u29(c, 0x01);

    // some functions need to serialize a string without marker,
    // so check string ref here instead of in `amf3_serialize_value'.
    int refidx = amf3_ref_table_find(c->string_refs, v);
    if (refidx < 0)
	amf3_ref_table_push(c->string_refs, v);
    else
	return amf3__serialize_object_ref(c, refidx);
    LOG(LOG_DEBUG, "string_ref[%d] = \"%s\"\n",
	    c->string_refs->nref - 1, amf3_string_cstr(v));

    int wrote = amf3_serialize_u29(c, (amf3_string_len(v) << 1) | 1);
    wrote += amf3_serialize_write_func(c, amf3_string_cstr(v),
	    amf3_string_len(v));
    return wrote;
}

static void *amf3__serialize_kv_cb(
	List list, int idx, void *INLIST, void *c) {
    struct amf3_kv *inlist = (struct amf3_kv *)INLIST;
    amf3__serialize_string((AMF3SerializeContext)c, inlist->key);
    amf3_serialize_value((AMF3SerializeContext)c, inlist->value);
    return NULL;
}

static void *amf3__serialize_v_cb(
	List list, int idx, void *elem, void *c) {
    amf3_serialize_value((AMF3SerializeContext)c, (AMF3Value)elem);
    return NULL;
}

static int amf3__serialize_array(AMF3SerializeContext c, AMF3Value v) {
    assert(v->type == AMF3_ARRAY);
    int currlen = c->length;
    amf3_serialize_u29(c, (list_count(v->v.array.dense_list) << 1) | 1);
    list_foreach(v->v.array.assoc_list, amf3__serialize_kv_cb, c);
    amf3_serialize_u29(c, 0x01);
    list_foreach(v->v.array.dense_list, amf3__serialize_v_cb, c);
    return c->length - currlen;
}

static int amf3__serialize_object(AMF3SerializeContext c, AMF3Value v) {
    // object ref already considered in amf3_serialize_value
    // only take care of traits ref here
    int wrote = 0;

    AMF3Value traits = v->v.object.traits;
    struct amf3_traits *t = &traits->v.traits;
    int refidx = amf3_ref_table_find(c->traits_refs, traits);
    if (refidx >= 0) {
	wrote += amf3_serialize_u29(c, (refidx << 2) | 1);
    } else {
	int oref = 0;
	if (t->externalizable)
	    oref = 0x7;
	else
	    oref |= (t->nmemb << 4) | ((t->dynamic ? 1 : 0) << 3) | 3;
	wrote += amf3_serialize_u29(c, oref);
	wrote += amf3__serialize_string(c, t->type);

	int i;
	for (i = 0; i < t->nmemb; i++)
	    wrote += amf3__serialize_string(c, t->members[i]);

	amf3_ref_table_push(c->traits_refs, traits);
	LOG(LOG_DEBUG, "traits_ref[%d] = type:%s\n",
		c->traits_refs->nref - 1, amf3_string_cstr(t->type));
    }

    if (t->externalizable) {
	const struct amf3_plugin_parser *pp =
	    amf3__find_plugin_parser(t->type);
	if (pp)
	    wrote += pp->serializefunc(c, t->type, v->v.object.m.external_ctx);
	else
	    LOG(LOG_ERROR, "%s: external serializer of type '%s' returns error\n",
		    __func__, amf3_string_cstr(t->type));
    } else {
	int i;
	for (i = 0; i < t->nmemb; i++)
	    wrote += amf3_serialize_value(c, v->v.object.m.i.member_values[i]);

	if (t->dynamic) {
	    list_foreach(v->v.object.m.i.dynmemb_list, amf3__serialize_kv_cb, c);
	    amf3_serialize_u29(c, 0x01);
	}
    }
    return wrote;
}

int amf3_serialize_value(AMF3SerializeContext c, AMF3Value v) {
    char mark = v->type;

    int wrote = amf3_serialize_write_func(c, &mark, sizeof(mark));

    // reference-capable
    int refidx = -1;
    switch (mark) {
	case AMF3_XMLDOC:
	case AMF3_XML:
	case AMF3_BYTEARRAY:
	case AMF3_DATE:
	case AMF3_ARRAY:
	case AMF3_OBJECT:
	    refidx = amf3_ref_table_find(c->object_refs, v);
	    if (refidx < 0)
		amf3_ref_table_push(c->object_refs, v);
	    else
		return wrote + amf3__serialize_object_ref(c, refidx);
	    break;

	case AMF3_STRING:
	    // string ref are handled in `amf3__serialize_string'
	    break;

	default:
	    break;
    }

    switch (mark) {
	case AMF3_UNDEFINED:
	case AMF3_NULL:
	case AMF3_FALSE:
	case AMF3_TRUE:
	    // marker only
	    break;

	case AMF3_INTEGER:
	    wrote += amf3_serialize_u29(c, v->v.integer);
	    break;

	case AMF3_DOUBLE:
	    wrote += amf3__serialize_double(c, v->v.real);
	    break;

	case AMF3_STRING:
	    wrote += amf3__serialize_string(c, v);
	    break;

	case AMF3_XMLDOC:
	case AMF3_XML:
	case AMF3_BYTEARRAY:
	    wrote += amf3_serialize_u29(c, (v->v.binary.length << 1) | 1);
	    wrote += amf3_serialize_write_func(c, v->v.binary.data,
		    v->v.binary.length);
	    break;

	case AMF3_DATE:
	    wrote += amf3_serialize_u29(c, 1);
	    wrote += amf3__serialize_double(c, v->v.date.value);
	    break;

	case AMF3_ARRAY:
	    wrote += amf3__serialize_array(c, v);
	    break;

	case AMF3_OBJECT:
	    wrote += amf3__serialize_object(c, v);
	    break;

	default:
	    LOG(LOG_ERROR, "%s: unknown type %02X\n", __func__, mark);
	    return -1;
    }

    return wrote;
}
