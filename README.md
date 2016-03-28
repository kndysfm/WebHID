# WebHID
HTTP server to access HID

## For what
- HID evaluation program could be made by HTML as multiplatform app.
- You could test a remote HID device whose Host PC is in a same network.

## How to

Currently only for Windows

Web client must support WebSocket

1. Execute, then HTTP server starts to run
2. Access "http://localhost:8000/", you will get ".\bin\index.html"
3. Send HTTP request to "/hid/enumerate", 
you will get JSON data which includes a enumerated list of HID(s) being connected to your PC
4. The JSON has field named as "virtualPath" 
with a value like "/hid/0001/0123/abcd/0001/0002/" 
(These 4-digits hex values mean Interface#, VendorID, ProductID, UsagePage, and Usage of a HID I/F )
5. Try open a WebSocket connection to "{virtualPath}"
6. The server does NOT send data from itself.
So send an empty(or any) packet via WebSocket, 
then you would received HID input report(s)
7. Or if you send some binary data, 
it would be passed-through to the handle-opened HID I/F as HID output report

## Using Libraries
 This software depends on following C libraries:
 
* Mongoose (GPLv2, https://github.com/cesanta/mongoose)
  
  It is HTTP server-client library.
  This project is based on "restful_server" sample code of this.
  
* HID API (GPLv3, https://github.com/signal11/hidapi)
  
  Multiplatform API to access HID
  
* POSIX Threads for Windows (LGPLv2, https://sourceforge.net/projects/pthreads4w/)
  
  API for multiplatform threading 


