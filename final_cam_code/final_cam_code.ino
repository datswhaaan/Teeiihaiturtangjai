#include "Arduino.h"
#include "esp_camera.h"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"
#include <FS.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <ctime>
#include <TaskScheduler.h>

Scheduler runner;

void streamVideo();
void imageForAi();

Task taskStreamVideo(1, TASK_FOREVER, &streamVideo);
Task taskImageForAi(1000, TASK_FOREVER, &imageForAi);

#define SPIFFS LITTLEFS
 
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
 
WiFiServer server(80);
bool connected = false;
WiFiClient live_client;
 
String index_html = "<meta charset=\"utf-8\"/>\n" \
                    "<style>\n" \
                    "body {\n" \
                    "  background-color: #000000;\n" \
                    "  margin: 0;\n" \
                    "  padding: 0;\n" \
                    "}\n" \
                    "#content {\n" \
                    "  display: flex;\n" \
                    "  flex-direction: column;\n" \
                    "  justify-content: center;\n" \
                    "  align-items: center;\n" \
                    "  text-align: center;\n" \
                    "  min-height: 100vh;\n" \
                    "}\n" \
                    "img {\n" \
                    "  max-width: 100%;\n" \
                    "  height: auto;\n" \
                    "}\n" \
                    "</style>\n" \
                    "<body>\n" \
                    "  <div id=\"content\">\n" \
                    "    <img src=\"video\">\n" \
                    "  </div>\n" \
                    "</body>";

const char* ssid = "whanwhan";
const char* password = "whanwhanjubjub";

// Firebase Configuration
#define DATABASE_URL "https://teeii-hai-tur-tang-jai-default-rtdb.asia-southeast1.firebasedatabase.app"
#define DATABASE_SECRET "541nnI9oYudvpBW1YVhXr4VixhF5Hrousg427z2l"
#define API_KEY "AIzaSyD9HjGiG7b3AXJAg1zomlLXFJG7k6X5ZIw"
#define STORAGE_BUCKET_ID "teeii-hai-tur-tang-jai.firebasestorage.app"

#define FILE_PHOTO_PATH "/photo.jpg"
#define BUCKET_PHOTO "/data"

// Firebase Objects
FirebaseData fbdo;
FirebaseAuth fbauth;
FirebaseConfig fbconfig;

void fcsUploadCallback(FCS_UploadStatusInfo info);

bool taskCompleted = false;

// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS( void ) {
  // Dispose first pictures because of bad quality
  camera_fb_t* fb = NULL;
  // Skip first 3 frames (increase/decrease number as needed).
  for (int i = 0; i < 4; i++) {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = NULL;
  }
    
  // Take a new photo
  fb = NULL;  
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }  

  // Photo file name
  Serial.printf("Picture file name: %s\n", FILE_PHOTO_PATH);
  File file = LittleFS.open(FILE_PHOTO_PATH, FILE_WRITE);

  // Insert the data in the photo file
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  }
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.print("The picture has been saved in ");
    Serial.print(FILE_PHOTO_PATH);
    Serial.print(" - Size: ");
    Serial.print(fb->len);
    Serial.println(" bytes");
  }
  // Close the file
  file.close();
  esp_camera_fb_return(fb);
}

// Initialize Wi-Fi
void initWiFi() {
    Serial.print("Connecting to Wi-Fi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println();
    Serial.printf("Connected to Wi-Fi! IP address: %s\n", WiFi.localIP().toString().c_str());
}

void initLittleFS(){
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("LittleFS mounted successfully");
  }
}
 
void configCamera(){
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 10;
  config.fb_count = 1;
 
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  Serial.println("Camera initialized successfully!");
}

// Firebase Initialization
void initFirebase() {
    fbconfig.api_key = API_KEY;
    fbconfig.database_url = DATABASE_URL;
    fbconfig.signer.tokens.legacy_token = DATABASE_SECRET;
    //Assign the callback function for the long running token generation task
    fbconfig.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

    Firebase.begin(&fbconfig, &fbauth);
    Firebase.reconnectWiFi(true);
    Serial.println("Firebase initialized successfully!");
}

