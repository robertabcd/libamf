#include <stdio.h>
#include <stdlib.h>
#include "flex.h"
#include "amf3.h"

#define CALLOC(nobjs, type) ((type *)calloc(nobjs, sizeof(type)))

struct flex_flags {
    char *fl;
    int nfl;
    int pos;
};

static int flex_parse_flags(AMF3ParseContext c, struct flex_flags *ff) {
    char b;
    int i = 0;
    while(i < c->left && ((b = c->p[i]) & 0x80))
	i++;
    ff->nfl = i + 1;
    ff->pos = 0;
    ff->fl = CALLOC(ff->nfl, char);
    if (!ff->fl)
	return -1;
    for (i = 0; i < ff->nfl; i++)
	ff->fl[i] = c->p[i] & 0x7F;
    c->p += ff->nfl;
    c->left -= ff->nfl;
    return 0;
}

static void flex_flags_free(struct flex_flags *ff) {
    if (ff->fl)
	free(ff->fl);
    ff->fl = NULL;
    ff->pos = 0;
    ff->nfl = 0;
}

static char flex_flags_toggle(struct flex_flags *ff, char mask) {
    if (ff->pos < ff->nfl && (ff->fl[ff->pos] & mask)) {
	ff->fl[ff->pos] &= ~mask;
	return 1;
    }
    return 0;
}

static void flex_flags_next(struct flex_flags *ff) {
    if (ff->pos < ff->nfl)
	ff->pos++;
}

static int flex_flags_countbits(struct flex_flags *ff) {
    int count = 0;
    int i;
    char b;
    for (i = 0; i < ff->nfl; i++)
	for (b = ff->fl[i]; b; b >>= 1)
	    if (b & 1)
		count++;
    return count;
}

int flex_parse_abstractmessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    Flex_AbstractMessage *am = CALLOC(1, Flex_AbstractMessage);
    if (!am)
	return -1;

    struct flex_flags ff;
    flex_parse_flags(c, &ff);

    if (flex_flags_toggle(&ff, BODY_FLAG))
	am->body = amf3_parse_value(c);
    if (flex_flags_toggle(&ff, CLIENT_ID_FLAG))
	am->client_id = amf3_parse_value(c);
    if (flex_flags_toggle(&ff, DESTINATION_FLAG))
	am->destination = amf3_parse_value(c);
    if (flex_flags_toggle(&ff, HEADERS_FLAG))
	am->headers = amf3_parse_value(c);
    if (flex_flags_toggle(&ff, MESSAGE_ID_FLAG))
	am->message_id = amf3_parse_value(c);
    if (flex_flags_toggle(&ff, TIMESTAMP_FLAG))
	am->timestamp = amf3_parse_value(c);
    if (flex_flags_toggle(&ff, TIME_TO_LIVE_FLAG))
	am->ttl = amf3_parse_value(c);
    flex_flags_next(&ff);

    if (flex_flags_toggle(&ff, CLIENT_ID_BYTES_FLAG))
	am->client_id_bytes = amf3_parse_value(c);
    if (flex_flags_toggle(&ff, MESSAGE_ID_BYTES_FLAG))
	am->message_id_bytes = amf3_parse_value(c);

    int ntails = flex_flags_countbits(&ff);
    flex_flags_free(&ff);
    for (; ntails > 0; ntails--)
	amf3_release(amf3_parse_value(c));

    *external_ctx = am;
    return 0;
}

void flex_free_abstractmessage(void *AM) {
    Flex_AbstractMessage *am = (Flex_AbstractMessage *)AM;
    if (am->body)
	amf3_release(am->body);
    if (am->client_id)
	amf3_release(am->client_id);
    if (am->destination)
	amf3_release(am->destination);
    if (am->headers)
	amf3_release(am->headers);
    if (am->message_id)
	amf3_release(am->message_id);
    if (am->timestamp)
	amf3_release(am->timestamp);
    if (am->ttl)
	amf3_release(am->ttl);
    if (am->client_id_bytes)
	amf3_release(am->client_id_bytes);
    if (am->message_id_bytes)
	amf3_release(am->message_id_bytes);
    free(am);
}

static void flex__dump_amf3_value(const char *key, AMF3Value value, int depth) {
    if (!value)
	return;
    amf3__print_indent(depth);
    fprintf(stderr, "\"%s\": ", key);
    amf3_dump_value(value, depth + 1);
}

