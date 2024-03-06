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
// ��SendFile����������һ���µĺ������ڳ�ʱ�ش�
//void timeoutResend(SOCKET socket, SOCKADDR_IN& serveraddr, int filelen, std::chrono::time_point<std::chrono::system_clock>* packetTimes, double timeout, double* bufferTime);

bool resend = 0;

// ���Ľṹ�嶨��
struct message
{
#pragma pack(1)
    u_long flag{};//ռ���ĸ��ֽ�
    u_short seq{};//���к�
    u_short ack{};//ȷ�Ϻ�
    u_long len{};//���ݲ��ֳ���
    u_long num{}; //���͵���Ϣ����������
    u_short checksum{};//У���
    char data[1024]{};//���ݳ���
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
        // ����Ϣ�ṹ����Ϊu_short���飬��������16λ�����ĺ�
        for (int i = 0; i < words; i++) {
            sum += temp[i];
        }

        // ������ܵ����������16λ�ؾ���16λ
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        // ��У�����Ϊ�͵İ�λȡ��
        this->checksum = static_cast<u_short>(~sum);
    }

    bool corrupt() {
        u_short* temp = reinterpret_cast<u_short*>(this);
        int words = sizeof(message) / sizeof(u_short);

        u_long sum = 0;
        // ����Ϣ�ṹ����Ϊu_short���飬��������16λ�����ĺ�
        for (int i = 0; i < words; i++) {
            sum += temp[i];
        }

        // ������ܵ����������16λ�ؾ���16λ
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        // �ж���Ϣ�Ƿ��𻵣�У��ͺ���Ϣ�е�У����ֶ���ӣ����������0xFFFF���ʾ��
        return (checksum + static_cast<u_short>(sum)) != 0xFFFF;
    }


    void output() {
        //cout << "Client: �յ�seqΪ" << this->seq << "�����ݰ�" << endl;
        cout << "checksum=" << this->checksum << ", len=" << this->len << endl;
    }
};

void SetColor(int fore = 7, int back = 0) {
    unsigned char m_color = fore;
    m_color += (back << 4);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), m_color);
    return;
}

const int WINDOW_SIZE = 16; // ���ڴ�С
message window[WINDOW_SIZE]; // ���ʹ���
int base = 0; // ���ʹ��ڵĻ����
int nextSeqNum = 0; // ��һ�������͵����
bool acked[WINDOW_SIZE] = { false }; // ��Ƿ��ʹ����ڵ���Ϣ�Ƿ�ȷ��
// ���峬ʱʱ��
std::chrono::milliseconds timeout(800); // 800����ʾ����ʱʱ��
// ��¼ÿ�����ݰ��ķ���ʱ��
int packetTimes[WINDOW_SIZE];


SOCKADDR_IN serveraddr, clientaddr;
SOCKET client;
char filepath[20];
ifstream in;
int filelen;
int messagenum;
double loss_rate = 0.1;
double time_wait = 200;

std::atomic<bool> isRunning{true};//�̱߳�־

message recvmessage(SOCKET socket, SOCKADDR_IN& serverAddress) {
    message msg;

    int len = sizeof(SOCKADDR);
    int bytesReceived = recvfrom(socket, (char*)&msg, BUFFER, 0, (SOCKADDR*)&clientaddr, &len);
    if (bytesReceived == SOCKET_ERROR) {
        //cout << "Client: �������ݰ�ʧ��" << endl;
        return message(); // ���ؿ���Ϣ
    }
    //cout << "Client: �������ݰ��ɹ�" << endl;
    //cout << msg.isACK() << msg.isSYN() << endl;
    return msg;
}

void sendmessage(SOCKET socket, const SOCKADDR_IN& clientaddr, message msg) {
    msg.setEXT();
    msg.setchecksum();


    int bytesSent = sendto(socket, (char*)&msg, BUFFER, 0, (SOCKADDR*)&clientaddr, sizeof(SOCKADDR));
    if (bytesSent == SOCKET_ERROR) {
        cout << "Client: �������ݰ�ʧ��" << endl;
    }
}


// �������ݺ���
void sendData(SOCKET socket, const SOCKADDR_IN& clientaddr, message msg) {
    msg.setchecksum();
    if (nextSeqNum < base + WINDOW_SIZE) { // �������пռ�
        window[nextSeqNum % WINDOW_SIZE] = msg;
        cout << "window[" << nextSeqNum % WINDOW_SIZE << "].seq:" << window[nextSeqNum % WINDOW_SIZE].seq << endl;
        sendmessage(socket, serveraddr, msg);
        //nextSeqNum++;
    }
}

