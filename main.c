/*
 * newtest.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


static char VERSION[] = "XX.YY.ZZ";

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>


#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"

#include "ws2811.h"


#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                18
#define DMA                     10
//#define STRIP_TYPE            WS2811_STRIP_RGB		// WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE              WS2811_STRIP_GBR		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE            SK6812_STRIP_RGBW		// SK6812RGBW (NOT SK6812RGB)

#define TAPE_LED_NUM            60

enum {
	LED_COLOR_CLEAR  = 0x00000000,
	LED_COLOR_RED    = 0x00000040,  // R
	LED_COLOR_ORANGE = 0x00002040,
	LED_COLOR_YELLOW = 0x00004040,
	LED_COLOR_GREEN  = 0x00004000,  // G
	LED_COLOR_SKY    = 0x00404000,
	LED_COLOR_BLUE   = 0x00400000,  // B
	LED_COLOR_PURPLE = 0x00200020,
};

enum {
	LED_BRIGHTNESS_OFF = 0,
	LED_BRIGHTNESS_LV1 = 1,
	LED_BRIGHTNESS_LV2 = 2,
	LED_BRIGHTNESS_LV3 = 3,
};

enum {
//  LED_UPDATE_INTERVAL_NONE    = 0,        // ずっと点灯
	LED_UPDATE_INTERVAL_1S      = 1000000,
	LED_UPDATE_INTERVAL_500MS   =  500000,
	LED_UPDATE_INTERVAL_100MS   =  100000,
	LED_UPDATE_INTERVAL_50MS    =   50000,
	LED_UPDATE_INTERVAL_10MS    =   10000,
	LED_UPDATE_INTERVAL_1FRAME  =   16666,  // 60fps
};

enum {
	LED_PREV_CLEAR = 0,
	LED_PREV_STAY  = 1,
};

int gLedNum = TAPE_LED_NUM;
int clear_on_exit = 0;

typedef struct 
{
	uint8_t addr;
	ws2811_led_t color;
	uint8_t brightness;
} pointLed_t;

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .invert = 0,
            .count = TAPE_LED_NUM,
            .strip_type = STRIP_TYPE,
            .brightness = 32,
        },
        [1] =
        {
            .gpionum = 0,
            .invert = 0,
            .count = 0,
            .brightness = 0,
        },
    },
};

// ws2811_led_t *matrix;
ws2811_led_t *gpMatrix;
static uint8_t running = 1;

// Blink Pattern
ws2811_led_t blinkPtn_a[] =
{
	LED_COLOR_RED,
	LED_COLOR_CLEAR,
	LED_COLOR_GREEN,
	LED_COLOR_CLEAR,
	LED_COLOR_BLUE,
	LED_COLOR_CLEAR,
};

// Rainbow Pattern
ws2811_led_t blinkPtn_rainbow[] =
{
	LED_COLOR_RED,
	LED_COLOR_ORANGE,
	LED_COLOR_YELLOW,
	LED_COLOR_GREEN,
	LED_COLOR_SKY,
	LED_COLOR_BLUE,
	LED_COLOR_PURPLE,
};

pointLed_t pointDataSample[] =
{
	{0,  LED_COLOR_RED,    LED_BRIGHTNESS_LV1},
	{8,  LED_COLOR_BLUE,   LED_BRIGHTNESS_LV2},
	{10, LED_COLOR_GREEN,  LED_BRIGHTNESS_LV3},
	{21, LED_COLOR_ORANGE, LED_BRIGHTNESS_LV2},
	{30, LED_COLOR_ORANGE, LED_BRIGHTNESS_LV3},
	{59, LED_COLOR_PURPLE, LED_BRIGHTNESS_LV1},
	{60, LED_COLOR_YELLOW, LED_BRIGHTNESS_LV3},  // Illegal Data
};

// TapeLED 指定したColorMatrix情報をTapeLEDに反映させる
void tapeLed_render(void)
{
    int x;

    for (x = 0; x < gLedNum; x++)
    {
		ledstring.channel[0].leds[x] = gpMatrix[x];
    }
}

// TapeLED 全体を指定した色で点灯する
ws2811_return_t tapeLed_whole(ws2811_led_t lColor, uint8_t lBrightness)
{
	ws2811_return_t lRet;
	int x;
	for (x = 0; x < gLedNum; x++) {
		gpMatrix[x] = lColor * lBrightness;
	}
	tapeLed_render();
	if ((lRet = ws2811_render(&ledstring)) != WS2811_SUCCESS)
	{
		fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(lRet));
	}
	return lRet;
}

// TapeLED 特定位置を指定した色で点灯する
ws2811_return_t tapeLed_point(uint16_t lAddr, ws2811_led_t lColor, uint8_t lBrightness)
{
	ws2811_return_t lRet;
	int x;

	if (lAddr >= gLedNum) {
		lRet = WS2811_ERROR_OUT_OF_MEMORY;
		return lRet;
	}
	for (x = 0; x < gLedNum; x++) {
		gpMatrix[x] = LED_COLOR_CLEAR;
	}
	gpMatrix[lAddr] = lColor * lBrightness;
	tapeLed_render();
	if ((lRet = ws2811_render(&ledstring)) != WS2811_SUCCESS)
	{
		fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(lRet));
	}
	return lRet;
}

// TapeLED 特定位置を指定した色で点灯する, Table指定により複数個所を同時に設定可能
ws2811_return_t tapeLed_point2(pointLed_t *lpPointData, uint8_t lDataNum)
{
	ws2811_return_t lRet;
	int i, x;

	for (x = 0; x < gLedNum; x++) {
		gpMatrix[x] = LED_COLOR_CLEAR;
	}

	for (i = 0; i < lDataNum; i++) {
		if (lpPointData[i].addr >= gLedNum) {
			continue;
		}
		gpMatrix[lpPointData[i].addr] = lpPointData[i].color * lpPointData[i].brightness;
	}
	tapeLed_render();
	if ((lRet = ws2811_render(&ledstring)) != WS2811_SUCCESS)
	{
		fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(lRet));
	}
	return lRet;
}

// TapeLED 全体を指定した色テーブルで点滅させる
ws2811_return_t tapeLed_blink(ws2811_led_t *lpColorTbl, uint8_t lTblSize, uint8_t lBrightness, uint32_t lWaitUs)
{
	ws2811_return_t lRet;
	int i;

	i = 0;
    while (running) {
		lRet = tapeLed_whole(lpColorTbl[i], lBrightness);
		if (lRet != WS2811_SUCCESS)
		{
			fprintf(stderr, "tapeLed_whole failed: %s\n", ws2811_get_return_t_str(lRet));
			break;
		}
		usleep(lWaitUs);
		if (++i >= lTblSize) {
			i = 0;
		}
	}
	return lRet;
}

// TapeLED 点灯データ位置を指定量動かす
ws2811_return_t tapeLed_move(int16_t lMoveNum) 
{
	ws2811_return_t lRet;
	int x;
	int16_t lCnt;
	ws2811_led_t lTmpData;

	if (lMoveNum >= gLedNum) {
		lMoveNum = 0;
	}
	if (lMoveNum > 0) {
		for (lCnt = 0; lCnt < lMoveNum; lCnt++) {
			lTmpData = gpMatrix[gLedNum - 1];
			for (x = gLedNum - 1; x > 0; x--) {
				gpMatrix[x] = gpMatrix[x - 1];
			}
			gpMatrix[x] = lTmpData;
		}
	}
	else if (lMoveNum < 0) {
		for (lCnt = 0; lCnt < (-1) * lMoveNum; lCnt++) {
			lTmpData = gpMatrix[0];
			for (x = 1; x < gLedNum; x++) {
				gpMatrix[x - 1] = gpMatrix[x];
			}
			gpMatrix[gLedNum - 1] = lTmpData;
		}
	}
	tapeLed_render();
	if ((lRet = ws2811_render(&ledstring)) != WS2811_SUCCESS)
	{
		fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(lRet));
	}
	return lRet;
}

static void ctrl_c_handler(int signum)
{
	(void)(signum);
    running = 0;
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };

    sigaction(SIGINT, &sa, NULL);   // Ctrl+C
    sigaction(SIGTERM, &sa, NULL);  // Kill command
}

void parseargs(int argc, char **argv)
{
	int index;
	int c;

	static struct option longopts[] =
	{
		{"clear", no_argument, 0, 'c'},
		{0, 0, 0, 0}
	};

	while (1)
	{

		index = 0;
		c = getopt_long(argc, argv, "c", longopts, &index);

		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			/* handle flag options (array's 3rd field non-0) */
			break;
		case 'c':
			clear_on_exit=1;
			break;
		default:
			exit(-1);
		}
	}
}

