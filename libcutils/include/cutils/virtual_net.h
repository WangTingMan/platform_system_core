
/**
 * This is a Layer 3 virtual net card.
 * So only IP packets canbe passthough there.
 * And it needs administrator's permission to run
 */

#pragma once

#include <cutils/cutils_export.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef void (*ip_packet_callback)( int, uint16_t, const char*, uint16_t );

    /**
     * Create a new virtual net card.
     * if a_mac_address equals null, then the virtual net card will use
     * a random address. the address MUST have 6 bytes long.
     * return the virtual net card's ID.
     */
    CUTILS_EXPORT int open_virtual_net
        (
        const char* a_name /*the name of the virtual net card*/
        );

    /**
     * open a created virtual net card. return 0 if success
     */
    CUTILS_EXPORT int start_virtual_net
        (
        int a_virtual_net_id
        );

    /**
     * retrieve the virtual net card's mac address
     */
    CUTILS_EXPORT int retrieve_virtual_net_mac
        (
        int      a_virtual_net_id,
        uint8_t* a_mac_original,
        uint16_t a_mac_original_size
        );

    /**
     * send an ip packet, that is the packet specified by a_buf
     * is an ip packet.
     * this ip packet is from remote device, to send local windows kernel.
     */
    CUTILS_EXPORT int send_ip_packet_virtual_net
        (
        int         a_virtual_net_id,
        const char* a_buf,
        uint16_t    a_len
        );

    /**
     * send an arp packet, that is the packet specified by a_buffer
     * is an arp packet.
     * this arp packet is from remote device.
     */
    CUTILS_EXPORT int send_arp_packet_virtual_net
        (
        int         a_virtual_net_id,
        const char* a_local_mac,
        uint16_t    a_local_mac_size,
        const char* a_buffer,
        uint16_t    a_len
        );

    CUTILS_EXPORT void set_ip_packet_callback( ip_packet_callback a_callback );

    /**
     * release the memory received by receive_ip_virtual_net
     */
    CUTILS_EXPORT void release_packet
        (
        int          a_virtual_net_id,
        uint16_t     a_packet_type,
        const char*  a_packet
        );

    CUTILS_EXPORT void close_virtual_net
        (
        int a_virtual_net_id
        );

    /**
     * 替换 IP 帧中 DHCP 报文内的 MAC 地址（CHADDR字段）
     * a_ip_packet : IP 数据包起始地址（无以太网头）
     * a_ip_packet_size : IP 包总长度
     * a_mac_to_replace : 新MAC地址
     * a_mac_size : MAC长度（必须6）
     * a_mac_original: 数据包中原来MAC地址值，将会输出到该地址
     * a_mac_original_size: 必须是6
     * 如果替换成功，返回0；否则返回非0值
     */
    CUTILS_EXPORT int replace_mac_for_ip_packet
        (
        uint8_t* a_ip_packet,
        uint16_t a_ip_packet_size,
        uint8_t* a_mac_to_replace,
        uint16_t a_mac_size,
        uint8_t* a_mac_original,
        uint16_t a_mac_original_size
        );

#ifdef __cplusplus
}
#endif

