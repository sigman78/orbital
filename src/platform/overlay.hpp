#pragma once

// The platform side of the Dear ImGui overlay: OS input, timing and cursor
// for one window. The caller owns the ImGui context; the platform layer only
// feeds it, so no OS type reaches the application.
namespace space::platform {

class Window;

// Hooks the window's messages into the overlay and initialises its OS backend.
void attach_overlay(Window& window);
void detach_overlay(Window& window);
// Display size, delta time and mouse state for the frame about to be built.
void overlay_new_frame();

} // namespace space::platform
