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

#include "client.h"

using namespace std;
using namespace std::chrono;

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#endif

static struct option long_opt[] =
    {
        {"send", no_argument, NULL, 'a'},
        {"recv", no_argument, NULL, 'b'},
        {"response", no_argument, NULL, 'c'},
        {"stat", required_argument, NULL, 'd'},
        {"rhost", required_argument, NULL, 'e'},
        {"rport", required_argument, NULL, 'f'},
        {"proto", required_argument, NULL, 'g'},
        {"pktsize", required_argument, NULL, 'i'},
        {"pktrate", required_argument, NULL, 'j'},
        {"pktnum", required_argument, NULL, 'k'},
        {"sbufsize", required_argument, NULL, 'l'},
        {"rbufsize", required_argument, NULL, 'o'},
        {0, 0, 0, 0}};

int main(int argc, char **argv)
{
    struct arguments arguments;

    assignDefault(&arguments);

    int option_val = 0;
    int opt_inedx = 0;

    bool pktrateFlag = true;

    string temp;

    char short_opt[] = "abcd:e:f:g:h:i:j:k:l:m:n:";

    while ((option_val = getopt_long_only(argc, argv, short_opt, long_opt, NULL)) != -1)
    {
        switch (option_val)
        {
        case 'a':
            if (arguments.mode == 0)
            {
                arguments.mode = 1;
            }
            else
            {
                cout << "Exit: Only one mode supported at a time" << endl;
            }
            break;

        case 'b':
            if (arguments.mode == 0)
            {
                arguments.mode = 2;
            }
            else
            {
                cout << "Exit: Only one mode supported at a time" << endl;
            }
            break;

        case 'c':
            if (arguments.mode == 0)
            {
                arguments.mode = 3;
            }
            else
            {
                cout << "Exit: Only one mode supported at a time" << endl;
            }
            break;

        case 'd':
            arguments.stat = atoi(optarg);
            if (arguments.stat < 0)
            {
                cout << "Exit: Negative value not suppoted for -stat" << endl;
                exit(1);
            }
            break;

        case 'e':
            strncpy(arguments.rhost, optarg, sizeof(arguments.rhost));
            break;

        case 'f':
            arguments.rport = atoi(optarg);
            if (arguments.rport < 1024)
            {
                cout << "Exit: Port number has to be larger than 1023." << endl;
                exit(1);
            }
            break;

        case 'g':

            temp = optarg;
            transform(temp.begin(), temp.end(), temp.begin(), ::toupper);
            if (strcmp(temp.c_str(), "TCP") == 0)
            {
                arguments.proto = TCP;
            }
            else if (strcmp(temp.c_str(), "UDP") == 0)
            {
                arguments.proto = UDP;
            }
            else
            {
                cout << "Exit: Incorrect protocol" << endl;
                exit(1);
            }
            break;

        case 'i':
            arguments.pktsize = atoi(optarg);
            if (arguments.pktsize < 0)
            {
                cout << "Exit: Negative value not suppoted for -pktsize" << endl;
                exit(1);
            }
            break;

        case 'j':
            arguments.pktrate = atoi(optarg);
            pktrateFlag = false;
            if (arguments.pktrate < 0)
            {
                cout << "Exit: Negative value not suppoted for -pktrate" << endl;
                exit(1);
            }
            break;

        case 'k':
            arguments.pktnum = atoi(optarg);
            if (arguments.pktnum < 0)
            {
                cout << "Exit: Negative value not suppoted for -pktnum" << endl;
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

        case 'o':
            arguments.rbufsize = atoi(optarg);
            if (arguments.rbufsize < 0)
            {
                cout << "Exit: Negative value not suppoted for -rbufsize" << endl;
                exit(1);
            }
            break;

        default:
            cout << "Exit: Incorrect arguments" << endl;
            exit(1);
        }
    }

    if (arguments.mode == 0)
    {
        displayUsage();
        exit(1);
    }
    else
    {
        printInfo(&arguments);
    }

    // variable for debugging
    int iResult;

// establish connection
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
    // set up socket for connecting server
    int sendSock = 0;
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

    struct hostent *hostent = gethostbyname(arguments.rhost);
    struct in_addr **addr_list = (struct in_addr **)hostent->h_addr_list;

    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(arguments.rport);
    recvAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*addr_list[0]));

    if (inet_ntoa(*addr_list[0]) != NULL)
    {
        cout << "TCP remote host " << arguments.rhost << " lookup successful, IP address is " << inet_ntoa(*addr_list[0]) << endl;
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
    // sending inputted arguments to server
    struct arguments sendBuf;
    memset(&sendBuf, '\0', sizeof(sendBuf));
    memcpy(&sendBuf, &arguments, sizeof(arguments));

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

    // declare udpSock in case for receiving data from sever
    int udpSock = 0;

    // for the case of using UDP as protocol
    if (arguments.proto == UDP)
    {
        // receiving server udpSock port number use for receiving
        if (arguments.mode == 1)
        {
            int recvBuf;

            iResult = recv(sendSock, (char *)&recvBuf, sizeof(recvBuf), MSG_WAITALL);
            if (iResult == SOCKET_ERROR)
            {
                cout << "Exit: recv() on failed acceptSock with error: " << WSAGetLastError() << endl;
                closesocket(sendSock);
#ifdef _WIN32
                WSACleanup();
#endif
                exit(1);
            }
            arguments.rport = ntohs(recvBuf);
        }
        else if (arguments.mode == 2)
        {
            // initiate UDP socket
            udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (udpSock == SOCKET_ERROR)
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
                iResult = bind(udpSock, (sockaddr *)&udpRecvAddr, sizeof(udpRecvAddr));
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
        }
        else if (arguments.mode == 3)
        {
            // initiate UDP socket
            udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (udpSock == SOCKET_ERROR)
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
                iResult = bind(udpSock, (sockaddr *)&udpRecvAddr, sizeof(udpRecvAddr));
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
        }
        // closing TCP socket connected to server
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

    switch (arguments.mode)
    {
    case 1:
        if (arguments.proto == TCP)
        {
            modeSendTCP(&arguments, sendSock);
        }
        else if (arguments.proto == UDP)
        {
            modeSendUDP(&arguments);
        }
        break;

    case 2:
        if (arguments.proto == TCP)
        {
            modeRecvTCP(&arguments, sendSock);
        }
        else if (arguments.proto == UDP)
        {
            modeRecvUDP(&arguments, udpSock);
        }
        break;

    case 3:
        if (pktrateFlag == true)
        {
            arguments.pktrate = 10;
        }
        if (arguments.proto == TCP)
        {
            modeResponseTCP(&arguments, sendSock);
        }
        else if (arguments.proto == UDP)
        {
            modeResponseUDP(&arguments, udpSock);
        }
        break;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
