# Distributed-Socket-FileSystem
A distributed file system using C and socket programming. Clients interact with a main server (S1) to upload, download, delete, or list files. Based on file type, S1 routes files to sub-servers (S2, S3, S4). Everything runs through sockets, simulating a unified file system across multiple servers.
