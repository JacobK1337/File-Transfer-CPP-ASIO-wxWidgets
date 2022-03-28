# File Transfer System project
### File transfer system created using C++17, Asio C++ and wxWidgets

# Contents
#### The goal of the project was to create a system providing file transfer between the client and the server. The networking part of the project was implemented using Asio C++ library. The client application, with the help of wxWidgets, has been equipped with a graphical interface, supporting file browsing on the server side.
#### The user can view files and documents on the server side. Most importantly, he can select the files he wants to download or delete. Additionally, he can upload his own files to the server.
#### The server application does not have a graphical interface. It just works. More information on the implementation is provided below.

# How it works
## The basics
#### When it comes to sending requests and data or connecting to the server, it happens asynchronously. Asio provides many asynchronous features such as: *async_connect, async_accept, async_write, async_read*. I will not describe in detail how these functions work. You can read everything in the Asio documentation. At the moment, it is important that the names of these functions actually reflect very well what they actually do.
#### Establishing connection is pretty simple - client connects to the server (*async_connect*) and the server accepts client (*async_accept*). If connecting was successful, server creates a *connection* object which allows it to send responses to the client (*async_write*).
#### In the background, the received responses are asynchronously loaded (*async_read*), while checking whether they were actually received works iteratively. With the help of conditional variables and mutexes, it's not that resource-intensive.
## Requests and data
#### All information is sent in packets containing the header and the transmitted data in binary form.
#### Client requests are sent over the control connection, while data transfer from or to the server takes place over the data connection, so basically client tries to establish two connections at the start.
#### After the request is accepted, the server sends a unique identifier representing the aforementioned request. Given this identifier, data is transferred on the data connection. This can be complicated, although it introduces some kind of verification and of course takes the burden off the control connection.

#### Sending and uploading files is pretty intuitive. If the client requests to download a file, a file with the given name is created on his computer, and the application contains a pointer to that file. The server, however, after receiving the request, starts the data transfer. Virtually the same thing happens on the server side when uploading a file. 
#### Data transfer on the client and the server side is handled by another thread, which checks if there is still any data that needs to be sent. If not, with the help of *mutex* and *conditional variable*, he waits calmly.
##
#### Of course, a bit more things are happening in the app than described above. In any case, I think that's enough information anyway to know how it works more or less.
# Presentation
### For the purposes of the presentation, I installed Windows 10 on a virtual machine that acts as a server, while on my actual system I use a client application. In other words, the data transfer goes between the virtual machine and the host. In order to show how upload and download works, I will be using Ubuntu 20.04 ISO (2.6GB).


## Downloading files
#### In order to download files, the user should select the files he is interested in and then press the 'Save selected files' button. Double-clicking on a file also starts downloading it. 
![download-gif](https://user-images.githubusercontent.com/81765291/160424908-c2435118-2ad5-4675-a547-3e744ce4f0bc.gif)
##
#### After successful download, the user gets a log message. 
![download-successful-log](https://user-images.githubusercontent.com/81765291/160425206-c2654773-9e8d-4420-a283-0d0467d475c3.png)


## Uploading files
#### Uploading files is as simple as selecting the appropriate file, and then clicking the 'Upload chosen file' button.
![upload-gif](https://user-images.githubusercontent.com/81765291/160424955-fe9951c4-3bb3-4561-8122-1349d93a636e.gif)
##
#### After successful upload, the server and the client receive the appropriate message. 
![upload-successful-log](https://user-images.githubusercontent.com/81765291/160425879-3bfb2494-9c07-48ae-93b6-41bc01eb41d8.png)


## Deleting files
#### In order to delete files, simply select the appropriate files and press the DELETE button on the keyboard. It doesn't work for folders.
![delete-gif](https://user-images.githubusercontent.com/81765291/160419899-721c5a79-6993-42ea-9639-3ab578cee986.gif)

