#include <Arduino.h>
#include <WiFi.h>        
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Adafruit_GFX.h>
#include <AnimatedGIF.h>

#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

#ifdef ESP32
    #include <ESP32WebServer.h>
    #include <ESPmDNS.h>
    #include <SPIFFS.h>
    #include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

    #define PANEL_PIN_A 18
    #define PANEL_PIN_R1 23
    #define PANEL_PIN_G1 22
    #define PANEL_PIN_B1 21
    #define PANEL_PIN_R2 0
    #define PANEL_PIN_G2 2
    #define PANEL_PIN_CLK 14

    ESP32WebServer server(80);
    MatrixPanel_I2S_DMA* display = NULL;
#else
    #error "only support esp32"
#endif

typedef struct {
    // clock settings
    bool clock_enabled;
    uint8_t clock_position_x;
    uint8_t clock_position_y;
    uint16_t clock_hour_color;
    uint16_t clock_minute_color;

    // clock separator settings
    bool clock_separator_enabled;
    bool clock_separator_blinking;
    uint16_t clock_separator_color;
    char clock_separator_character;

    // networking settings
    int32_t utc_time_offset;
} Prefs_T;

void prefs_load(Prefs_T* prefs);

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t* data;
} Bitmap_T;

void show_bitmap(uint8_t x, uint8_t y, const Bitmap_T* bitmap, int16_t color);
void show_bitmap_centered(const Bitmap_T* bitmap, int16_t color);

enum State {
    STATE_CONNECTING,
    STATE_CONNECTED
};

void log(const char* msg, bool is_err);
void display_init(void);

void gif_draw_next_frame(void);

void home_page();
void handle_file_upload();

Prefs_T prefs;

WiFiUDP udp_socket;
NTPClient ntp_client(udp_socket, "pool.ntp.org");

const uint16_t WHITE = display->color565(255, 255, 255);
const uint16_t BLACK = display->color565(0, 0, 0);
const uint16_t RED   = display->color565(255, 100, 100);

const char* WIFI_SSID = "Keller";
const char* WIFI_PASSWD = "?Es1tlubHiJb!";
const char* SERVER_NAME = "esp-display"; // access http://esp-display in browser

const char* HTML_PAGE_HEADER PROGMEM = R"raw(<!DOCTYPE html><html><head><title>ESP Display</title></head><body>)raw";
const char* HTML_PAGE_FOOTER PROGMEM = R"raw(</body></html>)raw";

String html_page;

const char *GIF_FILENAME PROGMEM = "/gif.gif";
bool gif_is_open = false;
AnimatedGIF gif;
File gif_file;
uint8_t gif_offset[2] = {0, 0};

char hour[3] = "--";
char minute[3] = "--";

#include "bitmaps.h"

inline void set_state(State state) {
    show_bitmap_centered(&STATE_ICONS[state], WHITE);
}

void setup(void)
{    
    Serial.begin(115200);
    Serial.println();

    prefs_load(&prefs);

    display_init();

    set_state(STATE_CONNECTING);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    WiFi.mode(WIFI_MODE_STA);

    while(WiFi.status() != WL_CONNECTED)
        delay(10);

    Serial.println(("\nconnected to " + WiFi.SSID() + "\n(ip): " + WiFi.localIP().toString()).c_str());
    log(WiFi.localIP().toString().c_str(), false);

    if(!MDNS.begin(SERVER_NAME))
    {
        log("error setting up MDNS responder", true);
        delay(2000);
        ESP.restart();
    }

    if(!SPIFFS.begin(true))
    {
        log("error mounting internal flash fs", true);
        delay(2000);
        ESP.restart();
    }

    server.on("/", home_page);
    server.on("/fupload", HTTP_POST, [](){ server.send(200); }, handle_file_upload);

    server.begin();
    Serial.println(F("http server started."));
    
    ntp_client.begin();
    ntp_client.setTimeOffset(prefs.utc_time_offset);
    ntp_client.update();

    gif.begin(LITTLE_ENDIAN_PIXELS);
    
    display->clearScreen();
}

void loop(void)
{
    server.handleClient();  
    gif_draw_next_frame();     

    snprintf(hour, 3, "%02d", ntp_client.getHours());
    snprintf(minute, 3, "%02d", ntp_client.getMinutes());
}

void log(const char* msg, bool is_err)
{
    Serial.println(msg);

    display->setTextColor(is_err ? RED : WHITE);
    display->println(msg);
}

/*
 * Display
 */

void display_init(void) {
    HUB75_I2S_CFG mx_config(
        PANEL_RES_X,
        PANEL_RES_Y,
        PANEL_CHAIN
    );

    mx_config.gpio.a = PANEL_PIN_A;
    mx_config.gpio.r1 = PANEL_PIN_R1;
    mx_config.gpio.g1 = PANEL_PIN_G1;
    mx_config.gpio.b1 = PANEL_PIN_B1;
    mx_config.gpio.r2 = PANEL_PIN_R2;
    mx_config.gpio.g2 = PANEL_PIN_G2;
    mx_config.gpio.clk = PANEL_PIN_CLK;

    display = new MatrixPanel_I2S_DMA(mx_config);
    display->begin();
    display->setBrightness(255);
    display->clearScreen();
    display->setLatBlanking(2);
}

