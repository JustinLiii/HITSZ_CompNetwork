#include "net.h"
#include "ip.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"

int ip_id = 0;

/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void ip_in(buf_t *buf, uint8_t *src_mac)
{
    if(buf->len < sizeof(ip_hdr_t)) return;
    ip_hdr_t* hdr = (ip_hdr_t*)buf->data;

    // 检查
    // 无视可选长度
    // version
    if(hdr->version != IP_VERSION_4) return;

    // checksum
    uint16_t received_checksum = hdr->hdr_checksum16;

    hdr->hdr_checksum16 = 0;
    uint16_t cal_checksum = checksum16((uint16_t*)hdr, sizeof(ip_hdr_t));

    if (cal_checksum != received_checksum) return;
    hdr->hdr_checksum16 = received_checksum;

    // ip
    if(memcmp(hdr->dst_ip, net_if_ip, NET_IP_LEN)) return;
    uint8_t src_ip[NET_IP_LEN];
    memcpy(src_ip, hdr->src_ip, NET_IP_LEN);
    
    // padding，检查长度
    if(swap16(hdr->total_len16) > buf->len) return;  
    else if(swap16(hdr->total_len16) < buf->len)
    {
        buf_remove_padding(buf, buf->len - swap16(hdr->total_len16));
    }
    // 发送
    uint8_t protocol = hdr->protocol;
    switch (protocol)
    {
        case(NET_PROTOCOL_UDP):
        case(NET_PROTOCOL_ICMP):
        case(NET_PROTOCOL_TCP):
            buf_remove_header(buf, hdr->hdr_len * IP_HDR_LEN_PER_BYTE);
            net_in(buf, protocol, src_ip);
            break;
        default:
            icmp_unreachable(buf, src_ip, ICMP_CODE_PROTOCOL_UNREACH);
            break;
    }
}

/**
 * @brief 处理一个要发送的ip分片
 * 
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf)
{
    buf_add_header(buf, sizeof(ip_hdr_t));
    // 填写报头
    ip_hdr_t* hdr = (ip_hdr_t*)buf->data;
    hdr->hdr_len = sizeof(ip_hdr_t) / IP_HDR_LEN_PER_BYTE;
    hdr->version = IP_VERSION_4;
    hdr->tos = 0;
    hdr->total_len16 = swap16(buf->len);
    hdr->id16 = swap16(id);
    uint16_t flags_fragment = 0;
    if (mf) flags_fragment |= IP_MORE_FRAGMENT;
    flags_fragment |= (offset & 0x1FFF);
    hdr->flags_fragment16 = swap16(flags_fragment);
    hdr->ttl = 64;
    hdr->protocol = protocol;
    memcpy(hdr->src_ip, net_if_ip, NET_IP_LEN);
    memcpy(hdr->dst_ip, ip, NET_IP_LEN);
    hdr->hdr_checksum16 = 0;
    hdr->hdr_checksum16 = checksum16((uint16_t*)hdr, sizeof(ip_hdr_t));

    printf("fragment %lu bytes sent\n", buf->len);
    arp_out(buf, ip);
}

/**
 * @brief 处理一个要发送的ip数据包
 * 
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    static int ip_id = 0;
    printf("sending buf %lu bytes\n", buf->len);
    // 只考虑20字节的ip头时，最大数据长度是8的整数倍
    size_t max_data_len = IP_MTU - sizeof(ip_hdr_t);

    // 分片
    buf_t new_buf;
    uint32_t offset = 0;

    while (buf->len > max_data_len)
    {
        buf_init(&new_buf, max_data_len);
        memcpy(new_buf.data, buf->data, max_data_len);
        ip_fragment_out(&new_buf, ip, protocol, ip_id, offset, 1);
        buf_remove_header(buf, max_data_len);
        offset += max_data_len / IP_HDR_OFFSET_PER_BYTE;
    }
    buf_init(&new_buf, buf->len);
    memcpy(new_buf.data, buf->data, buf->len); // 最后一个分片，大小就等于该分片大小
    ip_fragment_out(&new_buf, ip, protocol, ip_id, offset, 0);
    ip_id++;
}

/**
 * @brief 初始化ip协议
 * 
 */
void ip_init()
{
    net_add_protocol(NET_PROTOCOL_IP, ip_in);
}