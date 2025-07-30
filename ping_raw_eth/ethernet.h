#pragma once
/*
 * Класс работы с пакетами Ethernet
 * Упрощённый вариант - максимальный объём фрейма 1514 байт
 *
 * Ethernet Header (14 bytes standard):
 *   Destination MAC Address (6 bytes)
 *   Source MAC Address (6 bytes)
 *   EtherType/Length (2 bytes)
 */

#include <linux/if.h>
#include <linux/if_ether.h>

class EthernetProtocol {
public:
    static constexpr unsigned int RECV_TIMEOUT = 1;            // timeout for receiving packets (in seconds)
    static constexpr unsigned int INTERFACE_MAX_COUNT = 10;    // максимальное количество сетевых интерфейсов

    EthernetProtocol() noexcept;
    ~EthernetProtocol();

    bool IsCreated() const noexcept;

    /* возвращает true при успешной отправке */
    bool SendRequest(const unsigned char* data, int data_len, const char* if_name) noexcept;

    /* возвращает количество прочитанных байт в payload, либо -1 при неудаче */
    int RcvReply(unsigned char* data, int max_data_len) noexcept;

    const unsigned char* GetDestinationMacAddr() const noexcept;

    const char* GetInterfaceName(int idx) const noexcept;

private:
    bool RetrieveInterfacesList() noexcept;
    int RcvConfigure() noexcept;

    bool created_ = false;
    int sock_fd_;
    char if_list_[INTERFACE_MAX_COUNT][IFNAMSIZ];
    unsigned char rcvd_mac_addr_[ETH_ALEN];
    char used_if_name_[IFNAMSIZ];
};
