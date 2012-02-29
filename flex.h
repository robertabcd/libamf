#ifndef _FLEX_H
#   define _FLEX_H
#   ifndef HAVE_FLEX_COMMON_OBJECTS
#	define HAVE_FLEX_COMMON_OBJECTS
#   endif

#include "amf3.h"

/* AbstactMessage */
/* Flag 1 */
#define BODY_FLAG		(0x01)
#define CLIENT_ID_FLAG		(0x02)
#define DESTINATION_FLAG	(0x04)
#define HEADERS_FLAG		(0x08)
#define MESSAGE_ID_FLAG		(0x10)
#define TIMESTAMP_FLAG		(0x20)
#define TIME_TO_LIVE_FLAG	(0x40)
/* Flag 2 */
#define CLIENT_ID_BYTES_FLAG	(0x01)
#define MESSAGE_ID_BYTES_FLAG	(0x02)

struct flex_abstractmessage {
    AMF3Value body;
    AMF3Value client_id;
    AMF3Value destination;
    AMF3Value headers;
    AMF3Value message_id;
    AMF3Value timestamp;
    AMF3Value ttl;
    AMF3Value client_id_bytes;
    AMF3Value message_id_bytes;
};

/* AsyncMessage */
#define CORRELATION_ID_FLAG	    (0x01)
#define CORRELATION_ID_BYTES_FLAG   (0x02)

struct flex_asyncmessage {
    struct flex_abstractmessage *am;
    AMF3Value correlation_id;
    AMF3Value correlation_id_bytes;
};

/* AsyncMessageExt */
#define flex_asyncmessageext flex_asyncmessage

/* AcknowledgeMessage */
struct flex_ackowledgemessage {
    struct flex_asyncmessage *am;
};

/* AcknowledgeMessageExt */
#define flex_ackowledgemessageext flex_ackowledgemessage

/* ErrorMessage */
#define flex_errormessage flex_ackowledgemessage

/* CommandMessage */
#define OPERATION_FLAG	(0x01)
struct flex_commandmessage {
    struct flex_asyncmessage *am;
    AMF3Value operation;
};

/* CommandMessageExt */
#define flex_commandmessageext flex_commandmessage

/* ArrayCollection */
struct flex_arraycollection {
    AMF3Value source;
};

/* ArrayList */
#define flex_arraylist flex_arraycollection

/* ObjectProxy */
struct flex_objectproxy {
    AMF3Value object;
};

/* ManagedObjectProxy */
#define flex_managedobjectproxy flex_objectproxy

/* SerializationProxy */
struct flex_serializationproxy {
    AMF3Value default_instance;
};


int flex_parse_abstractmessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_asyncmessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_asyncmessageext(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_acknowledgemessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_acknowledgemessageext(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_errormessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_commandmessage(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_commandmessageext(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_arraycollection(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_arraylist(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_objectproxy(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_managedobjectproxy(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
int flex_parse_serializationproxy(
	AMF3ParseContext c, AMF3Value classname, void **external_ctx);
#endif
