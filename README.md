# About project
This is a server-client model simulating real world server client connection, supporting both udp and tcp protocol.\

# About server
The server can be run on linux. It is designed to be concurrent, receiving, sending data and accepting new clients at the same time. It has two modes, select mode and thread pool mode. For thread pool mode, the server doubles the number of available threads once the pool is exhausted, and havles when the utilization of threads is kept under 50% for 1 minute. The maximum size of the thread pool is 512.\

# About client
The client can run on both windows and linux. It is designed to have three modes, "recv" which receives data from connected server, "send" which sends data to server, and "response" which measures the time for every connection to server.\

# Command-line arguments
  Netprobesrv\
  <-sbufsize bsize>                   set the outgoing socket buffer size to bsize bytes.\
  <-rbufsize bsize>                   set the incoming socket buffer size to bsize bytes.\
  <-lhost hostname>                   hostname to bind to. (Default late binding)\
  <-lport portnum>                    port number to bind to. (Default '4180')\
  <-servermodel [select|threadpool]   set the concurrent server model to either select()-based or thread pool\
  <-poolsize psize                    set the initial thread pool size (default 8 threads), valid for thread pool server model only\
  
  Netprobecli\
  [mode], see below:\
    -send sending mode.\
    -recv receiving mode.\
    -response means response time mode.\
  <parameters>, see below:\
  <-stat yyy>         update statistics once every yyy ms. (Default = 500 ms).\
  <-rhost hostname>   send data to host specified by hostname. (Default 'localhost').\
  <-rport portnum>    send data to remote host at port number portnum. (Default '4180').\
  <-proto [tcp|udp]>          send data using TCP or UDP. (Default UDP).\
  <-pktsize bsize>    send message of bsize bytes. (Default 1000 bytes).\
  <-pktrate txrate>   [send|recv] mode: data rate of txrate bytes per second (bps, Default 1000bps),\
                      [response] mode: request rate (per second, Default 10/s),\
                      0 means as fast as possible. (Default 1000 bytes/second).\
  <-pktnum num>       send or receive a total of num messages. (Default = infinite).\
  <-sbufsize bsize>   set the outgoing socket buffer size to bsize bytes.\
  <-rbufsize bsize>   set the incoming socket buffer size to bsize bytes.\

# About executables
"netprobecli" and "netprobesrv" are executables on Linux.\
"netprobecli.exe" are executables on Windows.

# About compilation
On Linux, files are compiled with cmd "g++ -std=c++11 -o netprobesrv netprobesrv.cpp -pthread" wtih compiler g++ version 9.3.\
On Windows, files are compiled with cmd "g++ -o netprobecli netprobecli.cpp -lws2_32" with compiler downloaded from MinGW.
