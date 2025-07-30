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
#include "icmp.h"

/* Проверка валидности IPv4 адреса */
bool CheckIPv4Valid(const char *ip) {
    unsigned char buf[sizeof(struct in_addr)];
    if (inet_pton(AF_INET, ip, buf) <= 0) {
        printf("Error. IPv4 address is not correct\n");
        return false;
    }
    return true;
}
/*
 * Разбор опций командной строки
 * В опциях командной строки ожидается один IPv4 адрес, если есть другие опции, то они игнорируются
 * Возвращает IPv4 адрес в бинарном формате
 */
bool OptionsParsing(int argc, char **argv) {
    if (argc < 2) {
        printf("Command error. The required parameter is not set - IPv4 address\n");
        return false;
    }
    return CheckIPv4Valid(argv[1]);
}

int main(int argc, char *argv[]) {
    static constexpr int ATTEMPTS = 5;

    if (!OptionsParsing(argc, argv)) {
        return 1;
    }

    Ping ping;
    if (!ping.IsCreated()) {
        return 2;
    }

    for (int i = 1; !ping.Do(argv[1]) && (i <= ATTEMPTS); ++i);

    return 0;
}
