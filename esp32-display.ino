#include <Adafruit_GFX.h>
#include <AnimatedGIF.h>
#include <Arduino.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <algorithm>

#include "WiFiClient.h"
#include "esp_idf_version.h"
#include "fonts/dogica4pt7b.h"
#define FONT dogica4pt7b

#include "version.h"

#define QUOTE(arg) #arg
#define QUOTE_EXPAND(arg) QUOTE(arg)
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

#ifdef ESP32
    #include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
    #include <ESP32WebServer.h>
    #include <ESPmDNS.h>
    #include <SPIFFS.h>
    #include <Update.h>

    #define PANEL_PIN_A 18
    #define PANEL_PIN_R1 23
    #define PANEL_PIN_G1 22
    #define PANEL_PIN_B1 21
    #define PANEL_PIN_R2 0
    #define PANEL_PIN_G2 2
    #define PANEL_PIN_CLK 14

    ESP32WebServer server(80);
    MatrixPanel_I2S_DMA *display = NULL;
#else
    #error "only support esp32"
#endif

#define OPENWEATHERMAP_API_KEY_LENGTH 32

typedef struct
{
    // clock settings
    bool c_enabled;
    uint8_t c_pos_x;
    uint8_t c_pos_y;
    uint16_t c_hour_col;
    uint16_t c_minute_col;
    uint8_t c_spacing;

    // clock separator settings
    bool sep_enabled;
    uint16_t sep_blink_interval;
    uint16_t sep_color;
    char sep_char;

    // networking settings
    int32_t utc_time_offset;

    // weather settings
    bool w_enabled;
    char w_api_key[OPENWEATHERMAP_API_KEY_LENGTH + 1];
    char w_coutry[3];
    char w_city[41];
} Prefs_T;

#define EEPROM_SIZE (sizeof(Prefs_T))

void prefs_load(void);

typedef struct
{
    uint8_t width;
    uint8_t height;
    uint8_t *data;
} Bitmap_T;

void show_bitmap(uint8_t x, uint8_t y, const Bitmap_T *bitmap, int16_t color);
void show_bitmap_centered(const Bitmap_T *bitmap, int16_t color);

enum State
{
    STATE_CONNECTING,
    STATE_CONNECTED
};

void log(const char *msg, bool is_err);
void display_init(void);

void gif_draw_next_frame(void);

void home_page(void);
void handle_file_upload(void);
void handle_ota_update(void);
void handle_not_found(void);
void handle_pref_update(void);
void handle_restart(void);
void handle_reset(void);

Prefs_T prefs;

WiFiUDP udp_socket;
NTPClient ntp_client(udp_socket, "pool.ntp.org");

WiFiClient wifi_client;

const char *WEATHER_SERVER PROGMEM = "api.openweathermap.org";

const uint16_t WHITE = display->color565(255, 255, 255);
const uint16_t BLACK = display->color565(0, 0, 0);
const uint16_t RED = display->color565(255, 100, 100);

const char *WIFI_SSID = "Keller";
const char *WIFI_PASSWD = "?Es1tlubHiJb!";
const char *SERVER_NAME = "esp-display"; // access http://esp-display in browser

const char *HTML_PAGE_HEADER PROGMEM = "<!DOCTYPE html><html><head><title>ESP Display</title></head><body>";
const char *HTML_PAGE_FOOTER PROGMEM = "</body></html>";

String html_page;

const char *GIF_FILENAME PROGMEM = "/gif.gif";
bool gif_is_open = false;
AnimatedGIF gif;
File gif_file;
uint8_t gif_offset[2] = {0, 0};

char hour[3] = "--";
char minute[3] = "--";

#include "bitmaps.h"

inline void set_state(State state)
{
    show_bitmap_centered(&STATE_ICONS[state], WHITE);
}