// The Firebase Storage upload callback function
void fcsUploadCallback(FCS_UploadStatusInfo info){
    if (info.status == firebase_fcs_upload_status_init){
        Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
    }
    else if (info.status == firebase_fcs_upload_status_upload)
    {
        Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    }
    else if (info.status == firebase_fcs_upload_status_complete)
    {
        Serial.println("Upload completed\n");
        FileMetaInfo meta = fbdo.metaData();
        Serial.printf("Name: %s\n", meta.name.c_str());
        Serial.printf("Bucket: %s\n", meta.bucket.c_str());
        Serial.printf("contentType: %s\n", meta.contentType.c_str());
        Serial.printf("Size: %d\n", meta.size);
        Serial.printf("Generation: %lu\n", meta.generation);
        Serial.printf("Metageneration: %lu\n", meta.metageneration);
        Serial.printf("ETag: %s\n", meta.etag.c_str());
        Serial.printf("CRC32: %s\n", meta.crc32.c_str());
        Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
        Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
    }
    else if (info.status == firebase_fcs_upload_status_error){
        Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    }
}

void streamToFirebase() {
    if (Firebase.ready()){
        Serial.print("Uploading picture... ");
        String path = String(BUCKET_PHOTO) + "stream_image.jpg";

        //MIME type should be valid to avoid the download problem.
        //The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
        if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, FILE_PHOTO_PATH /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, path.c_str() /* path of remote file stored in the bucket */, "image/jpeg" /* mime type */)){
          Serial.println("Uploaded");
        }
        else{
          Serial.println(fbdo.errorReason());
        }
    }
}
 
//continue sending camera frame
void liveCam(WiFiClient &client){
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
      Serial.println("Frame buffer could not be acquired");
      return;
  }
  client.print("--frame\n");
  client.print("Content-Type: image/jpeg\n\n");
  client.flush();
  client.write(fb->buf, fb->len);
  client.flush();
  client.print("\n");
  esp_camera_fb_return(fb);
}
 
void setup() {
  Serial.begin(115200);

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  initWiFi();
  initLittleFS();
  configCamera();
  initFirebase();
  
  String IP = WiFi.localIP().toString();
  index_html.replace("server_ip", IP);
  server.begin();

  runner.addTask(taskStreamVideo);
  runner.addTask(taskImageForAi);

  taskStreamVideo.enable();
  taskImageForAi.enable();
}
    
void http_resp(){
  WiFiClient client = server.available();                           
  if (client.connected()) {     
      String req = "";
      while(client.available()){
        req += (char)client.read();
      }
      Serial.println("request " + req);
      int addr_start = req.indexOf("GET") + strlen("GET");
      int addr_end = req.indexOf("HTTP", addr_start);
      if (addr_start == -1 || addr_end == -1) {
          Serial.println("Invalid request " + req);
          return;
      }
      req = req.substring(addr_start, addr_end);
      req.trim();
      Serial.println("Request: " + req);
      client.flush();
  
      String s;
      if (req == "/")
      {
          s = "HTTP/1.1 200 OK\n";
          s += "Content-Type: text/html\n\n";
          s += index_html;
          s += "\n";
          client.print(s);
          client.stop();
      }
      else if (req == "/video")
      {
          live_client = client;
          live_client.print("HTTP/1.1 200 OK\n");
          live_client.print("Content-Type: multipart/x-mixed-replace; boundary=frame\n\n");
          live_client.flush();
          connected = true;
      }
      else
      {
          s = "HTTP/1.1 404 Not Found\n\n";
          client.print(s);
          client.stop();
      }
    }       
}

void streamVideo() {
  http_resp();
  if(connected == true){
    liveCam(live_client);
  }
}

void imageForAi() {
  capturePhotoSaveLittleFS();
  streamToFirebase();
}
 
void loop() {
  runner.execute();  // Run the scheduler
}