#include "net.h"
#include "icmp.h"
#include "ip.h"

/**
 * @brief 发送icmp响应
 * 
 * @param req_buf 收到的icmp请求包
 * @param src_ip 源ip地址
 */
static void icmp_resp(buf_t *req_buf, uint8_t *src_ip)
{
    icmp_hdr_t* req_hdr = (icmp_hdr_t*)req_buf->data;
    uint16_t* req_data = (uint16_t*)(req_buf->data + sizeof(icmp_hdr_t));
    size_t data_len = req_buf->len - sizeof(icmp_hdr_t);
    buf_t* buf = &txbuf;
    buf_init(buf, data_len);
    memcpy(buf->data, req_data, data_len);

    buf_add_header(buf, sizeof(icmp_hdr_t));
    icmp_hdr_t* hdr = (icmp_hdr_t*)buf->data;
    hdr->type = ICMP_TYPE_ECHO_REPLY;
    hdr->code = 0;
    hdr->checksum16 = 0;
    hdr->id16 = req_hdr->id16;
    hdr->seq16 = req_hdr->seq16;
    hdr->checksum16 = checksum16((uint16_t*)hdr, buf->len);

    ip_out(buf, src_ip, NET_PROTOCOL_ICMP);
}

/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 * @param src_ip 源ip地址
 */
void icmp_in(buf_t *buf, uint8_t *src_ip)
{
    if (buf->len < sizeof(icmp_hdr_t)) return;
    icmp_hdr_t* hdr = (icmp_hdr_t*)buf->data;

    // checksum
    uint16_t received_checksum = hdr->checksum16;
    hdr->checksum16 = 0;
    uint16_t cal_checksum = checksum16((uint16_t*)buf->data, buf->len);
    if (received_checksum != cal_checksum) return;
    hdr->checksum16 = received_checksum;

    if (hdr->type == ICMP_TYPE_ECHO_REQUEST)
    {
        icmp_resp(buf, src_ip);
    }
}

/**
 * @brief 发送icmp不可达
 * 
 * @param recv_buf 收到的ip数据包
 * @param src_ip 源ip地址
 * @param code icmp code，协议不可达或端口不可达
 */
void icmp_unreachable(buf_t *recv_buf, uint8_t *src_ip, icmp_code_t code)
{
    buf_t* buf = &txbuf;
    buf_init(buf, sizeof(ip_hdr_t) + 8);
    memcpy(buf->data, recv_buf->data, sizeof(ip_hdr_t) + 8);

    buf_add_header(buf, sizeof(icmp_hdr_t));
    icmp_hdr_t* hdr = (icmp_hdr_t*)buf->data;
    hdr->type = ICMP_TYPE_UNREACH;
    hdr->code = code;
    hdr->checksum16 = 0;
    hdr->id16 = 0;
    hdr->seq16 = 0;
    hdr->checksum16 = checksum16((uint16_t*)hdr, buf->len);

    ip_out(buf, src_ip, NET_PROTOCOL_ICMP);
}

/**
 * @brief 初始化icmp协议
 * 
 */
void icmp_init(){
    net_add_protocol(NET_PROTOCOL_ICMP, icmp_in);
}