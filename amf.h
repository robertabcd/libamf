#ifndef _AMF_H
#   define _AMF_H

#include <stdint.h>

#define AMF_NUMBER  (0)
#define AMF_BOOLEAN (1)
#define AMF_STRING  (2)
#define AMF_OBJECT  (3)
#define AMF_NULL    (5)
#define AMF_ARRAY   (8)
#define AMF_OAEND   (9)

struct amf_kvlist;
struct amf_vlist;

struct amf_string {
    uint16_t length;
    char *value;
};

struct amf_array {
    uint32_t count;
    struct amf_vlist *first;
};

struct amf_value {
    int retain_count;
    char type;
    union {
	double number;
	char boolean;
	struct amf_string string;
	struct amf_kvlist *object;
	struct amf_array array;
    } v;
};

struct amf_kv {
    struct amf_value *key;
    struct amf_value *value;
};

struct amf_kvlist {
    struct amf_kv entry;
    struct amf_kvlist *next;
};

struct amf_vlist {
    struct amf_value *value;
    struct amf_vlist *next;
};


typedef struct amf_value *AMFValue;

typedef double AMFNumber;
typedef char AMFBoolean;

typedef struct {
    struct amf_value *array;
    struct amf_vlist *current;
} AMFArrayIter;

typedef struct {
    struct amf_value *object;
    struct amf_kvlist *current;
} AMFObjectIter;

typedef struct amf_kv AMFKeyValuePair;


AMFValue amf_retain(AMFValue v);
void amf_release(AMFValue v);

AMFValue amf_new_number(double number);
AMFValue amf_new_boolean(char boolean);
AMFValue amf_new_string(const char *string, int length);
AMFValue amf_new_object();
AMFValue amf_new_null();
AMFValue amf_new_array();

const char *amf_cstr(AMFValue v);
int amf_strlen(AMFValue v);
int amf_strcmp(AMFValue a, AMFValue b);

AMFValue amf_object_get(AMFValue v, AMFValue key);
AMFValue amf_object_set(AMFValue v, AMFValue key, AMFValue value);

void amf_objectiter_init(AMFObjectIter *it, AMFValue v);
void amf_objectiter_cleanup(AMFObjectIter *it);
void amf_objectiter_next(AMFObjectIter *it);
AMFKeyValuePair *amf_objectiter_current(AMFObjectIter *it);

AMFValue amf_array_push(AMFValue v, AMFValue e);

void amf_arrayiter_init(AMFArrayIter *it, AMFValue v);
void amf_arrayiter_cleanup(AMFArrayIter *it);
void amf_arrayiter_next(AMFArrayIter *it);
AMFValue amf_arrayiter_current(AMFArrayIter *it);

void amf_dump(AMFValue v);

#endif