// �������ֽ�������
void Connect() {
    SetColor(9, 0); // ��ɫ�ı�����ɫ����
    int iMode = 1; //1����������0������
    ioctlsocket(client, FIONBIO, (u_long FAR*) & iMode);//����������
    message synMsg;
    synMsg.setSYN();
    synMsg.seq = 1;  // �ͻ��˳�ʼ���к�
    synMsg.len = 0;

    sendmessage(client, serveraddr, synMsg);
    cout << "Client: ����SYN" << endl;

    // �ȴ���������ȷ����Ϣ
    message ackMsg;
    int count = 0;
    while (true) {

        ackMsg = recvmessage(client, serveraddr);

        if (!ackMsg.isEXT()) {
            Sleep(50);
            if (count >= time_wait) {
                cout << "Client: �ȴ�ʱ��̫�����˳�����" << endl;
                closesocket(client);
                WSACleanup();
                exit(EXIT_FAILURE);
            }
            continue;
        }

        if (ackMsg.isACK() && ackMsg.isSYN() && ackMsg.ack == synMsg.seq + 1) {
            cout << "Client: �յ���������ȷ��SYN" << endl;
            break;
        }

        count++;
    }


    message sendMsg;
    sendMsg.setACK();
    sendMsg.ack = ackMsg.seq + 1;
    sendmessage(client, serveraddr, sendMsg);
    cout << "Client: ����ȷ��ACK" << endl;
    cout << "Client: �������ֽ������ӳɹ�" << endl << endl;




}


// �Ĵλ��ֹر�����
void Close() {
    SetColor(9, 0); // ��ɫ�ı�����ɫ����

    message finMsg;
    finMsg.setFIN();
    finMsg.setchecksum();

    sendmessage(client, serveraddr, finMsg);
    cout << "Client: ����FIN" << endl;


    // �ȴ�����������FIN
    while (true) {
        message finMsg = recvmessage(client, serveraddr);
        if (finMsg.isFIN()) {
            cout << "Client: �յ���������FIN" << endl;
            break;
        }
    }

    // ����ȷ��ACK
    message ackMsg;
    ackMsg.setACK();
    ackMsg.ack = finMsg.seq + 1;
    sendmessage(client, serveraddr, ackMsg);
    cout << "Client: ����ȷ��ACK" << endl;

    cout << "Client: �Ĵλ��ֹر����ӳɹ�" << endl;
    SetColor(15, 0);
    closesocket(client);
    WSACleanup();
}


// �����ļ���
void SendName(const char* filepath) {
    SetColor(7, 0);
    message startMsg;
    startMsg.setSTART();
    startMsg.len = strlen(filepath);
    strncpy(startMsg.data, filepath, startMsg.len);
    startMsg.num = messagenum; // ���ݰ�
    startMsg.setchecksum();
    //cout << startMsg.data << endl;
    Sleep(time_wait);
    sendmessage(client, serveraddr, startMsg);

    // �ȴ���������ȷ�� ACK
    while (true) {
        message ackMsg = recvmessage(client, serveraddr);
        if (!ackMsg.isEXT()) {
            continue;
        }
        //cout << "ackMsg.isACK():" << ackMsg.ack << endl;
        if (ackMsg.isACK() && ackMsg.ack == startMsg.seq) {
            cout << "Client: �յ���������ȷ��ACK����ʼ�����ļ�����" << endl;
            return SendFile(filepath);
        }
    }
}


