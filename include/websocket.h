#include <libwebsockets.h>
#include <signal.h>

#define SERVER_PORT 8765
#define MAX_PAYLOAD_SIZE 65536

int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
                  void *in, size_t len);

struct lws_context *create_context(void);

void close_context(struct lws_context *context);

typedef unsigned char *(*message_callbackf)(const unsigned char *message,
                                            size_t len);
void register_message_callback(message_callbackf mf);
