
#include <cstdint>
#include <atomic>
#include <unordered_map>
#include <string>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <chrono>

class Server {
public: 
    Server(int port);  
    ~Server();

    bool Initialize();
    void Start();
    void Stop();

private:
    int port;  
    int tcp_socket;
    int udp_socket;
    int epoll_fd;

    std::atomic<size_t> total_connect;
    std::atomic<size_t> current_connections;
    std::unordered_map<int, std::string> client_buffers_;

    std::atomic<bool> running;

    std::string GetStats() const;
    std::string GetCurrentTime() const;
    std::string process_command(const std::string& command);

    bool CreateTCPSocket();
    bool CreateUDPSocket();
    bool SetupEpoll();
    void CloseClient(int client_fd);

    void HandleTCPconnection();
    void HandleTCPdata(int client_fd);
    void HandleUDPdata();
};

