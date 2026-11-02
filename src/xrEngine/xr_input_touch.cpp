#include "stdafx.h"

#include "xr_input_touch.h"
#include "IInputReceiver.h"
#include "IGame_Level.h"
#include "XR_IOConsole.h"
#include "xr_level_controller.h"

#include <imgui.h>

#if defined(XR_PLATFORM_ANDROID) || defined(XR_PLATFORM_IOS)
ENGINE_API int psTouchControls = 1;
#else
ENGINE_API int psTouchControls = 0;
#endif
ENGINE_API float psTouchLookSens = 1.f;
ENGINE_API float psTouchOpacity = 0.5f;

namespace
{
// Layout in screen heights, so it keeps its size and shape on any aspect ratio.
// x >= 0 is measured from the left edge and x < 0 from the right one; y likewise from the top/bottom.
struct ButtonDesc
{
    EGameActions action;
    pcstr label;
    float x, y;
    float radius;
    bool sticky; // a tap toggles the key, for actions that are held for long stretches
    bool look; // dragging from the button also looks around, to aim while firing
};

// Placed around the SoC HUD: minimap top left, health and ammo bottom right.
constexpr ButtonDesc BUTTONS[] =
{
    // Right thumb
    { kWPN_FIRE,    "FIRE",   -0.30f, -0.45f, 0.110f, false, true  },
    { kWPN_ZOOM,    "AIM",    -0.12f, -0.66f, 0.075f, true,  true  },
    { kJUMP,        "JUMP",   -0.12f, -0.34f, 0.070f, false, false },
    { kCROUCH,      "DUCK",   -0.47f, -0.20f, 0.065f, true,  false },
    { kWPN_RELOAD,  "RELOAD", -0.65f, -0.22f, 0.060f, false, false },
    { kUSE,         "USE",    -0.55f, -0.48f, 0.060f, false, false },

    // Top, right of the minimap
    { kQUIT,        "MENU",    0.38f,  0.08f, 0.050f, false, false },
    { kINVENTORY,   "INV",     0.50f,  0.08f, 0.050f, false, false },
    { kACTIVE_JOBS, "PDA",     0.62f,  0.08f, 0.050f, false, false },
    { kTORCH,       "LIGHT",   0.74f,  0.08f, 0.050f, false, false },
    { kQUICK_SAVE,  "SAVE",    0.86f,  0.08f, 0.050f, false, false },

    // Top right: weapon slots, in two rows so they clear the top row on 4:3 screens too
    { kWPN_1,       "1",      -0.26f,  0.07f, 0.042f, false, false },
    { kWPN_2,       "2",      -0.17f,  0.07f, 0.042f, false, false },
    { kWPN_3,       "3",      -0.08f,  0.07f, 0.042f, false, false },
    { kWPN_4,       "4",      -0.26f,  0.16f, 0.042f, false, false },
    { kWPN_5,       "5",      -0.17f,  0.16f, 0.042f, false, false },
    { kWPN_6,       "6",      -0.08f,  0.16f, 0.042f, false, false },
};

constexpr float STICK_ZONE_RIGHT = 0.45f; // screen widths
constexpr float STICK_ZONE_TOP = 0.18f; // screen heights, keeps the top row clear
constexpr float STICK_RADIUS = 0.12f; // screen heights
constexpr float STICK_DEAD_ZONE = 0.1f;
constexpr float BUTTON_SLACK = 1.2f; // fingers are imprecise, accept touches a bit outside
constexpr float LOOK_SCALE = 900.f; // mouse counts per screen height of finger travel
constexpr float LONG_PRESS_MOVE = 0.02f; // screen fraction
constexpr u32 LONG_PRESS_TIME = 500; // ms

// Keyboard or mouse key bound to an action. Gamepad bindings are skipped: their holds come from
// ControllerUpdate(), which does nothing without a real gamepad.
int ActionKey(EGameActions action)
{
    for (int i = 0; i < 2; ++i)
    {
        const int key = GetActionDik(action, i);
        if (key > SDL_SCANCODE_UNKNOWN && key < MOUSE_MAX)
            return key;
    }
    return SDL_SCANCODE_UNKNOWN;
}

bool IsMoveAxis(int key) { return key > XR_CONTROLLER_AXIS_INVALID && key < XR_CONTROLLER_AXIS_MAX; }
} // namespace