void flex_dump_abstractmessage(void *AM, int depth) {
    Flex_AbstractMessage *am = (Flex_AbstractMessage *)AM;
    flex__dump_amf3_value("body", am->body, depth);
    flex__dump_amf3_value("clientId", am->client_id, depth);
    flex__dump_amf3_value("destination", am->destination, depth);
    flex__dump_amf3_value("headers", am->headers, depth);
    flex__dump_amf3_value("messageId", am->message_id, depth);
    flex__dump_amf3_value("timestamp", am->timestamp, depth);
    flex__dump_amf3_value("timeToLive", am->ttl, depth);
    flex__dump_amf3_value("clientIdBytes", am->client_id_bytes, depth);
    flex__dump_amf3_value("messageIdBytes", am->message_id_bytes, depth);
}

int flex_serialize_abstractmessage(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    Flex_AbstractMessage *am = (Flex_AbstractMessage *)external_ctx;
    int wrote = 0;

    unsigned char fl0 = 0;
    if (am->body)	fl0 |= BODY_FLAG;
    if (am->client_id)	fl0 |= CLIENT_ID_FLAG;
    if (am->destination)fl0 |= DESTINATION_FLAG;
    if (am->headers)	fl0 |= HEADERS_FLAG;
    if (am->message_id) fl0 |= MESSAGE_ID_FLAG;
    if (am->timestamp)	fl0 |= TIMESTAMP_FLAG;
    if (am->ttl)	fl0 |= TIME_TO_LIVE_FLAG;

    unsigned char fl1 = 0;
    if (am->client_id_bytes) fl1 |= CLIENT_ID_BYTES_FLAG;
    if (am->message_id_bytes)fl1 |= MESSAGE_ID_BYTES_FLAG;
    if (fl1) fl0 |= 0x80;

    wrote += amf3_serialize_write_func(c, &fl0, sizeof(fl0));
    if (fl1)
	wrote += amf3_serialize_write_func(c, &fl1, sizeof(fl1));

    if (am->body)	wrote += amf3_serialize_value(c, am->body);
    if (am->client_id)	wrote += amf3_serialize_value(c, am->client_id);
    if (am->destination)wrote += amf3_serialize_value(c, am->destination);
    if (am->headers)	wrote += amf3_serialize_value(c, am->headers);
    if (am->message_id) wrote += amf3_serialize_value(c, am->message_id);
    if (am->timestamp)	wrote += amf3_serialize_value(c, am->timestamp);
    if (am->ttl)	wrote += amf3_serialize_value(c, am->ttl);

    if (fl1) {
	if (am->client_id_bytes)
	    wrote += amf3_serialize_value(c, am->client_id_bytes);
	if (am->message_id_bytes)
	    wrote += amf3_serialize_value(c, am->message_id_bytes);
    }

    return wrote;
}

int flex_parse_asyncmessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    Flex_AsyncMessage *am = CALLOC(1, Flex_AsyncMessage);
    if (!am)
	return -1;

    if (flex_parse_abstractmessage(c, classname, (void **)&am->am) != 0) {
	free(am);
	return -1;
    }

    struct flex_flags ff;
    flex_parse_flags(c, &ff);

    if (flex_flags_toggle(&ff, CORRELATION_ID_FLAG))
	am->correlation_id = amf3_parse_value(c);
    if (flex_flags_toggle(&ff, CORRELATION_ID_BYTES_FLAG))
	am->correlation_id_bytes = amf3_parse_value(c);

    int ntails = flex_flags_countbits(&ff);
    flex_flags_free(&ff);
    for (; ntails > 0; ntails--)
	amf3_release(amf3_parse_value(c));

    *external_ctx = am;
    return 0;
}

void flex_free_asyncmessage(void *AM) {
    Flex_AsyncMessage *am = (Flex_AsyncMessage *)AM;
    flex_free_abstractmessage(am->am);
    if (am->correlation_id)
	amf3_release(am->correlation_id);
    if (am->correlation_id_bytes)
	amf3_release(am->correlation_id_bytes);
    free(am);
}

void flex_dump_asyncmessage(void *AM, int depth) {
    Flex_AsyncMessage *am = (Flex_AsyncMessage *)AM;
    flex_dump_abstractmessage(am->am, depth);
    flex__dump_amf3_value("correlationId", am->correlation_id, depth);
    flex__dump_amf3_value("correlationIdBytes", am->correlation_id_bytes, depth);
}

