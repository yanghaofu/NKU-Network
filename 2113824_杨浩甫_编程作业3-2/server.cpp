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


// ���Ľṹ�嶨��
struct message
{
#pragma pack(1)
    u_long flag{};
    u_short seq{};//���к�
    u_short ack{};//ȷ�Ϻ�
    u_long len{};//���ݲ��ֳ���
    u_long num{}; //���͵���Ϣ����������
    u_short checksum{};//У���
    char data[1024]{};//��������
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
        cout << "Server: �յ�seqΪ" << this->seq << "�����ݰ�" << endl;
        cout << "Server: checksum=" << this->checksum << ", len=" << this->len << endl;
    }
};

void ReceiveName(SOCKET serverSocket, SOCKADDR_IN& clientAddress);

const int WINDOW_SIZE = 1; // ���ڴ�С
message window[WINDOW_SIZE]; // ���ʹ���
int base = 0; // ���ʹ��ڵĻ����
int nextSeqNum = 0; // ��һ�������͵����
bool acked[WINDOW_SIZE]; // ��Ƿ��ʹ����ڵ���Ϣ�Ƿ�ȷ��


SOCKADDR_IN serveraddr, clientaddr;
SOCKET server;
double loss_rate = 0.01;
double time_wait = 200;

void SetColor(int fore = 7, int back = 0) {
    unsigned char m_color = fore;
    m_color += (back << 4);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), m_color);
    return;
}

message recvmessage(SOCKET socket, SOCKADDR_IN& clientaddr) {
    message msg;

    int len = sizeof(SOCKADDR);
    int bytesReceived = recvfrom(socket, (char*)&msg, BUFFER, 0, (SOCKADDR*)&clientaddr, &len);
    if (bytesReceived == SOCKET_ERROR) {
        //cout << "�������ݰ�ʧ��" << endl;
        return message(); // ���ؿ���Ϣ
    }
    //cout << msg.data << endl;

    return msg;
}

void sendmessage(SOCKET socket, const SOCKADDR_IN& clientaddr, message msg) {
    msg.setEXT();
    msg.setchecksum();

    int bytesSent = sendto(socket, (char*)&msg, BUFFER, 0, (SOCKADDR*)&clientaddr, sizeof(SOCKADDR));
    if (bytesSent == SOCKET_ERROR) {
        cout << "�������ݰ�ʧ��" << endl;
    }
}


int saveFile(const char* filepath, const char* data, int len)
{
    ofstream outFile(filepath, ios::out | ios::binary);

    if (!outFile.is_open())
    {
        return -1;  // ���ļ�ʧ��
    }

    outFile.write(data, len);
    outFile.close();


    return 0;  // �ļ�����ɹ�
}

