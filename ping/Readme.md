# Решение задания 1
Реализовать утилиту, которая осуществляет отправку ping запросов на заданный ipv4 адрес, принимает ответ и печатает MAC адрес отправителя в stdout

# Сборка
Сборка осуществляется под ОС Linux. Проверялось на Ubuntu Desktop 24.04

Команда сборки:
```bash
g++ -O3 ping.cpp -o ping.out
```

# Использование
Запуск осуществляется с правами суперпользователя:
```bash
sudo ./ping.out 192.168.1.1
```
В результатае успешной работы в консоль выводится MAC адрес для указанного IPv4 адреса.

В процессе работы могут быть выведены следующие ошибки:
- Command error. The required parameter is not set - IPv4 address
- Error. Socket file descriptor not received! (сокет для отправки сообщений по сети)
- Error. Setting socket options to TTL failed!
- Error. Setting socket options to receive timeout failed!
- Error. Packet Sending Failed!
- Error. Packet receive failed!
- Error. Socket file descriptor for ARP not received!
- Error. Can't receive interfaces!
- Error. Failed to get MAC address from ARP table

# Особенности работы
1. Поиск MAC адреса осуществляется только в локальной ARP таблице машины, на которой запускается приложение.
2. Обновление ARP таблицы может занять время, поэтому в некоторых случаях вывод MAC адреса может произойди со второго или с третьего запуска команды с одним и тем же IP адресом.