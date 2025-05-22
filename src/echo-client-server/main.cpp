#include <iostream>
#include <cstring>
#include <string>
#include <exception>
#include <list>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>


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
void ConnectionManagerThread(SocketInformation serverInformation, list<SocketInformation>& clients, const uint8_t mode);
//static void CommThread(SocketInformation* clientInformationPtr, const uint8_t mode);
void CommThread(SocketInformation& clientInformation, const uint8_t mode);

mutex g_mtx{};
condition_variable g_cv{};

int main(int argc, char* argv[]) {
    uint8_t mod{};
    if(!(mod = parse(argc, argv))) return -1;

    uint16_t port = stoi(argv[1]);

    thread connectionManager{};
    SocketInformation serverInfomation{};
    list<SocketInformation> clients{};

    try {
        if((serverInfomation.sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) throw runtime_error("Failed to call socket");

        int option = 1;

        if (setsockopt(serverInfomation.sock_, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1) throw runtime_error("Failed to call setsockopt");

        serverInfomation.information_.sin_family = AF_INET;
        serverInfomation.information_.sin_port = htons(port);
        serverInfomation.information_.sin_addr.s_addr = INADDR_ANY;

        if(bind(serverInfomation.sock_, reinterpret_cast<sockaddr*>(&serverInfomation.information_), sizeof(serverInfomation.information_)))
            throw runtime_error("Failed to call bind");

        if(listen(serverInfomation.sock_, SOMAXCONN)) throw runtime_error("Failed to call listen");

        /*while(1) {
            list<SocketInformation> clients{};
            uint32_t informationLen = sizeof(SocketInformation::information_);

            while(1) {
                SocketInformation tmp{};

                tmp.information_.sin_family = AF_INET;

                if((tmp.sock_ = accept(serverInfomation.sock_, reinterpret_cast<sockaddr*>(&tmp.information_), &informationLen)) == -1)
                    runtime_error("Fialed to call accept");

                clients.push_back(tmp);
                clients.remove_if([](SocketInformation si) {
                    if(si.sock_ == NULL) return true;
                    return false;
                });

                thread t(CommThread, &clients.back(), mod);
                t.detach();
            }
        }*/

        connectionManager = thread(ConnectionManagerThread, serverInfomation, std::ref(clients), mod);

        string input;

        while(1) {
            cout<<"===CONTROLLER==="<<endl;
            cout<<"INPUT : "; getline(cin, input); //cin>>input;

            if(input.find("end") != -1) break;

            unique_lock<mutex> lck(g_mtx);
            for(const SocketInformation& c : clients)
                if(send(c.sock_, input.c_str(), input.length(), 0) < 0) throw runtime_error("Failed to call send");
	    break;
        }
    }catch(const exception& e) {
        LOG(WARNING)<<"[main] "<<e.what()<<endl;
        LOG(WARNING)<<"ERROR : "<<strerror(errno)<<endl;
    }

    if(serverInfomation.sock_ != NULL) close(serverInfomation.sock_);
    if(connectionManager.joinable()) connectionManager.join();
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

void ConnectionManagerThread(SocketInformation serverInformation, list<SocketInformation>& clients, const uint8_t mode) {
    list<thread> threadHandles{};

    uint32_t informationLen = sizeof(SocketInformation::information_);

    try {
        while(1) {
            SocketInformation tmp{};

            tmp.information_.sin_family = AF_INET;

            if((tmp.sock_ = accept(serverInformation.sock_, reinterpret_cast<sockaddr*>(&serverInformation.information_), &informationLen)) == -1)
                throw runtime_error("Failed to call accept");

            unique_lock lck(g_mtx);
            clients.push_back(tmp);
            clients.remove_if([](SocketInformation si) {
                if(si.sock_ == NULL) return true;
                return false;
            });

            threadHandles.push_back(thread(CommThread, ref(clients.back()), mode));
            lck.unlock();
            //thread t(CommThread, ref(clients.back()), mode);
            //t.detach();
        }
    }catch(const exception& e) {
        LOG(WARNING)<<"[ConnectionThread] "<<e.what()<<endl;
    }

    for(const SocketInformation& c : clients)
        if(c.sock_ != NULL) close(c.sock_);

    for(thread& t : threadHandles)
       if(t.joinable()) t.join();
}

void CommThread(SocketInformation& clientInformation, const uint8_t mod) {
    //SocketInformation* sockInfo = reinterpret_cast<SocketInformation*>(arg);

    char ip[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &clientInformation.information_.sin_addr, ip, sizeof(ip));

    char buf[BUFSIZE]{};

    bool echo = mod & mode::echo;
    bool broadcast = mod & mode::broadcast;

    try {
        //if(clientInformation == nullptr) throw runtime_error("Failed to convert SocketInformation");

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
        }

    }catch(const exception& e) {
        LOG(WARNING)<<"[CommThread] "<<e.what()<<endl;
    }

    close(clientInformation.sock_);
    clientInformation.sock_ = NULL;
}
