/*
 * Консольная утилита, которая самостоятельно формирует и отправляет ICMPv4 echo request запросы на заданный позиционным параметром IPv4 адрес,
 * принимает ответ и извлекает из echo reply пакета MAC адрес, печатает его в stdout.
 *
 * При разработке нельзя использовать:
 * - libstdc++,
 * - исключения,
 * - RTTI,
 * - сторонние библиотеки.
 * Программа должна быть написана под Linux.
 */
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/if_arp.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

constexpr unsigned int PING_PKT_SIZE = 64;  // ping packet size
constexpr unsigned int PORT_NUMBER = 0;     // automatic port number
constexpr unsigned int RECV_TIMEOUT = 1;    // timeout for receiving packets (in seconds)

namespace {
    /* Вычисляем контрольную сумму (RFC 1071) */
    unsigned short Checksum(void *b, int len) {
        unsigned short *buf = (unsigned short*)b;
        unsigned int sum = 0;

        for (; len > 1; len -= 2) {
            sum += *buf++;
        }
        if (len == 1) {
            sum += *(unsigned char *)buf;
        }
        sum = (sum >> 16) + (sum & 0xFFFF);
        sum += (sum >> 16);
        return ~sum;
    }
    /* Получаем MAC адрес
     * - Задаем IP для поиска в ARP-таблице
     * - получаем список всех интерфейсов
     * - Запрашиваем ARP-запись для каждого из интерфейсов
     * Для правильной работы ioctl(..., SIOCGARP, ...) требуется указать корретное имя сетевого устройства.
     * Но мы не можем получить ни MAC адрес ни имя сетевого интерфейса из ICMP ответа, поэтому
     * перебираем все интерфейсы в поисках нужного.
     * Нужным считаем первый интефейс, с положительным результатом выполнения ioctl */
    bool GetMac(const struct in_addr &ping_ip, unsigned char* hw_addr) {
        int arp_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (arp_sock < 0) {
            printf("Error. Socket file descriptor for ARP not received!\n");
            return false;
        }

        struct ifaddrs *ifaddr;
        if (getifaddrs(&ifaddr) < 0) {
            printf("Error. Can't receive interfaces!\n");
            return false;
        }

        struct arpreq req;
        memset(&req, 0, sizeof(req));
        ((struct sockaddr_in *)&req.arp_pa)->sin_family = AF_INET;
        ((struct sockaddr_in *)&req.arp_pa)->sin_addr = ping_ip;
        ((struct sockaddr_in *)&req.arp_ha)->sin_family = ARPHRD_ETHER;

        bool found = false;
        for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) {
                continue;
            }
            strncpy(req.arp_dev, ifa->ifa_name, 15);

            if (ioctl(arp_sock, SIOCGARP, (caddr_t)&req) < 0) {
                continue;
            } else {
                found = true;
                break;
            }
        }
        freeifaddrs(ifaddr);

        if (!found) {
            printf("Error. Failed to get MAC address from ARP table\n");
            close(arp_sock);
            return false;
        }
        close(arp_sock);
        memcpy(hw_addr, (unsigned char *)req.arp_ha.sa_data, sizeof(req.arp_ha.sa_data));
        return true;
    }
}
/*
 * Класс, реализующий выполнение ICMP запроса
 * USAGE:
 * Ping ping; // если успешно создан, то IsCreated вернёт true
 * ping.Do(); // выведет MAC адрес и вернёт true при успешном получении MAC адреса
 */
class Ping {
public:
    /* Настраиваем TTL = 64, таймаут ответа = 1 секунда */
    Ping() {
        sock_fd_ = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (sock_fd_ < 0) {
            printf("Error. Socket file descriptor not received!\n");
        } else {
            int ttl_val = 64;
            if (setsockopt(sock_fd_, SOL_IP, IP_TTL, &ttl_val, sizeof(ttl_val)) != 0) {
                printf("Error. Setting socket options to TTL failed!\n");
                close(sock_fd_);
            } else {
                struct timeval tv_out{RECV_TIMEOUT, 0};
                if (setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv_out, sizeof(tv_out)) != 0) {
                    printf("Error. Setting socket options to receive timeout failed!\n");
                    close(sock_fd_);
                } else {
                    created_ = true;
                }
            }
        }
    }
    ~Ping() {
        if (created_) {
            created_= false;
            close(sock_fd_);
        }
    }

    bool IsCreated() const noexcept {
        return created_;
    }

    bool Do(char* ip) noexcept {
        if (created_) {
            struct in_addr ping_ip;
            inet_aton(ip, &ping_ip);
            struct sockaddr_in ping_addr{AF_INET, htons(PORT_NUMBER), ping_ip};

            auto id = getpid() & 0xFFFF;
            if (!SendICMPRequest(id, &ping_addr)) {
                return false;
            }
            if (!RcvICMPReply(id)) {
                return false;
            }
            unsigned char hw[sizeof(arpreq::arp_ha.sa_data)];
            if (!GetMac(ping_ip, hw)) {
                return false;
            }
            printf("%02x:%02x:%02x:%02x:%02x:%02x\n", hw[0], hw[1], hw[2], hw[3], hw[4], hw[5]);
            return true;
        } else {
            return false;
        }
    }

private:
    struct PingPkt {
        struct icmphdr hdr;
        char msg[PING_PKT_SIZE - sizeof(struct icmphdr)];
    };

    bool SendICMPRequest(unsigned short id, const struct sockaddr_in* ping_addr) noexcept {
        struct PingPkt pckt;
        memset(&pckt, 0, sizeof(pckt));
        pckt.hdr.type = ICMP_ECHO;
        pckt.hdr.un.echo.id = id;
        pckt.hdr.un.echo.sequence = 0;
        pckt.hdr.checksum = Checksum(&pckt, sizeof(pckt));
        if (sendto(sock_fd_, &pckt, sizeof(pckt), 0, (struct sockaddr *)ping_addr, sizeof(*ping_addr)) <= 0) {
            printf("Error. Packet Sending Failed!\n");
            close(sock_fd_);
            return false;
        }
        return true;
    }

    bool RcvICMPReply(unsigned short id) noexcept {
        struct sockaddr_in remote_addr{};
        char rcv_buf[128];
        socklen_t addr_len = sizeof(remote_addr);
        if (recvfrom(sock_fd_, rcv_buf, sizeof(rcv_buf), 0, (struct sockaddr *)&remote_addr, &addr_len) <= 0) {
            printf("Error. Packet receive failed!\n");
            close(sock_fd_);
            return false;
        }
        return true;
    }

    bool created_ = false;
    int sock_fd_;
};
