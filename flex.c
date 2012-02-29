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
    struct flex_abstractmessage *am = CALLOC(1, struct flex_abstractmessage);
    if (!am)
	return -1;

    struct flex_flags ff;
    flex_parse_flags(c, &ff);

    if (flex_flags_toggle(&ff, BODY_FLAG))
	am->body = amf3_parse_value(c);
    if (flex_flags_toggle(&ff, CLIENT_ID_FLAG))
	am->client_id = amf3_parse_value(c);
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

int flex_parse_asyncmessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    struct flex_asyncmessage *am = CALLOC(1, struct flex_asyncmessage);
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

int flex_parse_asyncmessageext(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_asyncmessage(c, classname, external_ctx);
}

int flex_parse_acknowledgemessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    struct flex_ackowledgemessage *am = CALLOC(1, struct flex_ackowledgemessage);
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

int flex_parse_acknowledgemessageext(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_acknowledgemessage(c, classname, external_ctx);
}

int flex_parse_errormessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_acknowledgemessage(c, classname, external_ctx);
}

int flex_parse_commandmessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    struct flex_commandmessage *am = CALLOC(1, struct flex_commandmessage);
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

int flex_parse_commandmessageext(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_commandmessage(c, classname, external_ctx);
}

int flex_parse_arraycollection(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    struct flex_arraycollection *am = CALLOC(1, struct flex_arraycollection);
    if (!am)
	return -1;
    am->source = amf3_parse_value(c);
    *external_ctx = am;
    return 0;
}

int flex_parse_arraylist(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_arraycollection(c, classname, external_ctx);
}

int flex_parse_objectproxy(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    struct flex_objectproxy *am = CALLOC(1, struct flex_objectproxy);
    if (!am)
	return -1;
    am->object = amf3_parse_value(c);
    *external_ctx = am;
    return 0;
}

int flex_parse_managedobjectproxy(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    return flex_parse_objectproxy(c, classname, external_ctx);
}

int flex_parse_serializationproxy(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx) {
    struct flex_serializationproxy *am = CALLOC(1, struct flex_serializationproxy);
    if (!am)
	return -1;
    am->default_instance = amf3_parse_value(c);
    *external_ctx = am;
    return 0;
}
