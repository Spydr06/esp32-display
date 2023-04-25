#pragma once

// gif loading library
#include <AnimatedGIF.h>

namespace gif {
    const char* filename PROGMEM = "/gif.gif";

    bool gif_open = false;
    AnimatedGIF gif;
    File file;

    void* open(const char* filename, int32_t* file_size) {
        file = LittleFS.open(filename, "r");
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
       /* uint8_t *s;
        int x, y, iWidth;
        static uint8_t ucPalette[256]; // thresholded palette

        if (p_draw->y == 0) // first line, convert palette to 0/1
        {
            for (x = 0; x < 256; x++) {
                uint16_t usColor = p_draw->pPalette[x];
                int gray = (usColor & 0xf800) >> 8; // red
                gray += ((usColor & 0x7e0) >> 2);   // plus green*2
                gray += ((usColor & 0x1f) << 3);    // plus blue
                ucPalette[x] = (gray >> 9);         // 0->511 = 0, 512->1023 = 1
            }
        }
        y = p_draw->iY + p_draw->y; // current line
        iWidth = p_draw->iWidth;
        if (iWidth > SCREEN_WIDTH)
            iWidth = SCREEN_WIDTH;

        s = p_draw->pPixels;
        if (p_draw->ucDisposalMethod == 2) // restore to background color
        {
            for (x = 0; x < iWidth; x++) {
              if (s[x] == p_draw->ucTransparent)
                s[x] = p_draw->ucBackground;
            }
            p_draw->ucHasTransparency = 0;
        }
        // Apply the new pixels to the main image
        if (p_draw->ucHasTransparency) // if transparency used
        {
            uint8_t c, ucTransparent = p_draw->ucTransparent;
            int x;
            for (x = 0; x < iWidth; x++) {
                c = *s++;
                if (c != ucTransparent)
                    display.drawPixel(p_draw->iX + x, y, ucPalette[c] ? WHITE_COLOR : BLACK_COLOR);
            }
        } else {
            s = p_draw->pPixels;
            // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
            for (x = 0; x < p_draw->iWidth; x++)
                display.drawPixel(p_draw->iX + x, y, ucPalette[*s++] ? WHITE_COLOR : BLACK_COLOR);
        }
        if (p_draw->y == p_draw->iHeight - 1) // last line, render it to the display
            display.showBuffer(); */

        uint8_t *s;
        uint16_t *d, *usPalette, usTemp[320];
        int x, y, iWidth;

        usPalette = p_draw->pPalette;
        y = p_draw->iY + p_draw->y; // current line

        s = p_draw->pPixels;
        if (p_draw->ucDisposalMethod == 2) // restore to background color
        {
          for (x = 0; x < iWidth; x++)
          {
            if (s[x] == p_draw->ucTransparent)
              s[x] = p_draw->ucBackground;
          }
          p_draw->ucHasTransparency = 0;
        }
        // Apply the new pixels to the main image
        if (p_draw->ucHasTransparency) // if transparency used
        {
          uint8_t *pEnd, c, ucTransparent = p_draw->ucTransparent;
          int x, iCount;
          pEnd = s + p_draw->iWidth;
          x = 0;
          iCount = 0; // count non-transparent pixels
          while (x < p_draw->iWidth)
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
                display.drawPixel(x + xOffset, y, usTemp[xOffset]);
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
        else
        {
          s = p_draw->pPixels;
          // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
          for (x = 0; x < p_draw->iWidth; x++)
          {
            display.drawPixel(x, y, usPalette[*s++]);
          }
        }

        if(p_draw->y == p_draw->iHeight - 1)
        {
            display.showBuffer();
            display.fillScreen(BLACK_COLOR);
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

            gif_open = true;
        }

        if(gif_open && !gif.playFrame(true, NULL)) {
            gif.close();
            gif_open = false;
        }

        return 0;
    }
}