// ��������
void Connect(bool& connectionEstablished) {
    message msg = recvmessage(server, clientaddr);
    SetColor(9, 0);

    if (msg.isSYN()) {
        cout << "Server: ���յ�һ���ͻ��˵���������" << endl;
        // ����ȷ����Ϣ
        message ackMsg;
        ackMsg.setACK();
        ackMsg.setSYN();
        ackMsg.ack = msg.seq + 1;
        sendmessage(server, clientaddr, ackMsg);
        cout << "Server: ���ͷ���������������" << endl;
        //ackMsg.output();
        int count = 0;
        while (true) {
            Sleep(50);
            if (count >= 10) {
                cout << "Server: �ȴ�ʱ��̫�����˳�����" << endl;
                return;
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
        cout << "Server: ����������ȷ�ϣ�������������ɡ�" << endl<<endl;
        connectionEstablished = true;
    }
    int iMode = 0; //1����������0������
    ioctlsocket(server, FIONBIO, (u_long FAR*) & iMode);//����������
}

void Close() {
    SetColor(9, 0);
    int iMode = 1; //1����������0������
    ioctlsocket(server, FIONBIO, (u_long FAR*) & iMode);//����������
    cout << endl;
    cout << "Server: ���յ�һ���ر���������" << endl;
    // �Ĵλ���
    message finMsg;
    finMsg.setFIN();
    finMsg.setchecksum();
    sendmessage(server, clientaddr, finMsg);
    cout << "Server: ��ͻ��˷���FIN" << endl;

    while (true) {
        message msg = recvmessage(server, clientaddr);

        if (msg.isACK()) {
            cout << "Server: �յ����Կͻ��˵� ACK���ر����ӡ�" << endl;
            break;
        }
    }
    SetColor(15, 0);
}

char receivedFilePath[MAX_FILEPATH] = { 0 };
void ReceiveFile(SOCKET serverSocket, SOCKADDR_IN& clientAddress, int num) {
    int iMode = 0; //1����������0������
    ioctlsocket(serverSocket, FIONBIO, (u_long FAR*) & iMode);//����������
    int start = clock();
    int end;
    message msg;
    u_short expectedSeq = 0; // �����ĳ�ʼ���к�
    int last_ack=0;
    while (1) {
        SetColor(15, 0);
        msg = recvmessage(serverSocket, clientaddr);
        
        if (!msg.isEXT()) {
            continue;
        }

        if (msg.isEND()) {
            message ackMsg;
            ackMsg.setACK();
            ackMsg.ack = msg.ack + 1;
            ackMsg.setEND();
            sendmessage(server, clientaddr, ackMsg);
            SetColor(13, 0);
            cout << "Server: �����ļ�"<< receivedFilePath<<"�ɹ�����" << endl << endl;
            cout << "************************************************" << endl;
            outFile.close();
            outFile.clear();
            int iMode = 0; //1����������0������
            ioctlsocket(server, FIONBIO, (u_long FAR*) & iMode);//����������
            SetColor(15, 0);
            return ReceiveName(server, clientaddr);
        }

        else if (msg.seq >= base && msg.seq < base + WINDOW_SIZE) {
            if (msg.seq == expectedSeq ) {
                // �յ���ȷ�����ݰ������� ACK
                message ackMsg;
                ackMsg.setACK();
                ackMsg.ack = msg.seq + 1;
                last_ack = ackMsg.ack;
                acked[msg.seq % WINDOW_SIZE] = true;

                double random_value = (double)rand() / RAND_MAX;
                if (random_value < loss_rate) {
                    SetColor(11, 0);
                    cout << endl;
                    cout << "Server: ģ�ⶪ����δ�������ݰ�" << endl;
                    cout << endl;
                    //sendmessage(server, serveraddr, message()); // ���ؿ���Ϣ
                    continue;
                }

                sendmessage(server, clientaddr, ackMsg);
                cout << endl;
                msg.output();
                cout << "Server: ����ȷ���յ������ݰ�(��Ӧ��ack)" << endl;
                

                outFile.write(msg.data, msg.len);

                num--;
                expectedSeq++;

                // ��������
                while (acked[base % WINDOW_SIZE]) {
                    acked[base % WINDOW_SIZE] = false;
                    base = base++;
                }
            }
        }
        else if(msg.seq == expectedSeq+1){
            SetColor(10, 0);
            message ackMsg;
            ackMsg.setACK();
            ackMsg.ack = last_ack-1;
            ackMsg.setRE();
            sendmessage(server, clientaddr, ackMsg);
            cout << endl;
            //msg.output();
            cout << "Server: �ظ�����ȷ���յ������ݰ��� " << ackMsg.ack << endl;
        }

    }

}

void ReceiveName(SOCKET serverSocket, SOCKADDR_IN& clientAddress)
{
    base = 0; 
    nextSeqNum = 0; 
    SetColor(15, 0);
    cout << "Server: �ȴ��ļ���������..." << endl;
    message msg;
    int num; //��Ҫ�������ݰ���С

    int count = 0;
    // �����ļ�����
    while (true)
    {
        int len = sizeof(SOCKADDR);
        msg = recvmessage(serverSocket, clientaddr);

        if (msg.isFIN()) {
            cout << "�ͻ���׼���Ͽ����ӣ��������ģʽ��" << endl;
            Close();
            break;
        }
        // �����ļ�����Ϣ
        if (msg.isSTART()) {
            cout << msg.data << endl;
            memset(receivedFilePath, 0, MAX_FILEPATH);
            strncpy(receivedFilePath, msg.data, msg.len);
            cout << "Server: ���յ��ļ���Ϊ" << receivedFilePath << endl;

            num = msg.num;
            message ackMsg;
            ackMsg.setACK();
            ackMsg.ack = msg.seq;
            sendmessage(serverSocket, clientaddr, ackMsg);
            cout << "Server: ��ʼ�����ļ�����" << endl;

            outFile.open(receivedFilePath, ios::out | ios::binary);
            return ReceiveFile(serverSocket, clientaddr, num);
        }

        // ������յ������ݰ�
        else if (!msg.isEXT())
        {
            Sleep(50);
            if (count >= time_wait) {
                cout << "Server: ���ӳ�ʱ�����½����ļ���" << endl;
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
        cout << "�׽��ֳ�ʼ��ʧ�ܣ�" << err << endl;
        return -1;
    }

    int iMode = 1; //1����������0������
    ioctlsocket(server, FIONBIO, (u_long FAR*) & iMode);//����������
    server = socket(AF_INET, SOCK_DGRAM, 0);
    serveraddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVER_PORT);
    err = bind(server, (SOCKADDR*)&serveraddr, sizeof(SOCKADDR));

    if (err)
    {
        cout << "�󶨶˿�" << SERVER_PORT << "����" << err << endl;
        WSACleanup();
        return -1;
    }

    cout << "���������������ȴ��ͻ�������..." << endl;

    bool connectionEstablished = false;
    //Sleep(time_wait);

    // �����������
    while (!connectionEstablished) {
        Connect(connectionEstablished);
    }

    // �����ļ�
    ReceiveName(server, clientaddr);

    closesocket(server);
    WSACleanup();
    return 0;
}

