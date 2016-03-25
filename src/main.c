/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */


#include "webhid.h"

static sig_atomic_t s_signal_received = 0;

static const char *s_http_port = "8000";
static const char *s_document_root = "../bin/html";
static struct mg_serve_http_opts s_http_server_opts;

#define _MIN(a,b) (((a)<=(b))?(a):(b))

static void signal_handler(int sig_num) {
	signal(sig_num, signal_handler);  // Reinstantiate signal handler
	s_signal_received = sig_num;
}

static int is_websocket(const struct mg_connection *nc) {
	return nc->flags & MG_F_IS_WEBSOCKET;
}

static void print_mbuf(const struct mbuf *src, size_t lim) {
	char *buf;
	int sz;
	int is_ws_frame = 0;
	
	sz = lim == 0? src->len: _MIN(src->len, lim);
	if (sz == 0) return;

	// websocket frame check
	if (src->len > 1) {
		int len = src->buf[1];
		int left = src->len - 2;
		if (src->buf[1] & 0x80) { // is masked
			left -= 4;
			len &= 0x7f;
		}
		if (len <= 0x7D) { // => 7bit length
			if (len == left) {
				is_ws_frame = 1;
			}
		} else if (len == 0x7E) { // => 16bit length
			len = src->buf[2] | (src->buf[3] << 8);
			left -= 2;
			if (len == left) is_ws_frame = 1;
		}
		// else if (len == 0x7F) => 64bit length, ignore 
	}

	if (!is_ws_frame) { // print as string
		buf = (char *)malloc((sz+1) * sizeof(char));
		memcpy(buf, src->buf, sz);
		buf[sz] = '\0';
	} else { // print as byte array
		int i;
		size_t sz_buf = (sz+1) * sizeof(char) * 3;
		buf = (char *)malloc(sz_buf);
		buf[0] = '\0';
		for (i = 0; i < sz; i++)
		{
			char str_byte[4];
			_snprintf_s(str_byte, 4, 4, "%02x ", (uint8_t)src->buf[i]);
			strcat_s(buf, sz_buf, str_byte);
		}
	}
	printf("%s\r\n", buf);
	free(buf);
}

#ifdef _DEBUG

static void print_recv_mbuf(struct mg_connection *nc) {
	printf("<<<\r\n");
	print_mbuf(&nc->recv_mbuf, 0);
}

static void print_send_mbuf(struct mg_connection *nc) {
	printf(">>>\r\n");
	print_mbuf(&nc->send_mbuf, 0);
}

#define MG_EV_CASE(EV) case EV: printf("[ %s ]\r\n", #EV); break

static void print_ev(int ev)
{
	switch (ev) {
		MG_EV_CASE(MG_EV_SSI_CALL);
		MG_EV_CASE(MG_EV_HTTP_REPLY);
		MG_EV_CASE(MG_EV_HTTP_CHUNK);
		MG_EV_CASE(MG_EV_HTTP_REQUEST);
		MG_EV_CASE(MG_EV_WEBSOCKET_HANDSHAKE_REQUEST);
		MG_EV_CASE(MG_EV_WEBSOCKET_HANDSHAKE_DONE);
		MG_EV_CASE(MG_EV_WEBSOCKET_FRAME);
		MG_EV_CASE(MG_EV_WEBSOCKET_CONTROL_FRAME);
		//MG_EV_CASE(MG_EV_POLL);
		MG_EV_CASE(MG_EV_ACCEPT);
		MG_EV_CASE(MG_EV_CONNECT);
		MG_EV_CASE(MG_EV_RECV);
		MG_EV_CASE(MG_EV_SEND);
		MG_EV_CASE(MG_EV_TIMER);
		MG_EV_CASE(MG_EV_CLOSE);
	}
}

static void print_ws_opcode(int op)
{
	switch (op) {
		MG_EV_CASE(WEBSOCKET_OP_CONTINUE);
		MG_EV_CASE(WEBSOCKET_OP_TEXT	);
		MG_EV_CASE(WEBSOCKET_OP_BINARY	);
		MG_EV_CASE(WEBSOCKET_OP_CLOSE	);
		MG_EV_CASE(WEBSOCKET_OP_PING	);
		MG_EV_CASE(WEBSOCKET_OP_PONG	);
	}
}

