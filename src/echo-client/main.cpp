#include <iostream>
#include <cstring>
#include <string>
#include <exception>
#include <list>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <signal.h>

extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
}

#include <glog/logging.h>

#include "../../include/ip.h"


#define BUFSIZE         1024

using namespace std;

struct SocketInformation {
    int sock_{};
    sockaddr_in information_{};
};

void usage();
bool parse(int argc,char* argv[]);
bool Connection(const Ip& ip, const uint16_t port);

void CommThread();
void Clean();
void SignalHandler(int signal);

SocketInformation g_clientInformation{};

int main(int argc, char* argv[]) {
    if(!parse(argc, argv)) return -1;

    Ip serverIP(argv[1]);
    uint16_t port = stoi(argv[2]);

    try {
        signal(SIGTERM, SignalHandler);
        signal(SIGINT, SignalHandler);

        if(!Connection(htonl(serverIP), port)) throw runtime_error("Failed to call Connection");

        thread(CommThread).detach();

        string input;

        while(1) {
            cout<<"[Client] : ";
            getline(std::cin, input);

            if(g_clientInformation.sock_ == -1) break;
            if(send(g_clientInformation.sock_, input.c_str(), input.length(), 0) == -1) throw runtime_error("Failed to call send");
            input.clear();

            this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }catch(const exception& e) {
        LOG(WARNING)<<"[main] "<<e.what()<<endl;
        LOG(WARNING)<<"ERROR : "<<strerror(errno)<<endl;
    }

    Clean();
}

void usage() {
    printf("echo-client: \n");
    printf("syntax : echo-client <ip> <port> \n");
    printf("sample : echo-client 192.168.10.2 1234 \n");
}

bool parse(int argc, char* argv[]) {
    if(argc != 3) {
        usage();
        return false;
    }

    return true;
}

bool Connection(const Ip& ip, const uint16_t port) {
    try {
        if((g_clientInformation.sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) throw runtime_error("Failed to call socket");

        //int flag = 1;
        //setsockopt(g_clientInformation.sock_, IPPROTO_TCP, TCP_NODELAY, (void*)&flag, sizeof(int));

        g_clientInformation.information_.sin_family = AF_INET;
        g_clientInformation.information_.sin_port = htons(port);
        g_clientInformation.information_.sin_addr.s_addr = ip;

        if(connect(g_clientInformation.sock_, reinterpret_cast<sockaddr*>(&g_clientInformation.information_), sizeof(g_clientInformation.information_)) == -1)
            throw runtime_error(string("Failed to call conenct").append(strerror(errno)));

        return true;
    }catch(const exception& e) {
        LOG(WARNING)<<"[Connection] "<<e.what()<<endl;
    }

    return false;
}

void CommThread() {
    char buf[BUFSIZE]{};
    int ret{};

    try {
        while(1) {
            //switch ((ret = recv(g_clientInformation.sock_, buf, BUFSIZE-1, 0))) {
	    switch ((ret = recv(g_clientInformation.sock_, buf, sizeof(buf), 0))) {
            case -1:
                sprintf(buf, "Terminated : %s \n", strerror(errno));
                throw runtime_error(buf);
                break;
            case 0:
                throw runtime_error("Disconnected \n");
                break;
            default:
		buf[ret] = '\0';
                printf("[Server] %s \n", buf);
                fflush(stdout);
                break;
            }
        }
    }catch(const exception& e) {
        LOG(WARNING)<<"[CommThread] "<<e.what()<<endl;
    }

    close(g_clientInformation.sock_);
    g_clientInformation.sock_ = -1;

    exit(0);
}

void Clean() {
    cout<<"\n===Client Close==="<<endl;

    if(g_clientInformation.sock_ != -1)
        close(g_clientInformation.sock_);
}

void SignalHandler(int signal) {
    Clean();

    exit(0);
}