/*
 * Preferences
 */

void prefs_default(Prefs_T* prefs) {
    prefs->clock_enabled = true;
    prefs->clock_position_x = 2;
    prefs->clock_position_y = 2;
    prefs->clock_hour_color = WHITE;
    prefs->clock_minute_color = WHITE;
    prefs->clock_separator_enabled = true;
    prefs->clock_separator_blinking = true;
    prefs->clock_separator_color = WHITE;
    prefs->clock_separator_character = ':';
    prefs->utc_time_offset = 7200;
}

void prefs_load(Prefs_T* prefs) {
    prefs_default(prefs);
}

void prefs_get_html(Prefs_T* prefs) {
    
}

/*
 * Bitmap
 */

void show_bitmap_centered(const Bitmap_T* bitmap, int16_t color) {
    display->fillScreen(BLACK);
    uint8_t x = (PANEL_RES_X - bitmap->width) / 2;
    uint8_t y = (PANEL_RES_Y - bitmap->height) / 2;
    
    display->drawBitmap(x, y, bitmap->data, bitmap->width, bitmap->height, color);
}

void show_bitmap(uint8_t x, uint8_t y, const Bitmap_T* bitmap, int16_t color) {
    display->drawBitmap(x, y, bitmap->data, bitmap->width, bitmap->height, color);
}

/*
 * Overlay
 */

inline void overlay_clock(uint16_t x, uint16_t y) {
    if(prefs.clock_separator_enabled && (!prefs.clock_separator_blinking || prefs.clock_separator_blinking && millis() % 1000 < 500))
        display->drawChar(x + 10, y, prefs.clock_separator_character, prefs.clock_separator_color, BLACK, 1);
    display->drawChar(x + 0, y, hour[0], prefs.clock_hour_color, BLACK, 1);
    display->drawChar(x + 6, y, hour[1], prefs.clock_hour_color, BLACK, 1);
    display->drawChar(x + 14, y, minute[0], prefs.clock_minute_color, BLACK, 1);
    display->drawChar(x + 20, y, minute[1], prefs.clock_minute_color, BLACK, 1);
}

void draw_overlay() {
    if(prefs.clock_enabled)
        overlay_clock(prefs.clock_position_x, prefs.clock_position_y);
}

/*
 * GIF
 */

void* gif_open(const char* filename, int32_t* file_size) {
    gif_file = SPIFFS.open(filename, "r");
    if (!gif_file)
        return NULL;

    *file_size = gif_file.size();
    return (void *)&gif_file;
}

void gif_close(void* handle) {
    File* f = (File*) handle;
    if(f)
        f->close();
}

int32_t gif_read(GIFFILE* file, uint8_t* buf, int32_t len) {
    File* f = (File*) file->fHandle;

    int32_t bytes_read = len;
    if (file->iSize - file->iPos < len)
        bytes_read = file->iSize - file->iPos - 1;
    if (bytes_read <= 0)
        return 0;

    bytes_read = (int32_t) f->read(buf, bytes_read);
    file->iPos = f->position();
    return bytes_read;
}

int32_t gif_seek(GIFFILE* file, int32_t position) {
    File *f = (File*) file->fHandle;

    f->seek(position);
    file->iPos = (int32_t)f->position();
    
    return file->iPos;
}

void gif_draw(GIFDRAW* pDraw) {
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y, iWidth;

    iWidth = pDraw->iWidth;
    if (iWidth > PANEL_RES_X)
        iWidth = PANEL_RES_X;

    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line
    s = pDraw->pPixels;
    
    // restore to background color
    if(pDraw->ucDisposalMethod == 2) 
    {
        for (x = 0; x < iWidth; x++)
        {
            if (s[x] == pDraw->ucTransparent)
                s[x] = pDraw->ucBackground;
        }
        pDraw->ucHasTransparency = 0;
    }
    
    // Apply the new pixels to the main image
    if(pDraw->ucHasTransparency) // if transparency used
    {
        uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
        int x, iCount;
        pEnd = s + pDraw->iWidth;
        x = 0;
        iCount = 0; // count non-transparent pixels
        
        while (x < pDraw->iWidth)
        {
            c = ucTransparent - 1;
            d = usTemp;
            while (c != ucTransparent && s < pEnd)
            {
                c = *s++;
                if (c == ucTransparent) // done, stop
                {
                    s--; // back up to treat it like transparent
                }
                else // opaque
                {
                    *d++ = usPalette[c];
                    iCount++;
                }
            }           // while looking for opaque pixels
            if (iCount) // any opaque pixels?
            {
                for (int xOffset = 0; xOffset < iCount; xOffset++)
                {
                    display->drawPixel(x + xOffset, y,
                                            usTemp[xOffset]); // 565 Color Format
                }
                x += iCount;
                iCount = 0;
            }
            // no, look for a run of transparent pixels
            c = ucTransparent;
            while (c == ucTransparent && s < pEnd)
            {
                c = *s++;
                if (c == ucTransparent)
                    iCount++;
                else
                    s--;
            }
            if (iCount)
            {
                x += iCount; // skip these
                iCount = 0;
            }
        }
    }
    else // does not have transparency
    {
        s = pDraw->pPixels;
        // Translate the 8-bit pixels through the RGB565 palette (already byte
        // reversed)
        for (x = 0; x < pDraw->iWidth; x++)
        {
            display->drawPixel(x, y, usPalette[*s++]); // color 565
        }
    }
    
    if(pDraw->y == pDraw->iHeight - 1)
        draw_overlay();
}

