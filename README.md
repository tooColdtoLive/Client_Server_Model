# Tony Li Kam Tong

# About demo videos
"netprobecli" and "netprobesrv" are executables on Linux.\
"netprobecli.exe" are executables on Windows.

# About compilation
On Linux, files are compiled with cmd "g++ -std=c++11 -o netprobesrv netprobesrv.cpp -pthread" wtih compiler g++ version 9.3.\
On Windows, files are compiled with cmd "g++ -o netprobecli netprobecli.cpp -lws2_32" with compiler downloaded from MinGW.

# About demo videos
There are two videos located inside folder "demo_videos". One is "srv_select", demoing server in select model, and the other is "srv_threadpool", demoing the server in thread pool model.\
Three clients were run in each video, with the three different client modes and two different protocols. \
In "srv_threadpool", you can see the server thread pool decreased in size. I have set the shrinking cycle to 3s in order to perform a short demo. The 60s cycle is restored in the final code.