// 動作モードSW(For Debug)
#define POINT   1
#define WHOLE   2
#define MOVE_P  3
#define MOVE_M  4
#define BLINK   5
#define RAINBOW 6

#define LED_MODE  BLINK

int myTapeLed()
{
	ws2811_return_t ret;

	// initialize
	gpMatrix = malloc(sizeof(ws2811_led_t) * gLedNum);
    setup_handlers();  // Set SignalHandlers

    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        return ret;
    }

    while (running)
    {
#if LED_MODE == POINT
		ret = tapeLed_point(10, LED_COLOR_GREEN, LED_BRIGHTNESS_LV3);
#elif LED_MODE == WHOLE
		ret = tapeLed_whole(LED_COLOR_ORANGE, LED_BRIGHTNESS_LV3);
#elif LED_MODE == MOVE_P
		ret = tapeLed_point2(pointDataSample, (uint8_t)ARRAY_SIZE(pointDataSample));
		usleep(1000000);  // 1sec
		while (running) {
			ret = tapeLed_move(1);
			usleep(100000);
		}
#elif LED_MODE == MOVE_M
		ret = tapeLed_point2(pointDataSample, (uint8_t)ARRAY_SIZE(pointDataSample));
		usleep(1000000);  // 1sec
		while (running) {
			ret = tapeLed_move(-10);
			usleep(100000);
		}
#elif LED_MODE == BLINK
		ret = tapeLed_blink(blinkPtn_a, (uint8_t)ARRAY_SIZE(blinkPtn_a), LED_BRIGHTNESS_LV1, LED_UPDATE_INTERVAL_500MS);
#elif LED_MODE == RAINBOW
		ret = tapeLed_blink(blinkPtn_rainbow, (uint8_t)ARRAY_SIZE(blinkPtn_rainbow), LED_BRIGHTNESS_LV2, LED_UPDATE_INTERVAL_500MS);
#endif
	}

    if (clear_on_exit) {
		tapeLed_whole(LED_COLOR_CLEAR, LED_BRIGHTNESS_OFF);
    }

    ws2811_fini(&ledstring);

    printf ("\n");
    return ret;
}

#define VER_MAJOR  0
#define VER_MINOR  3

int main(int argc, char *argv[])
{
	//int verMajor = VER_MAJOR, verMinor = VER_MINOR;
    ws2811_return_t ret;

	// version info
    sprintf(VERSION, "%d.%d", VER_MAJOR, VER_MINOR);
	printf("06hackason: v%s\n", VERSION);

	// Parse Argument
	parseargs(argc, argv);

	ret = myTapeLed();
    return ret;
}