void gif_draw_next_frame(void) {
    if (!gif_is_open)
    {
        if (!gif.open(GIF_FILENAME, gif_open, gif_close, gif_read, gif_seek, gif_draw))
        {
            String err("gif error: ");
            err += gif.getLastError();
            log(err.c_str(), true);
        }

        gif_offset[0] = (PANEL_RES_X - gif.getCanvasWidth()) / 2;
        gif_offset[1] = (PANEL_RES_Y - gif.getCanvasHeight()) / 2;
        gif_is_open = true;
    }

    if (gif_is_open && !gif.playFrame(true, NULL))
        gif.reset();
}

/*
 * Webserver
 */

void send_content(void)
{
    server.sendContent(html_page);
    html_page = "";
}

inline void send_stop(void)
{
    server.sendContent("/");
    server.client().stop();
}

inline void send_header(void)
{
    server.sendHeader("Cache-control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent(HTML_PAGE_HEADER);
    html_page = "";
}

void home_page()
{
    static const auto page = F(R"raw(
<h1>ESP8266 Display Configuration</h1>
<h3>Common Settings:</h3>
<button>Restart</button><br>
<h3>Select GIF file to upload</h3>
<FORM action='/fupload' method='post' enctype='multipart/form-data'>
<input class='buttons' type='file' name='fupload' id='fupload' value=''>
<button class='buttons' type='submit'>Upload GIF</button><br>
<h3>Clock Configuration</h3>
<label>Show clock:</label>
<input type='checkbox'><br><br>
<label>Position:</label>
<input type='number' min='0' max='64'>
<input type='number' min='0' max='32'><br><br>
<label>Hour color:</label>
<input type='color'><br><br>
<label>Minute color:</label>
<input type='color'><br><br>
<label>Separator:</label><br><br>
<label>Show separator:</label>
<input type='checkbox'><br><br>
<label>Blink separator:</label>
<input type='checkbox'><br><br>
<label>Color:</label>
<input type='color'><br><br>
<label>Character:</label>
<input type='text' minlength='1' maxlength='1'><br><br>
<label>UTC time offset:</label><br>
<input type='number' min='-43200' max='43200'><br><br>
<h3>Weather Configuration</h3>
<label>Show weather:</label>
<input type='checkbox'><br><br>
<label>openweathermap.org API key:</label>
<input type='text' maxlength='20'><br><br>
)raw");

    send_header();
    html_page += page; 
    html_page += HTML_PAGE_FOOTER;
    send_content();
    send_stop();
}

void report_could_not_create_file(const char* target) {
    send_header();
    html_page += F("<h3>Error creating uploaded file. Make sure the file is not bigger than 3Mb!</h3>");
    html_page += F("<a href='/");
    html_page += target;
    html_page += F("'>[Back]</a><br>");
    html_page += HTML_PAGE_FOOTER;
    send_content();
    send_stop();    
}

void handle_file_upload()
{
    static File dest_file;
    HTTPUpload& upload_file = server.upload();
    if(upload_file.name.length() == 0)
    {
        report_could_not_create_file("upload");
        return;
    }

    switch(upload_file.status) {
    case UPLOAD_FILE_START: {
        SPIFFS.remove(GIF_FILENAME);
        dest_file = SPIFFS.open(GIF_FILENAME, "w");       
    } break;
    case UPLOAD_FILE_WRITE:
        if(dest_file)
            dest_file.write(upload_file.buf, upload_file.currentSize);
        break;
    case UPLOAD_FILE_END:
        if(dest_file)
        {
            dest_file.close();
            if(gif_is_open) {
                gif.close();
                gif_is_open = false;
            }

            html_page = "";
            html_page += HTML_PAGE_HEADER;
            html_page += F("<h3>File was successfully uploaded</h3>"); 
            html_page += F("<h2>Uploaded File Name: ");
            html_page += upload_file.filename+"</h2>";
            html_page += F("<h2>File Size: ");
            html_page += upload_file.totalSize;
            html_page += F("</h2><br>"); 
            html_page += F("<a href='/'>[Back]</a><br>");
            html_page += HTML_PAGE_FOOTER;
            server.send(200, "text/html", html_page);
        }
        else
            report_could_not_create_file("upload");
        break;
    default:
        report_could_not_create_file("upload");
    }    
}
