# Async TCP/UDP Server 

## системные требования :
- GNU/Linux с ядром ≥ 2.6
- g++ > 11 / С++23
- Systemd
## Установка 
- sudo apt install -y git build-essential g++ make dpkg-dev

### Через make : 
- git clone https://github.com/QQQxF991/tcp_udp_async_Server
- cd tcp_udp_async_Server
- make && sudo make install
- sudo systemctl start async-server
- (опционально: автозагрузка ) sudo systemctl enable async-server

### Через deb (пример с установкой через пакетный менеджер apt) :
- git clone https://github.com/QQQxF991/tcp_udp_async_Server
- cd tcp_udp_async_Server
- make package
- sudo dpkg -i async-server_1.0_amd64.deb
- sudo apt install ./async-server_1.0_amd64.deb
- sudo systemctl enable async-server
- sudo systemctl start async-server
- (проверка работы) sudo systemctl status async-server
  
## Тестирование:
- echo "/stats" | nc localhost 8080
- echo "/time" | nc localhost 8080
- echo "/stats" | nc -u localhost 8080 -w 1
- echo "/time" | nc -u localhost 8080 -w 1

### альтернатива  через nc :
- nc localhost 8080
- Hello
- /time
- /stats

## Остановка сервера
sudo systemctl stop async-server

## Перезапуск сервера
sudo systemctl restart async-server

## Просмотр логов в реальном времени
sudo journalctl -u async-server -f

## Проверка использования порта
sudo netstat -tlnp | grep 8080

## Запуск сервера на другом порту
./async_server 9090
