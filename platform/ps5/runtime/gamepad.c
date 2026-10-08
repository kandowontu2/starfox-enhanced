// Feed native Pad state into SDL3's public virtual-gamepad API.
#include "native.h"

static struct Pad {
    int user, handle;
    SDL_JoystickID id;
    SDL_Joystick *joystick;
} pads[4];
static bool initialized;
// Set when SDL cannot attach virtual gamepads: retrying would only reopen pads.
static bool attach_unavailable;
static Uint64 next_scan;

static void update(void *userdata)
{
    struct Pad *pad = userdata;
    if (!pad->joystick) return;
    PS5_PadData state;
    SDL_zero(state);
    // Input the system has taken (the PS menu is open) reads as a neutral pad,
    // as for a disconnected one (ProsperoEden headless/pad.cpp).
    bool connected = scePadReadState(pad->handle, &state) >= 0 && state.connected
        && (state.buttons & PS5_PAD_BUTTON_INTERCEPTED) == 0;
    // SDL gamepad button order: south, east, west, north, back, guide, start,
    // left stick, right stick, left shoulder, right shoulder, d-pad up, down,
    // left, right. Back is the touchpad or Create; the PS button is the system's.
    static const Uint32 masks[] = {
        PS5_PAD_BUTTON_CROSS, PS5_PAD_BUTTON_CIRCLE, PS5_PAD_BUTTON_SQUARE, PS5_PAD_BUTTON_TRIANGLE,
        PS5_PAD_BUTTON_TOUCH_PAD | PS5_PAD_BUTTON_CREATE, 0, PS5_PAD_BUTTON_OPTIONS,
        PS5_PAD_BUTTON_L3, PS5_PAD_BUTTON_R3, PS5_PAD_BUTTON_L1, PS5_PAD_BUTTON_R1,
        PS5_PAD_BUTTON_UP, PS5_PAD_BUTTON_DOWN, PS5_PAD_BUTTON_LEFT, PS5_PAD_BUTTON_RIGHT
    };
    for (int i = 0; i < (int)SDL_arraysize(masks); ++i)
        SDL_SetJoystickVirtualButton(pad->joystick, i, connected && (state.buttons & masks[i]) != 0);
    const Uint8 axes[] = {state.leftStick.x, state.leftStick.y,
        state.rightStick.x, state.rightStick.y, state.analogButtons.l2, state.analogButtons.r2};
    for (int i = 0; i < 6; ++i) {
        Sint16 value = connected ? (Sint16)((int)axes[i] * 257 - 32768) : (i >= 4 ? -32768 : 0);
        SDL_SetJoystickVirtualAxis(pad->joystick, i, value);
    }
}
static bool rumble(void *userdata, Uint16 low, Uint16 high)
{
    struct Pad *pad = userdata;
    PS5_PadVibration vibration = {(Uint8)(low >> 8), (Uint8)(high >> 8)};
    int result = scePadSetVibration(pad->handle, &vibration);
    return result >= 0 || SDL_SetError("scePadSetVibration: 0x%x", result);
}
static void close_pad(struct Pad *pad)
{
    // SDL_Quit shuts joysticks down before video (whose quit closes the pads):
    // by then SDL has already closed and detached the virtual devices.
    if (SDL_WasInit(SDL_INIT_JOYSTICK)) {
        if (pad->joystick) SDL_CloseJoystick(pad->joystick);
        if (pad->id) SDL_DetachVirtualJoystick(pad->id);
    }
    if (pad->handle >= 0) scePadClose(pad->handle);
    SDL_zero(*pad);
    pad->handle = -1;
    pad->user = -1;
}
void StarfoxPS5_CloseGamepads(void)
{
    if (!initialized) return;
    for (int i = 0; i < 4; ++i) close_pad(&pads[i]);
    initialized = false;
    next_scan = 0;
}
// Opens the user's pad and attaches it to SDL as a virtual gamepad.
static void open_pad(int slot, int user)
{
    struct Pad *pad = &pads[slot];
    pad->handle = scePadOpen(user, PS5_PAD_PORT_TYPE_STANDARD, 0, NULL);
    // A pad this process already holds (after a reinitialisation) is reused.
    if (pad->handle < 0) pad->handle = scePadGetHandle(user, PS5_PAD_PORT_TYPE_STANDARD, 0);
    SDL_Log("pad: player %d user 0x%x handle 0x%x", slot + 1, (unsigned)user, (unsigned)pad->handle);
    if (pad->handle < 0) {
        pad->handle = -1;
        return;
    }
    pad->user = user;
    // Rumble needs the pad's vibration mode set (ps5-payload-dev/SDL);
    // the pad still works for input without it.
    int mode = scePadSetVibrationMode(pad->handle, 2);
    if (mode < 0) SDL_Log("pad: scePadSetVibrationMode 0x%x", (unsigned)mode);
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = 6;
    desc.nbuttons = 15;
    desc.vendor_id = 0x054c;
    desc.product_id = 0x0ce6;
    desc.name = "DualSense (native)";
    desc.button_mask = (1u << 15) - 1;
    desc.axis_mask = (1u << 6) - 1;
    desc.userdata = pad;
    desc.Update = update;
    desc.Rumble = rumble;
    pad->id = SDL_AttachVirtualJoystick(&desc);
    if (pad->id) pad->joystick = SDL_OpenJoystick(pad->id);
    if (!pad->joystick) {
        SDL_Log("pad: SDL virtual gamepad failed: %s", SDL_GetError());
        attach_unavailable = true;
        close_pad(pad);
        return;
    }
    SDL_SetJoystickPlayerIndex(pad->joystick, slot);
}

