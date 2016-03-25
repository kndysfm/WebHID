/**
 * Module to connect WebSocket with HIDAPI 
 */

#ifndef WEBHID_H_
#define WEBHID_H_

#include <mongoose.h>

//////////////////////////////////////////////////////////////////////////
/// HTTP request APIs
//////////////////////////////////////////////////////////////////////////

/**
 *  Handle a request to enumerate HID IFs
 */
void webhid_enumerate(struct mg_connection *nc, struct http_message *hm);

/**
 *  Handle a request to get/set HID report (Feature/ Input/ Output)
 */
void webhid_request_report(struct mg_connection *nc, struct http_message *hm);

/**
 *  Handle and route a HTTP Request
 *  It returns 1 when the request is handled properly; 0 on passed through
 */
int webhid_handle_request(struct mg_connection *nc, struct http_message *hm);

//////////////////////////////////////////////////////////////////////////
/// WebSocket APIs
//////////////////////////////////////////////////////////////////////////

/**
 *  Initialize the module for usage of WebSockets
 */
void webhid_initialize(void);

/**
 *  Returns number of Websocket-HID connection(s) still alive
 */
int webhid_get_numof_connection(void);

/**
 *  Connect WebSocket to a HID IF and start to read HID input report asynchronously
 *  Returns 1 on success; 0 on fail
 */
int webhid_connect(struct mg_connection *nc, struct http_message *hm);

/**
 *  Check Websocket-HID connection exists or not
 *  Returns 1 on exist; 0 on not
 */
int webhid_exists(struct mg_connection *nc);

/**
 *  Close connection between WebSocket and HID IF and HID handle
 */
void webhid_disconnect(struct mg_connection *nc);

/**
 *  Read HID input report held in module internal FIFO
 *  It returns size of the report to be read 
 *  When buffer is NULL or length is zero, it returns size of buffer to read held report
 */
int webhid_read_input(struct mg_connection *nc, uint8_t *buffer, size_t length);

/**
 *  Write HID output report
 *  It returns size to be written
 */
int webhid_write_output(struct mg_connection *nc, const uint8_t *buffer, size_t length);

/**
 *  Handle and route a WebSocket Frame
 *  It returns 1 when the request is handled properly; 0 on passed through
 */
int webhid_handle_frame(struct mg_connection *nc, struct websocket_message *wm);

/**
 *  Handle and route a WebSocket Handshake request
 *  It returns 1 when the request is handled properly; 0 on passed through
 */
int webhid_handshake(struct mg_connection *nc, struct http_message *hm);

/**
 *  Handle and route a WebSocket Control Frame
 *  It returns 1 when the request is handled properly; 0 on passed through
 */
int webhid_control(struct mg_connection *nc, struct websocket_message *wm);

/**
 *  Release the resources for Webhid module
 */
void webhid_finalize(void);

#endif // _WEBHID_H_