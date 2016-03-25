/**
 * Module to connect WebSocket with HIDAPI 
 */

#include <pthread.h>
#include <hidapi.h>

#include "webhid.h"
#include "vl_queue.h"
#include "bdl_list.h"

#ifdef _WIN32
#define msleep(x)	Sleep(x)
#else
#define msleep(x)	usleep(((x)+500)/1000)
#endif

#ifdef _DEBUG
#define WEBHID_ASSERT(exp) \
	do { if(!(exp)) printf("%s (% 4d): [ASSERT] \"%s\" is falsy\r\n", __FUNCTION__, __LINE__, #exp); } while(0)
#define WEBHID_TRACE(msg) \
	printf("%s (% 4d): %s\r\n", __FUNCTION__, __LINE__, msg)
#else /*_DEBUG */
#define WEBHID_ASSERT(exp)
#define WEBHID_TRACE(msg)
#endif /*_DEBUG */

/**
 *  Make virtual device path by information about HID
 *  (IF number, Vendor ID, Product ID, Usage Page, Usage)
 */
#define HID_VIRTUAL_PATH_FORMAT	"/hid/%04x/%04x/%04x/%04x/%04x/"
/**
 *  Length of the virtual-path should be fixed
 */
#define HID_VIRTUAL_PATH_LENGTH	(30)
/**
 *  rule of virtual path format
 */
static int uri_is_virtual_path(const char *p)
{
	return	memcmp(p+9, "/", 1) == 0 && 
			memcmp(p+14, "/", 1) == 0 && 
			memcmp(p+19, "/", 1) == 0 && 
			memcmp(p+24, "/", 1) == 0 && 
			memcmp(p+29, "/", 1) == 0 ;
}
/**
 *  formatting path with device information
 */
static int make_virtual_path(char *buf, size_t size_buf, const struct hid_device_info *info)
{
	/* Make string with same format for HID virtual path */
	return _snprintf_s(buf, size_buf, size_buf/sizeof(char), HID_VIRTUAL_PATH_FORMAT, 
		info->interface_number & 0xffff,  info->vendor_id & 0xffff, info->product_id & 0xffff,
		info->usage_page & 0xffff, info->usage & 0xffff);
}

static char * format_hid_info(char *buf, size_t size_buf, size_t count, const struct hid_device_info *info)
{
	char path[HID_VIRTUAL_PATH_LENGTH + 2]; 
	make_virtual_path(path, sizeof(path), info);

	_snprintf_s(buf, size_buf, count, "{"\
		"\"interfaceNumber\": %d, "\
		"\"vendorId\": %d, "\
		"\"productId\": %d, "\
		"\"usagePage\": %d, "\
		"\"usage\": %d, "\
		"\"manufacturerString\": \"%S\", "\
		"\"productString\": \"%S\", "\
		"\"virtualPath\": \"%s\" "\
		"}", 
		info->interface_number, 
		info->vendor_id,
		info->product_id,
		info->usage_page,
		info->usage,
		info->manufacturer_string,
		info->product_string,
		path);

	return buf;
}


const struct hid_device_info *search_hid_info(const struct hid_device_info *root, struct mg_str *virtual_path)
{
	const struct hid_device_info *info = root;

	while(info)
	{
		char str_test[32]; 
		size_t len;
		make_virtual_path(str_test, sizeof(str_test), info);
		len = strlen(str_test);
		if (len > virtual_path->len) {
			/* generated path is longer than argument which would include extra words */
			continue; 
		}
		else if (memcmp(virtual_path->p, str_test, len) == 0) { 
			break; /* found! */
		}

		info = info->next;
	}

	return info;
}

static hid_device *open_hid_virtual_path(struct mg_str *virtual_path) {
	struct hid_device_info * root_info;
	const struct hid_device_info *info;
	hid_device *dev;

	root_info = hid_enumerate(0, 0); /* enumerate all devices */

	info = search_hid_info(root_info, virtual_path);
	dev = info? hid_open_path(info->path): 0;

	hid_free_enumeration(root_info);
	return dev;
}


void webhid_enumerate(struct mg_connection *nc, struct http_message *hm) {
	char str_vid[16], str_pid[16];
	uint16_t vid = 0, pid = 0;
	struct hid_device_info * root_info, *info;
	int count = 0;

	/* Get form variables */
	mg_get_http_var(&hm->body, "vid", str_vid, sizeof(str_vid));
	vid = (uint16_t) strtol(str_vid, NULL, 0);
	mg_get_http_var(&hm->body, "pid", str_pid, sizeof(str_pid));
	pid = (uint16_t) strtol(str_pid, NULL, 0);

	/* Send headers */
	mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

	root_info = hid_enumerate(vid, pid); /* enumerate certain devices */
	WEBHID_TRACE(root_info? "HID(s) is enumerated": "No HID was found");
	info = root_info;
	/* Start enumaration into JSON Array */
	mg_printf_http_chunk(nc, "{\"devices\": [");
	while (info)
	{
		char str_info[1024];

		format_hid_info(str_info, sizeof(str_info), sizeof(str_info)/sizeof(char), info);
		/* Compute the result and send it back as a JSON object */
		mg_printf_http_chunk(nc, 
			"%s%s", 
			str_info,
			info->next? ", ": " "
			);

		count++;
		info = info->next;
	}
	mg_printf_http_chunk(nc, "], \"count\": %d }", count); /* Close JSON Array */
	hid_free_enumeration(root_info);
	mg_send_http_chunk(nc, "", 0);  /* Send empty chunk, the end of response */
}

void webhid_request_report(struct mg_connection *nc, struct http_message *hm) {
	hid_device *dev;
	int is_set_request, is_get_request;
	int is_feature, is_input, is_output;
	uint8_t data[256];
	int returned_size = 0;
	const char *msg_err = 0;
	const wchar_t *wmsg_hid_err = 0;

	dev = open_hid_virtual_path(&hm->uri);
	if (dev == 0) {
		WEBHID_TRACE("No HID was found");
		msg_err = "HID virtual-path is incorrect";
		goto HID_FEATURE_ERROR_404;
	} else {
		WEBHID_TRACE("A HID was found");
	}

	is_set_request = (mg_vcmp(&hm->method, "POST") == 0 || mg_vcmp(&hm->method, "PUT") == 0);
	is_get_request = (!is_set_request && mg_vcmp(&hm->method, "GET") == 0);
	is_feature = (memcmp(hm->uri.p+HID_VIRTUAL_PATH_LENGTH, "feature/", 8) == 0 && (is_get_request || is_set_request));
	is_input = (!is_feature && memcmp(hm->uri.p+HID_VIRTUAL_PATH_LENGTH, "input/", 6) == 0 && is_get_request);
	is_output = (!is_feature && !is_input && memcmp(hm->uri.p+HID_VIRTUAL_PATH_LENGTH, "output/", 7) == 0 && is_set_request);

	if (is_feature) {
		/* Read Report ID from the tail of URI */
		uint8_t rid = (uint8_t) strtol(hm->uri.p+HID_VIRTUAL_PATH_LENGTH+8, NULL, 0); 

		if (is_get_request) { 
			WEBHID_TRACE("Get Feature Report");
			data[0] = rid; 
			returned_size = hid_get_feature_report(dev, (uint8_t *)data, sizeof(data));
			if (returned_size <= 0) {
				msg_err = "Fail to get HID feature report";
				wmsg_hid_err = hid_error(dev);
				goto HID_FEATURE_ERROR_500;
			}

			/* Send headers */
			mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
			mg_send_http_chunk(nc, (char *)data, returned_size);
			mg_send_http_chunk(nc, "", 0);  /* Send empty chunk, the end of response */

		} else if (is_set_request) { 
			WEBHID_TRACE("Set Feature Report");
			data[0] = rid;	/** if conflict between data and URI ?? */
			if (hm->body.len > 256) {
				WEBHID_TRACE("too long feature report");
				msg_err = "HID feature report is too long to read";
				wmsg_hid_err = hid_error(dev);
				goto HID_FEATURE_ERROR_500;
			}
			else if (hm->body.len < 1) {
				WEBHID_TRACE("feature report does not have body");
				msg_err = "HID feature report has no body but report ID";
				wmsg_hid_err = hid_error(dev);
				goto HID_FEATURE_ERROR_500;
			}
			memcpy(data+1, hm->body.p+1, hm->body.len-1);
			/* Set(update) Feature Report*/
			returned_size = hid_send_feature_report(dev, (const uint8_t *)data, sizeof(data));
			if (returned_size <= 0) {
				msg_err = "Fail to set HID feature report";
				wmsg_hid_err = hid_error(dev);
				goto HID_FEATURE_ERROR_500;
			}

			/* Send status */
			mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\n");
		}
		else {
			msg_err = "Other HTTP method is not supprted than POST, PUT and GET";
			goto HID_FEATURE_ERROR_404;
		}
	}
	else if (is_input) {
		WEBHID_TRACE("Get Input Report");
		returned_size = hid_read_timeout(dev, (uint8_t *)data, sizeof(data), 1000);
		if (returned_size <= 0) {
			msg_err = "Fail to read HID input report";
			wmsg_hid_err = hid_error(dev);
			goto HID_FEATURE_ERROR_500;
		}

		/* Send headers */
		mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
		mg_send_http_chunk(nc, (char *)data, returned_size);
		mg_send_http_chunk(nc, "", 0);  /* Send empty chunk, the end of response */
	}
	else if (is_output) {
		uint8_t rid = (uint8_t) strtol(hm->uri.p+HID_VIRTUAL_PATH_LENGTH+7, NULL, 0);
		WEBHID_TRACE("Set Output Report");

		data[0] = (rid && rid == hm->body.p[0])? rid: hm->body.p[0];
		if (hm->body.len > 255) goto HID_FEATURE_ERROR_500;
		memcpy(data+1, hm->body.p+1, hm->body.len-1);
		/* Set(update) Feature Report*/
		returned_size = hid_write(dev, (const uint8_t *)data, hm->body.len);
		if (returned_size <= 0) {
			msg_err = "Fail to send HID output report";
			wmsg_hid_err = hid_error(dev);
			goto HID_FEATURE_ERROR_500;
		}
		/* Send status */
		mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\n");
	}
	else {
		msg_err = "HID request type is invalid";
		goto HID_FEATURE_ERROR_404;
	}

	if (dev) hid_close(dev);
	return;

HID_FEATURE_ERROR_404:
	//mg_printf(nc, "%s", "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
	mg_printf(nc, "%s", "HTTP/1.1 404 Not Found\r\nTransfer-Encoding: chunked\r\n\r\n");
	if (msg_err) mg_send_http_chunk(nc, msg_err, strlen(msg_err));
	mg_send_http_chunk(nc, "", 0);  /* Send empty chunk, the end of response */
	if (dev) hid_close(dev);
	return;

HID_FEATURE_ERROR_500:
	//mg_printf(nc, "%s", "HTTP/1.1 500 Internal Server Error\r\n");
	mg_printf(nc, "%s", "HTTP/1.1 500 Internal Server Error\r\nTransfer-Encoding: chunked\r\n\r\n");
	if (msg_err) mg_send_http_chunk(nc, msg_err, strlen(msg_err));
	if (wmsg_hid_err) {
		size_t size_buf = 2 * wcslen(wmsg_hid_err);
		char *buf = (char *)malloc(size_buf);
		_snprintf_s(buf, size_buf, size_buf/sizeof(char), "\r\n%S\r\n", wmsg_hid_err);
		mg_send_http_chunk(nc, buf, strlen(buf));
		free(buf);
	}
	mg_send_http_chunk(nc, "", 0);  /* Send empty chunk, the end of response */
	if (dev) hid_close(dev);
	return;

}

int webhid_handle_request(struct mg_connection *nc, struct http_message *hm)
{
	if (memcmp(hm->uri.p, "/hid/", 5) == 0) {
		WEBHID_TRACE("Requested URI includes '/hid/'");
		if (uri_is_virtual_path(hm->uri.p)) {
			WEBHID_TRACE("Requested URI is matched to HID virtual-path");
			webhid_request_report(nc, hm);
		}
		else if(memcmp(hm->uri.p+4, "/enumerate", 9) == 0) {
			WEBHID_TRACE("Requested URI means HID enumeration");
			webhid_enumerate(nc, hm);
		}
		else {
			WEBHID_TRACE("URI was invalid to request HID");
			mg_printf(nc, "%s", "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
		}
		return 1;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
/// WebSocket APIs
//////////////////////////////////////////////////////////////////////////

/// Connection between WebSocket connection and HID IF handle
/// It manages thread to read input reports and FIFO to hold them

struct hidsocket_connection {
	struct mg_connection *connection;
	hid_device *device;
	uint8_t report_id;
	vl_queue_t queue_input;
	pthread_t th;
	pthread_mutex_t mtx;
	int requested_disconnect; /// boolean
};

static bdl_list_t hidsocket_connections_list = 0;

void webhid_initialize(void) {
	WEBHID_TRACE("webhid_initialize() called");
	hidsocket_connections_list = bdl_list_create();
}

int webhid_get_numof_connection(void) {
	return bdl_list_get_size(hidsocket_connections_list);
}

static bdl_list_node_t search_connection_node(struct mg_connection *nc) {
	bdl_list_node_t node = bdl_list_get_head(hidsocket_connections_list);

	while (node) {
		struct hidsocket_connection *hs_conn = 
			(struct hidsocket_connection *)bdl_list_extract_content(node);
		if (hs_conn->connection == nc){
			return node;
		}
		node = bdl_list_get_next(hidsocket_connections_list, node);
	}

	return 0;
}

static struct hidsocket_connection * search_connection(struct mg_connection *nc) {
	bdl_list_node_t node = search_connection_node(nc);
	return (struct hidsocket_connection *) (node? bdl_list_extract_content(node): 0);
}

static void *proc_reading_hid (void *param) {
	struct hidsocket_connection *conn = (struct hidsocket_connection *)param;

	while(conn->requested_disconnect == 0) {
		if (pthread_mutex_trylock(&conn->mtx) == 0) { // != EBUSY
			uint8_t data[256];
			int len = hid_read_timeout(conn->device, data+sizeof(uint32_t), sizeof(data)-sizeof(uint32_t), 0);
			if (len > 0 && (conn->report_id == 0 || conn->report_id == data[0]))
			{
				*(uint32_t *)data = len;
				vl_queue_push(conn->queue_input, data, len+sizeof(uint32_t));
			}
			pthread_mutex_unlock(&conn->mtx);
			msleep(1);// 1ms sleep
		}
	}

	return 0;
}

static void destroy_connection(struct hidsocket_connection* conn)
{
	pthread_mutex_lock(&conn->mtx);
	conn->requested_disconnect = 1;
	pthread_join(conn->th, NULL);
	pthread_mutex_destroy(&conn->mtx);

	if (conn->device) hid_close(conn->device);
	vl_queue_destroy(conn->queue_input);

	free(conn);
}

static void destroy_connectin_pvoid(void *conn)
{
	destroy_connection((struct hidsocket_connection*)conn);
}

static struct hidsocket_connection* init_connection(struct mg_connection *nc, hid_device *dev, uint8_t rid) {
	struct hidsocket_connection* conn = (struct hidsocket_connection*) malloc(sizeof(struct hidsocket_connection));
	if (conn) {
		conn->connection = nc;
		conn->device = dev;
		conn->report_id = rid;
		conn->queue_input = vl_queue_create();
		if (!conn->queue_input) WEBHID_TRACE("failed to create queue");
		conn->requested_disconnect = 0;

		if (pthread_create(&conn->th, 0, proc_reading_hid, conn) == 0) {
			pthread_mutex_init(&conn->mtx, NULL);
		}
		else {
			WEBHID_TRACE("failed to create thread");
			// ERROR!
			destroy_connection(conn);
			conn = 0;
		}
	}
	return conn;
}

int webhid_connect(struct mg_connection *nc, struct http_message *hm) 
{
	hid_device *dev = open_hid_virtual_path(&hm->uri);
	if (dev)
	{
		if (search_connection_node(nc)) {
			// already exists
			hid_close(dev);
		} else { // no connection registered in list was found
			uint8_t rid = (uint8_t) strtol(hm->uri.p+HID_VIRTUAL_PATH_LENGTH, NULL, 0); 
			struct hidsocket_connection* conn = init_connection(nc, dev, rid);
			bdl_list_node_t node_new =
				conn? bdl_list_append_node(hidsocket_connections_list, conn): 0;
			if (node_new) {
				// succeeded registeration
				return 1;
			} else {
				// fail
				WEBHID_TRACE("list node was not created");
			}
		}
	} else {
		WEBHID_TRACE("virtual path could not be opened");
	}
	return 0;
}

int webhid_exists(struct mg_connection *nc)
{
	bdl_list_node_t node = search_connection_node(nc);
	return (node != 0);
}

void webhid_disconnect(struct mg_connection *nc)
{
	bdl_list_node_t node = search_connection_node(nc);
	WEBHID_TRACE("webhid_disconnect() called");
	if (node) {
		struct hidsocket_connection *conn;
		conn = (struct hidsocket_connection *)bdl_list_delete_node(hidsocket_connections_list, node);
		if (conn) destroy_connection(conn);
		else WEBHID_TRACE("node had no content");
	} else {
		WEBHID_TRACE("connection is not found");
		///TODO: handle error..
	}
}

int webhid_read_input(struct mg_connection *nc, uint8_t *buffer, size_t length)
{
	struct hidsocket_connection *conn = search_connection(nc);
	int ret;
	if (conn) {
		pthread_mutex_lock(&conn->mtx);
		ret = vl_queue_pop_all(conn->queue_input, buffer, length);
		pthread_mutex_unlock(&conn->mtx);
	} else {
		WEBHID_TRACE("connection is not found");
		ret = -1;
	}
	return ret;
}

int webhid_write_output(struct mg_connection *nc, const uint8_t *buffer, size_t length)
{
	struct hidsocket_connection *conn = search_connection(nc);
	int ret;
	if (conn) {
		pthread_mutex_lock(&conn->mtx);
		ret = hid_write(conn->device, buffer, length);
		pthread_mutex_unlock(&conn->mtx);
	} else {
		WEBHID_TRACE("connection is not found");
		ret = -1;
	}
	return ret;
}

int webhid_handle_frame(struct mg_connection *nc, struct websocket_message *wm)
{
	struct hidsocket_connection *conn = search_connection(nc);
	WEBHID_TRACE("webhid_handle_frame() called");
	if (conn) {
		uint8_t opcode = (wm->flags & 0x0f);
		uint8_t *data;
		int len;
		if (opcode == WEBSOCKET_OP_BINARY && wm->size > 0) {
			webhid_write_output(nc, wm->data, wm->size);
		} 
		// Poll Input Report in every frame received
		len = webhid_read_input(nc, 0, 0);
		if (len > 0) {
			data = (uint8_t *)malloc(len);
			if (data) {
				int len2  = webhid_read_input(nc, data, len);
				WEBHID_ASSERT(len == len2);
				mg_send_websocket_frame(nc, WEBSOCKET_OP_BINARY, data, len);
				free(data);
			} else {
				WEBHID_TRACE("failed to allocate memory buffer for input report");
			}
		} else {
			const uint32_t zero = 0;
			mg_send_websocket_frame(nc, WEBSOCKET_OP_BINARY, &zero, sizeof(uint32_t));
		}
		return 1;
	} else {
		WEBHID_TRACE("connection is not found");
		return 0;
	}
}

int webhid_handshake(struct mg_connection *nc, struct http_message *hm)
{
	WEBHID_TRACE("webhid_handshake() called");
	if (uri_is_virtual_path(hm->uri.p)) {
		if (webhid_connect(nc, hm)) {
			WEBHID_TRACE("WebSocket and HID were connected");
			return 1;
		}
	}		
	WEBHID_TRACE("Connection was not opened");
	return 0;
}

int webhid_control(struct mg_connection *nc, struct websocket_message *wm)
{
	uint8_t opcode = (wm->flags & 0x0f);
	WEBHID_TRACE("webhid_control() called");
	if (opcode == WEBSOCKET_OP_CLOSE) {
		webhid_disconnect(nc);
		WEBHID_TRACE("webhid_control() Connection was closed");
		return 1;
	}

	return 0;
}

void webhid_finalize(void)
{
	WEBHID_TRACE("webhid_finalize() called");
	bdl_list_destroy(hidsocket_connections_list, destroy_connectin_pvoid);
	hidsocket_connections_list = 0;
}