#else //_DEBUG
#define print_recv_mbuf(nc)
#define print_send_mbuf(nc)
#define print_ev(ev)
#define print_ws_opcode(op)
#endif //_DEBUG

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {

	struct http_message *hm = (struct http_message *) ev_data;
	struct websocket_message *wm = (struct websocket_message *) ev_data;

	print_ev(ev);

	switch (ev) {
	case MG_EV_SSI_CALL:
		break;
	case MG_EV_HTTP_REPLY:
	case MG_EV_HTTP_CHUNK:
		break;
	case MG_EV_HTTP_REQUEST:
		if (webhid_handle_request(nc, hm)) {
			// Webhid module handled the message 
			printf("[NOTIFY] A webhid request was handled\n");
		}
		else {
			mg_serve_http(nc, hm, s_http_server_opts);  /* Serve static content */
			nc->flags |= MG_F_SEND_AND_CLOSE;
		}

		break;
	case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
		if (!webhid_handshake(nc, hm)) {
			nc->flags |= MG_F_CLOSE_IMMEDIATELY; // fail to open HID
			printf("[NOTIFY] Connection %08x failed to be open\n", nc);
		} else {
			printf("[NOTIFY] Connection %08x was opened\n", nc);
		}
		break;
	case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
		printf("[NOTIFY] %d connection(s) is alive\n", webhid_get_numof_connection());
		break;
	case MG_EV_WEBSOCKET_FRAME:
		print_ws_opcode(wm->flags & 0x0f);
		webhid_handle_frame(nc, wm);
		break;
	case MG_EV_WEBSOCKET_CONTROL_FRAME:
		print_ws_opcode(wm->flags & 0x0f);
		webhid_control(nc, wm);
		break;
	case MG_EV_ACCEPT	: /* New connection accepted. union socket_address * */
	case MG_EV_CONNECT	: /* connect() succeeded or failed. int *  */
		break;
	case MG_EV_RECV		: /* Data has benn received. int *num_bytes */
		print_recv_mbuf(nc);
		break;
	case MG_EV_SEND		: /* Data has been written to a socket. int *num_bytes */
		break;
	case MG_EV_TIMER	: /* now >= conn->ev_timer_time. double * */
		break;
	case MG_EV_CLOSE	: /* Connection is closed. NULL */
		if (is_websocket(nc)) {
			if (webhid_exists(nc)) {
				webhid_disconnect(nc);
				printf("[NOTIFY] Connection %08x was closed\n", nc);
			}
			printf("[NOTIFY] %d connection(s) is alive\n", webhid_get_numof_connection());
		}
		break;
	case MG_EV_POLL		: /* Sent to each connection on each mg_mgr_poll() call */
	default:
		return;
	}

	print_send_mbuf(nc);

}

int main(int argc, char *argv[]) {
  struct mg_mgr mgr;
  struct mg_connection *nc;
  int i;
  char *cp;
#ifdef MG_ENABLE_SSL
  const char *ssl_cert = NULL;
#endif

  printf(
	  "================================= WebHID Server ====================================== \n"
	  " This is a HTTP server application software for communication with HID via WebSocket \n"
	  " Its HTTP client is able to access to a remote HID I/F \n\n"
	  " This software is powered by following C libraries: \n"
	  "  * Mongoose (GPLv2, https://github.com/cesanta/mongoose)\n"
	  "  * HID API (GPLv3, https://github.com/signal11/hidapi)\n"
	  "  * POSIX Threads for Windows (LGPLv2, https://sourceforge.net/projects/pthreads4w/)\n"
	  "====================================================================================== \n"
	  );

  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);

  mg_mgr_init(&mgr, NULL);

  webhid_initialize();

  /* Process command line options to customize HTTP server */
  for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
	  mgr.hexdump_file = argv[++i];
	} else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
	  s_http_server_opts.document_root = argv[++i];
	} else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
	  s_http_port = argv[++i];
	} else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
	  s_http_server_opts.auth_domain = argv[++i];
#ifdef MG_ENABLE_JAVASCRIPT
	} else if (strcmp(argv[i], "-j") == 0 && i + 1 < argc) {
	  const char *init_file = argv[++i];
	  mg_enable_javascript(&mgr, v7_create(), init_file);
#endif
	} else if (strcmp(argv[i], "-P") == 0 && i + 1 < argc) {
	  s_http_server_opts.global_auth_file = argv[++i];
	} else if (strcmp(argv[i], "-A") == 0 && i + 1 < argc) {
	  s_http_server_opts.per_directory_auth_file = argv[++i];
	} else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
	  s_http_server_opts.url_rewrites = argv[++i];
#ifndef MG_DISABLE_CGI
	} else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
	  s_http_server_opts.cgi_interpreter = argv[++i];
#endif
#ifdef MG_ENABLE_SSL
	} else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
	  ssl_cert = argv[++i];
#endif
	} else {
	  fprintf(stderr, "Unknown option: [%s]\n", argv[i]);
	  exit(1);
	}
  }

  /* Set HTTP server options */
  nc = mg_bind(&mgr, s_http_port, ev_handler);
  if (nc == NULL) {
	fprintf(stderr, "Error starting server on port %s\n", s_http_port);
	exit(1);
  }

#ifdef MG_ENABLE_SSL
  if (ssl_cert != NULL) {
	const char *err_str = mg_set_ssl(nc, ssl_cert, NULL);
	if (err_str != NULL) {
	  fprintf(stderr, "Error loading SSL cert: %s\n", err_str);
	  exit(1);
	}
  }
#endif

  mg_set_protocol_http_websocket(nc);
  s_http_server_opts.document_root = s_document_root;
  s_http_server_opts.enable_directory_listing = "yes";

  /* Use current binary directory as document root */
  if (argc > 0 && ((cp = strrchr(argv[0], '/')) != NULL ||
	  (cp = strrchr(argv[0], '/')) != NULL)) {
	*cp = '\0';
	s_http_server_opts.document_root = argv[0];
  }

  printf("Starting HID streaming server on port %s\n", s_http_port);
  while (s_signal_received == 0) {
	  mg_mgr_poll(&mgr, 200);
  }

  webhid_finalize();

  mg_mgr_free(&mgr);

  return 0;
}