void setup(void)
{
    Serial.begin(115200);
    Serial.println();

    if (!EEPROM.begin(EEPROM_SIZE))
    {
        Serial.println("error mounting eeprom");
        delay(2000);
        ESP.restart();
    }

    prefs_load();
    display_init();

    set_state(STATE_CONNECTING);

    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    WiFi.mode(WIFI_MODE_STA);

    while (WiFi.status() != WL_CONNECTED)
        delay(10);

    Serial.println(("\nconnected to " + WiFi.SSID() + "\n(ip): " + WiFi.localIP().toString()).c_str());
    log(WiFi.localIP().toString().c_str(), false);

    if (!MDNS.begin(SERVER_NAME))
    {
        log("error setting up MDNS responder", true);
        delay(2000);
        ESP.restart();
    }

    if (!SPIFFS.begin(true))
    {
        log("error mounting internal flash fs", true);
        delay(2000);
        ESP.restart();
    }

    server.on("/", home_page);
    server.on("/fupload", HTTP_POST, [](){ server.send(200); }, handle_file_upload);
    server.on("/restart", handle_restart);
    server.on("/reset", handle_reset);
    server.on("/pref", HTTP_POST, handle_pref_update);
    server.on("/update", HTTP_POST, [](){
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
        ESP.restart();
    }, handle_ota_update);
    server.onNotFound(handle_not_found);

    server.begin();
    Serial.println(F("http server started."));

    ntp_client.begin();
    ntp_client.setTimeOffset(prefs.utc_time_offset);
    ntp_client.update();

    gif.begin(LITTLE_ENDIAN_PIXELS);

    display->clearScreen();

    Serial.println(F("done booting."));
}

void loop(void)
{
    server.handleClient();
    gif_draw_next_frame();

    snprintf(hour, 3, "%02d", ntp_client.getHours());
    snprintf(minute, 3, "%02d", ntp_client.getMinutes());
}

void log(const char *msg, bool is_err)
{
    Serial.println(msg);

    display->setTextColor(is_err ? RED : WHITE);
    display->println(msg);
}

/*
 * Display
 */

void display_init(void)
{
    HUB75_I2S_CFG mx_config(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);

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
    display->setFont(&FONT);
}

/*
 * Preferences
 */

void prefs_load(void)
{ 
    EEPROM.get(0, prefs);
}

void prefs_save(void)
{
    EEPROM.put(0, prefs);
    EEPROM.commit();
}

void prefs_reset(void)
{
    prefs.c_enabled = true;
    prefs.c_pos_x = 2;
    prefs.c_pos_y = 2;
    prefs.c_hour_col = WHITE;
    prefs.c_minute_col = WHITE;
    prefs.c_spacing = 6;
    prefs.sep_enabled = true;
    prefs.sep_blink_interval = 1000;
    prefs.sep_color = WHITE;
    prefs.sep_char = ':';
    prefs.utc_time_offset = 7200;
    prefs.w_enabled = true;
    memset(prefs.w_api_key, '\0', sizeof(prefs.w_api_key));
    memset(prefs.w_coutry, '\0', sizeof(prefs.w_coutry));
    memset(prefs.w_city, '\0', sizeof(prefs.w_city));

    ntp_client.setTimeOffset(prefs.utc_time_offset);
    ntp_client.update();

    prefs_save();
}

void color565_to_str(char buf[7], uint16_t color)
{
    uint8_t r, g, b;
    display->color565to888(color, r, g, b);
    snprintf(buf, 7, "%02x%02x%02x", r, g, b);
}

uint16_t str_to_color565(const char* str)
{
    if(str[0] == '#')
        str++;
    
    uint8_t r, g, b;
    sscanf(str, "%02x%02x%02x", &r, &g, &b);
    return display->color565(r, g, b);
}

