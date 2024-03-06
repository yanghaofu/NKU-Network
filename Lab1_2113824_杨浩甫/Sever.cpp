#define _CRT_SECURE_NO_WARNINGS

#include <WinSock2.h>
#include <string.h>
#include <iostream>
#include <thread>  // 添加线程支持
#include <vector>  // 使用std::vector来保存客户端线程
#include <mutex>   // 用于多线程安全
#include <ctime>  // 用于处理时间

#pragma comment (lib, "ws2_32.lib")
using namespace std;

const int PORT = 8000;
#define IP "127.0.0.1"
#define MaxClient 10
#define MaxBufSize 1024
int num = 0;

struct ClientInfo {
    SOCKET clientSocket;
    string username;
};

vector<ClientInfo> connectedClients;

// 用于处理客户端连接的线程函数
void HandleClient(SOCKET clientSocket) {
    char buffer[MaxBufSize];
    string username;
    int bytesReceived;

    // 接收客户端用户名
    bytesReceived = recv(clientSocket, buffer, MaxBufSize, 0);
    if (bytesReceived == SOCKET_ERROR) {
        cerr << "接受用户名失败" << endl;
    }
    else {
        // 确保接收到的数据以 null 终止
        buffer[bytesReceived] = '\0';
        username = buffer;
        cout << "[system]: User '" << username << "' connected  ︿(￣︶￣)︿" << endl;
    }


    // 向其他客户端广播用户连接消息
    for (auto it = connectedClients.begin(); it != connectedClients.end(); ++it) {
        const ClientInfo& client = *it;

        if (client.clientSocket != clientSocket) {
            string message = "-------- '" + username + "' 加入了聊天室 -------";
            send(client.clientSocket, message.c_str(), message.size(), 0);
        }
    }

    // 添加新用户到已连接客户端列表
    connectedClients.push_back({ clientSocket, username });

    while (true) {
        bytesReceived = recv(clientSocket, buffer, MaxBufSize, 0);
        if (bytesReceived == SOCKET_ERROR || bytesReceived == 0) {
            // 客户端断开连接
            auto it = connectedClients.begin();
            while (it != connectedClients.end()) {
                if (it->clientSocket == clientSocket) {
                    cout << "[system]: User '" << it->username << "' disconnected  ヾ(￣▽￣)Bye~Bye~" << endl;
                    connectedClients.erase(it);
                    break;
                }
                ++it;
            }

            // 向其他客户端广播用户离开消息 
            for (const ClientInfo& client : connectedClients) {
                string message = "------- '" + username + "' 退出了聊天室 -------";
                send(client.clientSocket, message.c_str(), message.size(), 0);
            }
            
            // 关闭套接字和线程
            closesocket(clientSocket);
            break;
        }
        else {
            // 处理接收到的消息
            string message(buffer, bytesReceived);
            string broadcastMessage = "[" + username + "]: " + message;
            //cout << broadcastMessage << endl;
            // 向其他客户端广播消息

            // 获取当前时间并格式化
            time_t rawtime;
            struct tm* timeinfo;
            char buffer[80];
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

            // 拼接时间和消息
            string fullMessage = string(buffer) + " " + broadcastMessage;
            cout << fullMessage << endl;

            for (const ClientInfo& client : connectedClients) {
                if (client.clientSocket != clientSocket) {
                    send(client.clientSocket, fullMessage.c_str(), fullMessage.size(), 0);
                }
            }
        }
    }
}

int main() {
    //WinSock库初始化，确保网络库的正确初始化和资源分配。
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cout << "WSAStartup 初始化失败原因: " << result << endl;
        return 1;
    }

    //创建套接字，IPv4，流式套接字，自动选择协议
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        cout << "套接字创建失败" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;//服务端地址
    serverAddress.sin_family = AF_INET;//连接方式
    serverAddress.sin_port = htons(PORT);//服务器监听端口 
    serverAddress.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//指定服务器监听的IP地址(实际就是本机地址)d。

    //绑定服务器
    result = bind(serverSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress));
    if (result == SOCKET_ERROR) {
        cout << "绑定服务器失败" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    //进入监听状态
    result = listen(serverSocket, MaxClient);
    if (result == SOCKET_ERROR) {
        cout << "进入监听失败" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "[system]:服务器正在监听 " << IP << ":" << PORT << endl;

    cout << "[system]:正在等待客户端连接o(*^＠^*)o" << endl;

    cout << "-------------------------------------------" << endl;

    vector<thread> clientThreads;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            cout << "接受客户端连接失败" << endl;
        }
        else {
            // 启动一个新线程来处理客户端
            //这一行通过emplace_back方法将一个新的线程对象添加到clientThreads容器中。
            //这个线程将执行HandleClient函数，处理与刚刚连接的客户端的通信。
            //HandleClient函数将在独立的线程中运行，这样服务器可以同时处理多个客户端的连接请求。
            clientThreads.emplace_back(HandleClient, clientSocket);
        }
    }


    closesocket(serverSocket);
    WSACleanup();

    return 0;
}