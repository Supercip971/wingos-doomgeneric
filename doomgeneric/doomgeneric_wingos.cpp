
#include <string.h>
#include "protocols/hi/human_interface.hpp"

#include "doomkeys.h"
#include "doomgeneric.h"
#include "protocols/clock/clock.hpp"
#include "protocols/compositor/window.hpp"

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned int s_ScreenWidth = 0;
static unsigned int s_ScreenHeight = 0;

static unsigned char convertToDoomKey(unsigned char scancode)
{
    unsigned char key = 0;

    switch (scancode)
    {
    case 0x9C:
    case 0x1C:
        key = KEY_ENTER;
        break;
    case 0x01:
        key = KEY_ESCAPE;
        break;
    case 0xCB:
    case 0x4B:
        key = KEY_LEFTARROW;
        break;
    case 0xCD:
    case 0x4D:
        key = KEY_RIGHTARROW;
        break;
    case 0xC8:
    case 0x48:
        key = KEY_UPARROW;
        break;
    case 0xD0:
    case 0x50:
        key = KEY_DOWNARROW;
        break;
    case 0x1D:
        key = KEY_FIRE;
        break;
    case 0x39:
        key = KEY_USE;
        break;
    case 0x2A:
    case 0x36:
        key = KEY_RSHIFT;
        break;
    case 0x15:
        key = 'y';
        break;
    default:
        break;
    }

    return key;
}

void addKeyToQueue(int pressed, unsigned char keyCode)
{
    // printf("key hex %x decimal %d\n", keyCode, keyCode);

    unsigned char key = convertToDoomKey(keyCode);

    unsigned short keyData = (pressed << 8) | key;

    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

void disableRawMode()
{
    // printf("returning original termios\n");
}

void enableRawMode()
{
}

void *bb;
prot::WindowGetAttributeSize size;
static prot::WindowConnection* g_window = nullptr;
static prot::ClockConnection* g_clock = nullptr;

static prot::HIConnection* g_inputs;
void DG_Init()
{
    // Create window and store it globally so it doesn't go out of scope

    g_inputs = new prot::HIConnection(prot::HIConnection::connect().copied());

    g_window = new prot::WindowConnection(prot::WindowConnection::create(true).copied());
    g_clock = new prot::ClockConnection(prot::ClockConnection::connect().copied());
    auto asset = g_window->get_framebuffer().copied();

    bb = asset.ptr();

    size = g_window->get_attribute_size().unwrap();

    s_ScreenWidth = size.width;
    s_ScreenHeight = size.height;

    log::log$("DG_Init: Window created {}x{}, framebuffer at {}", s_ScreenWidth, s_ScreenHeight, (uintptr_t)bb);

    g_inputs->start_listen();
    enableRawMode();
}

static void handleKeyInput()
{
    unsigned char scancode = 0;
    g_inputs->event_queue().update_event();


    while(true)
    {
        auto current_event = g_inputs->event_queue().poll_event();

        if(current_event.is_error())
        {
            break;
        }
        auto ev = current_event.unwrap();

        if(ev.type == prot::HI_EVENT_TYPE_KEYBOARD)
        {
            addKeyToQueue(ev.keyboard.pressed, ev.keyboard.keycode);
        }
    }

}

void DG_DrawFrame()
{
    static int frame_count = 0;
    frame_count++;

    if (frame_count % 60 == 0) {
        log::log$("DG_DrawFrame: frame {} - window {}x{}, bb={}",
                  frame_count, size.width, size.height, (uintptr_t)bb);
    }

    if (bb == nullptr) {
        log::err$("DG_DrawFrame: framebuffer is NULL!");
        return;
    }

    if (DG_ScreenBuffer == nullptr) {
        log::err$("DG_DrawFrame: DG_ScreenBuffer is NULL!");
        return;
    }

    // DOOM's screen buffer is DOOMGENERIC_RESX x DOOMGENERIC_RESY (640x400)
    // Copy it to the window framebuffer
    size_t copy_width = (size.width < DOOMGENERIC_RESX) ? size.width : DOOMGENERIC_RESX;
    size_t copy_height = (size.height < DOOMGENERIC_RESY) ? size.height : DOOMGENERIC_RESY;

    // Copy row by row in case strides differ
    for (size_t y = 0; y < copy_height; y++)
    {
        memcpy((uint8_t*)bb + y * size.width * 4,
               (uint8_t*)DG_ScreenBuffer + y * DOOMGENERIC_RESX * 4,
               copy_width * 4);
    }

    // Tell compositor to display the frame
    if (g_window != nullptr)
    {
        g_window->swap_buffers();
    }
    else
    {
        log::err$("DG_DrawFrame: g_window is NULL!");
    }

    handleKeyInput();
}

void DG_SleepMs(uint32_t ms)
{


    if (g_clock != nullptr)
    {
        log::log$("Sleeping for {}ms", ms);
        g_clock->sleep_ms(ms).unwrap();
    }
    // No sleep syscall available yet
}

uint32_t DG_GetTicksMs()
{





    if (g_clock != nullptr)
    {
        auto v = g_clock->get_system_time().unwrap().milliseconds;
        return v;
    }
    return 0;
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
    {
        // key queue is empty

        return 0;
    }
    else
    {
        unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
        s_KeyQueueReadIndex++;
        s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

        *pressed = keyData >> 8;
        *doomKey = keyData & 0xFF;

        return 1;
    }
}

void DG_SetWindowTitle(const char *title)
{
    (void)title;
}

int main(int argc, char **argv)
{
    log::log$("Starting doomgeneric on WingOS");
    doomgeneric_Create(argc, argv);

    log::log$("doomgeneric_Create completed, entering main loop");

    while (1)
    {
        doomgeneric_Tick();
    }

    return 0;
}
