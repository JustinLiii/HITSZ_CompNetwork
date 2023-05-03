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

    // ip
    if(memcmp(hdr->dst_ip, net_if_ip, NET_IP_LEN)) return;
    uint8_t src_ip[NET_IP_LEN];
    memcpy(src_ip, hdr->src_ip, NET_IP_LEN);
    
    // padding，检查长度
    if(hdr->total_len16 > swap16(buf->len)) return;  
    else if(hdr->total_len16 < swap16(buf->len))
    {
        buf_remove_padding(buf, swap16(hdr->total_len16) - buf->len);
    }

    // 发送
    uint16_t protocol = swap16(hdr->protocol);
    switch (protocol)
    {
        case(NET_PROTOCOL_UDP):
        case(NET_PROTOCOL_ICMP):
        case(NET_PROTOCOL_TCP):
            buf_remove_header(buf, swap16(hdr->hdr_len) * IP_HDR_LEN_PER_BYTE);
            net_in(buf, protocol, src_ip);
            break;
        default:
            icmp_unreachable(buf, src_ip, ICMP_CODE_PROTOCOL_UNREACH);
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
    hdr->hdr_checksum16 = 0;
    memcpy(hdr->src_ip, net_if_ip, NET_IP_LEN);
    memcpy(hdr->dst_ip, ip, NET_IP_LEN);
    hdr->hdr_checksum16 = checksum16((uint16_t*)hdr, sizeof(ip_hdr_t));

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
    // 只考虑20字节的ip头时，最大数据长度是8的整数倍
    size_t max_data_len = IP_MTU - sizeof(ip_hdr_t);
    if (buf->len < max_data_len)
    {
        ip_fragment_out(buf, ip, protocol, ip_id++, 0, 0);
    }
    else
    {
        // 分片
        buf_t original_buf;
        memcpy(&original_buf, buf, sizeof(buf_t));
        uint8_t *data = original_buf.data;
        int len = original_buf.len;
        uint16_t offset = 0;
        int mf = 1;
        while (mf)
        {
            size_t data_len;
            if (len > max_data_len)
            {
                data_len = max_data_len;
            }
            else 
            {
                mf = 0;
                if (len & 0x7)
                {
                    buf_add_padding(&original_buf, (len & ~0x7) + IP_HDR_OFFSET_PER_BYTE - len);
                    data_len = (len & ~0x7) + IP_HDR_OFFSET_PER_BYTE;
                }
                else
                {
                    data_len = len;
                }
            }

            buf_init(buf, data_len);
            memcpy(buf->data, data, data_len);
            ip_fragment_out(buf, ip, protocol, ip_id, offset, mf);

            offset += data_len / IP_HDR_OFFSET_PER_BYTE;
            data += data_len;
            len -= data_len;
        }
        ip_id++;
    }
}

/**
 * @brief 初始化ip协议
 * 
 */
void ip_init()
{
    net_add_protocol(NET_PROTOCOL_IP, ip_in);
}