TouchControls::TouchControls(CInput& input) : input(input), buttons(std::size(BUTTONS)) {}

TouchControls::Mode TouchControls::CurrentMode() const
{
    // Without a level (intro videos, menus) a tap is a click too, e.g. to skip a video
    if (input.uiCursorVisible || !g_pGameLevel)
        return Mode::Pointer;
    return Mode::Game;
}

void TouchControls::Update()
{
    // SDL's own touch <-> mouse emulation would double up with this, so it follows the setting
    if (enabled != !!psTouchControls)
    {
        Reset();
        enabled = !!psTouchControls;
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, enabled ? "0" : "1");
        SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
        visible = enabled && SDL_GetNumTouchDevices() > 0;
    }

    SDL_Event events[64];
    const int count = SDL_PeepEvents(events, std::size(events), SDL_GETEVENT, SDL_FINGERDOWN, SDL_FINGERMOTION);
    // SDL2 also reports multi-finger gestures that nothing reads; don't let them fill the queue
    SDL_FlushEvents(SDL_DOLLARGESTURE, SDL_MULTIGESTURE);

    if (!enabled || !input.CurrentIR())
        return;

    const Mode newMode = CurrentMode();
    if (newMode != mode)
    {
        Reset();
        mode = newMode;
    }

    int w, h;
    SDL_GetWindowSize(Device.m_sdlWnd, &w, &h);
    screen.set(float(_max(w, 1)), float(_max(h, 1)));

    for (int i = 0; i < count; ++i)
    {
        const SDL_TouchFingerEvent& event = events[i].tfinger;
        if (event.touchId == SDL_MOUSE_TOUCHID)
            continue;
        if (event.type == SDL_FINGERDOWN)
            visible = true;

        const Fvector2 pos{ event.x * screen.x, event.y * screen.y };
        switch (mode)
        {
        case Mode::Pointer: OnPointerEvent(event); break;
        case Mode::Game: OnGameEvent(event, pos); break;
        }
    }

    switch (mode)
    {
    case Mode::Pointer:
        UpdatePointer();
        break;
    case Mode::Game:
        UpdateStick();
        UpdateLook();
        break;
    }
}

void TouchControls::Reset()
{
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        auto& button = buttons[i];
        if (button.key != SDL_SCANCODE_UNKNOWN)
            ReleaseKey(button.key);
        button = {};
    }

    if (stickActive || !fis_zero(stickState.magnitude))
    {
        stickActive = false;
        UpdateStick();
    }

    if (pointerActive && (pointerKey != MOUSE_1 || !longPressDone))
        ReleaseKey(pointerKey);
    pointerActive = false;

    fingers.clear();
    lookDelta.set(0.f, 0.f);
    lookRemainder.set(0.f, 0.f);
}

bool TouchControls::GetPointer(Fvector2& pos) const
{
    if (!enabled || !pointerFromTouch || mode != Mode::Pointer)
        return false;
    pos = pointerPos;
    return true;
}

//-----------------------------------------------------------------------
// UI pointer

void TouchControls::OnPointerEvent(const SDL_TouchFingerEvent& event)
{
    const Fvector2 pos{ event.x, event.y };

    switch (event.type)
    {
    case SDL_FINGERDOWN:
        if (pointerActive)
            return; // one finger points, the others are ignored
        pointerActive = true;
        pointerFinger = event.fingerId;
        pointerDownPos = pos;
        pointerDownTime = SDL_GetTicks();
        pointerMoved = false;
        // Nothing to point at (intro videos and other sequences): a tap skips, like Escape does
        pointerKey = input.uiCursorVisible ? int(MOUSE_1) : ActionKey(kQUIT);
        longPressDone = pointerKey != MOUSE_1;
        MovePointer(pos);
        PressKey(pointerKey);
        break;

    case SDL_FINGERMOTION:
        if (!pointerActive || event.fingerId != pointerFinger)
            return;
        if (pos.distance_to(pointerDownPos) > LONG_PRESS_MOVE)
            pointerMoved = true;
        MovePointer(pos);
        break;

    case SDL_FINGERUP:
        if (!pointerActive || event.fingerId != pointerFinger)
            return;
        MovePointer(pos);
        if (pointerKey != MOUSE_1 || !longPressDone)
            ReleaseKey(pointerKey);
        pointerActive = false;
        break;
    }
}

