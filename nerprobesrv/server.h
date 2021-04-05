#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>

#define MSG_WAITALL 0x8

#elif __linux__
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <error.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <pthread.h>
#include <unistd.h>

#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define WSAGetLastError() (errno)
#define closesocket(x) close(x)
#endif

#include <stdio.h>
#include <algorithm>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <getopt.h>
#include <chrono>
#include <vector>
#include <iomanip>

#include "threadpool.h"

using namespace std;
using namespace std::chrono;

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#endif

#define READ 1
#define WRITE 2
#define RESPONSE 3
#define TCP 0
#define UDP 1
#define THREADPOOL 0
#define SELECT 1
// structure for user argument input and client argument exchange
struct arguments
{
    int mode;
    int stat;
    char rhost[40];
    int rport;
    int proto;
    int pktsize;
    int pktrate;
    int pktnum;
    int sbufsize;
    char lhost[40];
    int lport;
    int rbufsize;
    int socket;
};
// structure for SELECT socket status tracking
struct socketCond
{
    unsigned long pktNum = 0;
    unsigned long byte = 0;
    unsigned long data = 1;
    string dataStr = "";
    high_resolution_clock::time_point start;
    struct sockaddr_in addr;
    unsigned long lossPkt = 0;
};
// variables for select()
int selectRet;
const int maxSockNum = 10;

struct readSock
{
    int readActiveSocket = 0;
    fd_set readSockSet;
    int readSocketPool[maxSockNum];
    bool readSocketValid[maxSockNum] = {false};
    struct arguments readSocketInfo[maxSockNum];
    struct socketCond readSocketCond[maxSockNum];
} readSock;

struct writeSock
{
    int writeActiveSocket = 0;
    fd_set writeSockSet;
    int writeSocketPool[maxSockNum];
    bool writeSocketValid[maxSockNum] = {false};
    struct arguments writeSocketInfo[maxSockNum];
    struct socketCond writeSocketCond[maxSockNum];
} writeSock;

int srvMode = THREADPOOL;
int poolSize = 8;

void assignDefault(struct arguments *arguments)
{
    arguments->stat = 500;
    arguments->sbufsize = 0;
    arguments->rbufsize = 0;
    strncpy(arguments->lhost, "not specified", sizeof(arguments->lhost));
    arguments->lport = 4180;
}

void assignDefaultCond(struct socketCond *socketCond)
{
    socketCond->pktNum = 0;
    socketCond->byte = 0;
    socketCond->data = 1;
    socketCond->lossPkt = 0;
}

void printInfo(struct arguments *arguments)
{
    cout << endl;
    cout << "NetProbeSrv <parameters>, see below:" << endl;
    cout << "   <-sbufsize bsize>                   set the outgoing socket buffer size to bsize bytes." << endl;
    cout << "   <-rbufsize bsize>                   set the incoming socket buffer size to bsize bytes." << endl;
    cout << "   <-lhost hostname>                   hostname to bind to. (Default late binding)" << endl;
    cout << "   <-lport portnum>                    port number to bind to. (Default '4180')" << endl;
    cout << "   <-servermodel [select|threadpool]   set the concurrent server model to either select()-based or thread pool" << endl;
    cout << "   <-poolsize psize                    set the initial thread pool size (default 8 threads), valid for thread pool server model only" << endl;
    cout << endl;
}

void printInfoDebug(struct arguments *arguments)
{
    cout << "pConfig->eMode = " << arguments->mode << endl;

    cout << "pConfig->eProtocol = " << arguments->proto << endl;

    cout << "NetProbe Configurations:" << endl;

    cout << " Mode: ";
    if (arguments->mode == 1)
    {
        cout << "SEND";
    }
    else if (arguments->mode == 2)
    {
        cout << "RECV";
    }
    else
    {
        cout << "HOST";
    }
    cout << "    Protocol: ";
    if (arguments->proto == 0)
    {
        cout << "TCP" << endl;
    }
    else
    {
        cout << "UDP" << endl;
    }
    cout << endl;

    cout << " -stat = " << arguments->stat << endl;
    cout << " -pktsize = " << arguments->pktsize << endl;
    cout << " -pktrate = " << arguments->pktrate << endl;
    cout << " -pktnum = " << arguments->pktnum << endl;
    cout << " -sbufsize = " << arguments->sbufsize << endl;
    cout << " -rbufsize = " << arguments->rbufsize << endl;
    cout << " -rhost = " << arguments->rhost << endl;
    cout << " -rport = " << arguments->rport << endl;
    cout << endl;
}

