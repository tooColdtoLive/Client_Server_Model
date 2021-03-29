//Li Kam Tong 1155110041

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

#include "server.h"
// #include "threadpool.h"
#include "threadpool.c"

using namespace std;
using namespace std::chrono;

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#endif

static struct option long_opt[] =
    {
        {"stat", required_argument, NULL, 'k'},
        {"sbufsize", required_argument, NULL, 'l'},
        {"lhost", required_argument, NULL, 'm'},
        {"lport", required_argument, NULL, 'n'},
        {"rbufsize", required_argument, NULL, 'o'},
        {"servermodel", required_argument, NULL, 'p'},
        {"poolsize", required_argument, NULL, 'q'},
        {0, 0, 0, 0}};

int main(int argc, char **argv)
{
    struct arguments arguments;

    assignDefault(&arguments);

    int option_val = 0;
    int opt_inedx = 0;

    string temp;

    char short_opt[] = "l:m:n:o:p:q:";

    while ((option_val = getopt_long_only(argc, argv, short_opt, long_opt, NULL)) != -1)
    {
        switch (option_val)
        {
        case 'k':
            arguments.stat = atoi(optarg);
            if (arguments.stat < 0)
            {
                cout << "Exit: Negative value not suppoted for -stat" << endl;
                exit(1);
            }
            break;

        case 'l':
            arguments.sbufsize = atoi(optarg);
            if (arguments.sbufsize < 0)
            {
                cout << "Exit: Negative value not suppoted for -sbufsize" << endl;
                exit(1);
            }
            break;

        case 'm':
            strncpy(arguments.lhost, optarg, sizeof(arguments.lhost));
            break;

        case 'n':
            arguments.lport = atoi(optarg);
            if (arguments.lport < 1024)
            {
                cout << "Exit: Port number has to be larger than 1023." << endl;
                exit(1);
            }
            break;

        case 'o':
            arguments.rbufsize = atoi(optarg);
            if (arguments.rbufsize < 0)
            {
                cout << "Exit: Negative value not suppoted for -rbufsize" << endl;
                exit(1);
            }
            break;

        case 'p':
            temp = optarg;
            transform(temp.begin(), temp.end(), temp.begin(), ::toupper);
            if (strcmp(temp.c_str(), "SELECT") == 0)
            {
                srvMode = SELECT;
            }
            else if (strcmp(temp.c_str(), "THREADPOOL") == 0)
            {
                srvMode = THREADPOOL;
            }
            else
            {
                cout << "Exit: Incorrect server mode" << endl;
                exit(1);
            }
            break;

        case 'q':
            poolSize = atoi(optarg);
            if (poolSize < 0)
            {
                cout << "Exit: Negative value not suppoted for -poolsize" << endl;
                exit(1);
            }
            break;

        default:
            cout << "Exit: Incorrect arguments" << endl;
            exit(1);
        }
    }

    printInfo(&arguments);

    // variable for debugging
    int iResult;

#ifdef _WIN32
    WSADATA wsaData = {0};
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        cout << "Exit: WSAStartup() failed with error: " << iResult << endl;
        WSACleanup();
        exit(1);
    }
