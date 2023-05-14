#pragma once

// gif loading library
#include <AnimatedGIF.h>

namespace gif
{
    const char *filename PROGMEM = "/gif.gif";

    bool gif_open = false;
    AnimatedGIF gif;
    File file;

    uint8_t offset[2] = {0, 0};

    void *open(const char *filename, int32_t *file_size)
    {
        file = FS.open(filename, "r");
        if (!file)
            return NULL;

        *file_size = file.size();
        return (void *)&file;
    }

    void close(void *handle)
    {
        File *f = static_cast<File *>(handle);
        if (f)
            f->close();
    }

    int32_t read(GIFFILE *file, uint8_t *buf, int32_t len)
    {
        File *f = static_cast<File *>(file->fHandle);

        int32_t bytes_read = len;
        if (file->iSize - file->iPos < len)
            bytes_read = file->iSize - file->iPos - 1;
        if (bytes_read <= 0)
            return 0;

        bytes_read = (int32_t)f->read(buf, bytes_read);
        file->iPos = f->position();
        return bytes_read;
    }

    int32_t seek(GIFFILE *file, int32_t position)
    {
        File *f = static_cast<File *>(file->fHandle);

        f->seek(position);
        file->iPos = (int32_t)f->position();

        return file->iPos;
    }
#define BRIGHT_SHIFT 2

    void draw(GIFDRAW *pDraw)
    {
        /* uint8_t *s = p_draw->pPixels;
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
         // Translate the 8-bit pixels through the RGB565 palette (already byte
         reversed) for (x = 0; x < p_draw->iWidth; x++) display::dma->drawPixel(x +
         offset[0], y + offset[1], p_draw->pPalette[*s++]); if(p_draw->y ==
         p_draw->iHeight - 1) overlay::draw();*/

        uint8_t *s;
        uint16_t *d, *usPalette, usTemp[320];
        int x, y, iWidth;

        iWidth = pDraw->iWidth;
        if (iWidth > PANEL_RES_X)
            iWidth = PANEL_RES_X;

        usPalette = pDraw->pPalette;
        y = pDraw->iY + pDraw->y; // current line

        s = pDraw->pPixels;
        if (pDraw->ucDisposalMethod == 2) // restore to background color
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
                        display::dma->drawPixel(x + xOffset, y,
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
                display::dma->drawPixel(x, y, usPalette[*s++]); // color 565
            }
        }

        if(pDraw->y == pDraw->iHeight - 1)
            overlay::draw();
    }

    int draw_next_frame()
    {
        if (!gif_open)
        {
            if (!gif.open(filename, open, close, read, seek, draw))
            {
                String err("gif error: ");
                err += gif.getLastError();
                log(err.c_str(), true);

                return gif.getLastError();
            }

            offset[0] = (PANEL_RES_X - gif.getCanvasWidth()) / 2;
            offset[1] = (PANEL_RES_Y - gif.getCanvasHeight()) / 2;

            gif_open = true;
        }

        if (gif_open && !gif.playFrame(true, NULL))
            gif.reset();

        return 0;
    }
}
