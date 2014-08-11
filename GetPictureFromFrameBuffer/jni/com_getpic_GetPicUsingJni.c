#include <android/log.h>
#define LOG_TAG "debug"
#define LOGI(fmt, args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##args)

	/******************************************************************** 
	    created:    2012/02/07 
	    filename:   myfb.c 
	    author:      
	     
	    purpose:     
	*********************************************************************/  
	#ifndef WIN32  
	//-------------------------------------------------------------------  
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <linux/kd.h>

#include <memory.h>
#include <jni.h>

typedef struct {
	long filesize;
	char reserved[2];
	long headersize;
	long infoSize;
	long width;
	long depth;
	short biPlanes;
	short bits;
	long biCompression;
	long biSizeImage;
	long biXPelsPerMeter;
	long biYPelsPerMeter;
	long biClrUsed;
	long biClrImportant;
} BMPHEAD;

	struct FB
	{
	   unsigned short *bits;
	   unsigned size;
	   int fd;
	   struct fb_fix_screeninfo fi;
	   struct fb_var_screeninfo vi;
	};

	int fb_bpp(struct FB *fb)
	{
	    if (fb)
	    {
	    	return fb->vi.bits_per_pixel;
	    }
	    return 0;
	}

	int fb_width(struct FB *fb)
	{
	    if (fb)
	    {
	        return fb->vi.xres;
	    }
	    return 0;
	}

	int fb_height(struct FB *fb)
	{
	    if (fb)
	    {
	        return fb->vi.yres;
	    }
	    return 0;
	}

	int fb_size(struct FB *fb)
	{
	    if (fb)
	    {
	        unsigned bytespp = fb->vi.bits_per_pixel / 8;
	        return (fb->vi.xres * fb->vi.yres * bytespp);
	    }
	    return 0;
	}

	int fb_virtual_size(struct FB *fb)
	{
	    if (fb)
	    {
	        unsigned bytespp = fb->vi.bits_per_pixel / 8;
	        return (fb->vi.xres_virtual * fb->vi.yres_virtual * bytespp);
	    }
	    return 0;
	}

	void * fb_bits(struct FB *fb)
	{
	    unsigned short * bits = NULL;
	    if (fb)
	    {
	    	int offset, bytespp;
	        bytespp = fb->vi.bits_per_pixel / 8;

	        /* HACK: for several of our 3d cores a specific alignment
	        * is required so the start of the fb may not be an integer number of lines
	        * from the base.  As a result we are storing the additional offset in
	        * xoffset. This is not the correct usage for xoffset, it should be added
	        * to each line, not just once at the beginning */
	        offset = fb->vi.xoffset * bytespp;
	        offset += fb->vi.xres * fb->vi.yoffset * bytespp;
	        bits = fb->bits + offset / sizeof(*fb->bits);
	    }
	    return bits;
	}

	void fb_update(struct FB *fb)
	{
	    if (fb)
	    {
	        fb->vi.yoffset = 1;
	        ioctl(fb->fd, FBIOPUT_VSCREENINFO, &fb->vi);
	        fb->vi.yoffset = 0;
	        ioctl(fb->fd, FBIOPUT_VSCREENINFO, &fb->vi);
	    }
	}

	int fb_map_size(struct FB *fb)
	{
	    int offset = (fb->vi.xoffset + fb->vi.yoffset*fb->vi.xres) * 3;
	    int w = fb->vi.xres;
	    int h = fb->vi.yres;
	    LOGI("offset x = %d y = %d size = %d", fb->vi.xoffset, fb->vi.yoffset, offset);
	    LOGI("w = %d, h = %d", w, h);
	    int size = w*h*3;
	    int mapsize = offset + size;

	    return mapsize;
	}

	static int fb_open(struct FB *fb)
	{
	    if (NULL == fb)
	    {
	        return -1;
	    }

	    fb->fd = open("/dev/graphics/fb0", O_RDONLY);
	    if (fb->fd < 0)
	    {
	        printf("open(\"/dev/graphics/fb0\") failed!\n");
			LOGI("---open(\"/dev/graphics/fb0\") failed!---");
	        return -1;
	    }

	    if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->fi) < 0)
	    {
	    	printf("FBIOGET_FSCREENINFO failed!\n");
			LOGI("---FBIOGET_FSCREENINFO failed!---");
	        goto fail;
	    }
	    if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->vi) < 0)
	    {
	    	printf("FBIOGET_VSCREENINFO failed!\n");
			LOGI("---FBIOGET_VSCREENINFO failed!---");
	        goto fail;
	    }

	    fb->bits = mmap(0, fb_virtual_size(fb), PROT_READ, MAP_SHARED, fb->fd, 0);

		//查看framebuffer位图格式
		LOGI("---framebuffer位图格式为%d位：---", fb->vi.bits_per_pixel);
		LOGI("framebuffer size = %d", fb_map_size(fb));

	    if (fb->bits == MAP_FAILED) {
	        printf("mmap() failed!\n");
			LOGI("---mmap()失败！---");
	        goto fail;
	    }

	    return 0;

	fail:
		LOGI("---fb_open()失败！---");
	    close(fb->fd);
	    return -1;
	}

	static void fb_close(struct FB *fb)
	{
		if (fb)
		{
			munmap(fb->bits, fb_virtual_size(fb));
			close(fb->fd);
	    }
	}

	static struct FB g_fb;
	struct FB * fb_create(void)
	{
	    memset(&g_fb, 0, sizeof(struct FB));
	    if (fb_open(&g_fb))
	    {
	        return NULL;
	    }
	    return &g_fb;
	}

	void fb_destory(struct FB *fb)
	{
	    fb_close(fb);
	}