void modeResponseTCP_S(int i)
{
    int sendSock = writeSock.writeSocketPool[i];

    struct arguments arguments;
    memcpy(&arguments, &writeSock.writeSocketInfo[i], sizeof(arguments));

    // variable for debugging
    int iResult;

    char sendBuf[65536];
    memset(sendBuf, '\0', sizeof(sendBuf));
    strcpy(sendBuf, "response");

    iResult = send(sendSock, sendBuf, arguments.pktsize, 0);
    if (iResult == SOCKET_ERROR)
    {
        cout << endl;

        cout << "Exit: send() failed in modeResponseTCP_S() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError();
    }

    iResult = closesocket(sendSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed in modeResponseTCP_S() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError();
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }

    writeSock.writeActiveSocket--;
    writeSock.writeSocketValid[i] = false;
    writeSock.writeSocketPool[i] = 0;
    memset(&writeSock.writeSocketInfo[i], '\0', sizeof(writeSock.writeSocketInfo[i]));
    assignDefaultCond(&writeSock.writeSocketCond[i]);
    if (writeSock.writeActiveSocket == maxSockNum - 1)
    {
        readSock.readSocketValid[0] = true;
    }
}

void modeResponseUDP_S(int i)
{
    int sendSock = writeSock.writeSocketPool[i];

    struct arguments arguments;
    memcpy(&arguments, &writeSock.writeSocketInfo[i], sizeof(arguments));

    memset(&writeSock.writeSocketCond[i].addr, '\0', sizeof(writeSock.writeSocketCond[i].addr));

    struct hostent *hostent = gethostbyname(arguments.rhost);
    struct in_addr **addr_list = (struct in_addr **)hostent->h_addr_list;

    writeSock.writeSocketCond[i].addr.sin_family = AF_INET;
    writeSock.writeSocketCond[i].addr.sin_port = htons(arguments.rport);
    writeSock.writeSocketCond[i].addr.sin_addr.s_addr = inet_addr(inet_ntoa(*addr_list[0]));

    if (inet_ntoa(*addr_list[0]) != NULL)
    {
        // cout << "UDP remote host " << arguments.rhost << " lookup successful, IP address is " << inet_ntoa(*addr_list[0]) << endl;
    }
    else
    {
        cout << "Exit: Remote host lookup failed in modeResponseUDP_S()  on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;

        closesocket(sendSock);

        return;
    }

    // variable for debugging
    int iResult;

    char sendBuf[65536];
    memset(sendBuf, '\0', sizeof(sendBuf));
    strcpy(sendBuf, "response");

    iResult = sendto(sendSock, sendBuf, arguments.pktsize, 0, (sockaddr *)&writeSock.writeSocketCond[i].addr, sizeof(writeSock.writeSocketCond[i].addr));
    if (iResult == SOCKET_ERROR)
    {
        cout << endl;

        cout << "Exit: sendto() failed in modeResponseUDP_S() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
    }

    iResult = closesocket(sendSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed in modeResponseUDP_S() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }

    writeSock.writeActiveSocket--;
    writeSock.writeSocketValid[i] = false;
    writeSock.writeSocketPool[i] = 0;
    memset(&writeSock.writeSocketInfo[i], '\0', sizeof(writeSock.writeSocketInfo[i]));
    assignDefaultCond(&writeSock.writeSocketCond[i]);
    if (writeSock.writeActiveSocket == maxSockNum - 1)
    {
        readSock.readSocketValid[0] = true;
    }
}