int flex_serialize_asyncmessage(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    Flex_AsyncMessage *am = (Flex_AsyncMessage *)external_ctx;
    int wrote = 0;

    wrote += flex_serialize_abstractmessage(c, classname, am->am);

    unsigned char fl = 0;
    if (am->correlation_id)
	fl |= CORRELATION_ID_FLAG;
    if (am->correlation_id_bytes)
	fl |= CORRELATION_ID_BYTES_FLAG;

    wrote += amf3_serialize_write_func(c, &fl, sizeof(fl));
    if (am->correlation_id)
	wrote += amf3_serialize_value(c, am->correlation_id);
    if (am->correlation_id_bytes)
	wrote += amf3_serialize_value(c, am->correlation_id_bytes);

    return wrote;
}

int flex_parse_asyncmessageext(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_asyncmessage(c, classname, external_ctx);
}

void flex_free_asyncmessageext(void *am) {
    flex_free_asyncmessage((Flex_AsyncMessage *)am);
}

void flex_dump_asyncmessageext(void *am, int depth) {
    flex_dump_asyncmessage(am, depth);
}

int flex_serialize_asyncmessageext(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    return flex_serialize_asyncmessage(c, classname, external_ctx);
}

int flex_parse_acknowledgemessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    Flex_AcknowledgeMessage *am = CALLOC(1, Flex_AcknowledgeMessage);
    if (!am)
	return -1;

    if (flex_parse_asyncmessage(c, classname, (void **)&am->am) != 0) {
	free(am);
	return -1;
    }

    struct flex_flags ff;
    flex_parse_flags(c, &ff);

    /* No flags defined */

    int ntails = flex_flags_countbits(&ff);
    flex_flags_free(&ff);
    for (; ntails > 0; ntails--)
	amf3_release(amf3_parse_value(c));

    *external_ctx = am;
    return 0;
}

void flex_free_acknowledgemessage(void *AM) {
    Flex_AcknowledgeMessage *am = (Flex_AcknowledgeMessage *)AM;
    flex_free_asyncmessage(am->am);
    free(am);
}

void flex_dump_acknowledgemessage(void *AM, int depth) {
    Flex_AcknowledgeMessage *am = (Flex_AcknowledgeMessage *)AM;
    flex_dump_asyncmessage(am->am, depth);
}

int flex_serialize_acknowledgemessage(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    Flex_AcknowledgeMessage *am = (Flex_AcknowledgeMessage *)external_ctx;
    int wrote = flex_serialize_asyncmessage(c, classname, am->am);
    unsigned char fl = 0;
    return wrote + amf3_serialize_write_func(c, &fl, sizeof(fl));
}

int flex_parse_acknowledgemessageext(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_acknowledgemessage(c, classname, external_ctx);
}

void flex_free_acknowledgemessageext(void *am) {
    flex_free_acknowledgemessage((Flex_AcknowledgeMessageExt *)am);
}

void flex_dump_acknowledgemessageext(void *am, int depth) {
    flex_dump_acknowledgemessage(am, depth);
}

int flex_serialize_acknowledgemessageext(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    return flex_serialize_acknowledgemessage(c, classname, external_ctx);
}

int flex_parse_errormessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_acknowledgemessage(c, classname, external_ctx);
}

void flex_free_errormessage(void *em) {
    flex_free_acknowledgemessage((Flex_ErrorMessage *)em);
}

void flex_dump_errormessage(void *em, int depth) {
    flex_dump_acknowledgemessage(em, depth);
}

int flex_serialize_errormessage(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    return flex_serialize_errormessage(c, classname, external_ctx);
}

int flex_parse_commandmessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    Flex_CommandMessage *am = CALLOC(1, Flex_CommandMessage);
    if (!am)
	return -1;

    if (flex_parse_asyncmessage(c, classname, (void **)&am->am) != 0) {
	free(am);
	return -1;
    }

    struct flex_flags ff;
    flex_parse_flags(c, &ff);

    if (flex_flags_toggle(&ff, OPERATION_FLAG))
	am->operation = amf3_parse_value(c);

    int ntails = flex_flags_countbits(&ff);
    flex_flags_free(&ff);
    for (; ntails > 0; ntails--)
	amf3_release(amf3_parse_value(c));

    *external_ctx = am;
    return 0;
}

void flex_free_commandmessage(void *CM) {
    Flex_CommandMessage *cm = (Flex_CommandMessage *)CM;
    flex_free_asyncmessage(cm->am);
    amf3_release(cm->operation);
    free(cm);
}

