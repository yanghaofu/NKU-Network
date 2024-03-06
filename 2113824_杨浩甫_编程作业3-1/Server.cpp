#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <iostream>
#include <fstream>
using namespace std;
#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8000
#define BUFFER sizeof(message)
#define MAX_FILEPATH 255
ofstream outFile;
// 报文结构体定义
struct message
{
#pragma pack(1)
    u_long flag{};
    u_short seq{};//序列号
    u_short ack{};//确认号
    u_long len{};//数据部分长度
    u_long num{}; //发送的消息包含几个包
    u_short checksum{};//校验和
    char data[1024]{};//数据内容
#pragma pack()
    message() {
        memset(this, 0, sizeof(message));
    }
    bool isSYN() {
        return this->flag & 1;
    }
    bool isFIN() {
        return this->flag & 2;
    }
    bool isSTART() {
        return this->flag & 4;
    }
    bool isEND() {
        return this->flag & 8;
    }
    bool isACK() {
        return this->flag & 16;
    }
    bool isEXT() {
        return this->flag & 32;
    }
    bool isRE() {
        return this->flag & 64;
    }
    void setSYN() {
        this->flag |= 1;
    }
    void setFIN() {
        this->flag |= 2;
    }
    void setSTART() {
        this->flag |= 4;
    }
    void setEND() {
        this->flag |= 8;
    }
    void setACK() {
        this->flag |= 16;
    }
    void setEXT() {
        this->flag |= 32;
    }
    void setRE() {
        this->flag |= 64;
    }

    void setchecksum() {
        u_short* temp = reinterpret_cast<u_short*>(this);
        int words = sizeof(message) / sizeof(u_short);

        u_long sum = 0;
        // 将消息结构体视为u_short数组，计算所有16位整数的和
        for (int i = 0; i < words; i++) {
            sum += temp[i];
        }

        // 处理可能的溢出，将高16位回卷到低16位
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        // 将校验和设为和的按位取反
        this->checksum = static_cast<u_short>(~sum);
    }

    bool corrupt() {
        u_short* temp = reinterpret_cast<u_short*>(this);
        int words = sizeof(message) / sizeof(u_short);

        u_long sum = 0;
        // 将消息结构体视为u_short数组，计算所有16位整数的和
        for (int i = 0; i < words; i++) {
            sum += temp[i];
        }

        // 处理可能的溢出，将高16位回卷到低16位
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        // 判断消息是否损坏，校验和和消息中的校验和字段相加，如果不等于0xFFFF则表示损坏
        return (checksum + static_cast<u_short>(sum)) != 0xFFFF;
    }

    void output() {
        cout << "Server: 收到seq为" << this->seq << "的数据包" << endl;
        cout << "Server: checksum=" << this->checksum << ", len=" << this->len << endl;
    }
};

void ReceiveName(SOCKET serverSocket, SOCKADDR_IN& clientAddress);

SOCKADDR_IN serveraddr, clientaddr;
SOCKET server;
double loss_rate = 0.01;
double time_wait = 200;

message recvmessage(SOCKET socket, SOCKADDR_IN& clientaddr) {
    message msg;

    int len = sizeof(SOCKADDR);
    int bytesReceived = recvfrom(socket, (char*)&msg, BUFFER, 0, (SOCKADDR*)&clientaddr, &len);
    if (bytesReceived == SOCKET_ERROR) {
        //cout << "接收数据包失败" << endl;
        return message(); // 返回空消息
    }
    //cout << msg.data << endl;

    return msg;
}

void sendmessage(SOCKET socket, const SOCKADDR_IN& clientaddr, message msg) {
    msg.setEXT();
    msg.setchecksum();

    int bytesSent = sendto(socket, (char*)&msg, BUFFER, 0, (SOCKADDR*)&clientaddr, sizeof(SOCKADDR));
    if (bytesSent == SOCKET_ERROR) {
        cout << "发送数据包失败" << endl;
    }
}


int saveFile(const char* filepath, const char* data, int len)
{
    ofstream outFile(filepath, ios::out | ios::binary);

    if (!outFile.is_open())
    {
        return -1;  // 打开文件失败
    }

    outFile.write(data, len);
    outFile.close();


    return 0;  // 文件保存成功
}

