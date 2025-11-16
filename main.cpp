#include "server.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <memory>

std::unique_ptr<Server> server;

void signal_handler(int signal) {  
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (server) {
        server->Stop(); 
    }
}

int main(int argc, const char* argv[]) {
    signal(SIGPIPE, SIG_IGN);

    int port{8080};
    if (argc > 1) port = std::atoi(argv[1]);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        server = std::make_unique<Server>(port);
        if (!server->Initialize()) { 
            std::cerr << "ERROR: Failed to initialize server" << std::endl;
            return 1;
        }
        server->Start();  
    }
    catch(const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
