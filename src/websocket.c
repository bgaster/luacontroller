#include <libwebsockets.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <websocket.h>

static message_callbackf message_callback = NULL;

void register_message_callback(message_callbackf mf) { message_callback = mf; }

/* *
 * Callback for handling incoming WebSocket traffic.
 */
int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
                  void *in, size_t len) {
  switch (reason) {
  case LWS_CALLBACK_ESTABLISHED:
    printf("Server: Connection established\n");
    break;

  case LWS_CALLBACK_RECEIVE:
    if (len && in) {
      // Echo the received message back to the client
      // printf("Server received: %.*s\n", (int)len, (char *)in);
      if (message_callback != NULL) {
        unsigned char *result = message_callback(in, len);
        if (result != NULL) {
          lws_write(wsi, result, strlen((char *)result), LWS_WRITE_TEXT);
          free(result);
        }
      }
    }
    break;

  case LWS_CALLBACK_CLOSED:
    printf("Server: Connection closed\n");
    break;

  case LWS_CALLBACK_SERVER_WRITEABLE:
    //  We don't have anything to write until the client sends us something
    break;
  case LWS_CALLBACK_HTTP:
    if (len > 0 && in != NULL) {
      printf("Server: Got an HTTP request\n");
    }
    break;

  default:
    break;
  }

  return 0;
}

/* *
 * Array of protocols supported by the server.  In this case, we only
 * support a single WebSocket protocol.
 */
static struct lws_protocols protocols[] = {
    {
        "iii-protocol", //  Name of our protocol.  Should match the client.
        callback_http,  //  Callback function to handle messages.
        0,              //  Per-session data size (not used here).
        MAX_PAYLOAD_SIZE,
    },
    {NULL, NULL, 0, 0} //  End of the list.  Must be all zeros.
};

void close_context(struct lws_context *context) {
  lws_context_destroy(context);
}

struct lws_context *create_context(void) {
  struct lws_context_creation_info info;
  struct lws_context *context;
  const char *interface = "127.0.0.1";
  // const char *cert_path = NULL; // No certificate
  // const char *key_path = NULL;    // No key
  const char *ca_filepath = NULL;
  // int use_ssl = 0; // 0 for no SSL, 1 for SSL.  We are using  no SSL
  int require_ssl = 0;

  memset(&info, 0, sizeof info); // Clear the context creation info struct

  // Set up the context creation info
  info.port = SERVER_PORT;
  info.iface = interface;
  info.protocols = protocols;
  // info.ssl_cert_filepath = cert_path;
  // info.ssl_private_key_filepath = key_path;
  info.ssl_cert_filepath = NULL;
  info.ssl_private_key_filepath = NULL;
  info.ssl_ca_filepath = ca_filepath;
  info.gid = -1;
  info.uid = -1;
  info.options = 0;
  info.fd_limit_per_thread = 1024; // Example value, adjust as needed
  // info.address = address;
  // info.require_ssl = require_ssl;

  // Create the WebSocket context
  context = lws_create_context(&info);
  if (context == NULL) {
    fprintf(stderr, "Failed to create WebSocket context\n");
    return NULL;
  }

  printf("WebSocket server started on port %d\n", SERVER_PORT);

  return context;
}
