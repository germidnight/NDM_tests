#include "ping.h"

#include <stdlib.h>

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
    if (!OptionsParsing(argc, argv)) {
        return EXIT_FAILURE;
    }

    Ping ping;
    if (!ping.IsCreated() || !(ping.Do(argv[1]))) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
