#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>

#define XHTTP_DOC_DIR               "../htmldocs"
#define BUFFER_SIZE                 1024

// the end of a header line array is marked by end line defined below
typedef struct header_line {
    char key[1024];
    char value[1024];
} header_line_t;

static const header_line_t header_line_end = { .key = " ", .value = " " };

typedef enum http_method {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST
} http_method_t;

typedef enum http_version {
    HTTP_VERSION_1_0,
} http_version_t;

typedef enum http_status {
    HTTP_404_NOT_FOUND = 404,
    HTTP_200_OK = 200,
} http_status_t;

typedef struct http_req_hdr {
    http_method_t method;
    char* url;
    http_version_t version;
    header_line_t* headers;
} http_hdr_t;

typedef struct http_resp_hdr {
    http_version_t version;
    http_status_t status_code;
    char* status_msg;
    header_line_t* headers;
} http_resp_hdr_t;

int http_server_open(uint16_t port);
void http_server_run(void);

#endif
