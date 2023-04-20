#include "ethernet.h"
#include "utils.h"
#include "driver.h"
#include "arp.h"
#include "ip.h"
/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 */
void ethernet_in(buf_t *buf)
{
    uint16_t protocal;

    // 判断长度
    if(buf->len < sizeof(ether_hdr_t)) return;

    // 拆包
    ether_hdr_t *hdr = (ether_hdr_t *)(buf->data);
    uint8_t* dst = hdr->dst;

    if (memcmp(dst, ether_broadcast_mac, NET_MAC_LEN) 
    && memcmp(dst, net_if_mac, NET_MAC_LEN)) return;

    protocal = swap16(hdr->protocol16);

    buf_remove_header(buf, sizeof(ether_hdr_t));

    net_in(buf, protocal, hdr->src);
}
/**
 * @brief 处理一个要发送的数据包
 * 
 * @param buf 要处理的数据包
 * @param mac 目标MAC地址
 * @param protocol 上层协议
 */
void ethernet_out(buf_t *buf, const uint8_t *mac, net_protocol_t protocol)
{
    // 检查长度，若不足46则填充
    if (buf->len < ETHERNET_MIN_TRANSPORT_UNIT)
        buf_add_padding(buf, ETHERNET_MIN_TRANSPORT_UNIT - buf->len);

    // 添加Eth包头
    buf_add_header(buf, sizeof(ether_hdr_t));
    ether_hdr_t *hdr = (ether_hdr_t *)(buf->data);
    memcpy(hdr->dst, mac, NET_MAC_LEN*sizeof(uint8_t));
    memcpy(hdr->src, net_if_mac, NET_MAC_LEN*sizeof(uint8_t));
    hdr->protocol16 = swap16(protocol);
    
    // 发送
    driver_send(buf);
}

/**
 * @brief 初始化以太网协议
 * 
 */
void ethernet_init()
{
    buf_init(&rxbuf, ETHERNET_MAX_TRANSPORT_UNIT + sizeof(ether_hdr_t));
}

/**
 * @brief 一次以太网轮询
 * 
 */
void ethernet_poll()
{
    if (driver_recv(&rxbuf) > 0)
        ethernet_in(&rxbuf);
}