void TouchControls::UpdatePointer()
{
    if (!pointerActive || pointerMoved || longPressDone || SDL_GetTicks() - pointerDownTime < LONG_PRESS_TIME)
        return;

    // Holding still is a right click (context menus in the inventory)
    longPressDone = true;
    ReleaseKey(MOUSE_1);
    if (PressKey(MOUSE_2))
        ReleaseKey(MOUSE_2);
}

void TouchControls::MovePointer(Fvector2 pos)
{
    const Fvector2 prev = pointerPos;
    pointerPos = pos;
    pointerFromTouch = true;

    // The UI cursor reads the absolute position from GetPointer(); the delta is for everyone else
    const int dx = iFloor((pos.x - prev.x) * screen.x + 0.5f);
    const int dy = iFloor((pos.y - prev.y) * screen.y + 0.5f);
    input.CurrentIR()->IR_OnMouseMove(dx, dy);
}

//-----------------------------------------------------------------------
// Game

void TouchControls::OnGameEvent(const SDL_TouchFingerEvent& event, Fvector2 pos)
{
    const auto finger = std::find_if(fingers.begin(), fingers.end(),
        [&](const Finger& f) { return f.id == event.fingerId; });

    switch (event.type)
    {
    case SDL_FINGERDOWN:
    {
        if (finger != fingers.end())
            return;

        Finger f{ event.fingerId, Role::Look, -1, pos };
        if (const int idx = HitButton(pos); idx >= 0)
        {
            f.role = Role::Button;
            f.button = idx;
            buttons[idx].down = true;
            PressButton(idx);
        }
        else if (!stickActive && pos.x < STICK_ZONE_RIGHT * screen.x && pos.y > STICK_ZONE_TOP * screen.y)
        {
            f.role = Role::Stick;
            stickActive = true;
            stickFinger = event.fingerId;
            stickOrigin = pos;
            stickPos = pos;
        }
        fingers.emplace_back(f);
        break;
    }

    case SDL_FINGERMOTION:
    {
        if (finger == fingers.end())
            return;

        const Fvector2 delta = Fvector2(pos).sub(finger->pos);
        finger->pos = pos;
        switch (finger->role)
        {
        case Role::Stick:
        {
            stickPos = pos;
            // Drag the stick base along when the finger goes past the rim
            Fvector2 offset = Fvector2(pos).sub(stickOrigin);
            const float radius = STICK_RADIUS * screen.y;
            const float distance = offset.magnitude();
            if (distance > radius)
                stickOrigin.add(offset.mul((distance - radius) / distance));
            break;
        }
        case Role::Button:
            if (BUTTONS[finger->button].look)
                lookDelta.add(delta);
            break;
        case Role::Look:
            lookDelta.add(delta);
            break;
        }
        break;
    }

    case SDL_FINGERUP:
    {
        if (finger == fingers.end())
            return;

        switch (finger->role)
        {
        case Role::Stick:
            stickActive = false;
            break;
        case Role::Button:
            buttons[finger->button].down = false;
            if (!BUTTONS[finger->button].sticky)
                ReleaseButton(finger->button);
            break;
        case Role::Look:
            break;
        }
        fingers.erase(finger);
        break;
    }
    }
}

