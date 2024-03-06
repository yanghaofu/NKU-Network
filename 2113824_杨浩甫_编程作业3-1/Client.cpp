#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <iostream>
#include <fstream>
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
        cout << "checksum=" << this->checksum << ", len=" << this->len << endl;
    }
};

SOCKADDR_IN serveraddr, clientaddr;
SOCKET client;
char filepath[20];
ifstream in;
int filelen;
int messagenum;
double loss_rate =0.1;
double time_wait = 200;

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

// �������ֽ�������
void Connect() {
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
    cout << "Client: �������ֽ������ӳɹ�" << endl;



}


// �Ĵλ��ֹر�����
void Close() {
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
    closesocket(client);
    WSACleanup();
}


// �����ļ���
void SendName(const char* filepath) {
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
        if (ackMsg.isACK() && ackMsg.ack == startMsg.seq ) {
            cout << "Client: �յ���������ȷ��ACK����ʼ�����ļ�����" << endl;
            return SendFile(filepath);
        }
    }
}

// �����ļ�����
void SendFile(const char* filepath) {
    int iMode = 1; //1����������0������
    ioctlsocket(client, FIONBIO, (u_long FAR*) & iMode);//����������
    ifstream inFile(filepath, ios::in | ios::binary);
    if (!inFile.is_open()) {
        cout << "Client: ���ļ�ʧ��" << endl;
        return;
    }

    char buffer[1024];
    int seq = 1;
    int num = 0;

    while (filelen) {
        num++;

        message msg;
        msg.seq = seq++;
        
        msg.len = min(filelen, 1024);
        in.read(msg.data, msg.len);
        filelen -= msg.len;

        msg.num = num;

        msg.setchecksum();
        sendmessage(client, serveraddr, msg);

        // �ȴ���������ȷ�� ACK
        int start = clock();
        int end;
        while (true) {

            message ackMsg = recvmessage(client, serveraddr);
            end = clock();
            if (end - start > time_wait) {
                cout << "Client: ��ʱ�ش�" << endl;
                msg.setchecksum();
                sendmessage(client, serveraddr, msg);
                start = clock();
            }

            if (!ackMsg.isEXT()) {
                continue;
            }

            if (ackMsg.isACK() && ackMsg.ack == msg.seq+1 ) {
                cout << "Client: �յ�ackΪ" << ackMsg.ack << "�����ݰ�" << endl;
                break;
            }
        }
    }

    inFile.close();

    // �����ļ�������־
    message endMsg;
    endMsg.setEND();
    endMsg.setchecksum();
    sendmessage(client, serveraddr, endMsg);

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
    cin >> order ;
    cout << endl;
    // ����������ֽ�������
    if (strcmp(order, "start") == 0) {
        Connect();
    }
    else
        return 0;


    // �����ļ�
    //const char* filepath = "example.txt";  // �ļ�·��
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