void flex_dump_commandmessage(void *CM, int depth) {
    Flex_CommandMessage *cm = (Flex_CommandMessage *)CM;
    flex_dump_asyncmessage(cm->am, depth);
    flex__dump_amf3_value("operation", cm->operation, depth);
}

int flex_serialize_commandmessage(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    Flex_CommandMessage *cm = (Flex_CommandMessage *)external_ctx;
    int wrote = flex_serialize_asyncmessage(c, classname, cm->am);

    unsigned char fl = 0;
    if (cm->operation)
	fl |= OPERATION_FLAG;

    wrote += amf3_serialize_write_func(c, &fl, sizeof(fl));
    if (cm->operation)
	wrote += amf3_serialize_value(c, cm->operation);

    return wrote;
}

int flex_parse_commandmessageext(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_commandmessage(c, classname, external_ctx);
}

void flex_free_commandmessageext(void *cm) {
    flex_free_commandmessage((Flex_CommandMessageExt *)cm);
}

void flex_dump_commandmessageext(void *cm, int depth) {
    flex_dump_commandmessage(cm, depth);
}

int flex_serialize_commandmessageext(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    return flex_serialize_commandmessage(c, classname, external_ctx);
}

int flex_parse_arraycollection(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    Flex_ArrayCollection *am = CALLOC(1, Flex_ArrayCollection);
    if (!am)
	return -1;
    am->source = amf3_parse_value(c);
    *external_ctx = am;
    return 0;
}

void flex_free_arraycollection(void *AC) {
    Flex_ArrayCollection *ac = (Flex_ArrayCollection *)AC;
    amf3_release(ac->source);
    free(ac);
}

void flex_dump_arraycollection(void *AC, int depth) {
    Flex_ArrayCollection *ac = (Flex_ArrayCollection *)AC;
    flex__dump_amf3_value("source", ac->source, depth);
}

int flex_serialize_arraycollection(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    return amf3_serialize_value(c, ((Flex_ArrayCollection *)external_ctx)->source);
}

int flex_parse_arraylist(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_arraycollection(c, classname, external_ctx);
}

void flex_free_arraylist(void *al) {
    flex_free_arraycollection((Flex_ArrayList *)al);
}

void flex_dump_arraylist(void *al, int depth) {
    flex_dump_arraycollection(al, depth);
}

int flex_serialize_arraylist(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    return flex_serialize_arraycollection(c, classname, external_ctx);
}

int flex_parse_objectproxy(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    Flex_ObjectProxy *am = CALLOC(1, Flex_ObjectProxy);
    if (!am)
	return -1;
    am->object = amf3_parse_value(c);
    *external_ctx = am;
    return 0;
}

void flex_free_objectproxy(void *OP) {
    Flex_ObjectProxy *op = (Flex_ObjectProxy *)OP;
    amf3_release(op->object);
    free(op);
}

void flex_dump_objectproxy(void *OP, int depth) {
    Flex_ObjectProxy *op = (Flex_ObjectProxy *)OP;
    flex__dump_amf3_value("object", op->object, depth);
}

int flex_serialize_objectproxy(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    return amf3_serialize_value(c, ((Flex_ObjectProxy *)external_ctx)->object);
}

int flex_parse_managedobjectproxy(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_objectproxy(c, classname, external_ctx);
}

void flex_free_managedobjectproxy(void *mop) {
    flex_free_objectproxy((Flex_ManagedObjectProxy *)mop);
}

void flex_dump_managedobjectproxy(void *mop, int depth) {
    flex_dump_objectproxy(mop, depth);
}

int flex_serialize_managedobjectproxy(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    return flex_serialize_objectproxy(c, classname, external_ctx);
}

int flex_parse_serializationproxy(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    Flex_SerializationProxy *am = CALLOC(1, Flex_SerializationProxy);
    if (!am)
	return -1;
    am->default_instance = amf3_parse_value(c);
    *external_ctx = am;
    return 0;
}

void flex_free_serializationproxy(void *SP) {
    Flex_SerializationProxy *sp = (Flex_SerializationProxy *)SP;
    amf3_release(sp->default_instance);
    free(sp);
}

void flex_dump_serializationproxy(void *SP, int depth) {
    Flex_SerializationProxy *sp = (Flex_SerializationProxy *)SP;
    flex__dump_amf3_value("defaultInstance", sp->default_instance, depth);
}

int flex_serialize_serializationproxy(
	AMF3SerializeContext c, AMF3Value classname, void *external_ctx) {
    return amf3_serialize_value(c,
	    ((Flex_SerializationProxy *)external_ctx)->default_instance);
}