#endif

    struct sockaddr_in sendAddr;
    memset(&sendAddr, '\0', sizeof(sendAddr));

    struct sockaddr_in recvAddr;
    memset(&recvAddr, '\0', sizeof(recvAddr));

    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(arguments.lport);
    recvAddr.sin_addr.s_addr = INADDR_ANY;

    // tcp socket for incoming connection
    int listenSock = 0;
    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == SOCKET_ERROR)
    {
        cout << "Exit: socket() on listenSock failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }

    cout << "Binding local socket to port " << arguments.lport;
    if (strcmp(arguments.lhost, "not specified") == 0)
    {
        cout << " using late binding ... ";
    }
    else
    {
        cout << sendAddr.sin_port << " and sender address " << inet_ntoa(sendAddr.sin_addr) << " " << endl;
    }

    iResult = bind(listenSock, (sockaddr *)&recvAddr, sizeof(recvAddr));
    if (iResult == SOCKET_ERROR)
    {
        cout << "failed" << endl;
        cout << "Exit: bind() on listenSock failed with error: " << WSAGetLastError() << endl;
        closesocket(listenSock);
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
    else
    {
        cout << "success." << endl;
    }

    // set listenSock recv buffer size
    int recvBufSize;
    if (arguments.rbufsize == 0)
    {
        recvBufSize = 65536;
        iResult = setsockopt(listenSock, SOL_SOCKET, SO_RCVBUF, (char *)&recvBufSize, sizeof(recvBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() failed in main() on listenSock recv buffer size with error: " << WSAGetLastError() << endl;
            closesocket(listenSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Default TCP socket recv buffer size = 65536 bytes" << endl;
        }
    }
    else
    {
        recvBufSize = arguments.rbufsize;
        iResult = setsockopt(listenSock, SOL_SOCKET, SO_RCVBUF, (char *)&recvBufSize, sizeof(recvBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() failed in main() on listenSock recv buffer size with error: " << WSAGetLastError() << endl;
            closesocket(listenSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Manual TCP socket recv buffer size = " << arguments.rbufsize << " bytes" << endl;
        }
    }
    // set listenSock send buffer size
    int sendBufSize;
    if (arguments.sbufsize == 0)
    {
        sendBufSize = 65536;
        iResult = setsockopt(listenSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, sizeof(sendBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() failed in main() on listenSock send buffer size with error: " << WSAGetLastError() << endl;
            closesocket(listenSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Default TCP socket send buffer size = 65536 bytes" << endl;
        }
    }
    else
    {
        sendBufSize = arguments.sbufsize;
        iResult = setsockopt(listenSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, sizeof(sendBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() failed in main() on listenSock send buffer size with error: " << WSAGetLastError() << endl;
            closesocket(listenSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Manual TCP socket send buffer size = " << arguments.sbufsize << " bytes" << endl;
        }
    }

    iResult = listen(listenSock, 10);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: listen() failed in main() on listenSock with error: " << WSAGetLastError() << endl;
        closesocket(listenSock);
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }

    readSock.readSocketPool[0] = listenSock;
    readSock.readSocketValid[0] = true;

    int acceptSock = 0;
    int sendAddrSize = sizeof(sendAddr);

    struct arguments recvBuf;

    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 3000000;

    cout << "Listening to incoming connection request ... " << endl;

    if (srvMode == SELECT)
    {
        while (1)
        {
            FD_ZERO(&readSock.readSockSet);
            FD_ZERO(&writeSock.writeSockSet);
            int topActiveSock = 0;
            for (int i = 0; i < maxSockNum; i++)
            {
                if (readSock.readSocketValid[i] == true)
                {
                    // cout << "readSocketValid " << i << endl;
                    FD_SET(readSock.readSocketPool[i], &readSock.readSockSet);
                    if (readSock.readSocketPool[i] > topActiveSock)
                    {
                        topActiveSock = readSock.readSocketPool[i];
                    }
                }
                if (writeSock.writeSocketValid[i] == true)
                {
                    // cout << "writeSocketValid " << i << endl;
                    FD_SET(writeSock.writeSocketPool[i], &writeSock.writeSockSet);
                    if (writeSock.writeSocketPool[i] > topActiveSock)
                    {
                        topActiveSock = writeSock.writeSocketPool[i];
                    }
                }
            }

            selectRet = select(topActiveSock + 1, &readSock.readSockSet, &writeSock.writeSockSet, NULL, &timeout);
            if (selectRet == SOCKET_ERROR)
            {
                cout << "Exit: select() failed with error: " << WSAGetLastError() << endl;
                closesocket(listenSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }

            for (int i = 0; i < maxSockNum; i++)
            {
                if (!readSock.readSocketValid[i] && !writeSock.writeSocketValid[i])
                {
                    continue;
                }
                if (FD_ISSET(readSock.readSocketPool[i], &readSock.readSockSet))
                {
                    if (i == 0)
                    {
                        acceptSock = accept(listenSock, (struct sockaddr *)&sendAddr, (socklen_t *)&sendAddrSize);
                        if (acceptSock == INVALID_SOCKET)
                        {
                            cout << "Exit: accept() failed with error: " << WSAGetLastError() << endl;
                            closesocket(listenSock);
#ifdef _WIN32
                            WSACleanup();
#endif
                            exit(1);
                        }
                        // receive arguments from client
                        memset(&recvBuf, '\0', sizeof(recvBuf));
                        iResult = recv(acceptSock, (char *)&recvBuf, sizeof(recvBuf), 0);
                        if (iResult == SOCKET_ERROR)
                        {
                            cout << "Exit: recv() failed on acceptSock with error: " << WSAGetLastError() << endl;
                            closesocket(acceptSock);
                            closesocket(listenSock);
#ifdef _WIN32
                            WSACleanup();
#endif
                            exit(1);
                        }

                        cout << endl;

                        cout << "Connected to " << inet_ntoa(sendAddr.sin_addr) << " port " << sendAddr.sin_port;
                        if (recvBuf.mode == READ)
                        {
                            cout << " SEND, ";
                        }
                        else if (recvBuf.mode == WRITE)
                        {
                            cout << " RECV, ";
                        }
                        else if (recvBuf.mode == RESPONSE)
                        {
                            cout << " RESPONSE, ";
                        }
                        if (recvBuf.proto == TCP)
                        {
                            cout << "TCP, ";
                        }
                        else
                        {
                            cout << "UDP, ";
                        }
                        if (recvBuf.pktrate == 0)
                        {
                            cout << "inf kbps, ";
                        }
                        else
                        {
                            cout << (double)((double)recvBuf.pktrate / 1000) << " kbps, ";
                        }
                        cout << "size " << (double)((double)recvBuf.pktsize / 1000) << " kb, ";
                        cout << recvBuf.pktnum << " pkts" << endl;

                        if (recvBuf.mode == RESPONSE)
                        {
                            for (int j = 0; j < maxSockNum; j++)
                            {
                                if (!writeSock.writeSocketValid[j])
                                {
                                    int recvPortBuf;
                                    if (recvBuf.proto == UDP)
                                    {
                                        iResult = recv(acceptSock, (char *)&recvPortBuf, (int)sizeof(recvPortBuf), 0);
                                        if (iResult == SOCKET_ERROR)
                                        {
                                            cout << "Exit: recv() failed on acceptSock, protocol UDP with error: " << WSAGetLastError() << endl;
                                            closesocket(acceptSock);
                                            closesocket(listenSock);
#ifdef _WIN32
                                            WSACleanup();
#endif
                                            exit(1);
                                        }
                                        // open UDP socket
                                        int udpSock = 0;
                                        udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                                        if (udpSock == SOCKET_ERROR)
                                        {
                                            cout << "Exit: socket() failed on udpSock, mode WRITE, with error: " << WSAGetLastError() << endl;
                                            closesocket(acceptSock);
                                            closesocket(listenSock);
#ifdef _WIN32
                                            WSACleanup();
#endif
                                            exit(1);
                                        }

                                        writeSock.writeSocketPool[j] = udpSock;
                                        writeSock.writeSocketValid[j] = true;
                                        memset(&writeSock.writeSocketInfo[j], '\0', sizeof(writeSock.writeSocketInfo[j]));
                                        memcpy(&writeSock.writeSocketInfo[j], &recvBuf, sizeof(recvBuf));
                                        strncpy(writeSock.writeSocketInfo[j].rhost, inet_ntoa(sendAddr.sin_addr), sizeof(writeSock.writeSocketInfo[j].rhost));
                                        writeSock.writeSocketInfo[j].rport = ntohs(recvPortBuf);
                                        writeSock.writeActiveSocket++;
                                        if (writeSock.writeActiveSocket == maxSockNum)
                                        {
                                            readSock.readSocketValid[0] = false;
                                        }

                                        iResult = closesocket(acceptSock);
                                        if (iResult == SOCKET_ERROR)
                                        {
                                            cout << "Exit: closesock() failed in main on acceptSock, mode WRITE, with error: " << WSAGetLastError() << endl;
                                            closesocket(udpSock);
                                            closesocket(listenSock);
#ifdef _WIN32
                                            WSACleanup();
#endif
                                            exit(1);
                                        }

                                        break;
                                    }
                                    else
                                    {
                                        writeSock.writeSocketPool[j] = acceptSock;
                                        writeSock.writeSocketValid[j] = true;
                                        memset(&writeSock.writeSocketInfo[j], '\0', sizeof(writeSock.writeSocketInfo[j]));
                                        memcpy(&writeSock.writeSocketInfo[j], &recvBuf, sizeof(recvBuf));
                                        strncpy(writeSock.writeSocketInfo[j].rhost, inet_ntoa(sendAddr.sin_addr), sizeof(writeSock.writeSocketInfo[j].rhost));
                                        writeSock.writeSocketInfo[j].rport = sendAddr.sin_port;
                                        writeSock.writeActiveSocket++;
                                        if (writeSock.writeActiveSocket == maxSockNum)
                                        {
                                            readSock.readSocketValid[0] = false;
                                        }

                                        break;
                                    }
                                }
                            }
                        }
                        else if (recvBuf.mode == WRITE)
                        { // server writes to client
                            for (int j = 0; j < maxSockNum; j++)
                            {
                                if (!writeSock.writeSocketValid[j])
                                {
                                    int recvPortBuf;
                                    if (recvBuf.proto == UDP)
                                    {
                                        iResult = recv(acceptSock, (char *)&recvPortBuf, (int)sizeof(recvPortBuf), 0);
                                        if (iResult == SOCKET_ERROR)
                                        {
                                            cout << "Exit: recv() failed on acceptSock, protocol UDP with error: " << WSAGetLastError() << endl;
                                            closesocket(acceptSock);
                                            closesocket(listenSock);
#ifdef _WIN32
                                            WSACleanup();
#endif
                                            exit(1);
                                        }
                                        // open UDP socket
                                        int udpSock = 0;
                                        udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                                        if (udpSock == SOCKET_ERROR)
                                        {
                                            cout << "Exit: socket() failed on udpSock, mode WRITE, with error: " << WSAGetLastError() << endl;
                                            closesocket(acceptSock);
                                            closesocket(listenSock);
#ifdef _WIN32
                                            WSACleanup();
#endif
                                            exit(1);
                                        }

                                        writeSock.writeSocketPool[j] = udpSock;
                                        writeSock.writeSocketValid[j] = true;
                                        memset(&writeSock.writeSocketInfo[j], '\0', sizeof(writeSock.writeSocketInfo[j]));
                                        memcpy(&writeSock.writeSocketInfo[j], &recvBuf, sizeof(recvBuf));
                                        strncpy(writeSock.writeSocketInfo[j].rhost, inet_ntoa(sendAddr.sin_addr), sizeof(writeSock.writeSocketInfo[j].rhost));
                                        writeSock.writeSocketInfo[j].rport = ntohs(recvPortBuf);
                                        writeSock.writeActiveSocket++;
                                        if (writeSock.writeActiveSocket == maxSockNum)
                                        {
                                            readSock.readSocketValid[0] = false;
                                        }

                                        iResult = closesocket(acceptSock);
                                        if (iResult == SOCKET_ERROR)
                                        {
                                            cout << "Exit: closesock() failed in main on acceptSock, mode WRITE, with error: " << WSAGetLastError() << endl;
                                            closesocket(udpSock);
                                            closesocket(listenSock);
#ifdef _WIN32
                                            WSACleanup();
#endif
                                            exit(1);
                                        }

                                        break;
                                    }
                                    else
                                    {
                                        writeSock.writeSocketPool[j] = acceptSock;
                                        writeSock.writeSocketValid[j] = true;
                                        memset(&writeSock.writeSocketInfo[j], '\0', sizeof(writeSock.writeSocketInfo[j]));
                                        memcpy(&writeSock.writeSocketInfo[j], &recvBuf, sizeof(recvBuf));
                                        strncpy(writeSock.writeSocketInfo[j].rhost, inet_ntoa(sendAddr.sin_addr), sizeof(writeSock.writeSocketInfo[j].rhost));
                                        writeSock.writeSocketInfo[j].rport = sendAddr.sin_port;
                                        writeSock.writeActiveSocket++;
                                        if (writeSock.writeActiveSocket == maxSockNum)
                                        {
                                            readSock.readSocketValid[0] = false;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        else if (recvBuf.mode == READ)
                        { // server reads from client
                            for (int j = 1; j < maxSockNum; j++)
                            {
                                if (!readSock.readSocketValid[j])
                                {
                                    if (recvBuf.proto == UDP)
                                    {
                                        // open UDP socket
                                        int udpSock = 0;
                                        udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                                        if (udpSock == SOCKET_ERROR)
                                        {
                                            cout << "Exit: socket() failed on udpSock, mode READ, with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
                                            WSACleanup();
#endif
                                            exit(1);
                                        }
                                        // enable reuse address
                                        int flag = 1;
                                        iResult = setsockopt(udpSock, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag));
                                        if (iResult == SOCKET_ERROR)
                                        {
                                            cout << "Exit: setsockopt() failed on udpSock, mode READ, reuse address with error: " << WSAGetLastError() << endl;
                                            closesocket(udpSock);
                                            closesocket(acceptSock);
                                            closesocket(listenSock);
#ifdef _WIN32
                                            WSACleanup();
#endif
                                            exit(1);
                                        }

                                        struct sockaddr_in udpRecvAddr;
                                        memset(&udpRecvAddr, '\0', sizeof(udpRecvAddr));

                                        udpRecvAddr.sin_family = AF_INET;
                                        udpRecvAddr.sin_port = htons(rand() % 1000 + 1024);
                                        udpRecvAddr.sin_addr.s_addr = INADDR_ANY;

                                        do
                                        {
                                            iResult = bind(udpSock, (sockaddr *)&udpRecvAddr, sizeof(udpRecvAddr));
                                            if (iResult == SOCKET_ERROR)
                                            {
                                                udpRecvAddr.sin_port = htons(rand() % 1000 + 1024);
                                            }
                                        } while (iResult == SOCKET_ERROR);

                                        int sendBuf = udpRecvAddr.sin_port;
                                        iResult = send(acceptSock, (char *)&sendBuf, sizeof(sendBuf), 0);
                                        if (iResult == SOCKET_ERROR)
                                        {
                                            cout << "Exit: send() failed on acceptSock, mode READ, with error: " << WSAGetLastError() << endl;
                                            closesocket(udpSock);
                                            closesocket(acceptSock);
                                            closesocket(listenSock);
#ifdef _WIN32
                                            WSACleanup();
#endif
                                            exit(1);
                                        }

                                        readSock.readSocketPool[j] = udpSock;
                                        readSock.readSocketValid[j] = true;
                                        memset(&readSock.readSocketInfo[j], '\0', sizeof(readSock.readSocketInfo[j]));
                                        memcpy(&readSock.readSocketInfo[j], &recvBuf, sizeof(recvBuf));
                                        strncpy(readSock.readSocketInfo[j].rhost, inet_ntoa(sendAddr.sin_addr), sizeof(readSock.readSocketInfo[j].rhost));
                                        readSock.readSocketInfo[j].rport = sendAddr.sin_port;
                                        readSock.readActiveSocket++;
                                        if (readSock.readActiveSocket == maxSockNum)
                                        {
                                            readSock.readSocketValid[0] = false;
                                        }

                                        iResult = closesocket(acceptSock);
                                        if (iResult == SOCKET_ERROR)
                                        {
                                            cout << "Exit: closesock() on acceptSock, mode READ, failed with error: " << WSAGetLastError() << endl;
                                            closesocket(udpSock);
                                            closesocket(listenSock);
#ifdef _WIN32
                                            WSACleanup();
#endif
                                            exit(1);
                                        }
                                        break;
                                    }
                                    else
                                    { // for TCP as protocol
                                        readSock.readSocketPool[j] = acceptSock;
                                        readSock.readSocketValid[j] = true;
                                        memset(&readSock.readSocketInfo[j], '\0', sizeof(readSock.readSocketInfo[j]));
                                        memcpy(&readSock.readSocketInfo[j], &recvBuf, sizeof(recvBuf));
                                        strncpy(readSock.readSocketInfo[j].rhost, inet_ntoa(sendAddr.sin_addr), sizeof(readSock.readSocketInfo[j].rhost));
                                        readSock.readSocketInfo[j].rport = sendAddr.sin_port;
                                        readSock.readActiveSocket++;
                                        if (readSock.readActiveSocket == maxSockNum)
                                        {
                                            readSock.readSocketValid[0] = false;
                                        }

                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        switch (readSock.readSocketInfo[i].proto)
                        {
                        case TCP:
                            modeRecvTCP_S(i);
                            break;

                        case UDP:
                            modeRecvUDP_S(i);
                            break;
                        }
                    }
                }

                if (FD_ISSET(writeSock.writeSocketPool[i], &writeSock.writeSockSet))
                {
                    if (writeSock.writeSocketInfo[i].mode == RESPONSE)
                    {
                        switch (writeSock.writeSocketInfo[i].proto)
                        {
                        case TCP:
                            modeResponseTCP_S(i);
                            break;

                        case UDP:
                            modeResponseUDP_S(i);
                            break;
                        }
                    }
                    else
                    {
                        switch (writeSock.writeSocketInfo[i].proto)
                        {
                        case TCP:
                            modeSendTCP_S(i);
                            break;

                        case UDP:
                            modeSendUDP_S(i);
                            break;
                        }
                    }
                }
                // enable will skip when only one connected client
                // if(selectRet - 1 < -1){
                //   break;
                // }
            }
        }
    }
    else
    {
        pthread_mutex_t lock;

        threadpool_t *pool;

        pthread_mutex_init(&lock, NULL);

        pool = threadpool_create(poolSize, 32, 0, arguments.stat);
        if (pool == NULL)
        {
            cout << "Exit: threadpool_create() failed" << endl;
            closesocket(listenSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }

        while (1)
        {
            // shutdown system if error
            if (pool->shutdown != 0)
            {
                threadpool_destroy(pool, 0);
                break;
            }

            acceptSock = accept(listenSock, (struct sockaddr *)&sendAddr, (socklen_t *)&sendAddrSize);
            if (acceptSock == INVALID_SOCKET)
            {
                cout << "Exit: accept() failed with error: " << WSAGetLastError() << endl;
                closesocket(listenSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
            // receive arguments from client
            memset(&recvBuf, '\0', sizeof(recvBuf));
            iResult = recv(acceptSock, (char *)&recvBuf, sizeof(recvBuf), 0);
            if (iResult == SOCKET_ERROR)
            {
                cout << "Exit: recv() failed on acceptSock with error: " << WSAGetLastError() << endl;
                closesocket(listenSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }

            cout << endl;

            if (recvBuf.mode == RESPONSE)
            {
                int recvPortBuf;
                if (recvBuf.proto == UDP)
                {
                    iResult = recv(acceptSock, (char *)&recvPortBuf, (int)sizeof(recvPortBuf), 0);
                    if (iResult == SOCKET_ERROR)
                    {
                        cout << "Exit: recv() failed on acceptSock, protocol UDP with error: " << WSAGetLastError() << endl;
                        closesocket(acceptSock);
                        closesocket(listenSock);
                        threadpool_destroy(pool, 0);
#ifdef _WIN32
                        WSACleanup();
#endif
                        exit(1);
                    }
                    // open UDP socket
                    int udpSock = 0;
                    udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    if (udpSock == SOCKET_ERROR)
                    {
                        cout << "Exit: socket() failed in main on udpSock, mode RESPONSE, with error: " << WSAGetLastError() << endl;
                        closesocket(acceptSock);
                        closesocket(listenSock);
                        threadpool_destroy(pool, 0);
#ifdef _WIN32
                        WSACleanup();
#endif
                        exit(1);
                    }

                    strncpy(recvBuf.rhost, inet_ntoa(sendAddr.sin_addr), sizeof(recvBuf.rhost));
                    recvBuf.rport = ntohs(recvPortBuf);
                    recvBuf.socket = udpSock;

                    iResult = threadpool_add(pool, &modeResponseUDP_MT, &recvBuf, 0);
                    if (iResult != 0)
                    {
                        cout << "Exit: threadpool_add() failed on modeResponseUDP_MT with error: " << iResult << endl;
                        closesocket(udpSock);
                    }

                    iResult = closesocket(acceptSock);
                    if (iResult == SOCKET_ERROR)
                    {
                        cout << "Exit: closesock() failed in main on acceptSock, mode RESPONSE, with error: " << WSAGetLastError() << endl;

                        closesocket(udpSock);
                        closesocket(listenSock);
                        threadpool_destroy(pool, 0);
#ifdef _WIN32
                        WSACleanup();
#endif
                        exit(1);
                    }
                }
                else
                {
                    strncpy(recvBuf.rhost, inet_ntoa(sendAddr.sin_addr), sizeof(recvBuf.rhost));
                    recvBuf.rport = sendAddr.sin_port;
                    recvBuf.socket = acceptSock;

                    iResult = threadpool_add(pool, &modeResponseTCP_MT, &recvBuf, 0);
                    if (iResult != 0)
                    {
                        cout << "Exit: threadpool_add() failed on modeResponseTCP_MT with error: " << iResult << endl;
                        iResult = closesocket(acceptSock);
                        if (iResult == SOCKET_ERROR)
                        {
                            cout << "Exit: closesock() failed in main on acceptSock, mode RESPONSE, with error: " << WSAGetLastError() << endl;

                            closesocket(listenSock);
                            threadpool_destroy(pool, 0);
#ifdef _WIN32
                            WSACleanup();
#endif
                            exit(1);
                        }
                    }
                }
            }
            else if (recvBuf.mode == WRITE)
            { // server writes to client
                int recvPortBuf;
                if (recvBuf.proto == UDP)
                {
                    iResult = recv(acceptSock, (char *)&recvPortBuf, (int)sizeof(recvPortBuf), 0);
                    if (iResult == SOCKET_ERROR)
                    {
                        cout << "Exit: recv() failed on acceptSock, protocol UDP with error: " << WSAGetLastError() << endl;
                        closesocket(acceptSock);
                        closesocket(listenSock);
                        threadpool_destroy(pool, 0);
#ifdef _WIN32
                        WSACleanup();
#endif
                        exit(1);
                    }
                    // open UDP socket
                    int udpSock = 0;
                    udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    if (udpSock == SOCKET_ERROR)
                    {
                        cout << "Exit: socket() failed in main on udpSock, mode WRITE, with error: " << WSAGetLastError() << endl;
                        closesocket(acceptSock);
                        closesocket(listenSock);
                        threadpool_destroy(pool, 0);
#ifdef _WIN32
                        WSACleanup();
#endif
                        exit(1);
                    }

                    strncpy(recvBuf.rhost, inet_ntoa(sendAddr.sin_addr), sizeof(recvBuf.rhost));
                    recvBuf.rport = ntohs(recvPortBuf);
                    recvBuf.socket = udpSock;

                    iResult = threadpool_add(pool, &modeSendUDP_MT, &recvBuf, 0);
                    if (iResult != 0)
                    {
                        cout << "Exit: threadpool_add() failed on modeSendUDP_MT with error: " << iResult << endl;
                    }

                    iResult = closesocket(acceptSock);
                    if (iResult == SOCKET_ERROR)
                    {
                        cout << "Exit: closesock() failed in main on acceptSock, mode WRITE, with error: " << WSAGetLastError() << endl;

                        closesocket(udpSock);
                        closesocket(listenSock);
                        threadpool_destroy(pool, 0);
#ifdef _WIN32
                        WSACleanup();
#endif
                        exit(1);
                    }
                }
                else
                {
                    strncpy(recvBuf.rhost, inet_ntoa(sendAddr.sin_addr), sizeof(recvBuf.rhost));
                    recvBuf.rport = sendAddr.sin_port;
                    recvBuf.socket = acceptSock;

                    iResult = threadpool_add(pool, &modeSendTCP_MT, &recvBuf, 0);
                    if (iResult != 0)
                    {
                        cout << "Exit: threadpool_add() failed on modeSendTCP_MT with error: " << iResult << endl;

                        iResult = closesocket(acceptSock);
                        if (iResult == SOCKET_ERROR)
                        {
                            cout << "Exit: closesock() failed in main on acceptSock, mode WRITE, with error: " << WSAGetLastError() << endl;

                            closesocket(listenSock);
                            threadpool_destroy(pool, 0);
#ifdef _WIN32
                            WSACleanup();
#endif
                            exit(1);
                        }
                    }
                }
            }
            else if (recvBuf.mode == READ)
            { // server reads from client
                if (recvBuf.proto == UDP)
                {
                    // open UDP socket
                    int udpSock = 0;
                    udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    if (udpSock == SOCKET_ERROR)
                    {
                        cout << "Exit: socket() failed on udpSock, mode READ, with error: " << WSAGetLastError() << endl;
                        closesocket(acceptSock);
                        closesocket(listenSock);
                        threadpool_destroy(pool, 0);
#ifdef _WIN32
                        WSACleanup();
#endif
                        exit(1);
                    }
                    // enable reuse address
                    int flag = 1;
                    iResult = setsockopt(udpSock, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag));
                    if (iResult == SOCKET_ERROR)
                    {
                        cout << "Exit: setsockopt() failed on udpSock, mode READ, reuse address with error: " << WSAGetLastError() << endl;
                        closesocket(udpSock);
                        closesocket(acceptSock);
                        closesocket(listenSock);
                        threadpool_destroy(pool, 0);
#ifdef _WIN32
                        WSACleanup();
#endif
                        exit(1);
                    }

                    struct sockaddr_in udpRecvAddr;
                    memset(&udpRecvAddr, '\0', sizeof(udpRecvAddr));

                    udpRecvAddr.sin_family = AF_INET;
                    udpRecvAddr.sin_port = htons(rand() % 1000 + 1024);
                    udpRecvAddr.sin_addr.s_addr = INADDR_ANY;

                    do
                    {
                        iResult = bind(udpSock, (sockaddr *)&udpRecvAddr, sizeof(udpRecvAddr));
                        if (iResult == SOCKET_ERROR)
                        {
                            udpRecvAddr.sin_port = htons(rand() % 1000 + 1024);
                        }
                    } while (iResult == SOCKET_ERROR);

                    int sendBuf = udpRecvAddr.sin_port;
                    iResult = send(acceptSock, (char *)&sendBuf, sizeof(sendBuf), 0);
                    if (iResult == SOCKET_ERROR)
                    {
                        cout << "Exit: send() failed in main on acceptSock, mode READ, with error: " << WSAGetLastError() << endl;
                        closesocket(udpSock);
                        closesocket(acceptSock);
                        closesocket(listenSock);
                        threadpool_destroy(pool, 0);
#ifdef _WIN32
                        WSACleanup();
#endif
                        exit(1);
                    }
                    strncpy(recvBuf.rhost, inet_ntoa(sendAddr.sin_addr), sizeof(recvBuf.rhost));
                    recvBuf.rport = sendAddr.sin_port;
                    recvBuf.socket = udpSock;

                    iResult = threadpool_add(pool, &modeRecvUDP_MT, &recvBuf, 0);
                    if (iResult != 0)
                    {
                        cout << "Exit: threadpool_add() failed on modeRecvUDP_MT with error: " << iResult << endl;
                    }

                    iResult = closesocket(acceptSock);
                    if (iResult == SOCKET_ERROR)
                    {
                        cout << "Exit: closesock() on acceptSock, mode READ, failed with error: " << WSAGetLastError() << endl;

                        closesocket(udpSock);
                        closesocket(listenSock);
                        threadpool_destroy(pool, 0);
#ifdef _WIN32
                        WSACleanup();
#endif
                        exit(1);
                    }
                }
                else
                { // for TCP as protocol
                    strncpy(recvBuf.rhost, inet_ntoa(sendAddr.sin_addr), sizeof(recvBuf.rhost));
                    recvBuf.rport = sendAddr.sin_port;
                    recvBuf.socket = acceptSock;

                    iResult = threadpool_add(pool, &modeRecvTCP_MT, &recvBuf, 0);
                    if (iResult != 0)
                    {
                        cout << "Exit: threadpool_add() failed on modeRecvTCP_MT with error: " << iResult << endl;

                        iResult = closesocket(acceptSock);
                        if (iResult == SOCKET_ERROR)
                        {
                            cout << "Exit: closesock() failed in main on acceptSock, mode WRITE, with error: " << WSAGetLastError() << endl;

                            closesocket(listenSock);
                            threadpool_destroy(pool, 0);
#ifdef _WIN32
                            WSACleanup();
#endif
                            exit(1);
                        }
                    }
                }
            }
        }
    }

    closesocket(listenSock);
#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
