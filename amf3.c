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
	flex_dump_acknowledgemessageext
    },
    {
	"flex.messaging.messages.AcknowledgeMessageExt",
	flex_parse_acknowledgemessageext,
	flex_free_acknowledgemessageext,
	flex_dump_acknowledgemessageext
    },
    {
	"DSA",
	flex_parse_asyncmessageext,
	flex_free_asyncmessageext,
	flex_dump_asyncmessageext
    },
    {
	"flex.messaging.messages.AsyncMessageExt",
	flex_parse_asyncmessageext,
	flex_free_asyncmessageext,
	flex_dump_asyncmessageext
    },
    {
	"DSC",
	flex_parse_commandmessageext,
	flex_free_commandmessageext,
	flex_dump_commandmessageext
    },
    {
	"flex.messaging.messages.CommandMessageExt",
	flex_parse_commandmessageext,
	flex_free_commandmessageext,
	flex_dump_commandmessageext
    },
    {
	"flex.messaging.io.ArrayCollection",
	flex_parse_arraycollection,
	flex_free_arraycollection,
	flex_dump_arraycollection
    },
#endif
    {NULL, NULL, NULL, NULL}
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
