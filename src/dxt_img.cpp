#include <vector>
#include <osg/Image>
using namespace std;

struct Color {
    int r;
    int g;
    int b;
};

Color RGB565_RGB(unsigned short color0) {
    unsigned long temp;
    temp = (color0 >> 11) * 255 + 16;
    unsigned char r0 = (unsigned char)((temp / 32 + temp) / 32);
    temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
    unsigned char g0 = (unsigned char)((temp / 64 + temp) / 64);
    temp = (color0 & 0x001F) * 255 + 16;
    unsigned char b0 = (unsigned char)((temp / 32 + temp) / 32);
    return Color{ r0,g0,b0 };
}

Color Mix_Color(
    unsigned short color0, unsigned short color1, 
    Color c0, Color c1, int idx) {
    Color finalColor;
    if (color0 > color1)
    {
        switch (idx)
        {
        case 0:
            finalColor = Color{ c0.r, c0.g, c0.b };
            break;
        case 1:
            finalColor = Color{ c1.r, c1.g, c1.b };
            break;
        case 2:
            finalColor = Color{ 
                (2 * c0.r + c1.r) / 3, 
                (2 * c0.g + c1.g) / 3,
                (2 * c0.b + c1.b) / 3};
            break;
        case 3:
            finalColor = Color{
                (c0.r + 2 * c1.r) / 3,
                (c0.g + 2 * c1.g) / 3,
                (c0.b + 2 * c1.b) / 3 };
            break;
        }
    }
    else
    {
        switch (idx)
        {
        case 0:
            finalColor = Color{ c0.r, c0.g, c0.b };
            break;
        case 1:
            finalColor = Color{ c1.r, c1.g, c1.b };
            break;
        case 2:
            finalColor = Color{ (c0.r + c1.r) / 2, (c0.g + c1.g) / 2, (c0.b + c1.b) / 2 };
            break;
        case 3:
            finalColor = Color{ 0, 0, 0 };
            break;
        }
    }
    return finalColor;
}

void resize_Image(vector<unsigned char>& jpeg_buf, int width, int height, int new_w, int new_h) {
    vector<unsigned char> new_buf(new_w * new_h * 3);
    int scale = width / new_w;
    for (int row = 0; row < new_h ; row++)
    {
        for(int col = 0; col < new_w; col++) {
            int pos = row * new_w + col;
            int old_pos = (row * width + col) * scale;
            for (int i = 0; i < 3 ; i++)
            {
                new_buf[3 * pos + i] = jpeg_buf[3 * old_pos + i];
            }
        }
    }
    jpeg_buf = new_buf;
}

void fill_4BitImage(vector<unsigned char>& jpeg_buf, osg::Image* img, int& width, int& height) {
    jpeg_buf.resize(width * height * 3);
    unsigned char* pData = img->data();
    int imgSize = img->getImageSizeInBytes();
    int x_pos = 0;
    int y_pos = 0;
    for (size_t i = 0; i < imgSize; i += 8)
    {
        // 64 bit matrix
        unsigned short color0, color1;
        memcpy(&color0, pData, 2);
        pData += 2;
        memcpy(&color1, pData, 2);
        pData += 2;
        Color c0 = RGB565_RGB(color0);
        Color c1 = RGB565_RGB(color1);
        for (size_t i = 0; i < 4; i++)
        {
            unsigned char idx[4];
            idx[0] = (*pData >> 6) & 0x03;
            idx[1] = (*pData >> 4) & 0x03;
            idx[2] = (*pData >> 2) & 0x03;
            idx[3] = (*pData) & 0x03;
            // 4 pixel color
            for (size_t pixel_idx = 0; pixel_idx < 4; pixel_idx++)
            {
                Color cf = Mix_Color(color0, color1, c0, c1, idx[pixel_idx]);
                int cell_x_pos = x_pos + pixel_idx;
                int cell_y_pos = y_pos + i;
                int byte_pos = (cell_x_pos + cell_y_pos * width) * 3;
                jpeg_buf[byte_pos] = cf.r;
                jpeg_buf[byte_pos + 1] = cf.g;
                jpeg_buf[byte_pos + 2] = cf.b;
            }
            pData++;
        }
        x_pos += 4;
        if (x_pos >= width) {
            x_pos = 0;
            y_pos += 4;
        }
    }
    int max_size = 512;
    if (width > max_size || height > max_size) {
        int new_w = width, new_h = height;
        while (new_w > max_size || new_h > max_size)
        {
            new_w /= 2;
            new_h /= 2;
        }
        resize_Image(jpeg_buf, width, height, new_w, new_h);
        width = new_w;
        height = new_h;
    }
}