//-------------------------------------------------------------------

/********************************************************************
 created:    2012/02/07
 filename:   savebmp.c
 author:

 purpose:
 *********************************************************************/

//-------------------------------------------------------------------
/*
 位图文件的组成
 结构名称 符 号
 位图文件头 (bitmap-file header) BITMAPFILEHEADER bmfh
 位图信息头 (bitmap-information header) BITMAPINFOHEADER bmih
 彩色表　(color table) RGBQUAD aColors[]
 图象数据阵列字节 BYTE aBitmapBits[]
 */
typedef struct bmp_header {
	short twobyte; //两个字节，用来保证下面成员紧凑排列，这两个字符不能写到文件中
	//14B
	char bfType[2]; //!文件的类型,该值必需是0x4D42，也就是字符'BM'
	unsigned int bfSize; //!说明文件的大小，用字节为单位
	unsigned int bfReserved1; //保留，必须设置为0
	unsigned int bfOffBits; //!说明从文件头开始到实际的图象数据之间的字节的偏移量，这里为14B+sizeof(BMPINFO)
} BMPHEADER;

typedef struct bmp_info {
	//40B
	unsigned int biSize; //!BMPINFO结构所需要的字数
	int biWidth; //!图象的宽度，以象素为单位
	int biHeight; //!图象的宽度，以象素为单位,如果该值是正数，说明图像是倒向的，如果该值是负数，则是正向的
	unsigned short biPlanes; //!目标设备说明位面数，其值将总是被设为1
	unsigned short biBitCount; //!比特数/象素，其值为1、4、8、16、24、或32
	unsigned int biCompression; //说明图象数据压缩的类型
#define BI_RGB        0L    //没有压缩
#define BI_RLE8       1L    //每个象素8比特的RLE压缩编码，压缩格式由2字节组成（重复象素计数和颜色索引）；
#define BI_RLE4       2L    //每个象素4比特的RLE压缩编码，压缩格式由2字节组成
#define BI_BITFIELDS  3L    //每个象素的比特由指定的掩码决定。
	unsigned int biSizeImage; //图象的大小，以字节为单位。当用BI_RGB格式时，可设置为0
	int biXPelsPerMeter; //水平分辨率，用象素/米表示
	int biYPelsPerMeter; //垂直分辨率，用象素/米表示
	unsigned int biClrUsed; //位图实际使用的彩色表中的颜色索引数（设为0的话，则说明使用所有调色板项）
	unsigned int biClrImportant; //对图象显示有重要影响的颜色索引的数目，如果是0，表示都重要。
} BMPINFO;