void modeResponseTCP_MT(void *arg)
{
    struct arguments arguments;

    memcpy(&arguments, arg, sizeof(arguments));
    // printInfoDebug(&arguments);

    int sendSock = arguments.socket;
    // variable for debugging
    int iResult;

    char sendBuf[65536];
    memset(sendBuf, '\0', sizeof(sendBuf));
    strcpy(sendBuf, "response");

    iResult = send(sendSock, sendBuf, arguments.pktsize, 0);
    if (iResult == SOCKET_ERROR)
    {
        cout << endl;

        cout << "Exit: send() failed in modeResponseTCP_MT() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
    }

    iResult = closesocket(sendSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed in modeResponseTCP_MT() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void modeResponseUDP_MT(void *arg)
{
    struct arguments arguments;

    memcpy(&arguments, arg, sizeof(arguments));
    // printInfoDebug(&arguments);

    int sendSock = arguments.socket;

    struct sockaddr_in recvAddr;
    memset(&recvAddr, '\0', sizeof(recvAddr));

    struct hostent *hostent = gethostbyname(arguments.rhost);
    struct in_addr **addr_list = (struct in_addr **)hostent->h_addr_list;

    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(arguments.rport);
    recvAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*addr_list[0]));

    if (inet_ntoa(*addr_list[0]) != NULL)
    {
        // cout << "UDP remote host " << arguments.rhost << " lookup successful, IP address is " << inet_ntoa(*addr_list[0]) << endl;
    }
    else
    {
        cout << "Exit: Remote host lookup failed in modeResponseUDP_MT() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;

        closesocket(sendSock);

        return;
    }

    // variable for debugging
    int iResult;

    char sendBuf[65536];
    memset(sendBuf, '\0', sizeof(sendBuf));
    strcpy(sendBuf, "response");

    iResult = sendto(sendSock, sendBuf, arguments.pktsize, 0, (sockaddr *)&recvAddr, sizeof(recvAddr));
    if (iResult == SOCKET_ERROR)
    {
        cout << endl;

        cout << "Exit: sendto() failed in modeResponseUDP_MT() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
    }

    iResult = closesocket(sendSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed in modeResponseUDP_MT() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void modeSendTCP_MT(void *arg)
{
    struct arguments arguments;
    memcpy(&arguments, arg, sizeof(arguments));

    int sendSock = arguments.socket;
    // variable for debugging
    int iResult;

    double sentPktNum = 0, sentByte = 0;
    char sendBuf[65536];
    memset(sendBuf, '\0', 65536);
    unsigned long sendData = 1;
    string sendDataStr = to_string(sendData);
    strcpy(sendBuf, sendDataStr.c_str());

    // timer for elapsed time
    auto start = high_resolution_clock::now();
    while (sentPktNum < arguments.pktnum || arguments.pktnum == -1)
    {
        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(stop - start);
        // check if rate exceeds limit
        if (arguments.pktrate != 0 && sentByte != 0 && ((double)(sentByte / (double)(duration.count() / 1000)) >= arguments.pktrate))
        {
            // skip the later actions as rate exceeds limit
            continue;
        }

        iResult = send(sendSock, sendBuf, arguments.pktsize, 0);
        if (iResult == SOCKET_ERROR)
        {
            cout << endl;
            cout << "Exit: send() failed in modeSendTCP_MT() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;

            break;
        }
        else
        {
            sentPktNum++;
            sentByte += iResult;
            memset(sendBuf, '\0', sizeof(sendBuf));
            sendData++;
            sendDataStr = to_string(sendData);
            strcpy(sendBuf, sendDataStr.c_str());
        }
    }

    iResult = closesocket(sendSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed in modeSendTCP_MT() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void modeSendUDP_MT(void *arg)
{
    struct arguments arguments;
    memcpy(&arguments, arg, sizeof(arguments));

    int sendSock = arguments.socket;

    struct sockaddr_in recvAddr;
    memset(&recvAddr, '\0', sizeof(recvAddr));

    struct hostent *hostent = gethostbyname(arguments.rhost);
    struct in_addr **addr_list = (struct in_addr **)hostent->h_addr_list;

    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(arguments.rport);
    recvAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*addr_list[0]));

    if (inet_ntoa(*addr_list[0]) != NULL)
    {
        // cout << "UDP remote host " << arguments.rhost << " lookup successful, IP address is " << inet_ntoa(*addr_list[0]) << endl;
    }
    else
    {
        cout << "Exit: Remote host lookup failed in modeSendUDP_MT()" << endl;

        closesocket(sendSock);
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }

    // variable for debugging
    int iResult;

    // set udpSock send buffer
    int sendBufSize;
    if (arguments.sbufsize == 0)
    {
        sendBufSize = 65536;
        iResult = setsockopt(sendSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, sizeof(sendBufSize));
        if (iResult == SOCKET_ERROR)
        {
            closesocket(sendSock);
            cout << "Exit: setsockopt() failed in modeSendUDP_MT() on send buffer size with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
    }
    else
    {
        sendBufSize = arguments.sbufsize;
        iResult = setsockopt(sendSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, sizeof(sendBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() failed in modeSendUDP_MT() on send buffer size with error: " << WSAGetLastError() << endl;
            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
    }

    double sentPktNum = 0, sentByte = 0;
    char sendBuf[65536];
    memset(sendBuf, '\0', 65536);
    unsigned long sendData = 1;
    string sendDataStr = to_string(sendData);
    strcpy(sendBuf, sendDataStr.c_str());
    // timer for update interval
    auto updateStart = high_resolution_clock::now();
    // timer for elapsed time
    auto start = high_resolution_clock::now();
    while (sentPktNum < arguments.pktnum || arguments.pktnum == -1)
    {
        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(stop - start);
        // check if rate exceeds limit
        if (arguments.pktrate != 0 && sentByte != 0 && ((double)(sentByte / (double)(duration.count() / 1000)) >= arguments.pktrate))
        {
            // skip the later actions as rate exceeds limit
            continue;
        }

        iResult = sendto(sendSock, sendBuf, arguments.pktsize, 0, (sockaddr *)&recvAddr, sizeof(recvAddr));
        if (iResult == SOCKET_ERROR)
        {
            cout << endl;

            cout << "Exit: sendto() failed in modeSendUDP_MT() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;

            break;
        }
        else
        {
            sentPktNum++;
            sentByte += iResult;
            memset(sendBuf, '\0', arguments.pktsize);
            sendData++;
            if (sentPktNum < arguments.pktnum - 1 || arguments.pktnum == -1)
            {
                sendDataStr = to_string(sendData);
                strcpy(sendBuf, sendDataStr.c_str());
            }
            else
            {
                strcpy(sendBuf, "-999");
            }
        }
    }

    iResult = closesocket(sendSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed in modeSendUDP_MT() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void modeRecvTCP_MT(void *arg)
{
    struct arguments arguments;
    memcpy(&arguments, arg, sizeof(arguments));

    int recvSock = arguments.socket;

    // variable for debugging
    int iResult;

    double recvPktNum = 0, recvByte = 0, pktLossNum = 0;
    char recvBuf[65536];
    unsigned long lastData = 1;
    unsigned long expectData = 1;
    string expectDataStr = to_string(expectData);

    iResult = 1;
    double recvInter = 0, recvInterTotal = 0, jitter = 0, jitterTotal = 0;
    // timer for jitter
    auto interStart = high_resolution_clock::now();
    auto interStop = high_resolution_clock::now();
    auto interDuration = duration_cast<milliseconds>(interStop - interStart);
    // timer for elapsed time
    auto start = high_resolution_clock::now();
    while (iResult > 0)
    {
        memset(recvBuf, '\0', sizeof(recvBuf));
        iResult = recv(recvSock, recvBuf, arguments.pktsize, MSG_WAITALL);
        // jitter timer update
        if (recvPktNum > 0)
        {
            interStop = high_resolution_clock::now();
            interDuration = duration_cast<milliseconds>(interStop - interStart);
        }
        interStart = high_resolution_clock::now();

        if (iResult == SOCKET_ERROR || iResult == 0)
        {
            if (WSAGetLastError() == 10054 || WSAGetLastError() == 104 || iResult == 0)
            {
                break;
            }
            else
            {
                cout << endl;
                cout << "Exit: recv() failed in modeRecvTCP_MT() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;

                break;
            }
        }
        // packet loss check
        if (strcmp(recvBuf, "") == 0)
        {
            // cout << "empty recvBuf array" << endl;
        }
        else if (strcmp(expectDataStr.c_str(), recvBuf) != 0)
        {
            pktLossNum += atoi(recvBuf) - expectData;
            lastData = atoi(recvBuf);
            expectData = atoi(recvBuf) + 1;
            expectDataStr = to_string(expectData);
        }
        else
        {
            lastData = atoi(recvBuf);
            expectData++;
            expectDataStr = to_string(expectData);
        }
        // general data update
        recvPktNum++;
        recvByte += iResult;

        // jitter value update
        if (recvPktNum > 1)
        {
            recvInter = interDuration.count();
            recvInterTotal += recvInter;
            jitter = abs(recvInter - (recvInterTotal / recvPktNum));
            jitterTotal += jitter;
        }
    }

    iResult = closesocket(recvSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed in modeRecvTCP_MT on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void modeRecvUDP_MT(void *arg)
{
    struct arguments arguments;
    memcpy(&arguments, arg, sizeof(arguments));

    int recvSock = arguments.socket;

    // variable for debugging
    int iResult;

    // set recvSock recv buffer
    int recvBufSize;
    if (arguments.rbufsize == 0)
    {
        recvBufSize = 65536;
        iResult = setsockopt(recvSock, SOL_SOCKET, SO_RCVBUF, (char *)&recvBufSize, sizeof(recvBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() failed in modeRecvUDP_MT() on recvSock recv buffer size with error: " << WSAGetLastError() << endl;
            closesocket(recvSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
    }
    else
    {
        recvBufSize = arguments.rbufsize;
        iResult = setsockopt(recvSock, SOL_SOCKET, SO_RCVBUF, (char *)&recvBufSize, sizeof(recvBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() failed in modeRecvUDP_MT() on recvSock recv buffer size with error: " << WSAGetLastError() << endl;
            closesocket(recvSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
    }

    double recvPktNum = 0, recvByte = 0, pktLossNum = 0;
    char recvBuf[65536];
    unsigned long lastData = 1;
    unsigned long expectData = 1;
    string expectDataStr = to_string(expectData);

    struct sockaddr_in sendAddr;
    memset(&sendAddr, '\0', sizeof(sendAddr));
    int sendAddrSize = sizeof(sendAddr);

    iResult = 1;
    double recvInter = 0, recvInterTotal = 0, jitter = 0, jitterTotal = 0;
    // timer for jitter
    auto interStart = high_resolution_clock::now();
    auto interStop = high_resolution_clock::now();
    auto interDuration = duration_cast<milliseconds>(interStop - interStart);
    // timer for elapsed time
    auto start = high_resolution_clock::now();
    while (iResult > 0)
    {
        memset(recvBuf, '\0', arguments.pktsize);
        iResult = recvfrom(recvSock, recvBuf, arguments.pktsize, 0, (sockaddr *)&sendAddr, (socklen_t *)&sendAddrSize);
        // jitter timer update
        if (recvPktNum > 0)
        {
            interStop = high_resolution_clock::now();
            interDuration = duration_cast<milliseconds>(interStop - interStart);
        }
        interStart = high_resolution_clock::now();

        // general data update
        recvPktNum++;
        recvByte += iResult;

        if (iResult == SOCKET_ERROR || strcmp("-999", recvBuf) == 0 || iResult == 0)
        {
            if (strcmp("-999", recvBuf) == 0 || iResult == 0)
            {
                break;
            }
            else
            {
                cout << endl;
                cout << "Exit: recvfrom() failed in modeRecvUDP_MT() on [" << inet_ntoa(sendAddr.sin_addr) << ":" << sendAddr.sin_port << "]"
                     << " with error: " << WSAGetLastError() << endl;

                break;
            }
        }
        // packet loss check
        if (strcmp(expectDataStr.c_str(), recvBuf) != 0)
        {
            pktLossNum += atoi(recvBuf) - expectData;
            lastData = atoi(recvBuf);
            expectData = atoi(recvBuf) + 1;
            expectDataStr = to_string(expectData);
        }
        else
        {
            lastData = atoi(recvBuf);
            expectData++;
            expectDataStr = to_string(expectData);
        }
        // jitter value update
        if (recvPktNum > 1)
        {
            recvInter = interDuration.count();
            recvInterTotal += recvInter;
            jitter = abs(recvInter - (recvInterTotal / recvPktNum));
            jitterTotal += jitter;
        }
    }

    iResult = closesocket(recvSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed in modeRecvUDP_MT on [" << arguments.rhost << ":" << arguments.rport << "]"
             << " with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void modeSendTCP_S(int i)
{
    int sendSock = writeSock.writeSocketPool[i];

    struct arguments arguments;
    memcpy(&arguments, &writeSock.writeSocketInfo[i], sizeof(arguments));

    // variable for debugging
    int iResult;
    // first time the socket being operated
    if (writeSock.writeSocketCond[i].pktNum == 0)
    {
        writeSock.writeSocketCond[i].dataStr = to_string(writeSock.writeSocketCond[i].data);
        writeSock.writeSocketCond[i].start = high_resolution_clock::now();
    }

    char sendBuf[65536];
    memset(sendBuf, '\0', 65536);
    strcpy(sendBuf, writeSock.writeSocketCond[i].dataStr.c_str());

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - writeSock.writeSocketCond[i].start);
    // check if send data
    if (arguments.pktrate == 0 || writeSock.writeSocketCond[i].byte == 0 || ((double)(writeSock.writeSocketCond[i].byte / (double)(duration.count() / 1000)) < arguments.pktrate))
    {
        iResult = send(sendSock, sendBuf, arguments.pktsize, 0);
        if (iResult == SOCKET_ERROR)
        {
            cout << endl;

            cout << "Exit: send() failed in modeSendTCP() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
        }
        else
        {
            writeSock.writeSocketCond[i].pktNum++;
            writeSock.writeSocketCond[i].byte += iResult;
            writeSock.writeSocketCond[i].data++;
            writeSock.writeSocketCond[i].dataStr = to_string(writeSock.writeSocketCond[i].data);
        }
    }
    // check if sent all data
    if (writeSock.writeSocketCond[i].pktNum >= arguments.pktnum && arguments.pktnum != -1)
    {
        iResult = closesocket(sendSock);
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: closesock() failed in modeSendTCP() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }

        writeSock.writeActiveSocket--;
        writeSock.writeSocketValid[i] = false;
        writeSock.writeSocketPool[i] = 0;
        memset(&writeSock.writeSocketInfo[i], '\0', sizeof(writeSock.writeSocketInfo[i]));
        assignDefaultCond(&writeSock.writeSocketCond[i]);
        if (writeSock.writeActiveSocket == maxSockNum - 1)
        {
            readSock.readSocketValid[0] = true;
        }
    }
}

void modeSendUDP_S(int i)
{
    int sendSock = writeSock.writeSocketPool[i];

    struct arguments arguments;
    memcpy(&arguments, &writeSock.writeSocketInfo[i], sizeof(arguments));

    // variable for debugging
    int iResult;

    if (writeSock.writeSocketCond[i].pktNum == 0)
    {
        writeSock.writeSocketCond[i].dataStr = to_string(writeSock.writeSocketCond[i].data);

        memset(&writeSock.writeSocketCond[i].addr, '\0', sizeof(writeSock.writeSocketCond[i].addr));

        struct hostent *hostent = gethostbyname(arguments.rhost);
        struct in_addr **addr_list = (struct in_addr **)hostent->h_addr_list;

        writeSock.writeSocketCond[i].addr.sin_family = AF_INET;
        writeSock.writeSocketCond[i].addr.sin_port = htons(arguments.rport);
        writeSock.writeSocketCond[i].addr.sin_addr.s_addr = inet_addr(inet_ntoa(*addr_list[0]));

        // set udpSock send buffer
        int sendBufSize;
        if (arguments.sbufsize == 0)
        {
            sendBufSize = 65536;
            iResult = setsockopt(sendSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, sizeof(sendBufSize));
            if (iResult == SOCKET_ERROR)
            {
                cout << "Exit: setsockopt() failed in modeSendUDP() on send buffer size with error: " << WSAGetLastError() << endl;
                closesocket(sendSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
        }
        else
        {
            sendBufSize = arguments.sbufsize;
            iResult = setsockopt(sendSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, sizeof(sendBufSize));
            if (iResult == SOCKET_ERROR)
            {
                cout << "Exit: setsockopt() failed in modeSendUDP() on send buffer size with error: " << WSAGetLastError() << endl;
                closesocket(sendSock);
                writeSock.writeActiveSocket--;
                writeSock.writeSocketValid[i] = false;
                writeSock.writeSocketPool[i] = 0;
                memset(&writeSock.writeSocketInfo[i], '\0', sizeof(writeSock.writeSocketInfo[i]));
                assignDefaultCond(&writeSock.writeSocketCond[i]);
                if (writeSock.writeActiveSocket == maxSockNum - 1)
                {
                    readSock.readSocketValid[0] = true;
                }
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
        }

        writeSock.writeSocketCond[i].start = high_resolution_clock::now();
    }

    char sendBuf[65536];
    memset(sendBuf, '\0', 65536);
    if (writeSock.writeSocketCond[i].pktNum < arguments.pktnum - 1 || arguments.pktnum == -1)
    {
        strcpy(sendBuf, writeSock.writeSocketCond[i].dataStr.c_str());
    }
    else
    {
        strcpy(sendBuf, "-999");
    }

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - writeSock.writeSocketCond[i].start);
    // check if send data
    if (arguments.pktrate == 0 || writeSock.writeSocketCond[i].byte == 0 || ((double)(writeSock.writeSocketCond[i].byte / (double)(duration.count() / 1000)) < arguments.pktrate))
    {
        iResult = sendto(sendSock, sendBuf, arguments.pktsize, 0, (sockaddr *)&writeSock.writeSocketCond[i].addr, sizeof(writeSock.writeSocketCond[i].addr));
        if (iResult == SOCKET_ERROR)
        {
            cout << endl;

            cout << "Exit: sendto() failed in modeSendUDP() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError();

            return;
        }
        else
        {
            writeSock.writeSocketCond[i].pktNum++;
            writeSock.writeSocketCond[i].byte += iResult;
            writeSock.writeSocketCond[i].data++;
            if (writeSock.writeSocketCond[i].pktNum < arguments.pktnum - 1 || arguments.pktnum == -1)
            {
                writeSock.writeSocketCond[i].dataStr = to_string(writeSock.writeSocketCond[i].data);
            }
        }
        // check if sent all data
        if (writeSock.writeSocketCond[i].pktNum >= arguments.pktnum && arguments.pktnum != -1)
        {
            iResult = closesocket(sendSock);
            if (iResult == SOCKET_ERROR)
            {
                cout << "Exit: closesock() failed in modeSendTCP() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError();
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }

            writeSock.writeActiveSocket--;
            writeSock.writeSocketValid[i] = false;
            writeSock.writeSocketPool[i] = 0;
            memset(&writeSock.writeSocketInfo[i], '\0', sizeof(writeSock.writeSocketInfo[i]));
            assignDefaultCond(&writeSock.writeSocketCond[i]);
            if (writeSock.writeActiveSocket == maxSockNum - 1)
            {
                readSock.readSocketValid[0] = true;
            }
        }
    }
}

void modeRecvTCP_S(int i)
{
    int recvSock = readSock.readSocketPool[i];
    struct arguments arguments;
    memcpy(&arguments, &readSock.readSocketInfo[i], sizeof(arguments));

    if (readSock.readSocketCond[i].pktNum == 0)
    {
        readSock.readSocketCond[i].dataStr = to_string(readSock.readSocketCond[i].data);
        readSock.readSocketCond[i].start = high_resolution_clock::now();
    }
    // variable for debugging
    int iResult;
    char recvBuf[65536];

    memset(recvBuf, '\0', 65536);
    iResult = recv(recvSock, recvBuf, arguments.pktsize, MSG_WAITALL);
    if (iResult == SOCKET_ERROR || iResult == 0)
    {
        if (WSAGetLastError() == 10054 || WSAGetLastError() == 104 || iResult == 0)
        {
            iResult = closesocket(recvSock);
            if (iResult == SOCKET_ERROR)
            {
                cout << "Exit: closesock() failed in modeRecvTCP_S on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }

            readSock.readActiveSocket--;
            readSock.readSocketValid[i] = false;
            readSock.readSocketPool[i] = 0;
            memset(&readSock.readSocketInfo[i], '\0', sizeof(readSock.readSocketInfo[i]));
            assignDefaultCond(&readSock.readSocketCond[i]);
            if (readSock.readActiveSocket == maxSockNum - 1)
            {
                readSock.readSocketValid[0] = true;
            }

            return;
        }
        else
        {
            iResult = closesocket(recvSock);
            if (iResult == SOCKET_ERROR)
            {
                cout << "Exit: closesock() failed in modeRecvTCP_S on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }

            cout << endl;
            cout << "Exit: recv() failed in modeRecvTCP_S() on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
            readSock.readActiveSocket--;
            readSock.readSocketValid[i] = false;
            readSock.readSocketPool[i] = 0;
            memset(&readSock.readSocketInfo[i], '\0', sizeof(readSock.readSocketInfo[i]));
            assignDefaultCond(&readSock.readSocketCond[i]);
            if (readSock.readActiveSocket == maxSockNum - 1)
            {
                readSock.readSocketValid[0] = true;
            }

            return;
        }
    }
    // packet loss check
    if (strcmp(readSock.readSocketCond[i].dataStr.c_str(), recvBuf) != 0)
    {
        readSock.readSocketCond[i].lossPkt += atoi(recvBuf) - readSock.readSocketCond[i].data;
        readSock.readSocketCond[i].data = atoi(recvBuf);
        readSock.readSocketCond[i].dataStr = to_string(atoi(recvBuf) + 1);
    }
    else
    {
        readSock.readSocketCond[i].data = atoi(recvBuf);
        readSock.readSocketCond[i].dataStr = to_string(atoi(recvBuf) + 1);
    }
    // general data update
    readSock.readSocketCond[i].pktNum++;
    readSock.readSocketCond[i].byte += iResult;
}

void modeRecvUDP_S(int i)
{
    int recvSock = readSock.readSocketPool[i];
    struct arguments arguments;
    memcpy(&arguments, &readSock.readSocketInfo[i], sizeof(arguments));

    // variable for debugging
    int iResult;

    if (readSock.readSocketCond[i].pktNum == 0)
    {
        readSock.readSocketCond[i].dataStr = to_string(readSock.readSocketCond[i].data);
        // set recvSock recv buffer
        int recvBufSize;
        if (arguments.rbufsize == 0)
        {
            recvBufSize = 65536;
            iResult = setsockopt(recvSock, SOL_SOCKET, SO_RCVBUF, (char *)&recvBufSize, sizeof(recvBufSize));
            if (iResult == SOCKET_ERROR)
            {
                cout << "Exit: setsockopt() failed in modeRecvUDP() on recvSock recv buffer size with error: " << WSAGetLastError() << endl;
                closesocket(recvSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
        }
        else
        {
            recvBufSize = arguments.rbufsize;
            iResult = setsockopt(recvSock, SOL_SOCKET, SO_RCVBUF, (char *)&recvBufSize, sizeof(recvBufSize));
            if (iResult == SOCKET_ERROR)
            {
                cout << "Exit: setsockopt() failed in modeRecvUDP() on recvSock recv buffer size with error: " << WSAGetLastError() << endl;
                closesocket(recvSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
        }

        readSock.readSocketCond[i].start = high_resolution_clock::now();
    }

    struct sockaddr_in sendAddr;
    memset(&sendAddr, '\0', sizeof(sendAddr));
    int sendAddrSize = sizeof(sendAddr);

    char recvBuf[65536];
    memset(recvBuf, '\0', 65536);

    iResult = recvfrom(recvSock, recvBuf, arguments.pktsize, 0, (sockaddr *)&sendAddr, (socklen_t *)&sendAddrSize);
    if (iResult == SOCKET_ERROR || strcmp("-999", recvBuf) == 0 || iResult == 0)
    {
        if (strcmp("-999", recvBuf) == 0 || iResult == 0)
        {
            iResult = closesocket(recvSock);
            if (iResult == SOCKET_ERROR)
            {
                cout << "Exit: closesock() failed in modeRecvTCP_S on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }

            readSock.readActiveSocket--;
            readSock.readSocketValid[i] = false;
            readSock.readSocketPool[i] = 0;
            memset(&readSock.readSocketInfo[i], '\0', sizeof(readSock.readSocketInfo[i]));
            assignDefaultCond(&readSock.readSocketCond[i]);
            if (readSock.readActiveSocket == maxSockNum - 1)
            {
                readSock.readSocketValid[0] = true;
            }

            return;
        }
        else
        {
            iResult = closesocket(recvSock);
            if (iResult == SOCKET_ERROR)
            {
                cout << "Exit: closesock() failed in modeRecvTCP_S on [" << arguments.rhost << ":" << arguments.rport << "] with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }

            cout << endl;
            cout << "Exit: recvfrom() failed in modeRecvUDP() on [" << inet_ntoa(sendAddr.sin_addr) << ":" << sendAddr.sin_port << "]"
                 << " with error: " << WSAGetLastError() << endl;

            readSock.readActiveSocket--;
            readSock.readSocketValid[i] = false;
            readSock.readSocketPool[i] = 0;
            memset(&readSock.readSocketInfo[i], '\0', sizeof(readSock.readSocketInfo[i]));
            assignDefaultCond(&readSock.readSocketCond[i]);
            if (readSock.readActiveSocket == maxSockNum - 1)
            {
                readSock.readSocketValid[0] = true;
            }

            return;
        }
    }

    // packet loss check
    if (strcmp(readSock.readSocketCond[i].dataStr.c_str(), recvBuf) != 0)
    {
        readSock.readSocketCond[i].lossPkt += atoi(recvBuf) - readSock.readSocketCond[i].data;
        readSock.readSocketCond[i].data = atoi(recvBuf);
        readSock.readSocketCond[i].dataStr = to_string(atoi(recvBuf) + 1);
    }
    else
    {
        readSock.readSocketCond[i].data = atoi(recvBuf);
        readSock.readSocketCond[i].dataStr = to_string(atoi(recvBuf) + 1);
    }
    // general data update
    readSock.readSocketCond[i].pktNum++;
    readSock.readSocketCond[i].byte += iResult;
}
