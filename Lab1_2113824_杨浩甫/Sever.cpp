#define _CRT_SECURE_NO_WARNINGS

#include <WinSock2.h>
#include <string.h>
#include <iostream>
#include <thread>  // ����߳�֧��
#include <vector>  // ʹ��std::vector������ͻ����߳�
#include <mutex>   // ���ڶ��̰߳�ȫ
#include <ctime>  // ���ڴ���ʱ��

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

// ���ڴ���ͻ������ӵ��̺߳���
void HandleClient(SOCKET clientSocket) {
    char buffer[MaxBufSize];
    string username;
    int bytesReceived;

    // ���տͻ����û���
    bytesReceived = recv(clientSocket, buffer, MaxBufSize, 0);
    if (bytesReceived == SOCKET_ERROR) {
        cerr << "�����û���ʧ��" << endl;
    }
    else {
        // ȷ�����յ��������� null ��ֹ
        buffer[bytesReceived] = '\0';
        username = buffer;
        cout << "[system]: User '" << username << "' connected  ��(�����)��" << endl;
    }


    // �������ͻ��˹㲥�û�������Ϣ
    for (auto it = connectedClients.begin(); it != connectedClients.end(); ++it) {
        const ClientInfo& client = *it;

        if (client.clientSocket != clientSocket) {
            string message = "-------- '" + username + "' ������������ -------";
            send(client.clientSocket, message.c_str(), message.size(), 0);
        }
    }

    // ������û��������ӿͻ����б�
    connectedClients.push_back({ clientSocket, username });

    while (true) {
        bytesReceived = recv(clientSocket, buffer, MaxBufSize, 0);
        if (bytesReceived == SOCKET_ERROR || bytesReceived == 0) {
            // �ͻ��˶Ͽ�����
            auto it = connectedClients.begin();
            while (it != connectedClients.end()) {
                if (it->clientSocket == clientSocket) {
                    cout << "[system]: User '" << it->username << "' disconnected  �d(������)Bye~Bye~" << endl;
                    connectedClients.erase(it);
                    break;
                }
                ++it;
            }

            // �������ͻ��˹㲥�û��뿪��Ϣ 
            for (const ClientInfo& client : connectedClients) {
                string message = "------- '" + username + "' �˳��������� -------";
                send(client.clientSocket, message.c_str(), message.size(), 0);
            }
            
            // �ر��׽��ֺ��߳�
            closesocket(clientSocket);
            break;
        }
        else {
            // ������յ�����Ϣ
            string message(buffer, bytesReceived);
            string broadcastMessage = "[" + username + "]: " + message;
            //cout << broadcastMessage << endl;
            // �������ͻ��˹㲥��Ϣ

            // ��ȡ��ǰʱ�䲢��ʽ��
            time_t rawtime;
            struct tm* timeinfo;
            char buffer[80];
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

            // ƴ��ʱ�����Ϣ
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
    //WinSock���ʼ����ȷ����������ȷ��ʼ������Դ���䡣
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cout << "WSAStartup ��ʼ��ʧ��ԭ��: " << result << endl;
        return 1;
    }

    //�����׽��֣�IPv4����ʽ�׽��֣��Զ�ѡ��Э��
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        cout << "�׽��ִ���ʧ��" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;//����˵�ַ
    serverAddress.sin_family = AF_INET;//���ӷ�ʽ
    serverAddress.sin_port = htons(PORT);//�����������˿� 
    serverAddress.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//ָ��������������IP��ַ(ʵ�ʾ��Ǳ�����ַ)d��

    //�󶨷�����
    result = bind(serverSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress));
    if (result == SOCKET_ERROR) {
        cout << "�󶨷�����ʧ��" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    //�������״̬
    result = listen(serverSocket, MaxClient);
    if (result == SOCKET_ERROR) {
        cout << "�������ʧ��" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "[system]:���������ڼ��� " << IP << ":" << PORT << endl;

    cout << "[system]:���ڵȴ��ͻ�������o(*^��^*)o" << endl;

    cout << "-------------------------------------------" << endl;

    vector<thread> clientThreads;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            cout << "���ܿͻ�������ʧ��" << endl;
        }
        else {
            // ����һ�����߳�������ͻ���
            //��һ��ͨ��emplace_back������һ���µ��̶߳�����ӵ�clientThreads�����С�
            //����߳̽�ִ��HandleClient������������ո����ӵĿͻ��˵�ͨ�š�
            //HandleClient�������ڶ������߳������У���������������ͬʱ�������ͻ��˵���������
            clientThreads.emplace_back(HandleClient, clientSocket);
        }
    }


    closesocket(serverSocket);
    WSACleanup();

    return 0;
}