int TouchControls::HitButton(Fvector2 pos) const
{
    int best = -1;
    float bestDistance = flt_max;
    for (int i = 0; i < int(std::size(BUTTONS)); ++i)
    {
        if (ActionKey(BUTTONS[i].action) == SDL_SCANCODE_UNKNOWN)
            continue; // unbound, not shown
        const float distance = pos.distance_to(ButtonCenter(i));
        if (distance < BUTTONS[i].radius * screen.y * BUTTON_SLACK && distance < bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

Fvector2 TouchControls::ButtonCenter(int idx) const
{
    const auto& desc = BUTTONS[idx];
    const float x = desc.x * screen.y;
    const float y = desc.y * screen.y;
    return { x >= 0.f ? x : screen.x + x, y >= 0.f ? y : screen.y + y };
}

void TouchControls::PressButton(int idx)
{
    auto& button = buttons[idx];
    if (BUTTONS[idx].sticky && button.latched)
    {
        ReleaseButton(idx);
        return;
    }

    const int key = ActionKey(BUTTONS[idx].action);
    if (key != SDL_SCANCODE_UNKNOWN && PressKey(key))
    {
        button.key = key;
        button.latched = BUTTONS[idx].sticky;
    }
}

void TouchControls::ReleaseButton(int idx)
{
    auto& button = buttons[idx];
    if (button.key != SDL_SCANCODE_UNKNOWN)
        ReleaseKey(button.key);
    button.key = SDL_SCANCODE_UNKNOWN;
    button.latched = false;
}

int TouchControls::MoveStickKey()
{
    int key = SDL_SCANCODE_UNKNOWN;
    ForAllActionKeys(kMOVE_AROUND, [&](size_t, int k)
    {
        if (IsMoveAxis(k))
            key = k;
        return IsMoveAxis(k);
    });

    // The stick drives the gamepad movement path; restore its default binding if a config dropped it
    if (key == SDL_SCANCODE_UNKNOWN && !stickBindingChecked && Console)
    {
        stickBindingChecked = true;
        Console->Execute("bind_gpad move_around gpAXIS_LEFT");
        return MoveStickKey();
    }
    return key;
}

void TouchControls::UpdateStick()
{
    ControllerAxisState state{};
    if (stickActive)
    {
        Fvector2 offset = Fvector2(stickPos).sub(stickOrigin);
        offset.div(STICK_RADIUS * screen.y);
        const float magnitude = _min(offset.magnitude(), 1.f);
        if (magnitude > STICK_DEAD_ZONE)
        {
            // Same response as a gamepad stick past its dead zone: 0..1 from the rim of the dead zone
            const float scaled = (magnitude - STICK_DEAD_ZONE) / (1.f - STICK_DEAD_ZONE);
            offset.normalize_safe().mul(scaled);
            state = { offset, scaled };
        }
    }

    const bool active = !fis_zero(state.magnitude);
    const bool wasActive = !fis_zero(stickState.magnitude);
    stickState = state;
    if (!active && !wasActive)
        return;

    const int key = MoveStickKey();
    if (key == SDL_SCANCODE_UNKNOWN)
        return;

    IInputReceiver* receiver = input.CurrentIR();
    if (active && wasActive)
        receiver->IR_OnControllerHold(key, state);
    else if (active)
        receiver->IR_OnControllerPress(key, state);
    else
        receiver->IR_OnControllerRelease(key, state);
}

void TouchControls::UpdateLook()
{
    if (fis_zero(lookDelta.x) && fis_zero(lookDelta.y))
        return;

    // Screen-relative, so the same swipe turns as far on a phone as on a tablet
    const float scale = LOOK_SCALE * psTouchLookSens / screen.y;
    Fvector2 counts = Fvector2(lookDelta).mul(scale).add(lookRemainder);
    lookDelta.set(0.f, 0.f);

    const int dx = int(counts.x);
    const int dy = int(counts.y);
    lookRemainder.set(counts.x - float(dx), counts.y - float(dy));
    if (dx || dy)
        input.CurrentIR()->IR_OnMouseMove(dx, dy);
}

//-----------------------------------------------------------------------
// Keys go through the same state CInput keeps for the real keyboard and mouse,
// so holds and iGetAsyncKeyState() work as usual.

bool TouchControls::PressKey(int key)
{
    IInputReceiver* receiver = input.CurrentIR();
    if (key > SDL_SCANCODE_UNKNOWN && key < CInput::COUNT_KB_BUTTONS)
    {
        input.keyboardState[key] = true;
        receiver->IR_OnKeyboardPress(key);
        return true;
    }
    if (key > MOUSE_INVALID && key < MOUSE_MAX)
    {
        input.mouseState[key - (MOUSE_INVALID + 1)] = true;
        receiver->IR_OnMousePress(key);
        return true;
    }
    return false;
}

void TouchControls::ReleaseKey(int key)
{
    IInputReceiver* receiver = input.CurrentIR();
    if (!receiver)
        return;
    if (key > SDL_SCANCODE_UNKNOWN && key < CInput::COUNT_KB_BUTTONS)
    {
        input.keyboardState[key] = false;
        receiver->IR_OnKeyboardRelease(key);
    }
    else if (key > MOUSE_INVALID && key < MOUSE_MAX)
    {
        input.mouseState[key - (MOUSE_INVALID + 1)] = false;
        receiver->IR_OnMouseRelease(key);
    }
}

//-----------------------------------------------------------------------

void TouchControls::Draw() const
{
    if (!enabled || !visible || mode != Mode::Game)
        return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    const float opacity = clampr(psTouchOpacity, 0.f, 1.f);
    const float lineWidth = _max(2.f, screen.y * 0.004f);

    const auto color = [opacity](u8 r, u8 g, u8 b, float alpha)
    {
        return IM_COL32(r, g, b, u8(clampr(alpha * opacity, 0.f, 1.f) * 255.f));
    };

    for (int i = 0; i < int(std::size(BUTTONS)); ++i)
    {
        if (ActionKey(BUTTONS[i].action) == SDL_SCANCODE_UNKNOWN)
            continue;

        const auto& button = buttons[i];
        const bool lit = button.down || button.latched;
        const Fvector2 center = ButtonCenter(i);
        const ImVec2 c{ center.x, center.y };
        const float radius = BUTTONS[i].radius * screen.y;

        draw->AddCircleFilled(c, radius, lit ? color(255, 255, 255, 0.45f) : color(0, 0, 0, 0.5f), 40);
        draw->AddCircle(c, radius, color(255, 255, 255, lit ? 1.f : 0.7f), 40, lineWidth);

        const float textSize = _min(radius * 0.55f, screen.y * 0.035f);
        const ImVec2 extent = font->CalcTextSizeA(textSize, FLT_MAX, 0.f, BUTTONS[i].label);
        draw->AddText(font, textSize, ImVec2(c.x - extent.x * 0.5f, c.y - extent.y * 0.5f),
            color(255, 255, 255, 1.f), BUTTONS[i].label);
    }

    // Move stick: where it is while held, a hint where to put the thumb otherwise
    const float radius = STICK_RADIUS * screen.y;
    if (stickActive)
    {
        Fvector2 knob = Fvector2(stickPos).sub(stickOrigin);
        if (knob.magnitude() > radius)
            knob.normalize_safe().mul(radius);
        knob.add(stickOrigin);

        draw->AddCircleFilled(ImVec2(stickOrigin.x, stickOrigin.y), radius, color(0, 0, 0, 0.35f), 48);
        draw->AddCircle(ImVec2(stickOrigin.x, stickOrigin.y), radius, color(255, 255, 255, 0.7f), 48, lineWidth);
        draw->AddCircleFilled(ImVec2(knob.x, knob.y), radius * 0.45f, color(255, 255, 255, 0.6f), 32);
    }
    else
    {
        const ImVec2 hint{ screen.y * 0.25f, screen.y * 0.70f };
        draw->AddCircle(hint, radius, color(255, 255, 255, 0.3f), 48, lineWidth);
        draw->AddCircleFilled(hint, radius * 0.45f, color(255, 255, 255, 0.2f), 32);
    }
}