std::mutex mtx;
// ������� ACK �ĺ���  �ۼ�ȷ��
void receiveACK() {
    int iMode = 0; //1����������0������
    ioctlsocket(client, FIONBIO, (u_long FAR*) & iMode);//����������
    while (isRunning) {
        message ackMsg = recvmessage(client, serveraddr);
        if (ackMsg.isACK()) {
            std::lock_guard<std::mutex> lock(mtx); // �ڴ�������Զ���������������ʱ�Զ��ͷ�
            int ackNum = ackMsg.ack;
            while (base < ackNum) {
                acked[base % WINDOW_SIZE] = false;
                (base)++;
            }

            if (ackMsg.isRE()) {
                for (int i = base; i < nextSeqNum; ++i) {
                    int index = i % WINDOW_SIZE;
                    if (acked[index]) {
                        SetColor(11, 0); // ��ɫ�ı�����ɫ����
                        cout << "Client: ȷ���ش�" << window[index].seq << "�����ݰ�" << endl;
                        sendmessage(client, serveraddr, window[index]);
                        packetTimes[index] = clock(); // ���¼�¼����ʱ��
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
    int iMode = 1; //1����������0������
    ioctlsocket(client, FIONBIO, (u_long FAR*) & iMode);//����������
    ifstream inFile(filepath, ios::in | ios::binary);
    if (!inFile.is_open()) {
        cout << "Client: ���ļ�ʧ��" << endl;
        return;
    }

    char buffer[1024];
    int num = 0;

    //���������߳�
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
            cout << "Client:����" << msg.seq << "�����ݰ�" << endl;
            cout << "Client:��ʱ�ѷ��������Ҷ��ڴ��ڵ�λ��: " << nextSeqNum % WINDOW_SIZE << endl;
            msg.output();
            cout << endl;

            acked[nextSeqNum % WINDOW_SIZE] = true;
            packetTimes[nextSeqNum % WINDOW_SIZE] = clock(); // ��¼����ʱ��
            nextSeqNum++;
        }


        // ��ʱ�ش����
        if ((clock() - packetTimes[base % WINDOW_SIZE]) > timeout.count()) {
            // ��ʱ�ش�
            SetColor(11, 0); // ��ɫ�ı�����ɫ����
            for (int i = base; i < nextSeqNum; ++i) {
                int index = i % WINDOW_SIZE;
                if (acked[index]) {
                    cout << "Client: ��ʱ�ش�" << window[index].seq << "�����ݰ�" << endl;
                    sendmessage(client, serveraddr, window[index]);
                    packetTimes[index] = clock(); // ���¼�¼����ʱ��
                    //acked[index] = false;
                }
            }
            cout << endl;
        }
    }

    inFile.close();
    int end = clock();
    cout << "�ɹ������ļ���" << endl;
    double endtime = (double)(end - start) / CLOCKS_PER_SEC;
    cout << endl;
    cout << "������ʱ��: " << endtime << "s" << endl;
    cout << "������: " << (double)(messagenum)*BUFFER * 8 / endtime / 1024 << "kbps" << endl;
    cout << "***************************************************" << endl;
    SetColor(13, 0);
    // �����߳�
    isRunning = false;

    //if (receiveAckThread.joinable()) {
    //    isRunning = false; // �����˳���־
    //    receiveAckThread.join(); // �ȴ��߳�ִ�����
    //    receiveAckThread.detach(); // �����߳�
    //}


    message endMsg;
    endMsg.setEND();
    endMsg.setchecksum();
    sendmessage(client, serveraddr, endMsg);


    return;
    // �ȴ���������ȷ�� ACK
    while (true) {
        message ackMsg = recvmessage(client, serveraddr);
        if (!ackMsg.isEXT()) {
            continue;
        }

        if (ackMsg.isACK() && ackMsg.ack == endMsg.seq + 1) {
            cout << "Client: �ļ����ݷ�����ɣ��ȴ���������Ӧ" << endl;
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
        cout << "�׽��ֳ�ʼ��ʧ�ܣ�" << err << endl;
        return -1;
    }

    client = socket(AF_INET, SOCK_DGRAM, 0);
    clientaddr.sin_addr.S_un.S_addr = inet_addr(CLIENT_IP);
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_port = htons(CLIENT_PORT);

    serveraddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVER_PORT);

    cout << "�ͻ��������������ӷ�����..." << endl;

    cout << "����'start'��ʼ���ӷ�����" << endl;
    char order[255];
    cin >> order;
    cout << endl;
    // ����������ֽ�������
    if (strcmp(order, "start") == 0) {
        Connect();
    }
    else
        return 0;

    SetColor(13, 0);
    cout << "��ʱ����Ϊ��С��" << WINDOW_SIZE << endl << endl;


    // �����ļ�
    //const char* filepath = "example.txt";  // �ļ�·��
    SetColor(15, 0);
    while (true) {
        char filename[255];
        cout << "������Ҫ������ļ��� (1.jpg, 2.jpg, 3.jpg, helloworld.txt)������ 'exit' �˳���" << endl;
        cin >> filename;

        if (strcmp(filename, "exit") == 0) {
            break;
        }

        ifstream file(filename, ios::in | ios::binary);
        if (!file.is_open()) {
            cout << "Client: �޷����ļ���" << filename << endl;
            continue;
        }

        strcpy(filepath, filename);
        in.open(filepath, ifstream::in | ios::binary);
        in.seekg(0, std::ios_base::end);
        filelen = in.tellg();
        messagenum = (filelen + 1023) / 1024; // ���������ݰ���

        in.seekg(0, std::ios_base::beg);
        cout << "Client: �ļ���СΪ" << filelen << "Bytes,�ܹ���" << messagenum << "�����ݰ�" << endl;

        int start = clock();
        SendName(filepath);
        //SendFile(filepath);
        int end = clock();
        cout << "�ɹ������ļ���" << endl;
        double endtime = (double)(end - start) / CLOCKS_PER_SEC;
        cout << endl;
        cout << "������ʱ��: " << endtime << "s" << endl;
        cout << "������: " << (double)(messagenum)*BUFFER * 8 / endtime / 1024 << "kbps" << endl;
        cout << "***************************************************" << endl;
        in.close();

    }
    // �ر�����
    cout << endl;
    Close();

    closesocket(client);
    WSACleanup();
    return 0;
}
