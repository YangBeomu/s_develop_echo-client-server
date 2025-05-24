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
#include <arpa/inet.h>
}

#include <glog/logging.h>


#define BUFSIZE         1024

using namespace std;

namespace mode {
enum {
    port = 1,
    echo = 1 << 2,
    broadcast = 1 << 3
};
}

struct SocketInformation {
    int sock_{};
    sockaddr_in information_{};
};

void usage();
uint8_t parse(int argc,char* argv[]);
void ConnectionManagerThread(list<SocketInformation>& clients, const uint16_t port, const uint8_t mode);
void CommThread(SocketInformation& clientInformation, const uint8_t mode);
void Clean();
void SignalHandler(int signal);

mutex g_mtx{};
condition_variable g_cv{};
SocketInformation g_serverInfomation{};

int main(int argc, char* argv[]) {
    uint8_t mod{};
    if(!(mod = parse(argc, argv))) return -1;

    uint16_t port = stoi(argv[1]);

    list<SocketInformation> clients{};

    try {
        signal(SIGTERM, SignalHandler);
        signal(SIGINT, SignalHandler);

        cout<<"===Server Open==="<<endl;

        thread connectionManager(ConnectionManagerThread, std::ref(clients), port, mod);
        connectionManager.detach();

        bool broadCast = mod & mode::broadcast;

        while(1) {
            if(broadCast) {
                static string input;

                cout<<"=== Controller ==="<<endl;
                cout<<"INPUT : "; getline(cin, input); //cin>>input;

                if(input.find("end") != string::npos) break;

                unique_lock<mutex> lck(g_mtx);

                clients.remove_if([](SocketInformation si) {
                    if(si.sock_ == -1) return true;
                    return false;
                });

                for(const SocketInformation& c : clients)
                    if(send(c.sock_, input.c_str(), input.length(), 0) < 0) throw runtime_error("Failed to call send");
            }
        }
    }catch(const exception& e) {
        LOG(WARNING)<<"[main] "<<e.what()<<endl;
        LOG(WARNING)<<"ERROR : "<<strerror(errno)<<endl;
    }

    Clean();
}

void usage() {
    printf("echo-server: \n");
    printf("syntax: echo-server <port> [-e[-b]] \n");
    printf("sample: echo-server 1234 -e -b \n");
}

uint8_t parse(int argc, char* argv[]) {
    const constexpr char broadCast[] = "-b";
    const constexpr char echo[] = "-e";

    uint8_t ret{};

    if(argc < 2) {
        usage();
        return ret;
    }

    ret = mode::port;

    for(int i=2; i<argc; i++) {
        if(strncmp(argv[i], broadCast, sizeof(broadCast)) == 0) ret |= mode::broadcast;
        if(strncmp(argv[i], echo, sizeof(echo)) == 0) ret |= mode::echo;
    }

    return ret;
}

void ConnectionManagerThread(list<SocketInformation>& clients, const uint16_t port, const uint8_t mode) {
    uint32_t informationLen = sizeof(SocketInformation::information_);

    try {
        if((g_serverInfomation.sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) throw runtime_error("Failed to call socket");

        int option = 1;

        if (setsockopt(g_serverInfomation.sock_, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1) throw runtime_error("Failed to call setsockopt");

        g_serverInfomation.information_.sin_family = AF_INET;
        g_serverInfomation.information_.sin_port = htons(port);
        g_serverInfomation.information_.sin_addr.s_addr = INADDR_ANY;

        if(bind(g_serverInfomation.sock_, reinterpret_cast<sockaddr*>(&g_serverInfomation.information_), sizeof(g_serverInfomation.information_)))
            throw runtime_error("Failed to call bind");

        if(listen(g_serverInfomation.sock_, SOMAXCONN)) throw runtime_error("Failed to call listen");

        while(1) {    
            SocketInformation tmp{};

            tmp.information_.sin_family = AF_INET;

            if((tmp.sock_ = accept(g_serverInfomation.sock_, reinterpret_cast<sockaddr*>(&g_serverInfomation.information_), &informationLen)) == -1)
                throw runtime_error("Failed to call accept");

            unique_lock lck(g_mtx);
            clients.push_back(tmp);

            thread t(CommThread, ref(clients.back()), mode);
            t.detach();

            lck.unlock();
        }
    }catch(const exception& e) {
        LOG(WARNING)<<"[ConnectionThread] "<<e.what()<<endl;
    }

    for(const SocketInformation& c : clients)
        if(c.sock_ != -1) close(c.sock_);
}

void CommThread(SocketInformation& clientInformation, const uint8_t mod) {
    char ip[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &clientInformation.information_.sin_addr, ip, sizeof(ip));

    char buf[BUFSIZE]{};

    bool echo = mod & mode::echo;

    try {
        while(1) {
            switch (recv(clientInformation.sock_, buf, sizeof(buf), 0)) {
            case -1:
                sprintf(buf, "<%s> Terminated : %s \n", ip, strerror(errno));
                throw runtime_error(buf);
                break;
            case 0:
                sprintf(buf, "<%s> Disconnected \n", ip);
                throw runtime_error(buf);
                break;
            default:
                if(echo)
                    send(clientInformation.sock_, buf, sizeof(buf), 0);

                break;
            }

            memset(buf, 0, sizeof(buf));
        }



    }catch(const exception& e) {
        LOG(WARNING)<<"[CommThread] "<<e.what()<<endl;
    }

    unique_lock lck(g_mtx);
    close(clientInformation.sock_);
    clientInformation.sock_ = -1;
}

void Clean() {
    cout<<"\n===Server Close==="<<endl;

    if(g_serverInfomation.sock_ != -1)
        close(g_serverInfomation.sock_);
}

void SignalHandler(int signal) {
    Clean();

    exit(1);
}
