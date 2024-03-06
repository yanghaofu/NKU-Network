#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <iostream>
#include <thread>
#include <string>
#include <cstring>
#pragma comment (lib, "ws2_32.lib")
using namespace std;

const int PORT = 8000;
#define IP "127.0.0.1"
#define MaxBufSize 1024

// 用于接收服务器消息的线程函数
void ReceiveMessages(SOCKET clientSocket) {
    char buffer[MaxBufSize];
    int bytesReceived;

    while (true) {
        bytesReceived = recv(clientSocket, buffer, MaxBufSize, 0);
        if (bytesReceived == SOCKET_ERROR || bytesReceived == 0) {
            cout << "服务器连接关闭。" << endl;
            break;
        }
        else {
            buffer[bytesReceived] = '\0'; // 添加字符串终止符
            cout << buffer << endl;  
            cout << endl;
        }
    }
}

int main() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cerr << "WSAStartup 初始化失败： " << result << endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "套接字创建失败" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.S_un.S_addr = inet_addr(IP);

    //与服务器建立连接
    result = connect(clientSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress));
    if (result == SOCKET_ERROR) {
        cerr << "服务器连接失败" << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "[system]: Connected to the server. " << endl;
    cout << "[system]: 输入 exit 退出聊天室" << endl;
    cout << "[system]: Enter your username: " ;

    string username;
    getline(cin, username);
    send(clientSocket, username.c_str(), username.size(), 0);

    cout << "-------------------------------------------" << endl;

    // 启动一个新线程用于接收服务器消息
    thread receiveThread(ReceiveMessages, clientSocket);

    // 主线程用于发送消息给服务器
    string message;
    while (true) {
        getline(cin, message);
        if (message == "exit") {
            break;
        }
        send(clientSocket, message.c_str(), message.size(), 0);
        cout << endl;
    }

    // 关闭客户端套接字和等待接收线程结束
    closesocket(clientSocket);
    receiveThread.join();

    // 清理WinSock
    WSACleanup();

    return 0;
}
