#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>

using namespace std;
#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define CLIENT_IP "127.0.0.1"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8000
#define CLIENT_PORT 8080
#define BUFFER sizeof(message)
#define MAX_FILEPATH 255

ofstream outFile;
void SendFile(const char* filepath);
// 在SendFile函数中声明一个新的函数用于超时重传
//void timeoutResend(SOCKET socket, SOCKADDR_IN& serveraddr, int filelen, std::chrono::time_point<std::chrono::system_clock>* packetTimes, double timeout, double* bufferTime);

bool resend = 0;

// 报文结构体定义
struct message
{
#pragma pack(1)
    u_long flag{};//占用四个字节
    u_short seq{};//序列号
    u_short ack{};//确认号
    u_long len{};//数据部分长度
    u_long num{}; //发送的消息包含几个包
    u_short checksum{};//校验和
    char data[1024]{};//数据长度
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
        //cout << "Client: 收到seq为" << this->seq << "的数据包" << endl;
        cout << "checksum=" << this->checksum << ", len=" << this->len << endl;
    }
};

void SetColor(int fore = 7, int back = 0) {
    unsigned char m_color = fore;
    m_color += (back << 4);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), m_color);
    return;
}

const int WINDOW_SIZE = 16; // 窗口大小
message window[WINDOW_SIZE]; // 发送窗口
int base = 0; // 发送窗口的基序号
int nextSeqNum = 0; // 下一个待发送的序号
bool acked[WINDOW_SIZE] = { false }; // 标记发送窗口内的消息是否被确认
// 定义超时时间
std::chrono::milliseconds timeout(800); // 800毫秒示例超时时间
// 记录每个数据包的发送时间
int packetTimes[WINDOW_SIZE];


SOCKADDR_IN serveraddr, clientaddr;
SOCKET client;
char filepath[20];
ifstream in;
int filelen;
int messagenum;
double loss_rate = 0.1;
double time_wait = 200;

std::atomic<bool> isRunning{true};//线程标志

message recvmessage(SOCKET socket, SOCKADDR_IN& serverAddress) {
    message msg;

    int len = sizeof(SOCKADDR);
    int bytesReceived = recvfrom(socket, (char*)&msg, BUFFER, 0, (SOCKADDR*)&clientaddr, &len);
    if (bytesReceived == SOCKET_ERROR) {
        //cout << "Client: 接收数据包失败" << endl;
        return message(); // 返回空消息
    }
    //cout << "Client: 接收数据包成功" << endl;
    //cout << msg.isACK() << msg.isSYN() << endl;
    return msg;
}

void sendmessage(SOCKET socket, const SOCKADDR_IN& clientaddr, message msg) {
    msg.setEXT();
    msg.setchecksum();


    int bytesSent = sendto(socket, (char*)&msg, BUFFER, 0, (SOCKADDR*)&clientaddr, sizeof(SOCKADDR));
    if (bytesSent == SOCKET_ERROR) {
        cout << "Client: 发送数据包失败" << endl;
    }
}


// 发送数据函数
void sendData(SOCKET socket, const SOCKADDR_IN& clientaddr, message msg) {
    msg.setchecksum();
    if (nextSeqNum < base + WINDOW_SIZE) { // 窗口内有空间
        window[nextSeqNum % WINDOW_SIZE] = msg;
        cout << "window[" << nextSeqNum % WINDOW_SIZE << "].seq:" << window[nextSeqNum % WINDOW_SIZE].seq << endl;
        sendmessage(socket, serveraddr, msg);
        //nextSeqNum++;
    }
}

