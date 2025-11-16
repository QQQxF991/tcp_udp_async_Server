#include "server.hpp"
#include <iostream>
#include <cstring>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cctype>

#define MAX_EVENTS 64
#define BUFFER_SIZE 4096

static std::string trim(const std::string& str) {
    size_t end = str.find_last_not_of(" \n\r\t");
    return (end == std::string::npos) ? "" : str.substr(0, end + 1);
}

Server::Server(int port)
    : port(port), tcp_socket(-1), udp_socket(-1), epoll_fd(-1),
      total_connect(0), current_connections(0), running(false) {
}

Server::~Server() {
    Stop();
}

std::string Server::GetCurrentTime() const {
    auto current_time = std::chrono::system_clock::now();
    std::time_t time_t_formater = std::chrono::system_clock::to_time_t(current_time);
    std::tm locale_time; 
    localtime_r(&time_t_formater, &locale_time); 
    std::ostringstream string_formate_; 
    string_formate_ << std::put_time(&locale_time, "%Y-%m-%d %H:%M:%S");
    return string_formate_.str();
}

bool Server::CreateTCPSocket() {
    tcp_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if(tcp_socket == -1) {
        std::cerr << "ERROR : Failed to create TCP socket: " << strerror(errno) << std::endl;    
        return false;
    }

    int options = 1;
    if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &options, sizeof(options)) < 0) {
        std::cerr << "ERROR : Failed to set TCP socket options: " << strerror(errno) << std::endl;
        close(tcp_socket);
        return false;
    }

    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if(bind(tcp_socket, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "ERROR : Failed to bind TCP socket: " << strerror(errno) << std::endl;
        close(tcp_socket);
        return false;       
    }

    if(listen(tcp_socket, SOMAXCONN) < 0) { 
        std::cerr << "ERROR : Failed to listen on TCP socket: " << strerror(errno) << std::endl;
        close(tcp_socket);
        return false;
    }
    return true;
}

bool Server::CreateUDPSocket() {
    udp_socket = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
   
    if (udp_socket == -1) {
        std::cerr << "ERROR : Failed to create UDP socket: " << strerror(errno) << std::endl;
        return false;
    }

    sockaddr_in address;
    memset(&address, 0, sizeof(address));
   
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
   
    if (bind(udp_socket, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "ERROR : Failed to bind UDP socket: " << strerror(errno) << std::endl;
        close(udp_socket);
        return false;
    }
   
    return true;
}

bool Server::SetupEpoll() {
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cerr << "ERROR: Failed to create epoll: " << strerror(errno) << std::endl;
        return false;
    }

    epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = tcp_socket;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_socket, &event) < 0) {
        std::cerr << "ERROR : Failed to add TCP socket to epoll: " << strerror(errno) << std::endl;
        return false;
    }
    
    event.events = EPOLLIN;
    event.data.fd = udp_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_socket, &event) < 0) {
        std::cerr << "ERROR : Failed to add UDP socket to epoll: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

void Server::Start() {
    running = true;
    epoll_event events[MAX_EVENTS];
    std::cout << "Server started working" << std::endl;

    while (running) {
        int nums_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        if(nums_events == -1) {
            if(errno == EINTR) {
                continue;
            }
            std::cerr << "epoll_wait error: " << strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < nums_events; ++i) {
            if (events[i].data.fd == tcp_socket) {
                HandleTCPconnection();
            } else if (events[i].data.fd == udp_socket) {
                HandleUDPdata();
            } else {
                HandleTCPdata(events[i].data.fd);
            }
        }
    }
}

void Server::HandleTCPconnection() {
    sockaddr_in client_address;
    socklen_t address_len = sizeof(client_address);
    int client_fd = accept(tcp_socket, (sockaddr*)&client_address, &address_len);

    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "ERROR : Failed to accept connection: " << strerror(errno) << std::endl;
        }
        return;
    }

    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
     
    epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    event.data.fd = client_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0) {
        std::cerr << "ERROR : Failed to add client to epoll: " << strerror(errno) << std::endl;
        close(client_fd);
        return;
    }

    total_connect++;
    current_connections++;
    client_buffers_[client_fd] = "";     
    
    std::cout << "New TCP connection from : " << inet_ntoa(client_address.sin_addr)
              << ":" << ntohs(client_address.sin_port)
              << " (FD: "<< client_fd << ")" << std::endl;
}

void Server::HandleTCPdata(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t rbyte;
    
    while ((rbyte = read(client_fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[rbyte] = '\0';
        std::string message(buffer);
        
        message = trim(message);

        std::string response;
        if (!message.empty() && message[0] == '/') {
            response = process_command(message);
        } else {
            response = message; 
        }

        response += "\n";
        
        ssize_t send_bytes = send(client_fd, response.c_str(), response.length(), 0);
        if(send_bytes < 0) {
            std::cerr << "ERROR : Failed to send data to client: " << strerror(errno) << std::endl;
            break;
        }
    }

    if (rbyte == 0 || (rbyte < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        CloseClient(client_fd);
    }
}

void Server::HandleUDPdata() {
    char buffer[BUFFER_SIZE];
    sockaddr_in client_address;
    socklen_t address_len = sizeof(client_address);
    ssize_t rbyte = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&client_address, &address_len);
    if (rbyte > 0) {
        buffer[rbyte] = '\0';
        std::string message(buffer);
        
        message = trim(message);

        std::string response;
        if (!message.empty() && message[0] == '/') {
            response = process_command(message);
        } else {
            response = message;
        }

        sendto(udp_socket, response.c_str(), response.length(), 0, (sockaddr*)&client_address, address_len);
        total_connect++;
    }
}

std::string Server::process_command(const std::string& command) {
    std::string cmd = trim(command);
    
    if (cmd == "/time") {
        return GetCurrentTime();
    } else if (cmd == "/stats") {
        return GetStats(); 
    } else if (cmd == "/shutdown") {
        Stop();
        return "Server stopped";
    } else { 
        return "ERROR: Unknown command \"" + cmd + "\"";
    }
}

bool Server::Initialize() {
    if (!CreateTCPSocket() || !CreateUDPSocket() || !SetupEpoll()) {
        return false;
    }
    return true;
}

void Server::CloseClient(int client_fd) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
    client_buffers_.erase(client_fd);
    current_connections--;
    std::cout << "Client disconnected (FD : " << client_fd << " ) " << std::endl;
}

void Server::Stop() {
    running = false; 
    for (auto & pair : client_buffers_) {
        close(pair.first);
    }
    client_buffers_.clear();
    
    if (epoll_fd != -1) {
        close(epoll_fd);
        epoll_fd = -1;
    }
    if (tcp_socket != -1) {
        close(tcp_socket);
        tcp_socket = -1;
    }
    if (udp_socket != -1) {
        close(udp_socket);
        udp_socket = -1;
    }
    std::cout << "Server stopped" << std::endl;
}

std::string Server::GetStats() const {
    std::ostringstream oss;
    oss << "Total connections: " << total_connect 
        << "\nCurrent connections: " << current_connections;
    return oss.str();
}