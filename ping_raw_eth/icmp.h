#pragma once
/*
 * Класс для работы с ICMP пакетами
 */
#include "ip.h"
#include "utils.h"

#include <linux/icmp.h>

class Ping {
public:
    static constexpr unsigned int PING_PKT_SIZE = 64;

    bool IsCreated() const noexcept {
        return ip_proto_.IsCreated();
    }

    bool Do(const char* ip) noexcept {
        if (IsCreated()) {
            auto id = getpid() & 0xFFFF;
            if (!SendRequest(id, ip)) {
                return false;
            }
            int res = RcvReply(id);
            if (res != 0) {
                printf("%s\n", ((res == -1) ? "Problems with network" : "Host unreachable"));
                return false;
            }
            auto* hw = ip_proto_.GetDestinationMacAddr();
            printf("%02x:%02x:%02x:%02x:%02x:%02x\n", hw[0], hw[1], hw[2], hw[3], hw[4], hw[5]);
            return true;
        } else {
            return false;
        }
    }

private:
    bool SendRequest(unsigned short id, const char* ping_addr) noexcept {
        unsigned char send_buf[PING_PKT_SIZE];
        memset(send_buf, 0, sizeof(send_buf));
        struct icmphdr* icmp_header = (struct icmphdr*)send_buf;

        icmp_header->type = ICMP_ECHO;
        icmp_header->un.echo.id = id;
        icmp_header->un.echo.sequence = 0;
        icmp_header->checksum = utils::Checksum(send_buf, sizeof(send_buf));
        if (!ip_proto_.SendRequest(send_buf, sizeof(send_buf), ping_addr, IPPROTO_ICMP)) {
            printf("ICMP packet sending failed!\n");
            return false;
        }
        return true;
    }

    /* Получение ответа
     * Возвращает результата:
     * - -1 - ошибка получения ответа (IP протокол)
     * - 0 - пришёл нормальный ответ
     * - 1 - пришёл кривой ответ */
    int RcvReply([[maybe_unused]]unsigned short id) noexcept {
        static constexpr int BUF_LEN = 128;
        unsigned char rcv_buf[BUF_LEN];
        int icmp_len = ip_proto_.RcvReply(rcv_buf, sizeof(rcv_buf));
        if (icmp_len <= 0) {
            printf("ICMP packet receive failed!\n");
            return -1;
        }
        const struct icmphdr* icmp_header = (struct icmphdr*)rcv_buf;

        // можно добавить проверку id: icmp_header->un.echo.id == id
        if (icmp_header->type != 0) {
            return 1;
        }
        return 0;
    }

    IPProtocol ip_proto_;
};
