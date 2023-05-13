#pragma once

// gif loading library
#include <AnimatedGIF.h>

namespace gif {
    const char* filename PROGMEM = "/gif.gif";

    bool gif_open = false;
    AnimatedGIF gif;
    File file;

    uint8_t offset[2] = {0, 0};

    void* open(const char* filename, int32_t* file_size) {
        file = FS.open(filename, "r");
        if(!file)
            return NULL;

        *file_size = file.size();
        return (void*) &file;
    }

    void close(void* handle) {
        File* f = static_cast<File*>(handle);
        if(f)
            f->close();
    }

    int32_t read(GIFFILE* file, uint8_t *buf, int32_t len) {
        File* f = static_cast<File*>(file->fHandle);

        int32_t bytes_read = len;
        if(file->iSize - file->iPos < len)
            bytes_read = file->iSize - file->iPos - 1;
        if(bytes_read <= 0)
            return 0;

        bytes_read = (int32_t) f->read(buf, bytes_read);
        file->iPos = f->position();
        return bytes_read;
    }

    int32_t seek(GIFFILE* file, int32_t position) {
        File* f = static_cast<File*>(file->fHandle);

        f->seek(position);
        file->iPos = (int32_t) f->position();

        return file->iPos;
    }
    #define BRIGHT_SHIFT 2

    void draw(GIFDRAW* p_draw) {
        display::dma->setBrightness(GIF_BRIGHTNESS);

        uint8_t *s = p_draw->pPixels;
        int x, y = p_draw->iY + p_draw->y;

        if (p_draw->ucDisposalMethod == 2) // restore to background color
        {
            for (x = 0; x < p_draw->iWidth; x++)
            {
                if (s[x] == p_draw->ucTransparent)
                    s[x] = p_draw->ucBackground;
            }
            p_draw->ucHasTransparency = 0;
        }
        s = p_draw->pPixels;
        // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
        for (x = 0; x < p_draw->iWidth; x++)
            display::dma->drawPixel(x + offset[0], y + offset[1], p_draw->pPalette[*s++]);
        if(p_draw->y == p_draw->iHeight - 1)
        {
            overlay::draw();
        }
    }

    int draw_next_frame() {
        if(!gif_open)
        {
            if(!gif.open(filename, open, close, read, seek, draw)) {
                String err("gif error: ");
                err += gif.getLastError();
                log(err.c_str());

                return gif.getLastError();
            }

            offset[0] = (PANEL_RES_X - gif.getCanvasWidth()) / 2;
            offset[1] = (PANEL_RES_Y - gif.getCanvasHeight()) / 2;

            gif_open = true;
        }

        if(gif_open && !gif.playFrame(true, NULL))
            gif.reset();

        return 0;
    }
}


//namespace gif {
//    const char* filename PROGMEM = "/gif.gif";
//    bool gif_open = false;
//
//    void close() {
//        
//    }
//
//    int draw_next_frame() {
//        return 0;
//    }
//}
