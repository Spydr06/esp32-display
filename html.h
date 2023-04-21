#pragma once

namespace html {
    const char* page_header PROGMEM = R"raw(<!DOCTYPE html><html><head><title>ESP Display</title></head><body>)raw";
    const char* page_footer PROGMEM = R"raw(</body></html>)raw";

    String page;

    inline void send_content()
    {
        server.sendContent(page);
        page = "";
    }

    inline void send_stop()
    {
        server.sendContent("/");
        server.client().stop();
    }

    inline void send_header()
    {
        server.sendHeader("Cache-control", "no-cache, no-store, must-revalidate");
        server.sendHeader("Pragma", "no-cache");
        server.sendHeader("Expires", "-1");
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "text/html", "");
        server.sendContent(page_header);
        page = "";
    }
}