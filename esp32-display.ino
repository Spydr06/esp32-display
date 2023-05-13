#if defined(ESP32)
    #include <WiFi.h>        
    #include <WiFiMulti.h>
    #include <ESP32WebServer.h>
    #include <ESPmDNS.h>
    #include <SPIFFS.h>

    #define FS SPIFFS
    #define FS_Init() (SPIFFS.begin(true))

    ESP32WebServer server(80);
#else
    #error "only support esp32 and esp8266"
#endif

void log(const char* msg);

#include "display.h"
#include "network.h"
#include "html.h"
#include "overlay.h"
#include "gif.h"
#include "bitmap.h"
#include "state.h"

void setup(void)
{    
    Serial.begin(115200);
    Serial.println();

    display::init();

    state::set(state::CONNECTING);
    Serial.println(">> BEFORE WIFI <<");
    
    WiFi.begin(network::ssid, network::password);
    WiFi.mode(WIFI_MODE_STA);

    Serial.println(">> AFTER WIFI <<");
    while(WiFi.status() != WL_CONNECTED)
        delay(10);

    Serial.println(("\nconnected to " + WiFi.SSID() + "\n(ip): " + WiFi.localIP().toString()).c_str());
    log(WiFi.localIP().toString().c_str());

    if(!MDNS.begin(network::server_name))
    {
        log("error setting up MDNS responder");
        delay(2000);
        ESP.restart();
    }

    if(!FS_Init())
    {
        log("error mounting internal flash fs");
        delay(2000);
        ESP.restart();
    }

    server.on("/", home_page);
    server.on("/fupload", HTTP_POST, [](){ server.send(200); }, handle_file_upload);

    server.begin();
    Serial.println(F("http server started."));
    
    ntp::client.begin();
    ntp::client.setTimeOffset(ntp::time_offset);

    gif::gif.begin(LITTLE_ENDIAN_PIXELS);

    delay(2000);

    display::dma->clearScreen();
}

void loop(void)
{
    server.handleClient();  
    gif::draw_next_frame();         
}

void log(const char* msg)
{
    Serial.println(msg);

    display::dma->setBrightness(OVERLAY_BRIGHTNESS);
    display::dma->fillScreen(display::BLACK);
    display::dma->setTextColor(display::WHITE);
    display::dma->println(msg);
}

void home_page()
{
    static const char* page PROGMEM = R"raw(
<h1>ESP8266 Display Configuration</h1>
<h3>Select GIF file to upload</h3>
<FORM action='/fupload' method='post' enctype='multipart/form-data'>
<input class='buttons' type='file' name='fupload' id='fupload' value=''>
<button class='buttons' type='submit'>Upload GIF</button><br>
    )raw";
// <h3>Time Configuration</h3>
// <div>Clock position:</div><br>
// <input type='number' min='0' max='64'>
// <input type='number' min='0' max='32'><br>
// <div>UTC Time offset:</div><br>
// <input type='number' min='-43200' max='43200'><br>

    html::send_header();
    html::page += page; 
    html::page += html::page_footer;
    html::send_content();
    html::send_stop();
}

void handle_file_upload()
{
    static File dest_file;
    HTTPUpload& upload_file = server.upload();
        
    switch(upload_file.status) {
    case UPLOAD_FILE_START: {
        FS.remove(gif::filename);
        dest_file = FS.open(gif::filename, "w");       
    } break;
    case UPLOAD_FILE_WRITE:
        if(dest_file)
            dest_file.write(upload_file.buf, upload_file.currentSize);
        break;
    case UPLOAD_FILE_END:
        if(dest_file)
        {
            dest_file.close();
            if(gif::gif_open) {
                gif::gif.close();
                gif::gif_open = false;
            }

            html::page = "";
            html::page += html::page_header;
            html::page += F("<h3>File was successfully uploaded</h3>"); 
            html::page += F("<h2>Uploaded File Name: ");
            html::page += upload_file.filename+"</h2>";
            html::page += F("<h2>File Size: ");
            html::page += upload_file.totalSize;
            html::page += F("</h2><br>"); 
            html::page += html::page_footer;
            server.send(200, "text/html", html::page);
        }
        else
            report_could_not_create_file("upload");
        break;
    default:
        report_could_not_create_file("upload");
    }    
}

void report_could_not_create_file(const char* target) {
    html::send_header();
    html::page += F("<h3>Error creating uploaded file. Make sure the file is not bigger than 3Mb!</h3>");
    html::page += F("<a href='/");
    html::page += target;
    html::page += F("'>[Back]</a><br>");
    html::page += html::page_footer;
    html::send_content();
    html::send_stop();    
}