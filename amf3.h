#ifndef _AMF3_H
#   define _AMF3_H

#include "list.h"

#define AMF3_UNDEFINED	(0x00)
#define AMF3_NULL	(0x01)
#define AMF3_FALSE	(0x02)
#define AMF3_TRUE	(0x03)
#define AMF3_INTEGER	(0x04)
#define AMF3_DOUBLE	(0x05)
#define AMF3_STRING	(0x06)
#define AMF3_XMLDOC	(0x07)
#define AMF3_DATE	(0x08)
#define AMF3_ARRAY	(0x09)
#define AMF3_OBJECT	(0x0A)
#define AMF3_XML	(0x0B)
#define AMF3_BYTEARRAY	(0x0C)

/* types for internal use */
#define AMF3_TRAITS	(0x70)


struct amf3_value;
struct amf3_vlist;
struct amf3_kvlist;

struct amf3_date {
    double value;
};

struct amf3_array {
    List assoc_list;
    List dense_list;
};

struct amf3_traits {
    char externalizable;
    char dynamic;
    int nmemb;
    struct amf3_value **members;
};

struct amf3_object {
    struct amf3_value *type;
    struct amf3_value *traits;
    union {
	struct {
	    struct amf3_value **member_values;
	    List dynmemb_list;
	} i;
	void *external_ctx;
    } m;
};

struct amf3_binary {
    int length;
    char *data;
};

struct amf3_value {
    int retain_count;
    char type;
    union {
	int			integer;
	double			real;
	struct amf3_date	date;
	struct amf3_array	array;
	struct amf3_object	object;
	struct amf3_binary	binary;

	/* for internal use only */
	struct amf3_traits	traits;
    } v;
};

struct amf3_kv {
    struct amf3_value *key;
    struct amf3_value *value;
};


typedef struct amf3_value *AMF3Value;

#endif
