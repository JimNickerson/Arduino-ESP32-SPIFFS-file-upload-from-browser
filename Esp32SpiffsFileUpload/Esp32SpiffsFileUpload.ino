/**
 * ported from https://tttapa.github.io/ESP8266/Chap12%20-%20Uploading%20to%20Server.html
 * if you find this useful mention my name JimEsp32@jimnickerson.com
 * 
 * there are 3 files in the data dir to upload to spiffs manually from the Arduino IDE to get started
 *  index.html
 *  upload.html
 *  success.html
 *  
 *  when you access the ip address of the board index.html will be presented
 *  /dlist will list the spiffs files as clickable links to didplay the files
 */
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <SPIFFS.h>

WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80

File fsUploadFile;              // a File object to temporarily store the received file

String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleFileUpload();                // upload a new file to the SPIFFS

void handle_dlist()
{
  // got /dlist  list the spiffs directory as links on a web page
  String myDirList = listDir();
  server.send(200, "html", myDirList);
}

String listDir()
{
  String dirList = "<html><head></head><body><p>click a file link to view</p>";
  File root = SPIFFS.open("/");
 
  File file = root.openNextFile();
 
  while(file){
 
      Serial.print("FILE: ");
      Serial.println(file.name());
      String ofName = file.name();
      String fName = "";
      if(ofName[0] == '/')
      {
        fName += ofName.substring(1);
      }
      else
      {
        fName = ofName;
      }
      dirList += "<a href=\"";
      dirList += fName;
      dirList += "\">";
      dirList += fName;
      dirList += "</a>";
      dirList += "<br>";
      
      file = root.openNextFile();
  }
  dirList += "<br><a href=\"index.html\">do it again</a><br><br>";
  dirList += "</body></html>";
  return dirList;
}
void setup() {
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');

  WiFi.mode(WIFI_STA);
  WiFi.begin("yourssid", "yourpassword");
  
  Serial.println("Connecting ...");
  int i = 0;
  while(WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(++i); Serial.print('.');
  }
  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer

  if (!MDNS.begin("esp32")) {             // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");

  SPIFFS.begin();                           // Start the SPI Flash Files System

  server.on("/upload", HTTP_GET, []() {                 // if the client requests the upload page
    if (!handleFileRead("/upload.html"))                // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.on("/upload", HTTP_POST,                       // if the client posts to the upload page from the form
    [](){ server.send(200); },                          // Send status 200 (OK) to tell the client we are ready to receive
    handleFileUpload                                    // Receive and save the file
  );
  
  server.on("/dlist", []{
    handle_dlist();
  });

  
  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.begin();                           // Actually start the server
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      server.send(303);
      
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}
