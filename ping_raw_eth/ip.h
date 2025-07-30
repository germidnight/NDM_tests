#pragma once
/*
 * Класс работы с пакетами IP
 * Упрощённый вариант - длина пакета должна уместиться в 1500 байт
 */
#include <arpa/inet.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ethernet.h"
#include "utils.h"

namespace {
struct in_addr GetIfaceIp(const char* if_name) noexcept {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    ioctl(s, SIOCGIFADDR, &ifr);
    close(s);

    const struct sockaddr_in *sa = (struct sockaddr_in*)&ifr.ifr_addr;
    if(sa == NULL) {
        printf("Error getting IP of interface %s\n", if_name);
        struct in_addr myip;
        inet_aton("127.0.0.1", &myip);
        return myip;
    }
    return sa->sin_addr;
}
}

class IPProtocol {
public:
    bool IsCreated() const noexcept {
        return ether_.IsCreated();
    }

    /* Отправка IP пакета
     * - data - данные, которые будут отправлены в пакете
     * - data_len - длина в байтах параметра data
     * - dst_ip_addr - нуль терминированная строка IP адрес назначения
     * - protocol - протокол передачи вышестоящего уровня
     * возвращает true при успешной отправке
     * ! отправляем на первый интерфейс в списке интерфейсов */
    bool SendRequest(const unsigned char* data, int data_len, const char* dst_ip_addr, short protocol = IPPROTO_ICMP) noexcept {
        unsigned char send_buf[ETH_DATA_LEN];
        memset(send_buf, 0, sizeof(send_buf));

        data_len = (data_len > (ETH_DATA_LEN - sizeof(struct iphdr))) ? (ETH_DATA_LEN - sizeof(struct iphdr)) : data_len;

        struct iphdr *ip_h = (struct iphdr*)send_buf;
        ip_h->version   = 4;    // версия протокола IPv4
        ip_h->ihl       = 5;    // длина заголовка IP-пакета в 32-битных словах (dword), параметры не используем
        ip_h->ttl       = 64;   // время жизни (TTL) — число маршрутизаторов, которые может пройти этот пакет
        ip_h->tot_len   = htons((unsigned short)sizeof(struct iphdr) + (unsigned short)data_len);
        ip_h->protocol  = protocol;
        ip_h->daddr     = inet_addr(dst_ip_addr);
        memcpy(&send_buf[sizeof(struct iphdr)], data, data_len);

        auto* if_name = ether_.GetInterfaceName(0);
        if (if_name[0] == 0) {
            return false;
        }
        ip_h->saddr = GetIfaceIp(if_name).s_addr;
        ip_h->check = utils::Checksum((unsigned short *)ip_h, sizeof(struct iphdr));

        if (ether_.SendRequest(send_buf, data_len + sizeof(struct iphdr), if_name)) {
            return true;
        }

        return true;
    }

    /* возвращает количество прочитанных байт и записывает payload в массив data, либо -1 при неудаче */
    int RcvReply(unsigned char* data, int max_data_len) noexcept {
        unsigned char rcv_buf[ETH_DATA_LEN];
        memset(rcv_buf, 0, sizeof(rcv_buf));
        int data_read = ether_.RcvReply(rcv_buf, max_data_len + sizeof(struct iphdr));

        int header_len = sizeof(struct iphdr);
        int payload_len = data_read - header_len;
        if (payload_len < 0) {
            printf("Too small IP packet received (%d)!\n", data_read);
            return -1;
        }
        memcpy(data, &rcv_buf[header_len], ((max_data_len > payload_len) ? payload_len : max_data_len));
        return data_read;
    }

    const unsigned char* GetDestinationMacAddr() const noexcept {
        return ether_.GetDestinationMacAddr();
    }

private:
    EthernetProtocol ether_;
};
