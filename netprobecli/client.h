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

#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define WSAGetLastError() (errno)
#define closesocket(x) close(x)
#define Sleep(x) usleep(x * 1000)
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

using namespace std;
using namespace std::chrono;

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#endif

#define TCP 0
#define UDP 1

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

void assignDefault(struct arguments *arguments)
{
    arguments->mode = 0;
    arguments->stat = 500;
    strncpy(arguments->rhost, "127.0.0.1", sizeof(arguments->rhost));
    arguments->rport = 4180;
    arguments->proto = 1;
    arguments->pktsize = 1000;
    arguments->pktrate = 1000;
    arguments->pktnum = -1;
    arguments->sbufsize = 0;
    arguments->rbufsize = 0;
    strncpy(arguments->lhost, "not specified", sizeof(arguments->lhost));
    arguments->lport = 4180;
    arguments->socket = 0;
}

void printInfo(struct arguments *arguments)
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
    else if (arguments->mode == 3)
    {
        cout << "RESPONSE";
    }
    // else
    // {
    //     cout << "HOST";
    // }
    cout << "    Protocol: ";
    if (arguments->proto == TCP)
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

void modeResponseTCP(struct arguments *arguments, int recvSock)
{
    // variable for debugging
    int iResult;

    struct sockaddr_in recvAddr;
    memset(&recvAddr, '\0', sizeof(recvAddr));

    struct hostent *hostent = gethostbyname(arguments->rhost);
    struct in_addr **addr_list = (struct in_addr **)hostent->h_addr_list;

    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(arguments->rport);
    recvAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*addr_list[0]));

    if (inet_ntoa(*addr_list[0]) != NULL)
    {
        cout << "TCP remote host " << arguments->rhost << " lookup successful, IP address is " << inet_ntoa(*addr_list[0]) << endl;
    }
    else
    {
        cout << "Exit: Remote host lookup failed";
        closesocket(recvSock);
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }

    double recvPktNum = 0, recvByte = 0, pktLossNum = 0;
    char recvBuf[65536];

    double recvInter = 0, recvInterTotal = 0, jitter = 0, jitterTotal = 0, recvInterMax = 0, recvInterMin = 0;
    // timer for jitter
    auto interStart = high_resolution_clock::now();
    auto interStop = high_resolution_clock::now();
    auto interDuration = duration_cast<milliseconds>(interStop - interStart);
    // timer for update interval
    auto updateStart = high_resolution_clock::now();
    // timer for elapsed time
    auto start = high_resolution_clock::now();
    while (recvPktNum < arguments->pktnum || arguments->pktnum == -1)
    {
        memset(recvBuf, '\0', sizeof(recvBuf));
        iResult = recv(recvSock, recvBuf, arguments->pktsize, MSG_WAITALL);
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
                auto stop = high_resolution_clock::now();
                auto duration = duration_cast<milliseconds>(stop - start);

                cout << "\r"
                     << "R: Elapsed [" << duration.count() / 1000 << "s] Replies [" << recvPktNum << "] Min [" << recvInterMin << "ms] Max [" << recvInterMax << "ms] Avg [" << recvInterTotal / recvPktNum << "ms] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

                cout << endl;
                cout << "Exit: Connection closed" << endl;
                closesocket(recvSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
            else
            {
                auto stop = high_resolution_clock::now();
                auto duration = duration_cast<milliseconds>(stop - start);

                cout << "\r"
                     << "R: Elapsed [" << duration.count() / 1000 << "s] Replies [" << recvPktNum << "] Min [" << recvInterMin << "ms] Max [" << recvInterMax << "ms] Avg [" << recvInterTotal / recvPktNum << "ms] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

                cout << endl;
                cout << "Exit: recv() failed with error: " << WSAGetLastError() << endl;
                closesocket(recvSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
        }
        // general data update
        recvPktNum++;
        recvByte += iResult;

        // jitter value update
        if (recvPktNum == 2)
        {
            recvInterMax = interDuration.count();
            recvInterMin = interDuration.count();
        }
        if (recvPktNum > 1)
        {
            recvInter = interDuration.count();
            if (recvInter > recvInterMax)
            {
                recvInterMax = recvInter;
            }
            else if (recvInter < recvInterMin)
            {
                recvInterMin = recvInter;
            }
            recvInterTotal += recvInter;
            jitter = abs(recvInter - (recvInterTotal / recvPktNum));
            jitterTotal += jitter;
        }
        // check if update display
        auto updateStop = high_resolution_clock::now();
        auto updateDuration = duration_cast<milliseconds>(updateStop - updateStart);
        if (updateDuration.count() >= arguments->stat || recvPktNum == 1)
        {
            auto stop = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(stop - start);

            cout << "\r"
                 << "R: Elapsed [" << duration.count() / 1000 << "s] Replies [" << recvPktNum << "] Min [" << recvInterMin << "ms] Max [" << recvInterMax << "ms] Avg [" << recvInterTotal / recvPktNum << "ms] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

            updateStart = high_resolution_clock::now();
        }

        // break if received all pkts
        if (recvPktNum == arguments->pktnum)
        {
            break;
        }

        iResult = closesocket(recvSock);
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: closesock() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        // rate limit
        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(stop - start);
        while (recvPktNum * 1000 / duration.count() > arguments->pktrate && arguments->pktrate != 0)
        {
            stop = high_resolution_clock::now();
            duration = duration_cast<milliseconds>(stop - start);
        }

        // open new connection
        recvSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (recvSock == SOCKET_ERROR)
        {
            cout << "Exit: socket() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }

        iResult = connect(recvSock, (sockaddr *)&recvAddr, sizeof(recvAddr));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: connect() failed with error: " << WSAGetLastError() << endl;
            closesocket(recvSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }

        struct arguments sendBuf;
        memset(&sendBuf, '\0', sizeof(sendBuf));
        memcpy(&sendBuf, arguments, sizeof(sendBuf));

        iResult = send(recvSock, (char *)&sendBuf, sizeof(sendBuf), 0);
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: send() failed with error: " << WSAGetLastError() << endl;
            closesocket(recvSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
    }

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

    cout << "\r"
         << "R: Elapsed [" << duration.count() / 1000 << "s] Replies [" << recvPktNum << "] Min [" << recvInterMin << "ms] Max [" << recvInterMax << "ms] Avg [" << recvInterTotal / recvPktNum << "ms] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

    cout << endl;

    iResult = closesocket(recvSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void modeResponseUDP(struct arguments *arguments, int recvSock)
{
    // variable for debugging
    int iResult;

    int sendSock = 0;

    double recvPktNum = 0, recvByte = 0, pktLossNum = 0;
    char recvBuf[65536];

    struct sockaddr_in sendAddr;
    memset(&sendAddr, '\0', sizeof(sendAddr));
    int sendAddrSize = sizeof(sendAddr);

    double recvInter = 0, recvInterTotal = 0, jitter = 0, jitterTotal = 0, recvInterMax = 0, recvInterMin = 0;
    // timer for jitter
    auto interStart = high_resolution_clock::now();
    auto interStop = high_resolution_clock::now();
    auto interDuration = duration_cast<milliseconds>(interStop - interStart);
    // timer for update interval
    auto updateStart = high_resolution_clock::now();
    // timer for elapsed time
    auto start = high_resolution_clock::now();
    while (recvPktNum < arguments->pktnum || arguments->pktnum == -1)
    {
        memset(recvBuf, '\0', arguments->pktsize);
        iResult = recvfrom(recvSock, recvBuf, arguments->pktsize, 0, (sockaddr *)&sendAddr, (socklen_t *)&sendAddrSize);
        // jitter timer update
        if (recvPktNum > 0)
        {
            interStop = high_resolution_clock::now();
            interDuration = duration_cast<milliseconds>(interStop - interStart);
        }
        interStart = high_resolution_clock::now();

        if (iResult == SOCKET_ERROR)
        {
            auto stop = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(stop - start);

            cout << "\r"
                 << "R: Elapsed [" << duration.count() / 1000 << "s] Replies [" << recvPktNum << "] Min [" << recvInterMin << "ms] Max [" << recvInterMax << "ms] Avg [" << recvInterTotal / recvPktNum << "ms] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

            cout << endl;
            cout << "Exit: recvfrom() failed with error: " << WSAGetLastError() << endl;
            closesocket(recvSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        // general data update
        recvPktNum++;
        recvByte += iResult;

        // jitter value update
        if (recvPktNum == 2)
        {
            recvInterMax = interDuration.count();
            recvInterMin = interDuration.count();
        }
        if (recvPktNum > 1)
        {
            recvInter = interDuration.count();
            if (recvInter > recvInterMax)
            {
                recvInterMax = recvInter;
            }
            else if (recvInter < recvInterMin)
            {
                recvInterMin = recvInter;
            }
            recvInterTotal += recvInter;
            jitter = abs(recvInter - (recvInterTotal / recvPktNum));
            jitterTotal += jitter;
        }
        // check if update display
        auto updateStop = high_resolution_clock::now();
        auto updateDuration = duration_cast<milliseconds>(updateStop - updateStart);
        if (updateDuration.count() >= arguments->stat || recvPktNum == 1 || iResult == 0)
        {
            auto stop = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(stop - start);

            cout << "\r"
                 << "R: Elapsed [" << duration.count() / 1000 << "s] Replies [" << recvPktNum << "] Min [" << recvInterMin << "ms] Max [" << recvInterMax << "ms] Avg [" << recvInterTotal / recvPktNum << "ms] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

            updateStart = high_resolution_clock::now();
        }

        // break if received all pkts
        if (recvPktNum == arguments->pktnum)
        {
            break;
        }

        iResult = closesocket(recvSock);
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: closesock() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        // rate limit
        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(stop - start);
        while (recvPktNum * 1000 / duration.count() > arguments->pktrate && arguments->pktrate != 0)
        {
            stop = high_resolution_clock::now();
            duration = duration_cast<milliseconds>(stop - start);
        }

        // set up socket for connecting server
        sendSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sendSock == SOCKET_ERROR)
        {
            cout << "Exit: socket() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }

        struct sockaddr_in recvAddr;
        memset(&recvAddr, '\0', sizeof(recvAddr));

        struct hostent *hostent = gethostbyname(arguments->rhost);
        struct in_addr **addr_list = (struct in_addr **)hostent->h_addr_list;

        recvAddr.sin_family = AF_INET;
        recvAddr.sin_port = htons(arguments->rport);
        recvAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*addr_list[0]));

        if (inet_ntoa(*addr_list[0]) != NULL)
        {
        }
        else
        {
            cout << "Exit: Remote host lookup failed";
            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        // connecting server
        iResult = connect(sendSock, (sockaddr *)&recvAddr, sizeof(recvAddr));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: connect() failed with error: " << WSAGetLastError() << endl;
            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }

        struct arguments sendBuf;
        memset(&sendBuf, '\0', sizeof(sendBuf));
        memcpy(&sendBuf, arguments, sizeof(sendBuf));

        iResult = send(sendSock, (char *)&sendBuf, sizeof(sendBuf), 0);
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: send() failed with error: " << WSAGetLastError() << endl;
            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }

        // initiate UDP socket
        recvSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (recvSock == SOCKET_ERROR)
        {
            cout << "Exit: socket() failed on udpSock with error: " << WSAGetLastError() << endl;
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
        // loop in case the assigned port is in use
        do
        {
            iResult = bind(recvSock, (sockaddr *)&udpRecvAddr, sizeof(udpRecvAddr));
            if (iResult == SOCKET_ERROR)
            {
                udpRecvAddr.sin_port = htons(rand() % 1000 + 1024);
            }
        } while (iResult == SOCKET_ERROR);

        // send the listening port number to server
        int sendPortBuf = udpRecvAddr.sin_port;
        iResult = send(sendSock, (char *)&sendPortBuf, sizeof(sendPortBuf), 0);
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: send() failed on sendSock with error: " << WSAGetLastError() << endl;
            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        // close TCP connected socket
        iResult = closesocket(sendSock);
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: closesock() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
    }

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

    cout << "\r"
         << "R: Elapsed [" << duration.count() / 1000 << "s] Replies [" << recvPktNum << "] Min [" << recvInterMin << "ms] Max [" << recvInterMax << "ms] Avg [" << recvInterTotal / recvPktNum << "ms] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

    cout << endl;

    iResult = closesocket(recvSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void modeSendTCP(struct arguments *arguments, int sendSock)
{
    // variable for debugging
    int iResult;
    // set sender buffer size
    int sendBufSize;
    if (arguments->sbufsize == 0)
    {
        sendBufSize = 65536;
        iResult = setsockopt(sendSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, sizeof(sendBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() on buffer size failed with error: " << WSAGetLastError() << endl;
            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Default send buffer size = 65536 bytes" << endl;
        }
    }
    else
    {
        sendBufSize = arguments->sbufsize;
        iResult = setsockopt(sendSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, sizeof(sendBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() on buffer size failed with error: " << WSAGetLastError() << endl;
            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Manual send buffer size = " << arguments->sbufsize << " bytes" << endl;
        }
    }

    //int optlen = sizeof(sendBufSize);
    //getsockopt(sendSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, &optlen);
    //cout << "check send buffer size: " << sendBufSize << endl;

    cout << "Default recv buffer size = 65536 bytes" << endl;

    // cout << "Connecting to remote host " << arguments->rhost << " ... ";
    // cout << "success." << endl;

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
    while (sentPktNum < arguments->pktnum || arguments->pktnum == -1)
    {
        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(stop - start);
        // check if rate exceeds limit
        if (arguments->pktrate != 0 && sentByte != 0 && ((double)(sentByte / (double)(duration.count() / 1000)) >= arguments->pktrate))
        {
            auto updateStop = high_resolution_clock::now();
            auto updateDuration = duration_cast<milliseconds>(updateStop - updateStart);
            // chceck if update display
            if (updateDuration.count() >= arguments->stat || sentPktNum == arguments->pktnum)
            {
                stop = high_resolution_clock::now();
                duration = duration_cast<milliseconds>(stop - start);
                double rate = (double)(sentByte / 1024 / (double)(duration.count() / 1000));

                cout << "\r"
                     << "S: Elapsed [" << duration.count() / 1000 << "s] Rate [" << setprecision(3) << rate << "kbps]         " << flush;
                updateStart = high_resolution_clock::now();
            }
            // skip the later actions as rate exceeds limit
            continue;
        }

        iResult = send(sendSock, sendBuf, arguments->pktsize, 0);
        if (iResult == SOCKET_ERROR)
        {
            stop = high_resolution_clock::now();
            duration = duration_cast<milliseconds>(stop - start);
            double rate = (double)(sentByte / 1024 / (double)(duration.count() / 1000));

            cout << "\r"
                 << "S: Elapsed [" << duration.count() / 1000 << "s] Rate [" << setprecision(3) << rate << "kbps]         " << flush;

            cout << endl;

            cout << "Exit: send() failed with error: " << WSAGetLastError() << endl;
            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            sentPktNum++;
            sentByte += iResult;
            // DEBUG
            // cout << "sentByte = " << sentByte << "   sendBuf = " << sendBuf << endl;
            memset(sendBuf, '\0', sizeof(sendBuf));
            sendData++;
            sendDataStr = to_string(sendData);
            strcpy(sendBuf, sendDataStr.c_str());

            auto updateStop = high_resolution_clock::now();
            auto updateDuration = duration_cast<milliseconds>(updateStop - updateStart);
            // chceck if update display
            if (updateDuration.count() >= arguments->stat || sentPktNum == 1 || sentPktNum == arguments->pktnum)
            {
                stop = high_resolution_clock::now();
                duration = duration_cast<milliseconds>(stop - start);
                double rate = (double)(sentByte / 1024 / (double)(duration.count() / 1000));

                cout << "\r"
                     << "S: Elapsed [" << duration.count() / 1000 << "s] Rate [" << setprecision(3) << rate << "kbps]         " << flush;
                updateStart = high_resolution_clock::now();
            }
        }
    }

    cout << endl;

    iResult = closesocket(sendSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void modeSendUDP(struct arguments *arguments)
{
    int sendSock = 0;
    sendSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sendSock == SOCKET_ERROR)
    {
        cout << "Exit: socket() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }

    struct sockaddr_in sendAddr;
    memset(&sendAddr, '\0', sizeof(sendAddr));

    struct sockaddr_in recvAddr;
    memset(&recvAddr, '\0', sizeof(recvAddr));

    if (strcmp(arguments->rhost, "not specified") != 0)
    {
        struct hostent *hostent = gethostbyname(arguments->rhost);
        struct in_addr **addr_list = (struct in_addr **)hostent->h_addr_list;

        recvAddr.sin_family = AF_INET;
        recvAddr.sin_port = htons(arguments->rport);
        recvAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*addr_list[0]));

        if (inet_ntoa(*addr_list[0]) != NULL)
        {
            cout << "UDP remote host " << arguments->rhost << " lookup successful, IP address is " << inet_ntoa(*addr_list[0]) << endl;
        }
        else
        {
            cout << "Exit: Local host lookup failed" << endl;

            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
    }
    else
    {
        recvAddr.sin_family = AF_INET;
        recvAddr.sin_port = htons(arguments->rport);
        recvAddr.sin_addr.s_addr = INADDR_ANY;
    }

    // variable for debugging
    int iResult;

    // set sender buffer size
    int sendBufSize;
    if (arguments->sbufsize == 0)
    {
        sendBufSize = 65536;
        iResult = setsockopt(sendSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, sizeof(sendBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() on buffer size failed with error: " << WSAGetLastError() << endl;
            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Default send buffer size = 65536 bytes" << endl;
        }
    }
    else
    {
        sendBufSize = arguments->sbufsize;
        iResult = setsockopt(sendSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, sizeof(sendBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() on buffer size failed with error: " << WSAGetLastError() << endl;
            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Manual send buffer size = " << arguments->sbufsize << " bytes" << endl;
        }
    }

    // int optlen = sizeof(sendBufSize);
    // getsockopt(sendSock, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufSize, &optlen);
    // cout << "check send buffer size: " << sendBufSize << endl;

    cout << "Default recv buffer size = 65536 bytes" << endl;

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
    while (sentPktNum < arguments->pktnum || arguments->pktnum == -1)
    {
        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(stop - start);
        // check if rate exceeds limit
        if (arguments->pktrate != 0 && sentByte != 0 && ((double)(sentByte / (double)(duration.count() / 1000)) >= arguments->pktrate))
        {
            auto updateStop = high_resolution_clock::now();
            auto updateDuration = duration_cast<milliseconds>(updateStop - updateStart);
            // chceck if update display
            if (updateDuration.count() >= arguments->stat || sentPktNum == arguments->pktnum)
            {
                stop = high_resolution_clock::now();
                duration = duration_cast<milliseconds>(stop - start);
                double rate = (double)(sentByte / 1024 / (double)(duration.count() / 1000));

                cout << "\r"
                     << "S: Elapsed [" << duration.count() / 1000 << "s] Rate [" << setprecision(3) << rate << "kbps]         " << flush;
                updateStart = high_resolution_clock::now();
            }
            // skip the later actions as rate exceeds limit
            continue;
        }

        iResult = sendto(sendSock, sendBuf, arguments->pktsize, 0, (sockaddr *)&recvAddr, sizeof(recvAddr));
        if (iResult == SOCKET_ERROR)
        {
            stop = high_resolution_clock::now();
            duration = duration_cast<milliseconds>(stop - start);
            double rate = (double)(sentByte / 1024 / (double)(duration.count() / 1000));

            cout << "\r"
                 << "S: Elapsed [" << duration.count() / 1000 << "s] Rate [" << setprecision(3) << rate << "kbps]         " << flush;

            cout << endl;

            cout << "Exit: sendto() failed with error: " << WSAGetLastError() << endl;
            closesocket(sendSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            sentPktNum++;
            sentByte += iResult;
            memset(sendBuf, '\0', arguments->pktsize);
            sendData++;
            if (sentPktNum < arguments->pktnum - 1 || arguments->pktnum == -1)
            {
                sendDataStr = to_string(sendData);
                strcpy(sendBuf, sendDataStr.c_str());
            }
            else
            {
                strcpy(sendBuf, "-999");
            }

            auto updateStop = high_resolution_clock::now();
            auto updateDuration = duration_cast<milliseconds>(updateStop - updateStart);
            // chceck if update display
            if (updateDuration.count() >= arguments->stat || sentPktNum == 1 || sentPktNum == arguments->pktnum)
            {
                stop = high_resolution_clock::now();
                duration = duration_cast<milliseconds>(stop - start);
                double rate = (double)(sentByte / 1024 / (double)(duration.count() / 1000));

                cout << "\r"
                     << "S: Elapsed [" << duration.count() / 1000 << "s] Rate [" << setprecision(3) << rate << "kbps]         " << flush;
                updateStart = high_resolution_clock::now();
            }
        }
    }

    cout << endl;

    iResult = closesocket(sendSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void modeRecvTCP(struct arguments *arguments, int recvSock)
{
    cout << "Default send buffer size = 65536 bytes" << endl;
    // variable for debugging
    int iResult;
    // set receiver buffer size
    int recvBufSize;
    if (arguments->rbufsize == 0)
    {
        recvBufSize = 65536;
        iResult = setsockopt(recvSock, SOL_SOCKET, SO_RCVBUF, (char *)&recvBufSize, sizeof(recvBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() on buffer size failed with error: " << WSAGetLastError() << endl;
            closesocket(recvSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Default recv buffer size = 65536 bytes" << endl;
        }
    }
    else
    {
        recvBufSize = arguments->rbufsize;
        iResult = setsockopt(recvSock, SOL_SOCKET, SO_RCVBUF, (char *)&recvBufSize, sizeof(recvBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() on buffer size failed with error: " << WSAGetLastError() << endl;
            closesocket(recvSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Manual recv buffer size = " << arguments->rbufsize << " bytes" << endl;
        }
    }

    // cout << "Binding local socket to port 4180";
    // if(strcmp(arguments->lhost.c_str(), "not specified") == 0){
    //     cout << " using late binding ... ";
    // }else{
    //     cout << sendAddr.sin_port << " and sender address " << inet_ntoa(sendAddr.sin_addr) << " " << endl;
    // }

    // cout << "Receiving from [" << arguments->rhost << ":" << arguments->rport << "]:" << endl;

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
    // timer for update interval
    auto updateStart = high_resolution_clock::now();
    // timer for elapsed time
    auto start = high_resolution_clock::now();
    while (iResult > 0)
    {
        memset(recvBuf, '\0', sizeof(recvBuf));
        iResult = recv(recvSock, recvBuf, arguments->pktsize, MSG_WAITALL);
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
                auto stop = high_resolution_clock::now();
                auto duration = duration_cast<milliseconds>(stop - start);
                double rate = (double)(recvByte / 1000 / (double)(duration.count() / 1000));
                double lossRate = (double)pktLossNum / (double)recvPktNum * 100;

                cout << "\r"
                     << "R: Elapsed [" << duration.count() / 1000 << "s] Pkts [" << recvPktNum << "] Lost [" << pktLossNum << ", " << lossRate << "%] Rate [" << setprecision(3) << rate << "kbps] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

                cout << endl;
                cout << "Exit: Connection closed" << endl;
                closesocket(recvSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
            else
            {
                auto stop = high_resolution_clock::now();
                auto duration = duration_cast<milliseconds>(stop - start);
                double rate = (double)(recvByte / 1000 / (double)(duration.count() / 1000));
                double lossRate = (double)pktLossNum / (double)recvPktNum * 100;

                cout << "\r"
                     << "R: Elapsed [" << duration.count() / 1000 << "s] Pkts [" << recvPktNum << "] Lost [" << pktLossNum << ", " << lossRate << "%] Rate [" << setprecision(3) << rate << "kbps] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

                cout << endl;
                cout << "Exit: recv() failed with error: " << WSAGetLastError() << endl;
                closesocket(recvSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
        }
        // packet loss check
        if (strcmp(recvBuf, "") == 0 || atoi(recvBuf) < 0)
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
        // DEBUG
        // cout << "recvByte = " << recvByte << "   recvBuf = '" << recvBuf << "'" << endl;

        // jitter value update
        if (recvPktNum > 1)
        {
            recvInter = interDuration.count();
            recvInterTotal += recvInter;
            jitter = abs(recvInter - (recvInterTotal / recvPktNum));
            jitterTotal += jitter;
        }
        // check if update display
        auto updateStop = high_resolution_clock::now();
        auto updateDuration = duration_cast<milliseconds>(updateStop - updateStart);
        if (updateDuration.count() >= arguments->stat || recvPktNum == 1)
        {
            auto stop = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(stop - start);
            double rate = (double)(recvByte / 1000 / (double)(duration.count() / 1000));
            double lossRate = (double)pktLossNum / (double)recvPktNum * 100;

            cout << "\r"
                 << "R: Elapsed [" << duration.count() / 1000 << "s] Pkts [" << recvPktNum << "] Lost [" << pktLossNum << ", " << lossRate << "%] Rate [" << setprecision(3) << rate << "kbps] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

            updateStart = high_resolution_clock::now();
        }
    }

    iResult = closesocket(recvSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void modeRecvUDP(struct arguments *arguments, int recvSock)
{
    cout << "Default send buffer size = 65536 bytes" << endl;
    // variable for debugging
    int iResult;
    // set receiver buffer size
    int recvBufSize;
    if (arguments->rbufsize == 0)
    {
        recvBufSize = 65536;
        iResult = setsockopt(recvSock, SOL_SOCKET, SO_RCVBUF, (char *)&recvBufSize, sizeof(recvBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() on buffer size failed with error: " << WSAGetLastError() << endl;
            closesocket(recvSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Default recv buffer size = 65536 bytes" << endl;
        }
    }
    else
    {
        recvBufSize = arguments->rbufsize;
        iResult = setsockopt(recvSock, SOL_SOCKET, SO_RCVBUF, (char *)&recvBufSize, sizeof(recvBufSize));
        if (iResult == SOCKET_ERROR)
        {
            cout << "Exit: setsockopt() on buffer size failed with error: " << WSAGetLastError() << endl;
            closesocket(recvSock);
#ifdef _WIN32
            WSACleanup();
#endif
            exit(1);
        }
        else
        {
            cout << "Manual recv buffer size = " << arguments->rbufsize << " bytes" << endl;
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

    int endData = -999;
    string endDataStr = to_string(endData);

    iResult = 1;
    double recvInter = 0, recvInterTotal = 0, jitter = 0, jitterTotal = 0;
    // timer for jitter
    auto interStart = high_resolution_clock::now();
    auto interStop = high_resolution_clock::now();
    auto interDuration = duration_cast<milliseconds>(interStop - interStart);
    // timer for update interval
    auto updateStart = high_resolution_clock::now();
    // timer for elapsed time
    auto start = high_resolution_clock::now();
    while (iResult > 0)
    {
        memset(recvBuf, '\0', arguments->pktsize);
        iResult = recvfrom(recvSock, recvBuf, arguments->pktsize, 0, (sockaddr *)&sendAddr, (socklen_t *)&sendAddrSize);
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

        if (iResult == SOCKET_ERROR || strcmp("-999", recvBuf) == 0)
        {
            if (strcmp("-999", recvBuf) == 0 || iResult == 0)
            {
                auto stop = high_resolution_clock::now();
                auto duration = duration_cast<milliseconds>(stop - start);
                double rate = (double)(recvByte / 1000 / (double)(duration.count() / 1000));
                double lossRate = (double)pktLossNum / (double)recvPktNum * 100;

                cout << "\r"
                     << "R: Elapsed [" << duration.count() / 1000 << "s] Pkts [" << recvPktNum << "] Lost [" << pktLossNum << ", " << lossRate << "%] Rate [" << setprecision(3) << rate << "kbps] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

                cout << endl;
                cout << "Exit: Connection closed" << endl;
                closesocket(recvSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
            else
            {
                auto stop = high_resolution_clock::now();
                auto duration = duration_cast<milliseconds>(stop - start);
                double rate = (double)(recvByte / 1000 / (double)(duration.count() / 1000));
                double lossRate = (double)pktLossNum / (double)recvPktNum * 100;

                cout << "\r"
                     << "R: Elapsed [" << duration.count() / 1000 << "s] Pkts [" << recvPktNum << "] Lost [" << pktLossNum << ", " << lossRate << "%] Rate [" << setprecision(3) << rate << "kbps] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

                cout << endl;
                cout << "Exit: recvfrom() failed with error: " << WSAGetLastError() << endl;
                closesocket(recvSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
        }

        if (recvPktNum == 1)
        {
            cout << "connected to " << inet_ntoa(sendAddr.sin_addr) << " port " << sendAddr.sin_port << endl;
            cout << endl;
            cout << "Receiving from [" << inet_ntoa(sendAddr.sin_addr) << ":" << sendAddr.sin_port << "]:" << endl;
        }
        // packet loss check
        if (strcmp(recvBuf, "") == 0 || atoi(recvBuf) < 0)
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
        // jitter value update
        if (recvPktNum > 1)
        {
            recvInter = interDuration.count();
            recvInterTotal += recvInter;
            jitter = abs(recvInter - (recvInterTotal / recvPktNum));
            jitterTotal += jitter;
        }
        // check if update display
        auto updateStop = high_resolution_clock::now();
        auto updateDuration = duration_cast<milliseconds>(updateStop - updateStart);
        if (updateDuration.count() >= arguments->stat || recvPktNum == 1 || iResult == 0)
        {
            auto stop = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(stop - start);
            double rate = (double)(recvByte / 1000 / (double)(duration.count() / 1000));
            double lossRate = (double)pktLossNum / ((double)recvPktNum + (double)pktLossNum) * 100;

            cout << "\r"
                 << "R: Elapsed [" << duration.count() / 1000 << "s] Pkts [" << recvPktNum << "] Lost [" << pktLossNum << ", " << lossRate << "%] Rate [" << setprecision(3) << rate << "kbps] Jitter [" << jitterTotal / recvPktNum << "ms]        " << flush;

            updateStart = high_resolution_clock::now();
        }
    }

    iResult = closesocket(recvSock);
    if (iResult == SOCKET_ERROR)
    {
        cout << "Exit: closesock() failed with error: " << WSAGetLastError() << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        exit(1);
    }
}

void displayUsage()
{
    cout << "NetProbeCli [mode], see below:" << endl;
    cout << "       -send means sending mode;" << endl;
    cout << "       -recv means receiving mode;" << endl;
    // cout << "       -host means host information mode." << endl;
    cout << "       -response means response time mode." << endl;
    cout << endl;
    cout << "NetProbeCli <parameters>, see below:" << endl;
    cout << "   <-stat yyy>         update statistics once every yyy ms. (Default = 500 ms)" << endl;
    cout << "   <-rhost hostname>   send data to host specified by hostname. (Default 'localhost')" << endl;
    cout << "   <-rport portnum>    send data to remote host at port number portnum. (Default '4180')" << endl;
    cout << "   <-proto [tcp|udp]>          send data using TCP or UDP. (Default UDP)" << endl;
    cout << "   <-pktsize bsize>    send message of bsize bytes. (Default 1000 bytes)" << endl;
    cout << "   <-pktrate txrate>   [send|recv] mode: data rate of txrate bytes per second (bps, Default 1000bps)," << endl;
    cout << "                       [response] mode: request rate (per second, Default 10/s)," << endl;
    cout << "                       0 means as fast as possible. (Default 1000 bytes/second)" << endl;
    cout << "   <-pktnum num>       send or receive a total of num messages. (Default = infinite)" << endl;
    cout << "   <-sbufsize bsize>   set the outgoing socket buffer size to bsize bytes." << endl;
    cout << "   <-rbufsize bsize>   set the incoming socket buffer size to bsize bytes." << endl;
}
