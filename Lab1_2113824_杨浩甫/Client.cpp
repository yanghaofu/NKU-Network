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

// ���ڽ��շ�������Ϣ���̺߳���
void ReceiveMessages(SOCKET clientSocket) {
    char buffer[MaxBufSize];
    int bytesReceived;

    while (true) {
        bytesReceived = recv(clientSocket, buffer, MaxBufSize, 0);
        if (bytesReceived == SOCKET_ERROR || bytesReceived == 0) {
            cout << "���������ӹرա�" << endl;
            break;
        }
        else {
            buffer[bytesReceived] = '\0'; // ����ַ�����ֹ��
            cout << buffer << endl;  
            cout << endl;
        }
    }
}

int main() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cerr << "WSAStartup ��ʼ��ʧ�ܣ� " << result << endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "�׽��ִ���ʧ��" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.S_un.S_addr = inet_addr(IP);

    //���������������
    result = connect(clientSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress));
    if (result == SOCKET_ERROR) {
        cerr << "����������ʧ��" << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "[system]: Connected to the server. " << endl;
    cout << "[system]: ���� exit �˳�������" << endl;
    cout << "[system]: Enter your username: " ;

    string username;
    getline(cin, username);
    send(clientSocket, username.c_str(), username.size(), 0);

    cout << "-------------------------------------------" << endl;

    // ����һ�����߳����ڽ��շ�������Ϣ
    thread receiveThread(ReceiveMessages, clientSocket);

    // ���߳����ڷ�����Ϣ��������
    string message;
    while (true) {
        getline(cin, message);
        if (message == "exit") {
            break;
        }
        send(clientSocket, message.c_str(), message.size(), 0);
        cout << endl;
    }

    // �رտͻ����׽��ֺ͵ȴ������߳̽���
    closesocket(clientSocket);
    receiveThread.join();

    // ����WinSock
    WSACleanup();

    return 0;
}
