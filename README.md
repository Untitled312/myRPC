# myRPC — Remote Procedure Call over sockets

Учебный проект, реализующий механизм удалённого вызова команд Bash по сокетам.
Состоит из клиента `myrpc-client` и сервера-демона `myrpc-server`.

## Протокол (JSON)

**Запрос клиента:**
{"login":"user","command":"ls -la /tmp"}

**Ответ сервера:**
{"code":0,"result":"stdout данные..."}
{"code":1,"result":"описание ошибки из stderr"}

## Сборка

make        -    собрать всё  
make clean  -    очистить  
make deb    -   собрать deb-пакеты

## Установка

sudo apt install ./myrpc-client_1.0-1_amd64.deb
sudo apt install ./myrpc-server_1.0-1_amd64.deb

## Запуск сервера

sudo systemctl start myrpc-server

## В консольном режиме:
sudo myrpc-server --foreground

## Использование клиента

myrpc-client -h 192.168.1.10 -p 5000 -s -c "uname -a"
myrpc-client --host 127.0.0.1 --port 5000 --dgram --command "whoami"