#pragma once
#include <raylib.h>

// Central design tokens for the warm cream + terracotta UI.
namespace UI {

namespace Color {
inline constexpr ::Color bg            = {250, 249, 245, 255}; // #FAF9F5 canvas
inline constexpr ::Color surface       = {255, 255, 255, 255}; // #FFFFFF
inline constexpr ::Color surfaceAlt    = {240, 238, 230, 255}; // #F0EEE6
inline constexpr ::Color border        = {227, 218, 204, 255}; // #E3DACC
inline constexpr ::Color text          = { 31,  30,  29, 255}; // #1F1E1D
inline constexpr ::Color textMuted     = { 94,  93,  89, 255}; // #5E5D59
inline constexpr ::Color accent        = {217, 119,  87, 255}; // #D97757
inline constexpr ::Color accentHover   = {198,  97,  63, 255}; // #C6613F
inline constexpr ::Color accentPressed = {169,  78,  49, 255}; // #A94E31
// Primary (filled) button: white on #B8532F = 4.87:1, hover #A34A29 = 5.88:1, pressed #8A3F23
inline constexpr ::Color primary        = {184,  83,  47, 255};
inline constexpr ::Color primaryHover   = {163,  74,  41, 255};
inline constexpr ::Color primaryPressed = {138,  63,  35, 255};
inline constexpr ::Color onAccent      = {255, 255, 255, 255};
inline constexpr ::Color selected      = accent;
inline constexpr ::Color legalTarget   = {120, 140,  93, 255}; // #788C5D sage
inline constexpr ::Color legalFill     = {120, 140,  93,  90};
inline constexpr ::Color capture       = {191,  77,  67, 255}; // #BF4D43
inline constexpr ::Color hover         = {217, 119,  87,  64}; // accent @ 25%
inline constexpr ::Color presentLine   = {106, 155, 204, 255}; // #6A9BCC
inline constexpr ::Color timelineArrow = {138, 127, 114, 255}; // #8A7F72
inline constexpr ::Color branchArrow   = accent;
inline constexpr ::Color disabledBg    = {236, 233, 225, 255}; // #ECE9E1
inline constexpr ::Color disabledText  = {168, 163, 154, 255}; // #A8A39A
inline constexpr ::Color scrim         = { 31,  30,  29, 153}; // text @ 60%
inline constexpr ::Color shadow        = { 31,  30,  29,  60};
inline constexpr ::Color squareLight   = {240, 230, 214, 255}; // #F0E6D6
inline constexpr ::Color squareDark    = {181, 136,  99, 255}; // #B58863
inline constexpr ::Color whiteChip     = {255, 255, 255, 255};
inline constexpr ::Color blackChip     = { 31,  30,  29, 255};
}

namespace Font {
inline constexpr int title   = 40; // Montserrat Bold
inline constexpr int section = 28; // Montserrat Bold
inline constexpr int button  = 20; // Public Sans Bold
inline constexpr int body    = 18; // Public Sans
inline constexpr int mono    = 16; // Intel One Mono
inline constexpr int minimum = 14;
inline constexpr int hero     = 96; // Montserrat Bold: main-menu title
inline constexpr int subtitle = 26; // Public Sans Regular: main-menu tagline
}

namespace Space {
inline constexpr float xs = 4.0f;
inline constexpr float sm = 8.0f;
inline constexpr float md = 16.0f;
inline constexpr float lg = 24.0f;
inline constexpr float xl = 32.0f;
inline constexpr float buttonHeight = 44.0f; // minimum touch/click target
inline constexpr float buttonSpacing = 12.0f;
inline constexpr float actionButtonWidth = 140.0f; // in-game Undo / Deselect / Submit
inline constexpr float radius = 0.25f;       // DrawRectangleRounded roundness
inline constexpr float outline = 3.0f;       // selection outline
}

// Fonts are loaded lazily at their display size (Assets caches them per id and size) so text stays crisp.
namespace Fonts {
::Font title();    // Montserrat Bold, Font::title
::Font section();  // Montserrat Bold, Font::section
::Font button();   // Public Sans Bold, Font::button
::Font body();     // Public Sans Regular, Font::body
::Font mono();     // Intel One Mono, Font::mono
::Font hero();     // Montserrat Bold, Font::hero (main-menu title)
::Font subtitle(); // Public Sans Regular, Font::subtitle
}

// In-game screen-space layout shared by the HUD, the action buttons and the camera safe area. From the top: the HUD zone (back / save /
// copy, status pill, action row, Overview / Next board; no scene element enters it), a gap, the turn ruler, then the scene. Lanes and
// the present column start at the ruler or below.
namespace Layout {
inline constexpr float hudPillY       = 12.0f;
inline constexpr float actionRowY     = 68.0f;   // top of the Undo/Deselect/Submit row
inline constexpr float actionRowBottom = actionRowY + Space::buttonHeight;
inline constexpr float controlsBarH   = 32.0f;
inline constexpr float controlsBarMargin = 12.0f; // gap between the bar and the window bottom
inline constexpr float safeGap        = 12.0f;    // breathing room between UI and the boards
inline constexpr float sideInset      = 24.0f;
inline constexpr float backRight      = 25.0f + 200.0f + 12.0f; // the Back button (x 25, 200 wide) and the gap the status pill keeps from it
inline constexpr float hudBottom      = actionRowBottom + 8.0f;  // the HUD zone is [0, hudBottom)
inline constexpr float rulerY         = hudBottom + 8.0f;        // the turn ruler (T1 T2 ...) under the HUD zone
inline constexpr float rulerH         = 30.0f;
inline constexpr float laneLabelW     = 128.0f;  // left column of the lane labels (L0, L+1, ...)
inline constexpr float safeTop    = rulerY + rulerH + 10.0f;
inline constexpr float safeBottom = controlsBarH + controlsBarMargin + safeGap;
}

// Draw text horizontally centred on cx with its top at y. Returns the drawn size.
Vector2 drawTextCentered(::Font font, const char* text, float cx, float y, float size, ::Color color);
// Same title typography for every scene (centred, Montserrat Bold 40).
void drawSceneTitle(const char* text, float y = 48.0f);
// Rounded rectangle with 1-2 px border
void drawRoundedPanel(Rectangle r, ::Color fill, ::Color border, float borderThickness = 1.0f);
// Rounded card with an offset darker rect as a shadow
void drawCard(Rectangle r);
// Web only (no-op elsewhere): blending leaves the alpha channel below 1 wherever something translucent was drawn, and a
// WebGL canvas that has an alpha channel then shows the page behind it through those pixels (white blocks and halos).
// Call at the end of a frame, or of a render texture, to write alpha = 1 back over everything drawn.
void restoreOpaqueAlpha(int width, int height);
// Colour with replaced alpha
inline ::Color withAlpha(::Color c, unsigned char a) { return {c.r, c.g, c.b, a}; }

// Pointing-hand cursor over interactive items. Call Cursor::beginFrame() once per frame before
// drawing items; items call Cursor::requestHand() while drawn hovered. The result applies one
// frame later, and falls back to the default cursor when nothing requested it.
// Other cursors for the board: Grab (dragging the camera), NotAllowed (the opponent's piece, the computer's turn). The strongest
// request of a frame wins (NotAllowed > Grab > Hand).
namespace Cursor {
enum class Kind { Default, Hand, Grab, NotAllowed };
void beginFrame();
void request(Kind kind);
inline void requestHand() { request(Kind::Hand); }
}

}
