#include <string.h>
#include "http.h"
#include "tcp.h"
#include "net.h"
#include "assert.h"

#define TCP_FIFO_SIZE 40

typedef struct http_fifo {
    tcp_connect_t* buffer[TCP_FIFO_SIZE];
    uint8_t front, tail, count;
} http_fifo_t;

static http_fifo_t http_fifo_v;

static void http_fifo_init(http_fifo_t* fifo) {
    fifo->count = 0;
    fifo->front = 0;
    fifo->tail = 0;
}

static int http_fifo_in(http_fifo_t* fifo, tcp_connect_t* tcp) {
    if (fifo->count >= TCP_FIFO_SIZE) {
        return -1;
    }
    fifo->buffer[fifo->front] = tcp;
    fifo->front++;
    if (fifo->front >= TCP_FIFO_SIZE) {
        fifo->front = 0;
    }
    fifo->count++;
    return 0;
}

static tcp_connect_t* http_fifo_out(http_fifo_t* fifo) {
    if (fifo->count == 0) {
        return NULL;
    }
    tcp_connect_t* tcp = fifo->buffer[fifo->tail];
    fifo->tail++;
    if (fifo->tail >= TCP_FIFO_SIZE) {
        fifo->tail = 0;
    }
    fifo->count--;
    return tcp;
}

static size_t get_line(tcp_connect_t* tcp, char* buf, size_t size) {
    size_t i = 0;
    while (i < size) {
        char c;
        if (tcp_connect_read(tcp, (uint8_t*)&c, 1) > 0) {
            if (c == '\n') {
                break;
            }
            if (c != '\n' && c != '\r') {
                buf[i] = c;
                i++;
            }
        }
        net_poll();
    }
    buf[i] = '\0';
    return i;
}

static size_t http_send(tcp_connect_t* tcp, const char* buf, size_t size) {
    size_t send = 0;
    while (send < size) {
        send += tcp_connect_write(tcp, (const uint8_t*)buf + send, size - send);
        net_poll();
    }
    return send;
}

static void close_http(tcp_connect_t* tcp) {
    tcp_connect_close(tcp);
    printf("http closed.\n");
}

static size_t phrase_http_resp(http_resp_hdr_t* response, char* buf) {
    int write_idx = 0;
    memset(buf, 0, BUFFER_SIZE);
    // build header
    switch (response->version) {
        case HTTP_VERSION_1_0:
            write_idx += sprintf(buf + write_idx, "HTTP/1.0 ");
            break;
        default:
            assert(0);
    }
    switch (response->status_code) {
        case HTTP_404_NOT_FOUND:
            write_idx += sprintf(buf + write_idx, "404 ");
            break;
        case HTTP_200_OK:
            write_idx += sprintf(buf + write_idx, "200 ");
            break;
        default:
            assert(0);
    }
    write_idx += sprintf(buf + write_idx, "%s\r\n",response->status_msg);
    // build header lines
    header_line_t* line = response->headers;
    while (strcmp(line->key," "))
    {
        write_idx += sprintf(buf + write_idx, "%s: %s\r\n", line->key, line->value);
        line++;
    }
    // end
    write_idx += sprintf(buf + write_idx, "\r\n");
    printf("--- --- header ---\n%s\n--- --- size: %d bytes ---\n", buf, write_idx);
    return write_idx;
}

static void send_file(tcp_connect_t* tcp, const char* url) {
    FILE* file;
    // uint32_t size = 0;
    char file_path[255] = XHTTP_DOC_DIR;
    char tx_buffer[BUFFER_SIZE];

    /*
    解析url路径，查看是否是查看XHTTP_DOC_DIR目录下的文件
    如果不是，则发送404 NOT FOUND
    如果是，则用HTTP/1.0协议发送

    注意，本实验的WEB服务器网页存放在XHTTP_DOC_DIR目录中
    */
    // index.html 自动转换
    if (!strcmp(url, "/")) {
        url = "/index.html";
    }
    strcat(file_path, url);

    printf("--- - getting file %s\n", file_path);

    if((file = fopen(file_path, "rb")) == NULL) {
        printf("--- -- %s not found\n", file_path);
        // build 404 resp
        http_resp_hdr_t resp;
        header_line_t lines[1];
        lines[0] = header_line_end;
        char msg[] = "NOT FOUND";
        resp.version = HTTP_VERSION_1_0;
        resp.status_code = HTTP_404_NOT_FOUND;
        resp.status_msg = msg;
        resp.headers = lines;
        printf("--- -- sending 404\n");

        phrase_http_resp(&resp, tx_buffer);
        http_send(tcp, tx_buffer, strlen(tx_buffer));
    } else {
        // build 200 resp
        printf("--- -- found %s, sending\n", file_path);
        http_resp_hdr_t resp;

        header_line_t lines[1];
        lines[0] = header_line_end;

        char msg[] = "OK";
        resp.version = HTTP_VERSION_1_0;
        resp.status_code = HTTP_200_OK;
        resp.status_msg = msg;
        resp.headers = lines;

        phrase_http_resp(&resp, tx_buffer);
        http_send(tcp, tx_buffer, strlen(tx_buffer));

        memset(tx_buffer, 0, BUFFER_SIZE);
        int read_size = 0;
        while ((read_size = fread(tx_buffer, 1, BUFFER_SIZE, file)) > 0) {
            http_send(tcp, tx_buffer, read_size);
            memset(tx_buffer, 0, BUFFER_SIZE);
        }
        fclose(file);
    }
}

static void http_handler(tcp_connect_t* tcp, connect_state_t state) {
    if (state == TCP_CONN_CONNECTED) {
        http_fifo_in(&http_fifo_v, tcp);
        printf("http conntected.\n");
    } else if (state == TCP_CONN_DATA_RECV) {
    } else if (state == TCP_CONN_CLOSED) {
        printf("http closed.\n");
    } else {
        assert(0);
    }
}


// 在端口上创建服务器。

int http_server_open(uint16_t port) {
    if (!tcp_open(port, http_handler)) {
        return -1;
    }
    http_fifo_init(&http_fifo_v);
    return 0;
}

// 从FIFO取出请求并处理。新的HTTP请求时会发送到FIFO中等待处理。

void http_server_run(void) {
    tcp_connect_t* tcp;
    // char url_path[255];
    char rx_buffer[1024] = {0};

    while ((tcp = http_fifo_out(&http_fifo_v)) != NULL) {
        int i;
        char* c = rx_buffer;

        /*
        1、调用get_line从rx_buffer中获取一行数据，如果没有数据，则调用close_http关闭tcp，并继续循环
        */
        if (!get_line(tcp, c, 1024)) {
            printf("no data");
            close_http(tcp);
            continue;
        };


        /*
        2、检查是否有GET请求，如果没有，则调用close_http关闭tcp，并继续循环
        */
        if (strncmp(c, "GET", 3)) {
            printf("--- bad request %s\n", c);
            close_http(tcp);
            continue;
        }


        /*
        3、解析GET请求的路径，注意跳过空格，找到GET请求的文件，调用send_file发送文件
        */
        c += 4;
        i = 0;
        while(c[i++] != ' ');
        i--;
        c[i] = '\0';
        send_file(tcp, c);


        /*
        4、调用close_http关掉连接
        */
        close_http(tcp);

        printf("--- - !! final close\n");
    }
}
