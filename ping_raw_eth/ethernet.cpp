#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <linux/if_packet.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "ethernet.h"

namespace {
/* Получаем MAC адрес по имени интерфейса:
 * - если запрашивается имя loopback интерфеса, то возвращаем нули
 * - посредством ioctl получаем MAC адрес интерфейса, по заданному имени
 * входные параметры:
 * if_name - null-terminated string имя интерфейса
 * hw_addr - массив длиной HW_ADDR_LEN для записи результата
 * выходной параметр: ifr_ifindex. либо -1 при неудаче */
int GetIfMacByName(const char* if_name, int len, unsigned char* hw_addr, int sck_fd) {
    if (strcmp(if_name, "lo") == 0) {
        memset(hw_addr, 0, ETH_ALEN);
        return 1;
    }

    struct ifreq ifr;
    if (sck_fd < 0) {
        printf("Ethernet. Error. Socket file descriptor for getting my MAC address not received!\n");
        return -1;
    };

    strncpy(ifr.ifr_name, if_name, len);
    if (ioctl(sck_fd, SIOCGIFHWADDR, &ifr) < 0) {
        printf("Ethernet. Error while reading interface %s HW address.\n", if_name);
        return -1;
    }

    memcpy(hw_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

    if (ioctl(sck_fd, SIOCGIFINDEX, &ifr) < 0) {
        printf("Ethernet. Error while reading interface %s index.\n", if_name);
        return -1;
    }
    return ifr.ifr_ifindex;
}
}

/*
 * При создании объекта:
 * - открываем сокет
 * - настраиваем сокет
 * - получаем список сетевых интерфейсов в системе (кроме loopback)
 */
EthernetProtocol::EthernetProtocol() noexcept {
    memset(rcvd_mac_addr_, 0, ETH_ALEN);
    memset(used_if_name_, 0, IFNAMSIZ);

    sock_fd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock_fd_ < 0) {
        printf("Ethernet. Error. Socket file descriptor not received!\n");
    } else {
        struct timeval tv_out{RECV_TIMEOUT, 0};
        if (setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv_out, sizeof(tv_out)) != 0) {
            printf("Ethernet. Error setting socket options to receive timeout failed!\n");
            close(sock_fd_);
            return;
        }
        for (int i = 0; i < INTERFACE_MAX_COUNT; ++i) {
            memset(if_list_[i], 0, IFNAMSIZ);
        }
        if (RetrieveInterfacesList()) {
            created_ = true;
        }
    }
}
EthernetProtocol::~EthernetProtocol() {
    if (created_) {
        created_= false;
        close(sock_fd_);
    }
}

bool EthernetProtocol::IsCreated() const noexcept {
    return created_;
}

/* Отправка Ethernet пакета
 * входные параметры:
 * - data - данные, которые будут отправлены в пакете
 * - data_len - длина в байтах параметра data
 * - if_name - нуль терминированная строка - имя интерфейса */
bool EthernetProtocol::SendRequest(const unsigned char* data, int data_len, const char* if_name) noexcept {
    int tx_len = 0;
    unsigned char send_buf[ETH_FRAME_LEN];
    struct ether_header* eth_h = (struct ether_header*) send_buf;
    struct sockaddr_ll socket_address{};

    data_len = (data_len > ETH_DATA_LEN) ? ETH_DATA_LEN : data_len;

    memset(send_buf, 0, sizeof(send_buf));

    memset(eth_h->ether_dhost, 0xff, ETH_ALEN);
    eth_h->ether_type = htons(ETH_P_IP);
    tx_len += sizeof(*eth_h);

    memcpy(&(send_buf[tx_len]), data, data_len);
    tx_len += data_len;

    socket_address.sll_halen = ETH_ALEN;
    unsigned char my_mac_addr[ETH_ALEN];
    int if_idx = GetIfMacByName(if_name, IFNAMSIZ, my_mac_addr, sock_fd_);
    if (if_idx < 0) {
        return false;
    }
    memcpy(eth_h->ether_shost, my_mac_addr, ETH_ALEN);
    memcpy(socket_address.sll_addr, eth_h->ether_dhost, ETH_ALEN);
    socket_address.sll_ifindex = if_idx;

    strncpy(used_if_name_, if_name, IFNAMSIZ);
    if (sendto(sock_fd_, send_buf, tx_len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0) {
        printf("Ethernet. Send failed. %s\n", strerror(errno));
        return false;
    }
    return true;
}

/* Настройка интерфейса на приём данных
 * - привязка сокета к интерфейсу */
int EthernetProtocol::RcvConfigure() noexcept {
    if (setsockopt(sock_fd_, SOL_SOCKET, SO_BINDTODEVICE, used_if_name_, IFNAMSIZ) < 0) {
        printf("Ethernet. RcvReply. Error binding to device.\n");
        return -1;
    }
    return 0;
}

/* возвращает количество прочитанных байт в payload, либо -1 при неудаче */
int EthernetProtocol::RcvReply(unsigned char* data, int max_data_len) noexcept {
    unsigned char rcv_buf[ETH_FRAME_LEN];

    if (RcvConfigure() < 0) {
        return -1;
    }

    struct sockaddr_ll sll;
    socklen_t sll_len = sizeof(sll);
    int data_read = recvfrom(sock_fd_, rcv_buf, sizeof(rcv_buf), 0, (struct sockaddr*)&sll, &sll_len);

    if (data_read < 0) {
        printf("Ethernet. Packet receive failed!\n");
        return data_read;
    }
    int header_len = sizeof(struct ether_header);
    int payload_len = data_read - header_len;
    if (payload_len < 0) {
        printf("Ethernet. Error. Too small ethernet packet received (%d)!\n", data_read);
        return -1;
    }
    memcpy(data, &rcv_buf[header_len], ((max_data_len > payload_len) ? payload_len : max_data_len));
    memcpy(rcvd_mac_addr_, sll.sll_addr, ETH_ALEN);

    return payload_len;
}

const unsigned char* EthernetProtocol::GetDestinationMacAddr() const noexcept {
    return rcvd_mac_addr_;
}

/* Получает список рабочих интерфейсов в системе.
 * Используется при создании объекта класса.
 * Количество интерфейсов ограничено INTERFACE_MAX_COUNT
 * LOOPBACK интерфейсы исключаются */
bool EthernetProtocol::RetrieveInterfacesList() noexcept {
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;

    bool success = false;
    if (ioctl(sock_fd_, SIOCGIFCONF, &ifc) < 0) {
        printf("Error. Can't get list of interfaces.\n");
        return false;
    }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
    for (int i = 0; it != end; ++it) {
        strncpy(ifr.ifr_name, it->ifr_name, IFNAMSIZ);
        if (ioctl(sock_fd_, SIOCGIFFLAGS, &ifr) == 0) {
            if (!(ifr.ifr_flags & IFF_LOOPBACK)) {
                strncpy(&(if_list_[i++][0]), it->ifr_name, IFNAMSIZ);
                success = true;
            }
        } else {
            printf("Error. Can't get SIOCGIFFLAGS of interface named %s.\n", ifr.ifr_name);
        }
    }
    return success;
}

const char* EthernetProtocol::GetInterfaceName(int idx = 0) const noexcept {
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= INTERFACE_MAX_COUNT) {
        idx = INTERFACE_MAX_COUNT - 1;
    }
    return if_list_[idx];
}