// 建立连接
void Connect(bool &connectionEstablished) {
    message msg = recvmessage(server, clientaddr);

    if (msg.isSYN()) {
        cout << "Server: 接收到一个客户端的连接请求。" << endl;
        // 发送确认消息
        message ackMsg;
        ackMsg.setACK();
        ackMsg.setSYN();
        ackMsg.ack = msg.seq + 1;
        sendmessage(server, clientaddr, ackMsg);
        cout << "Server: 发送服务器的连接请求。" << endl;
        //ackMsg.output();
        int count = 0;
        while (true) {
            Sleep(50);
            if (count >= 10) {
                cout << "Server: 等待时间太长，退出连接" << endl;
                return ;    
            }
            message msg = recvmessage(server, clientaddr);
            if (!msg.isEXT()) {
                continue;
            }
            if (msg.isACK() && msg.ack == ackMsg.seq + 1) {
                break;
            }
            count++;
        }
        cout << "Server: 连接请求已确认，三次握手已完成。" << endl;
        connectionEstablished = true;
    }
    int iMode = 0; //1：非阻塞，0：阻塞
    ioctlsocket(server, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置
}

void Close() {
    int iMode = 1; //1：非阻塞，0：阻塞
    ioctlsocket(server, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置
    cout << endl;
    cout<< "Server: 接收到一个关闭连接请求。" << endl;
    // 四次挥手
    message finMsg;
    finMsg.setFIN();
    finMsg.setchecksum();
    sendmessage(server, clientaddr, finMsg);
    cout << "Server: 向客户端发送FIN" << endl;

    while (true) {
        message msg = recvmessage(server, clientaddr);
  
        if (msg.isACK()) {
            cout << "Server: 收到来自客户端的 ACK，关闭连接。" << endl;
            break;
        }
    }
}
void ReceiveFile(SOCKET serverSocket, SOCKADDR_IN& clientAddress, int num) {
    int iMode = 1; //1：非阻塞，0：阻塞
    ioctlsocket(server, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置
    int start = clock();
    int end;
    message msg;
    u_short expectedSeq = 1; // 期望的初始序列号
    while (1) {
        msg = recvmessage(server, clientaddr);
        int start = clock();
        int end;
        if (!msg.isEXT()) {
            continue;
        }

        if (msg.isEND()) {
            message ackMsg;
            ackMsg.setACK();
            ackMsg.ack = msg.ack + 1;
            sendmessage(server, clientaddr, ackMsg);
            cout << "Server: 接收文件成功！！" << endl << endl;
            cout << "************************************************" << endl;
            outFile.close();
            outFile.clear();
            int iMode = 0; //1：非阻塞，0：阻塞
            ioctlsocket(server, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置

            return ReceiveName(server, clientaddr);
        }

        if (!msg.isEXT()) {
            continue;
        }
        else if (msg.seq == expectedSeq&&msg.corrupt()) {
            cout << "msg.seq:" << msg.seq << "  expectedSeq:" << expectedSeq << endl;
            message ackMsg;
            ackMsg.setACK();
            ackMsg.ack = msg.seq + 1;

            //cout << "Server: 收到seq为" << msg.seq << "的数据包" << endl;
            msg.output();

            double random_value = (double)rand() / RAND_MAX;
            if (random_value < loss_rate) {
                cout << endl;
                cout << "Server: 模拟丢包，未发送数据包" << endl;
                cout << endl;
                //sendmessage(server, serveraddr, message()); // 返回空消息
                continue;
            }

            sendmessage(server, clientaddr, ackMsg);
            cout << "Server: 发送确认收到的数据包(对应的ack)" << endl;

            cout << endl;
            outFile.write(msg.data, msg.len);

            num--;
            expectedSeq++;
            continue;
        }
        
    }

}

void ReceiveName(SOCKET serverSocket, SOCKADDR_IN& clientAddress)
{
    cout << "Server: 等待文件传输请求..." << endl;
    message msg;
    int num; //需要接收数据包大小
    char receivedFilePath[MAX_FILEPATH] = { 0 };
    int count = 0;
    // 处理文件传送
    while (true)
    {
        int len = sizeof(SOCKADDR);
        msg=recvmessage(serverSocket, clientaddr);

        if (msg.isFIN()) {
            cout << "客户端准备断开连接！进入挥手模式！" << endl;
            Close();
            break;
        }
        // 处理文件名信息
        if (msg.isSTART()) {
            cout << msg.data << endl;
            memset(receivedFilePath, 0, MAX_FILEPATH);
            strncpy(receivedFilePath, msg.data, msg.len);
            cout << "Server: 接收到文件名为" << receivedFilePath << endl;

            num = msg.num;
            message ackMsg;
            ackMsg.setACK();
            ackMsg.ack = msg.seq;
            sendmessage(serverSocket, clientaddr, ackMsg);
            cout << "Server: 开始接收文件内容" << endl;

            outFile.open(receivedFilePath, ios::out | ios::binary);
            return ReceiveFile(serverSocket, clientaddr, num);
        }

        // 处理接收到的数据包
        else if (!msg.isEXT())
        {
            Sleep(50);
            if (count >= time_wait) {
                cout << "Server: 连接超时，重新接收文件名" << endl;
                count = 0;
                continue;
            }
        }
        count++;
    }
}

int main()
{
    WORD version;
    WSADATA wsaData;
    int err;

    version = MAKEWORD(2, 2);
    err = WSAStartup(version, &wsaData);

    if (err != 0)
    {
        cout << "套接字初始化失败：" << err << endl;
        return -1;
    }

    int iMode = 1; //1：非阻塞，0：阻塞
    ioctlsocket(server, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置
    server = socket(AF_INET, SOCK_DGRAM, 0);
    serveraddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVER_PORT);
    err = bind(server, (SOCKADDR*)&serveraddr, sizeof(SOCKADDR));

    if (err)
    {
        cout << "绑定端口" << SERVER_PORT << "出错：" << err << endl;
        WSACleanup();
        return -1;
    }

    cout << "服务器已启动，等待客户端连接..." << endl;

    bool connectionEstablished = false;
    //Sleep(time_wait);

    // 完成三次握手
    while (!connectionEstablished) {
        Connect(connectionEstablished);
    }

    // 接收文件
    ReceiveName(server, clientaddr);

    closesocket(server);
    WSACleanup();
    return 0;
}