static bool user_signed_in(const int users[4], int user)
{
    for (int i = 0; i < 4; ++i) {
        if (users[i] == user) return true;
    }
    return false;
}

// The sequence the console-validated titles use (PS5_Vulkan docs/RECIPES.md,
// ProsperoEden headless/pad.cpp and radio_input.c): User Service, then
// scePadInit, then player 1 opened for the title's initial user (else the
// foreground user), who keeps it. Players 2-4 follow the signed-in users.
void StarfoxPS5_PollGamepads(void)
{
    if (!initialized) {
        const int service = sceUserServiceInitialize(NULL);
        const int result = scePadInit();
        SDL_Log("pad: sceUserServiceInitialize 0x%x scePadInit 0x%x", (unsigned)service, (unsigned)result);
        if (result < 0) return;
        for (int i = 0; i < 4; ++i) { SDL_zero(pads[i]); pads[i].handle = -1; pads[i].user = -1; }
        initialized = true;
        int user = -1;
        int found = sceUserServiceGetInitialUser(&user);
        if (found < 0 || user < 0) found = sceUserServiceGetForegroundUser(&user);
        SDL_Log("pad: player 1 user 0x%x (0x%x)", (unsigned)user, (unsigned)found);
        if (found >= 0 && user >= 0) open_pad(0, user);
        if (attach_unavailable) return;
    }
    Uint64 now = SDL_GetTicks();
    if (attach_unavailable || now < next_scan) return;
    next_scan = now + 500;
    int users[4] = {-1, -1, -1, -1};
    if (sceUserServiceGetLoginUserIdList(users) < 0) return;
    for (int slot = 1; slot < 4; ++slot) {
        if (pads[slot].handle >= 0 && !user_signed_in(users, pads[slot].user)) close_pad(&pads[slot]);
    }
    for (int i = 0; i < 4; ++i) {
        if (users[i] < 0) continue;
        bool known = false;
        for (int slot = 0; slot < 4; ++slot) known |= pads[slot].handle >= 0 && pads[slot].user == users[i];
        if (known) continue;
        // Player 1 is taken by the first signed-in user if the initial user
        // could not be opened.
        for (int slot = 0; slot < 4; ++slot) {
            if (pads[slot].handle < 0) {
                open_pad(slot, users[i]);
                break;
            }
        }
    }
}
