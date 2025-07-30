# Решение задания 1
Реализовать утилиту, которая осуществляет отправку ping запросов на заданный ipv4 адрес, принимает ответ и печатает MAC адрес отправителя в stdout

# Сборка
Сборка осуществляется под ОС Linux. Проверялось на Ubuntu Desktop 24.04

Команда сборки:
```bash
mkdir build
g++ -O3 ethernet.cpp main.cpp -o ./build/ping.out
```

# Использование
Запуск осуществляется с правами суперпользователя:
```bash
sudo ./build/ping.out 192.168.1.1
```
В результате успешной работы в консоль выводится MAC адрес для указанного IPv4 адреса.

В процессе работы могут быть выведены следующие ошибки:
- Command error. The required parameter is not set - IPv4 address
- Ethernet. Error. Socket file descriptor not received!
- Ethernet. Error setting socket options to receive timeout failed!
- Ethernet. Error. Socket file descriptor for getting my MAC address not received!
- Ethernet. Error while reading interface <interface_name> HW address.
- Ethernet. Error while reading interface <interface_name> index.
- Error. Can't get list of interfaces.
- Error. Can't get SIOCGIFFLAGS of interface named <interface_name>.
- Ethernet. Send failed.
- Ethernet. RcvReply. Error binding to device.
- Ethernet. Packet receive failed!
- Ethernet. Error. Too small ethernet packet received (<amount_of_data_read>)!
- Error getting IP of interface <interface_name>
- Too small IP packet received (<amount_of_data_read>)!
- ICMP packet sending failed!
- ICMP packet receive failed!
- Problems with network - Не получен корректный ответ
- Host unreachable - поле Type заголовка ICMP пакета ответа имеет значение отличное от 0 (ICMP Reply)

# Особенности работы
Ввиду того, что роутеры (в том числе WiFi) работают на уровне L3 (IP протокол), при передаче Ethernet пакетов они перезаписывают поля src_addr и dst_addr заголовка Ethernet фрейма, соответственно получаем MAC адрес порта роутера.

Чтобы получить реальный MAC адрес устройства требуется иметь прямое подключение к устройству, либо подключиться через switch.