void prefs_page(void)
{
    html_page += F(R"raw(<h3>Clock Configuration</h3>
<form action='/pref' method='post' enctype='multipart/form-data'>
<label for='show-clock'>Show Clock:</label>)raw");

    if (!prefs.c_enabled)
    {
        html_page += F("<input type='checkbox' id='show-clock' name='show-clock'/><br/><br/>");
        goto end_clock_config;
    }

    html_page += F(R"raw(<input type='checkbox' id='show-clock' name='show-clock' checked/><br/><br/>
<label for='utc-time-offset'>UTC Time Offset: (timezone in seconds from GMT)</label>
<input type='number' min='-43200' max='43200' id='utc-time-offset' name='utc-time-offset' value=')raw");

    html_page += prefs.utc_time_offset;

    html_page += F(R"raw('/><br/><br/>
<label for='c_pos'>Position:</label>
<input type='number' id='c_pos' name='c_pos_x' min='0' max=')raw" QUOTE(PANEL_RES_X) "' value='");
    html_page += prefs.c_pos_x;
    html_page += F("'/><input type='number' id='c_pos' name='c_pos_y' min='0' max='" QUOTE(PANEL_RES_Y) "' value='");
    html_page += prefs.c_pos_y;
    html_page += F(R"raw('/><br/><br/>
<label for='c_hour_col'>Hour Color:</label>
<input type='color' id='c_hour_col' name='c_hour_col' value='#)raw");

    char hex_str[7];
    color565_to_str(hex_str, prefs.c_hour_col);
    html_page += hex_str;

    html_page += F(R"raw('/><br/><br/>
<label for='c_minute_col'>Minute Color:</label>
<input type='color' id='c_minute_col' name='c_minute_col' value='#)raw");

    color565_to_str(hex_str, prefs.c_minute_col);
    html_page += hex_str;

    html_page += F(R"raw('/><br/><br/>
<label for='c_spacing'>Spacing:</label>
<input type='number' id='c_spacing' name='c_spacing' min='0' max=')raw" QUOTE(UINT8_MAX) "' value='");
    html_page += prefs.c_spacing;
    html_page += F(R"raw('/><br/>
<h4>Separator:</h4>
<label for='sep_enabled'>Show Separator:</label>)raw");

    if (!prefs.sep_enabled)
    {
        html_page += F("<input type='checkbox' id='sep_enabled' name='sep_enabled'/><br/><br/>");
        goto end_clock_config;
    }

    html_page += F(R"raw(<input type='checkbox' checked id='sep_enabled' name='sep_enabled'/><br/><br/>
<label for='sep_blink_interval'>Blink Interval: (in ms, `0` means off)</label>
<input type='number' id='sep_blink_interval' name='sep_blink_interval' min='0' max=')raw" QUOTE(UINT16_MAX) "' value='");
    html_page += prefs.sep_blink_interval;
    html_page += F(R"raw('/><br/><br/>
<label for='sep_color'>Color:</label>
<input type='color' id='sep_color' name='sep_color' value='#)raw");

    color565_to_str(hex_str, prefs.sep_color);
    html_page += hex_str;

    html_page += F(R"raw('/><br/><br/>
<label for='sep_char'>Character:</label>
<input type='text' minlength='1' maxlength='1' id='sep_char' name='sep_char' value=')raw");

    html_page += prefs.sep_char;
    html_page += F("'/><br/>");

end_clock_config:

    html_page += F(R"raw(<h3>Weather Configuration</h3>
<label for='w_enabled'>Show Weather:</label>)raw");


    if(!prefs.w_enabled)
    {
        html_page += F("<input type='checkbox' id='w_enabled' name='w_enabled'/><br/><br/>");
        goto end_weather_config;
    }

    html_page += F(R"raw(<input type='checkbox' id='w_enabled' name='w_enabled' checked/><br/><br/>
<label for='w_api_key'><a href='https://openweathermap.org'>openweathermap.org</a> API Key</label>
<input type='text' minlength='0' maxlength=')raw" QUOTE(OPENWEATHERMAP_API_KEY_LENGTH) "' id='w_api_key' name='w_api_key' value='");
    html_page += prefs.w_api_key;

    html_page += F(R"raw('/><br/><br/>
<label for='w_country'>2-Letter Country Code:</label>
<input type='text' minlength='2' maxlength='2' id='w_country' name='w_country' value=')raw");
    html_page += prefs.w_coutry;

    html_page += F(R"raw('/><br/><br/>
<label for='w_city'>City:</label>
<input type='text' minlength='0' maxlength='40' id='w_city' name='w_city' value=')raw");
    html_page += prefs.w_city;
    html_page += F("'><br/><br/>");

end_weather_config:
    html_page += F("<button type='submit'>Update Settings</button></form></br>");
}

void pref_update(String item, String value)
{
    Serial.print(item);
    Serial.print(": ");
    Serial.println(value);

    if(item == "show-clock")
        prefs.c_enabled = true;
    else if(item == "utc-time-offset")
    {
        prefs.utc_time_offset = value.toInt();
        ntp_client.setTimeOffset(prefs.utc_time_offset);
        ntp_client.update();
    }
    else if(item == "c_pos_x")
        prefs.c_pos_x = value.toInt();
    else if(item == "c_pos_y")
        prefs.c_pos_y = value.toInt();
    else if(item == "c_hour_col")
        prefs.c_hour_col = str_to_color565(value.c_str());
    else if(item == "c_minute_col")
        prefs.c_minute_col = str_to_color565(value.c_str());
    else if(item == "c_spacing")
        prefs.c_spacing = value.toInt();
    else if(item == "sep_enabled")
        prefs.sep_enabled = true;
    else if(item == "sep_color")
        prefs.sep_color = str_to_color565(value.c_str());
    else if(item == "sep_char")
        prefs.sep_char = value.charAt(0);
    else if(item == "sep_blink_interval")
        prefs.sep_blink_interval = value.toInt();
    else if(item == "w_enabled")
        prefs.w_enabled = true;
    else if(item == "w_api_key")
        memcpy(prefs.w_api_key, value.c_str(), MIN(value.length(), OPENWEATHERMAP_API_KEY_LENGTH));
    else if(item == "w_country")
        memcpy(prefs.w_coutry, value.c_str(), 2);
    else if(item == "w_city")
        memcpy(prefs.w_city, value.c_str(), sizeof(prefs.w_city) - sizeof(char));
    else
    {
        html_page += F("<label>Unknown setting \"");
        html_page += item;
        html_page += ": ";
        html_page += value;
        html_page += F("\"</label><br/>");
    }
}

/*
 * Bitmap
 */

void show_bitmap_centered(const Bitmap_T *bitmap, int16_t color)
{
    display->fillScreen(BLACK);
    uint8_t x = (PANEL_RES_X - bitmap->width) / 2;
    uint8_t y = (PANEL_RES_Y - bitmap->height) / 2;

    display->drawBitmap(x, y, bitmap->data, bitmap->width, bitmap->height, color);
}

void show_bitmap(uint8_t x, uint8_t y, const Bitmap_T *bitmap, int16_t color)
{
    display->drawBitmap(x, y, bitmap->data, bitmap->width, bitmap->height, color);
}

/*
 * Overlay
 */

inline void overlay_clock(uint16_t x, uint16_t y)
{
    uint16_t iv = prefs.sep_blink_interval;
    uint8_t sp = prefs.c_spacing; 

    display->drawChar(x, y, hour[0], prefs.c_hour_col, BLACK, 1);
    display->drawChar(x += sp,      y, hour[1], prefs.c_hour_col, BLACK, 1);

    if (prefs.sep_enabled)
    {
        x += sp;
        if(!iv || iv && millis() % iv < iv >> 1)
            display->drawChar(x, y, prefs.sep_char, prefs.sep_color, BLACK, 1);
    }

    display->drawChar(x += sp,  y, minute[0], prefs.c_minute_col, BLACK, 1);
    display->drawChar(x += sp, y, minute[1], prefs.c_minute_col, BLACK, 1);
}

void draw_overlay()
{
    if (prefs.c_enabled)
        overlay_clock(prefs.c_pos_x, prefs.c_pos_y);
}

/*
 * GIF
 */

void *gif_open(const char *filename, int32_t *file_size)
{
    gif_file = SPIFFS.open(filename, "r");
    if (!gif_file)
        return NULL;

    *file_size = gif_file.size();
    return (void *)&gif_file;
}

void gif_close(void *handle)
{
    File *f = (File *)handle;
    if (f)
        f->close();
}

int32_t gif_read(GIFFILE *file, uint8_t *buf, int32_t len)
{
    File *f = (File *)file->fHandle;

    int32_t bytes_read = len;
    if (file->iSize - file->iPos < len)
        bytes_read = file->iSize - file->iPos - 1;
    if (bytes_read <= 0)
        return 0;

    bytes_read = (int32_t)f->read(buf, bytes_read);
    file->iPos = f->position();
    return bytes_read;
}

int32_t gif_seek(GIFFILE *file, int32_t position)
{
    File *f = (File *)file->fHandle;

    f->seek(position);
    file->iPos = (int32_t)f->position();

    return file->iPos;
}

void gif_draw(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y, iWidth;

    iWidth = pDraw->iWidth;
    if (iWidth > PANEL_RES_X)
        iWidth = PANEL_RES_X;

    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line
    s = pDraw->pPixels;

    if (pDraw->y == pDraw->iHeight - 1)
        draw_overlay();

    // restore to background color
    if (pDraw->ucDisposalMethod == 2)
    {
        for (x = 0; x < iWidth; x++)
        {
            if (s[x] == pDraw->ucTransparent)
                s[x] = pDraw->ucBackground;
        }
        pDraw->ucHasTransparency = 0;
    }

    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
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

    if (pDraw->y == pDraw->iHeight - 1)
        draw_overlay();
}

void gif_draw_next_frame(void)
{
    if (!gif_is_open)
    {
        if (!gif.open(GIF_FILENAME, gif_open, gif_close, gif_read, gif_seek, gif_draw))
        {
            String err("gif error: ");
            err += gif.getLastError();
            log(err.c_str(), true);

            return;
        }

        gif_offset[0] = (PANEL_RES_X - gif.getCanvasWidth()) / 2;
        gif_offset[1] = (PANEL_RES_Y - gif.getCanvasHeight()) / 2;
        gif_is_open = true;
    }

    if (gif_is_open && !gif.playFrame(true, NULL))
        gif.reset();
}

/*
 * Weather
 */

/*
bool start_json = false;

inline void weather_make_request(void)
{
    wifi_client.stop();

    if (!wifi_client.connect(WEATHER_SERVER, 80))
    {
        Serial.print("connection to ");
        Serial.print(WEATHER_SERVER);
        Serial.println(" failed");
    }

    wifi_client.printf(R"raw(GET /data/2.5/forecast/q=%s,%02s&APPID=%s&mode=json&units=metric&cnt=2 HTTP/1.1
Host: %s
User-Agent: ArduinoWiFi/1.1
Connection: close

)raw", prefs.w_city, prefs.w_coutry, prefs.w_api_key, WEATHER_SERVER);

    uint64_t timeout = millis();
    while (wifi_client.available() == 0)
    {
        if (millis() - timeout > 5000)
        {
            Serial.println(F(">>> Weather Client Timeout"));
            client.stop();
            return;
        }
    }

    char c;
    while (wifi_client.available())
    {
        c = client.read();
    }
}

void query_weather(void)
{

}
*/
/*
 * Webserver
 */

void handle_not_found(void)
{
    String message = F("File Not Found\n\nURI: ");
    message += server.uri();
    message += F("\nMethod: ");
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += F("\nArguments: ");
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++)
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    server.send(404, "text/plain", message);
}

void send_content(void)
{
    server.sendContent(html_page);
    html_page = "";
}

inline void send_stop(void) { server.client().stop(); }

inline void send_header(void)
{
    server.sendHeader(F("Cache-control"), F("no-cache, no-store, must-revalidate"));
    server.sendHeader(F("Pragma"), F("no-cache"));
    server.sendHeader(F("Expires"), "-1");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, F("text/html"), "");
    server.sendContent(HTML_PAGE_HEADER);
    html_page = "";
}

void home_page(void)
{
    send_header();

    html_page += F(R"raw(
<h1>ESP8266 Display Configuration</h1>
<a href='/restart'><button>Restart</button></a>
<a href='/reset'><button>Reset</button></a><br/>
<h3>Firmware Information</h3>
<table><tr><td>Version:</td><td>)raw" ESP32_DISPLAY_VERSION R"raw(</td></tr>
<tr><td>Build Date:</td><td>)raw" ESP32_DISPLAY_BUILD_DATE R"raw(</td></tr>
<tr><td>Build Time:</td><td>)raw" ESP32_DISPLAY_BUILD_TIME R"raw(</td></tr>
<tr><td>Espressif Idf Version:</td><td>)raw");

    html_page += esp_get_idf_version();

    html_page += F(R"raw(</td></tr>
<tr><td>Flash Size:</td><td>)raw");

    html_page += ESP.getFlashChipSize();
    html_page += F(R"raw( (bytes)</td></tr></table>)raw");

    html_page += F(R"raw(<h3>Firmware Update</h3>
<form action='/update' method='post' enctype='multipart/form-data'>
<input type='file' name='update' id='update' value=''/>
<button type='submit'>Upload New Firmware</button><br/></form>
<h3>Select GIF</h3>
<form action='/fupload' method='post' enctype='multipart/form-data'>
<input type='file' name='fupload' id='fupload' value=''/>
<button type='submit'>Upload GIF</button><br/></form>)raw");

    prefs_page();
    html_page += HTML_PAGE_FOOTER;
    send_content();
    send_stop();
}

void report_file_upload_error(String& filename, const char* message)
{
    send_header();
    html_page += F("<h3>Error Uploading ");
    html_page += filename;
    html_page += F(".</h3><label>");
    html_page += message;
    html_page += F("</label><a href='/'>[Back]</a><br/>");
    html_page += HTML_PAGE_FOOTER;
    send_content();
    send_stop();
}

void handle_pref_update(void) {
    Serial.println("updating settings...");

    html_page += HTML_PAGE_HEADER;
    html_page += F("<h3>Settings Updated</h3>");

    // TODO: find better way to post disabled checkboxes
    prefs.c_enabled = false;
    prefs.sep_enabled = false;
    prefs.w_enabled = false;

    for(int i = 0; i < server.args(); i++)
        pref_update(server.argName(i), server.arg(i));

    prefs_save();

    html_page += F("<a href='/'>[Back]</a><br/>");
    html_page += HTML_PAGE_FOOTER;
    server.send(200, "text/html", html_page);
}

void handle_file_upload(void)
{
    static File dest_file;
    HTTPUpload &upload_file = server.upload();
    if (!upload_file.filename.endsWith(".gif"))
    {
        report_file_upload_error(upload_file.filename, "Filename does not end in `.gif`");
        return;
    }

    switch (upload_file.status)
    {
    case UPLOAD_FILE_START:
    {
        SPIFFS.remove(GIF_FILENAME);
        dest_file = SPIFFS.open(GIF_FILENAME, "w");
    }
    break;
    case UPLOAD_FILE_WRITE:
        if (dest_file)
            dest_file.write(upload_file.buf, upload_file.currentSize);
        break;
    case UPLOAD_FILE_END:
        if (dest_file)
        {
            dest_file.close();
            if (gif_is_open)
            {
                gif.close();
                gif_is_open = false;
            }

            html_page = "";
            html_page += HTML_PAGE_HEADER;
            html_page += F("<h3>File was successfully uploaded</h3>");
            html_page += F("<h2>Uploaded File Name: ");
            html_page += upload_file.filename + "</h2>";
            html_page += F("<h2>File Size: ");
            html_page += upload_file.totalSize;
            html_page += F(" (bytes)</h2><br/>");
            html_page += F("<a href='/'>[Back]</a><br/>");
            html_page += HTML_PAGE_FOOTER;
            server.send(200, "text/html", html_page);
        }
        else
            report_file_upload_error(upload_file.filename, "Error creating destination file");
        break;
    default:
        report_file_upload_error(upload_file.filename, "Unknown file status");
    }
}

void handle_ota_update(void)
{
    HTTPUpload& upload_file = server.upload();
    if (!upload_file.filename.endsWith(".bin"))
    {
        report_file_upload_error(upload_file.filename, "Filename does not end in `.bin`");
        return;
    }

    switch(upload_file.status)
    {
    case UPLOAD_FILE_START:
        Serial.printf("Update: %s\n", upload_file.filename.c_str());
        if(!Update.begin(UPDATE_SIZE_UNKNOWN))
        {
            Update.printError(Serial);
            report_file_upload_error(upload_file.filename, Update.errorString());
        }

        break;
    case UPLOAD_FILE_WRITE:
        if(Update.write(upload_file.buf, upload_file.currentSize) != upload_file.currentSize)
        {
            Update.printError(Serial);
            report_file_upload_error(upload_file.filename, Update.errorString());
        }
        break;
    case UPLOAD_FILE_END:
        if(Update.end(true))
        {
            Serial.printf("Update Success: %u\nRebooting...\n", upload_file.totalSize);

            html_page = "";
            html_page += HTML_PAGE_HEADER;
            html_page += F("<h3>Update was successfully uploaded</h3>");
            html_page += F("<h2>Update Size: ");
            html_page += upload_file.totalSize;
            html_page += F(" (bytes)</h2><label>Please wait a few seconds until the display has restarted. Then go </label>");
            html_page += F("<a href='/'>[Back]</a><label>.</label><br/>");
            html_page += HTML_PAGE_FOOTER;
            server.send(200, "text/html", html_page);

            delay(2000);
        }
        else
        {
            Update.printError(Serial);
            report_file_upload_error(upload_file.filename, Update.errorString());
        }
    default:
        break;
    }
}

void handle_restart(void)
{
    send_header();
    html_page += F("<h3>Restarting...</h3>");
    html_page += F("<label>Please wait a few seconds until the display has restarted. Then go </label>");
    html_page += F("<a href='/'>[Back]</a><label>.</label><br/>");
    html_page += HTML_PAGE_FOOTER;
    send_content();
    send_stop();

    server.stop();

    prefs_save();
    EEPROM.end();
    
    ESP.restart();
}

void handle_reset(void)
{
    prefs_reset();

    send_header();
    html_page += F("<h3>Resetted All Settings</h3>");
    html_page += F("<a href='/'>[Back]</a>");
    html_page += HTML_PAGE_FOOTER;
    send_content();
    send_stop();
}
