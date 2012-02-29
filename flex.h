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
typedef struct flex_abstractmessage Flex_AbstractMessage;

/* AsyncMessage */
#define CORRELATION_ID_FLAG	    (0x01)
#define CORRELATION_ID_BYTES_FLAG   (0x02)

struct flex_asyncmessage {
    struct flex_abstractmessage *am;
    AMF3Value correlation_id;
    AMF3Value correlation_id_bytes;
};
typedef struct flex_asyncmessage Flex_AsyncMessage;

/* AsyncMessageExt */
typedef Flex_AsyncMessage Flex_AsyncMessageExt;

/* AcknowledgeMessage */
struct flex_ackowledgemessage {
    struct flex_asyncmessage *am;
};
typedef struct flex_ackowledgemessage Flex_AcknowledgeMessage;

/* AcknowledgeMessageExt */
typedef Flex_AcknowledgeMessage Flex_AcknowledgeMessageExt;

/* ErrorMessage */
typedef Flex_AcknowledgeMessage Flex_ErrorMessage;

/* CommandMessage */
#define OPERATION_FLAG	(0x01)
struct flex_commandmessage {
    struct flex_asyncmessage *am;
    AMF3Value operation;
};
typedef struct flex_commandmessage Flex_CommandMessage;

/* CommandMessageExt */
typedef Flex_CommandMessage Flex_CommandMessageExt;

/* ArrayCollection */
struct flex_arraycollection {
    AMF3Value source;
};
typedef struct flex_arraycollection Flex_ArrayCollection;

/* ArrayList */
typedef Flex_ArrayCollection Flex_ArrayList;

/* ObjectProxy */
struct flex_objectproxy {
    AMF3Value object;
};
typedef struct flex_objectproxy Flex_ObjectProxy;

/* ManagedObjectProxy */
typedef Flex_ObjectProxy Flex_ManagedObjectProxy;

/* SerializationProxy */
struct flex_serializationproxy {
    AMF3Value default_instance;
};
typedef struct flex_serializationproxy Flex_SerializationProxy;


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

void flex_free_abstractmessage(void *am);
void flex_free_asyncmessage(void *am);
void flex_free_asyncmessageext(void *am);
void flex_free_acknowledgemessage(void *am);
void flex_free_acknowledgemessageext(void *am);
void flex_free_errormessage(void *em);
void flex_free_commandmessage(void *cm);
void flex_free_commandmessageext(void *cm);
void flex_free_arraycollection(void *ac);
void flex_free_arraylist(void *al);
void flex_free_objectproxy(void *op);
void flex_free_managedobjectproxy(void *mop);
void flex_free_serializationproxy(void *sp);
#endif
