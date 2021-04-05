# About project
This is a server-client model simulating real world server client connection, supporting both udp and tcp protocol.\\
The server can be run on linux. It is designed to be concurrent, receiving, sending data and accepting new clients at the same time. It has two modes, select mode and thread pool mode. For thread pool mode, the server doubles the number of available threads once the pool is exhausted, and havles when the utilization of threads is kept under 50% for 1 minute. The maximum size of the thread pool is 512.\\
The client can run on both windows and linux. It is designed to have three modes, "recv" which receives data from connected server, "send" which sends data to server, and "response" which measures the time for every connection to server.\\


# About executables
"netprobecli" and "netprobesrv" are executables on Linux.\
"netprobecli.exe" are executables on Windows.

# About compilation
On Linux, files are compiled with cmd "g++ -std=c++11 -o netprobesrv netprobesrv.cpp -pthread" wtih compiler g++ version 9.3.\
On Windows, files are compiled with cmd "g++ -o netprobecli netprobecli.cpp -lws2_32" with compiler downloaded from MinGW.