// 三次握手建立连接
void Connect() {
    SetColor(9, 0); // 红色文本，蓝色背景
    int iMode = 1; //1：非阻塞，0：阻塞
    ioctlsocket(client, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置
    message synMsg;
    synMsg.setSYN();
    synMsg.seq = 1;  // 客户端初始序列号
    synMsg.len = 0;

    sendmessage(client, serveraddr, synMsg);
    cout << "Client: 发送SYN" << endl;

    // 等待服务器的确认消息
    message ackMsg;
    int count = 0;
    while (true) {

        ackMsg = recvmessage(client, serveraddr);

        if (!ackMsg.isEXT()) {
            Sleep(50);
            if (count >= time_wait) {
                cout << "Client: 等待时间太长，退出连接" << endl;
                closesocket(client);
                WSACleanup();
                exit(EXIT_FAILURE);
            }
            continue;
        }

        if (ackMsg.isACK() && ackMsg.isSYN() && ackMsg.ack == synMsg.seq + 1) {
            cout << "Client: 收到服务器的确认SYN" << endl;
            break;
        }

        count++;
    }


    message sendMsg;
    sendMsg.setACK();
    sendMsg.ack = ackMsg.seq + 1;
    sendmessage(client, serveraddr, sendMsg);
    cout << "Client: 发送确认ACK" << endl;
    cout << "Client: 三次握手建立连接成功" << endl << endl;




}


// 四次挥手关闭连接
void Close() {
    SetColor(9, 0); // 红色文本，蓝色背景

    message finMsg;
    finMsg.setFIN();
    finMsg.setchecksum();

    sendmessage(client, serveraddr, finMsg);
    cout << "Client: 发送FIN" << endl;


    // 等待服务器发送FIN
    while (true) {
        message finMsg = recvmessage(client, serveraddr);
        if (finMsg.isFIN()) {
            cout << "Client: 收到服务器的FIN" << endl;
            break;
        }
    }

    // 发送确认ACK
    message ackMsg;
    ackMsg.setACK();
    ackMsg.ack = finMsg.seq + 1;
    sendmessage(client, serveraddr, ackMsg);
    cout << "Client: 发送确认ACK" << endl;

    cout << "Client: 四次挥手关闭连接成功" << endl;
    SetColor(15, 0);
    closesocket(client);
    WSACleanup();
}


// 发送文件名
void SendName(const char* filepath) {
    SetColor(7, 0);
    message startMsg;
    startMsg.setSTART();
    startMsg.len = strlen(filepath);
    strncpy(startMsg.data, filepath, startMsg.len);
    startMsg.num = messagenum; // 数据包
    startMsg.setchecksum();
    //cout << startMsg.data << endl;
    Sleep(time_wait);
    sendmessage(client, serveraddr, startMsg);

    // 等待服务器的确认 ACK
    while (true) {
        message ackMsg = recvmessage(client, serveraddr);
        if (!ackMsg.isEXT()) {
            continue;
        }
        //cout << "ackMsg.isACK():" << ackMsg.ack << endl;
        if (ackMsg.isACK() && ackMsg.ack == startMsg.seq) {
            cout << "Client: 收到服务器的确认ACK，开始发送文件内容" << endl;
            return SendFile(filepath);
        }
    }
}


std::mutex mtx;
// 处理接收 ACK 的函数  累计确认
void receiveACK() {
    int iMode = 0; //1：非阻塞，0：阻塞
    ioctlsocket(client, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置
    while (isRunning) {
        message ackMsg = recvmessage(client, serveraddr);
        if (ackMsg.isACK()) {
            std::lock_guard<std::mutex> lock(mtx); // 在代码块中自动上锁，出作用域时自动释放
            int ackNum = ackMsg.ack;
            while (base < ackNum) {
                acked[base % WINDOW_SIZE] = false;
                (base)++;
            }

            if (ackMsg.isRE()) {
                for (int i = base; i < nextSeqNum; ++i) {
                    int index = i % WINDOW_SIZE;
                    if (acked[index]) {
                        SetColor(11, 0); // 红色文本，蓝色背景
                        cout << "Client: 确认重传" << window[index].seq << "号数据包" << endl;
                        sendmessage(client, serveraddr, window[index]);
                        packetTimes[index] = clock(); // 重新记录发送时间
                        //acked[index] = false;
                    }
                }
                continue;
            }
        }
        if (ackMsg.isEND()) {
            break;
        }
    }
    return;
}

void SendFile(const char* filepath) {
    SetColor(15, 0);
    int iMode = 1; //1：非阻塞，0：阻塞
    ioctlsocket(client, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置
    ifstream inFile(filepath, ios::in | ios::binary);
    if (!inFile.is_open()) {
        cout << "Client: 打开文件失败" << endl;
        return;
    }

    char buffer[1024];
    int num = 0;

    //启动接收线程
    std::thread receiveAckThread(receiveACK);
    

    for (int i = 0; i < WINDOW_SIZE; ++i) {
        packetTimes[i] = clock();
    }
    int start = clock();
    while (filelen || base != nextSeqNum) {
        if (nextSeqNum < base + WINDOW_SIZE && filelen > 0) {
            std::lock_guard<std::mutex> lock(mtx);
            SetColor(15, 0);
            message msg;
            msg.seq = nextSeqNum;
            msg.len = min(filelen, 1024);
            inFile.read(msg.data, msg.len);
            filelen -= msg.len;
            msg.setchecksum();
            sendData(client, serveraddr, msg);
            cout << "Client:发出" << msg.seq << "号数据包" << endl;
            cout << "Client:此时已发送数据右端在窗口的位置: " << nextSeqNum % WINDOW_SIZE << endl;
            msg.output();
            cout << endl;

            acked[nextSeqNum % WINDOW_SIZE] = true;
            packetTimes[nextSeqNum % WINDOW_SIZE] = clock(); // 记录发送时间
            nextSeqNum++;
        }


        // 超时重传检查
        if ((clock() - packetTimes[base % WINDOW_SIZE]) > timeout.count()) {
            // 超时重传
            SetColor(11, 0); // 红色文本，蓝色背景
            for (int i = base; i < nextSeqNum; ++i) {
                int index = i % WINDOW_SIZE;
                if (acked[index]) {
                    cout << "Client: 超时重传" << window[index].seq << "号数据包" << endl;
                    sendmessage(client, serveraddr, window[index]);
                    packetTimes[index] = clock(); // 重新记录发送时间
                    //acked[index] = false;
                }
            }
            cout << endl;
        }
    }

    inFile.close();
    int end = clock();
    cout << "成功发送文件！" << endl;
    double endtime = (double)(end - start) / CLOCKS_PER_SEC;
    cout << endl;
    cout << "传输总时间: " << endtime << "s" << endl;
    cout << "吞吐率: " << (double)(messagenum)*BUFFER * 8 / endtime / 1024 << "kbps" << endl;
    cout << "***************************************************" << endl;
    SetColor(13, 0);
    // 结束线程
    isRunning = false;

    //if (receiveAckThread.joinable()) {
    //    isRunning = false; // 设置退出标志
    //    receiveAckThread.join(); // 等待线程执行完成
    //    receiveAckThread.detach(); // 分离线程
    //}


    message endMsg;
    endMsg.setEND();
    endMsg.setchecksum();
    sendmessage(client, serveraddr, endMsg);


    return;
    // 等待服务器的确认 ACK
    while (true) {
        message ackMsg = recvmessage(client, serveraddr);
        if (!ackMsg.isEXT()) {
            continue;
        }

        if (ackMsg.isACK() && ackMsg.ack == endMsg.seq + 1) {
            cout << "Client: 文件内容发送完成，等待服务器响应" << endl;
            break;
        }
    }
    SetColor(15, 0);
    //Close();
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

    client = socket(AF_INET, SOCK_DGRAM, 0);
    clientaddr.sin_addr.S_un.S_addr = inet_addr(CLIENT_IP);
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_port = htons(CLIENT_PORT);

    serveraddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVER_PORT);

    cout << "客户端已启动，连接服务器..." << endl;

    cout << "输入'start'开始连接服务器" << endl;
    char order[255];
    cin >> order;
    cout << endl;
    // 完成三次握手建立连接
    if (strcmp(order, "start") == 0) {
        Connect();
    }
    else
        return 0;

    SetColor(13, 0);
    cout << "此时窗口为大小：" << WINDOW_SIZE << endl << endl;


    // 发送文件
    //const char* filepath = "example.txt";  // 文件路径
    SetColor(15, 0);
    while (true) {
        char filename[255];
        cout << "请输入要传输的文件名 (1.jpg, 2.jpg, 3.jpg, helloworld.txt)，输入 'exit' 退出：" << endl;
        cin >> filename;

        if (strcmp(filename, "exit") == 0) {
            break;
        }

        ifstream file(filename, ios::in | ios::binary);
        if (!file.is_open()) {
            cout << "Client: 无法打开文件：" << filename << endl;
            continue;
        }

        strcpy(filepath, filename);
        in.open(filepath, ifstream::in | ios::binary);
        in.seekg(0, std::ios_base::end);
        filelen = in.tellg();
        messagenum = (filelen + 1023) / 1024; // 计算总数据包数

        in.seekg(0, std::ios_base::beg);
        cout << "Client: 文件大小为" << filelen << "Bytes,总共有" << messagenum << "个数据包" << endl;

        int start = clock();
        SendName(filepath);
        //SendFile(filepath);
        int end = clock();
        cout << "成功发送文件！" << endl;
        double endtime = (double)(end - start) / CLOCKS_PER_SEC;
        cout << endl;
        cout << "传输总时间: " << endtime << "s" << endl;
        cout << "吞吐率: " << (double)(messagenum)*BUFFER * 8 / endtime / 1024 << "kbps" << endl;
        cout << "***************************************************" << endl;
        in.close();

    }
    // 关闭连接
    cout << endl;
    Close();

    closesocket(client);
    WSACleanup();
    return 0;
}
