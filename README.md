# WebHID
HTTP server to access HID

## How to

Currently only for Windows

Web client must support WebSocket

1. Execute, then HTTP server start to run
2. Access "http://localhost:8000/", you can get "index.html"
3. Send GET request to "^/enumerate", 
you would get JSON data which enumerates HID(s) connected to your PC
4. The JSON has field named as "virtualPath" 
with a value like "/hid/0001/0123/abcd/0001/0002/"
5. Try open a WebSocket connection to "^{virtualPath}"
6. Send empty packet via WebSocket, you would received HID input report(s)
7. Or if you send some binary data, it would be passed-through as HID output report

## Using Libraries
 This software is powered by following C libraries:
 
* Mongoose (GPLv2, https://github.com/cesanta/mongoose)
  
  It is HTTP server-client library.
  This project is based on "restful_server" sample code of this.
  
* HID API (GPLv3, https://github.com/signal11/hidapi)
  
  Multiplatform API to access HID
  
* POSIX Threads for Windows (LGPLv2, https://sourceforge.net/projects/pthreads4w/)
  
  API for multiplatform threading 