typedef struct tagRGBQUAD {
	unsigned char rgbBlue;
	unsigned char rgbGreen;
	unsigned char rgbRed;
	unsigned char rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO {
	BMPINFO bmiHeader;
	//RGBQUAD    bmiColors[1];
	unsigned int rgb[3];
} BITMAPINFO;

static int get_rgb888_header(int w, int h, BMPHEADER * head, BMPINFO * info) {
	int size = 0;
	if (head && info) {
		size = w * h * 3;
		memset(head, 0, sizeof(*head));
		memset(info, 0, sizeof(*info));
		head->bfType[0] = 'B';
		head->bfType[1] = 'M';
		head->bfOffBits = 14 + sizeof(*info);
		head->bfSize = head->bfOffBits + size;
		head->bfSize = (head->bfSize + 3) & ~3; //windows要求文件大小必须是4的倍数
		size = head->bfSize - head->bfOffBits;

		info->biSize = sizeof(BMPINFO);
		info->biWidth = w;
		info->biHeight = -h;
		info->biPlanes = 1;
		info->biBitCount = 24;
		info->biCompression = BI_RGB;
		info->biSizeImage = size;

		printf("rgb888:%dbit,%d*%d,%d\n", info->biBitCount, w, h, head->bfSize);
	}
	return size;
}

static int get_rgb8888_header(int w, int h, BMPHEADER * head, BMPINFO * info) {
	int size = 0;
	if (head && info) {
		size = w * h * 4;
		memset(head, 0, sizeof(*head));
		memset(info, 0, sizeof(*info));
		head->bfType[0] = 'B';
		head->bfType[1] = 'M';
		head->bfOffBits = 14 + sizeof(*info);
		head->bfSize = head->bfOffBits + size;
		head->bfSize = (head->bfSize + 3) & ~3; //windows要求文件大小必须是4的倍数
		size = head->bfSize - head->bfOffBits;

		info->biSize = sizeof(BMPINFO);
		info->biWidth = w;
		info->biHeight = -h;
		info->biPlanes = 1;
		info->biBitCount = 32;
		info->biCompression = BI_RGB;
		info->biSizeImage = size;

		printf("rgb8888:%dbit,%d*%d,%d\n", info->biBitCount, w, h, head->bfSize);
	}
	return size;
}

static int get_rgb565_header(int w, int h, BMPHEADER * head, BITMAPINFO * info) {
	int size = 0;
	if (head && info) {
		size = w * h * 2;
		memset(head, 0, sizeof(*head));
		memset(info, 0, sizeof(*info));
		head->bfType[0] = 'B';
		head->bfType[1] = 'M';
		head->bfOffBits = 14 + sizeof(*info);
		head->bfSize = head->bfOffBits + size;
		head->bfSize = (head->bfSize + 3) & ~3;
		size = head->bfSize - head->bfOffBits;

		info->bmiHeader.biSize = sizeof(info->bmiHeader);
		info->bmiHeader.biWidth = w;
		info->bmiHeader.biHeight = -h;
		info->bmiHeader.biPlanes = 1;
		info->bmiHeader.biBitCount = 16;
		info->bmiHeader.biCompression = BI_BITFIELDS;
		info->bmiHeader.biSizeImage = size;

		//只有16位才需要调试板，24与32位不需要
		info->rgb[0] = 0xF800;
		info->rgb[1] = 0x07E0;
		info->rgb[2] = 0x001F;

		printf("rgb565:%dbit,%d*%d,%d\n", info->bmiHeader.biBitCount, w, h,
				head->bfSize);
	}
	return size;
}

static int save_bmp_rgb565(FILE * hfile, int w, int h, void * pdata) {
	int success = 0;
	int size = 0;
	BMPHEADER head;
	BITMAPINFO info;

	size = get_rgb565_header(w, h, &head, &info);
	if (size > 0) {
		fwrite(head.bfType, 1, 14, hfile);//这里把head数据结构里面的所有数据都写入文件里了，因为是写入14B
		fwrite(&info, 1, sizeof(info), hfile);
		fwrite(pdata, 1, size, hfile);
		success = 1;
	}

	return success;
}

static int save_bmp_rgb888(FILE * hfile, int w, int h, void * pdata) {
	int success = 0;
	int size = 0;
	BMPHEADER head;
	BMPINFO info;

	size = get_rgb888_header(w, h, &head, &info);
	if (size > 0) {
		fwrite(head.bfType, 1, 14, hfile);
		fwrite(&info, 1, sizeof(info), hfile);
		fwrite(pdata, 1, size, hfile);
		success = 1;
	}

	return success;
}

static int save_bmp_rgb8888(FILE * hfile, int w, int h, void * pdata) {
	int success = 0;
	int size = 0;
	BMPHEADER head;
	BMPINFO info;

	size = get_rgb8888_header(w, h, &head, &info);
	if (size > 0) {
		fwrite(head.bfType, 1, 14, hfile);
		fwrite(&info, 1, sizeof(info), hfile);
		fwrite(pdata, 1, size, hfile);
		success = 1;
	}

	return success;
}

int save_bmp(const char * path, int w, int h, void * pdata, int bpp) {
	int success = 0;
	FILE * hfile = NULL;

	do {
		if (path == NULL || w <= 0 || h <= 0 || pdata == NULL) {
			printf("if (path == NULL || w <= 0 || h <= 0 || pdata == NULL)\n");
			LOGI("---条件未满足！---");
			if(path == NULL){
				LOGI("---path = NULL---");
			}
			if(pdata == NULL){
				LOGI("---pdata = NULL---");
			}
			break;
		}

		remove(path);
		hfile = fopen(path, "wb");
		if (hfile == NULL) {
			printf("open(%s) failed!\n", path);
			LOGI("---打开BMP文件失败！---");
			break;
		}

		switch (bpp) {
		case 16:
			success = save_bmp_rgb565(hfile, w, h, pdata);
			break;
		case 24:
			success = save_bmp_rgb888(hfile, w, h, pdata);
			break;
		case 32:
			success = save_bmp_rgb8888(hfile, w, h, pdata);
			break;
		default:
			printf("error: not support format!\n");
			LOGI("---不支持的格式！---");
			success = 0;
			break;
		}
	} while (0);

	if (hfile != NULL)
		fclose(hfile);

	return success;
}

int save(char *filePath)
{
			int w = g_fb.vi.xres + 8, h = g_fb.vi.yres, depth = g_fb.vi.bits_per_pixel;

				//convert pixel data
			uint8_t *rgb24;
			int mapSize = fb_map_size(&g_fb);

			rgb24 = (uint8_t *) malloc(w * h * 3);
			int i=0;

			for (;i<w * h;i++)
			{
				uint32_t pixel32 = ((uint32_t *)g_fb.bits)[i];
				// in rgb24 color max is 2^8 per channel
				rgb24[3*i+0]   =  pixel32 & 0x000000FF; 		//Blue
				rgb24[3*i+1]   = (pixel32 & 0x0000FF00) >> 8;	//Green
				rgb24[3*i+2]   = (pixel32 & 0x00FF0000) >> 16; 	//Red
				//LOGI("r = %d, g = %d, b = %d", rgb24[3*i+0], rgb24[3*i+1], rgb24[3*i+2]);
			}

			//save RGB 24 Bitmap
				int bytes_per_pixel = 3;
				BMPHEAD bh;
				memset ((char *)&bh,0,sizeof(BMPHEAD)); // sets everything to 0
				//bh.filesize  =   calculated size of your file (see below)
				//bh.reserved  = two zero bytes
				bh.headersize  = 54L;			// for 24 bit images
				bh.infoSize  =  0x28L;		// for 24 bit images
				bh.width     = w;			// width of image in pixels
				bh.depth     = h;			// height of image in pixels
				bh.biPlanes  =  1;			// for 24 bit images
				bh.bits      = 8 * bytes_per_pixel;	// for 24 bit images
				bh.biCompression = 0L;		// no compression
				int bytesPerLine;
				bytesPerLine = w * bytes_per_pixel;  	// for 24 bit images
				//round up to a dword boundary
				if (bytesPerLine & 0x0003)
				{
					bytesPerLine |= 0x0003;
					++bytesPerLine;
				}
				bh.filesize = bh.headersize + (long)bytesPerLine * bh.depth;
				FILE * bmpfile;
				//printf("Bytes per line : %d\n", bytesPerLine);
				bmpfile = fopen(filePath, "wb");
				if (bmpfile == NULL)
				{
					fb_destory(&g_fb);
					return -1;
				}
				fwrite("BM",1,2,bmpfile);
				fwrite((char *)&bh, 1, sizeof (bh), bmpfile);
				//fwrite(rgb24,1,w*h*3,bmpfile);
				char *linebuf;
				linebuf = (char *) calloc(1, bytesPerLine);
				if (linebuf == NULL)
				{
					fclose(bmpfile);
					fb_destory(&g_fb);
					return -1;
				}
				int line,x;
				for (line = h-1; line >= 0; line --)
				{
					// fill line linebuf with the image data for that line
					for( x =0 ; x < w; x++ )
					{
						*(linebuf+x*bytes_per_pixel) = *(rgb24 + (x+line*w)*bytes_per_pixel+2);
						*(linebuf+x*bytes_per_pixel+1) = *(rgb24 + (x+line*w)*bytes_per_pixel+1);
						*(linebuf+x*bytes_per_pixel+2) = *(rgb24 + (x+line*w)*bytes_per_pixel+0);
					}
					// remember that the order is BGR and if width is not a multiple
					// of 4 then the last few bytes may be unused
					fwrite(linebuf, 1, bytesPerLine, bmpfile);
				}
				fclose(bmpfile);
}

//-------------------------------------------------------------------
JNIEXPORT jint JNICALL Java_com_syouquan_script_ScriptEngine_nativeScreenShot
(JNIEnv *env, jobject thiz, jint width, jint height, jstring path)
{
	  LOGI("---进入到屏幕截图本地调用函数！---");

		fb_create();

		char *filePath = (char*)(*env)->GetStringUTFChars(env, path, NULL);

		int w = g_fb.vi.xres + 8, h = g_fb.vi.yres, depth = g_fb.vi.bits_per_pixel;

		LOGI("native capture 333333333");
		//convert pixel data
		uint8_t *rgb24;
		rgb24 = (uint8_t *) malloc(w * h * 3);
		int i=0;
		for (;i<w*h;i++)
		{
			uint32_t pixel32 = ((uint32_t *)g_fb.bits)[i];
			// in rgb24 color max is 2^8 per channel
			rgb24[3*i+0]   =  pixel32 & 0x000000FF; 		//Blue
			rgb24[3*i+1]   = (pixel32 & 0x0000FF00) >> 8;	//Green
			rgb24[3*i+2]   = (pixel32 & 0x00FF0000) >> 16; 	//Red
		}

		//save RGB 24 Bitmap
		int bytes_per_pixel = 3;
		BMPHEAD bh;
		LOGI("native capture 444444444444");
		memset ((char *)&bh,0,sizeof(BMPHEAD)); // sets everything to 0
		//bh.filesize  =   calculated size of your file (see below)
		//bh.reserved  = two zero bytes
		bh.headersize  = 54L;			// for 24 bit images
		bh.infoSize  =  0x28L;		// for 24 bit images
		bh.width     = w;			// width of image in pixels
		bh.depth     = h;			// height of image in pixels
		bh.biPlanes  =  1;			// for 24 bit images
		bh.bits      = 8 * bytes_per_pixel;	// for 24 bit images
		bh.biCompression = 0L;		// no compression
		int bytesPerLine;
		bytesPerLine = w * bytes_per_pixel;  	// for 24 bit images
		//round up to a dword boundary
		if (bytesPerLine & 0x0003)
		{
			bytesPerLine |= 0x0003;
			++bytesPerLine;
		}
		bh.filesize = bh.headersize + (long)bytesPerLine * bh.depth;
		FILE * bmpfile;
		//printf("Bytes per line : %d\n", bytesPerLine);
		bmpfile = fopen(filePath, "wb");
		if (bmpfile == NULL)
		{
			return -1;
		}
		fwrite("BM",1,2,bmpfile);
		fwrite((char *)&bh, 1, sizeof (bh), bmpfile);
		//fwrite(rgb24,1,w*h*3,bmpfile);
		char *linebuf;
		linebuf = (char *) calloc(1, bytesPerLine);
		if (linebuf == NULL)
		{
			fclose(bmpfile);
			return -1;
		}
		int line,x;
		for (line = h-1; line >= 0; line --)
		{
			// fill line linebuf with the image data for that line
			for( x =0 ; x < w; x++ )
			{
				*(linebuf+x*bytes_per_pixel) = *(rgb24 + (x+line*w)*bytes_per_pixel+2);
				*(linebuf+x*bytes_per_pixel+1) = *(rgb24 + (x+line*w)*bytes_per_pixel+1);
				*(linebuf+x*bytes_per_pixel+2) = *(rgb24 + (x+line*w)*bytes_per_pixel+0);
			}
			// remember that the order is BGR and if width is not a multiple
			// of 4 then the last few bytes may be unused
			fwrite(linebuf, 1, bytesPerLine, bmpfile);
		}
		fclose(bmpfile);


		fb_destory(&g_fb);
		if(i == 0)
		{
			LOGI("---生成文件失败！---");
			return -1;
		}
		LOGI("---生成文件成功！---");
		return 0;			
}

  #endif//#ifndef WIN32  

