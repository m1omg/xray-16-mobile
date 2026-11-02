#pragma once

#include <SDL.h>

#include "xrCore/_vector2.h"
#include "xr_input.h"

extern ENGINE_API int psTouchControls;
extern ENGINE_API float psTouchLookSens;
extern ENGINE_API float psTouchOpacity;

// Touch screen controls for phones and tablets, layered on the regular input pipeline:
// - while the UI cursor is shown (menus, inventory, PDA, dialogs) a finger is the cursor and
//   the left mouse button; holding it still for a moment is a right click;
// - in game, the lower left part of the screen is a floating move stick, dragging elsewhere
//   looks around, and on-screen buttons press whatever keys their actions are bound to.
class TouchControls
{
public:
    explicit TouchControls(CInput& input);

    void Update(); // consumes this frame's finger events
    void Draw() const; // must run inside the ImGui frame
    void Reset(); // lets go of everything that is held

    // Where the UI cursor should be, if a finger is driving it (0..1 window coordinates)
    bool GetPointer(Fvector2& pos) const;

    // Keyboard, mouse or gamepad used: hide the on-screen controls until the next touch
    void OnOtherInput() { visible = false; pointerFromTouch = false; }

private:
    enum class Mode
    {
        Pointer, // a finger is the mouse: menus, dialogs, videos
        Game, // move stick, look, buttons
    };

    enum class Role
    {
        Stick,
        Look,
        Button,
    };

    struct Finger
    {
        SDL_FingerID id;
        Role role;
        int button;
        Fvector2 pos; // pixels
    };

    struct ButtonState
    {
        int key{ SDL_SCANCODE_UNKNOWN }; // what was pressed, so the release matches it
        bool down{}; // a finger is on it
        bool latched{}; // sticky button switched on
    };

    Mode CurrentMode() const;

    void OnPointerEvent(const SDL_TouchFingerEvent& event); // 0..1 coordinates
    void OnGameEvent(const SDL_TouchFingerEvent& event, Fvector2 pos); // pixels
    void UpdatePointer();
    void UpdateStick();
    void UpdateLook();

    int HitButton(Fvector2 pos) const;
    Fvector2 ButtonCenter(int idx) const;
    void PressButton(int idx);
    void ReleaseButton(int idx);
    int MoveStickKey();

    bool PressKey(int key);
    void ReleaseKey(int key);
    void MovePointer(Fvector2 pos);

    CInput& input;

    Mode mode{ Mode::Pointer };
    bool enabled{};
    bool visible{};
    Fvector2 screen{ 1.f, 1.f }; // window size in pixels

    xr_vector<Finger> fingers;
    xr_vector<ButtonState> buttons;

    // Move stick
    bool stickActive{};
    SDL_FingerID stickFinger{};
    Fvector2 stickOrigin{};
    Fvector2 stickPos{};
    ControllerAxisState stickState{};
    bool stickBindingChecked{};

    // Look
    Fvector2 lookDelta{}; // pixels this frame
    Fvector2 lookRemainder{}; // fractional mouse counts carried to the next frame

    // UI pointer
    bool pointerActive{};
    bool pointerFromTouch{};
    SDL_FingerID pointerFinger{};
    int pointerKey{}; // left mouse button, or Escape when there is no cursor
    Fvector2 pointerPos{}; // 0..1
    Fvector2 pointerDownPos{};
    u32 pointerDownTime{};
    bool pointerMoved{};
    bool longPressDone{};
};
