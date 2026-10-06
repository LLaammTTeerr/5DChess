#include "play/BoardStyle.h"

namespace play {

namespace {

constexpr Color rgb(int r, int g, int b, int a = 255) {
  return {static_cast<unsigned char>(r), static_cast<unsigned char>(g), static_cast<unsigned char>(b), static_cast<unsigned char>(a)};
}

// A warm paper map: tinted lanes, card-style boards, terracotta present.
BoardStyle atlas() {
  BoardStyle s{};
  s.id = BoardView::Atlas;
  s.background = BoardStyle::Background::Paper;
  s.lane = BoardStyle::Lane::Band;
  s.present = BoardStyle::Present::Glow;
  s.card = BoardStyle::Card::Paper;
  s.connector = BoardStyle::Connector::Curve;
  s.bg0 = rgb(247, 240, 224);
  s.bg1 = s.bg2 = s.bg0;
  s.grid = rgb(226, 211, 180);
  s.ink = rgb(74, 61, 48);
  s.muted = rgb(120, 104, 82); // darker than the mockup's #8a7a64: 4.5:1 on the lane tints
  s.accent = rgb(207, 106, 67);
  s.accent2 = rgb(200, 133, 26);
  s.check = rgb(214, 40, 57);
  s.whiteBranch = rgb(207, 106, 67);
  s.blackBranch = rgb(58, 52, 48);
  s.thread = rgb(110, 86, 56, 97);
  const Color tint[4] = {rgb(230, 232, 208), rgb(244, 220, 203), rgb(221, 225, 230), rgb(226, 221, 230)};
  const Color pill[4] = {rgb(122, 138, 82), rgb(184, 83, 47), rgb(112, 125, 140), rgb(58, 52, 48)};
  for (int i = 0; i < 4; ++i) { s.laneTint[i] = tint[i]; s.lanePill[i] = pill[i]; }
  s.squareLight = rgb(245, 233, 204);
  s.squareDark = rgb(203, 165, 118);
  s.targetDot = rgb(58, 40, 24, 128);
  s.selectTint = rgb(244, 190, 70, 204);
  s.checkTint = rgb(214, 40, 57, 128);
  s.cardWhite = rgb(255, 250, 240);
  s.cardBlack = rgb(58, 51, 44);
  s.cardEdgeWhite = rgb(229, 214, 184);
  s.cardEdgeBlack = rgb(31, 26, 22);
  s.glowWhite = rgb(88, 60, 30, 66);
  s.glowBlack = rgb(88, 60, 30, 66);
  s.mandatory = rgb(207, 106, 67);
  s.optional = rgb(122, 138, 82);
  s.hudFill = rgb(255, 250, 240);
  s.hudBorder = rgb(229, 214, 184);
  s.hudShadow = rgb(88, 60, 30, 56);
  s.hudText = rgb(74, 61, 48);
  s.hudMuted = rgb(120, 104, 82);
  s.hudHint = rgb(184, 83, 47);
  s.skin = {rgb(255, 250, 240), rgb(244, 234, 212), rgb(229, 214, 184), rgb(207, 106, 67), rgb(74, 61, 48),
            rgb(184, 83, 47),   rgb(163, 74, 41),   rgb(138, 63, 35),   rgb(255, 255, 255),
            rgb(207, 106, 67),  rgb(255, 255, 255), rgb(240, 232, 214), rgb(168, 156, 136), 0.25f};
  return s;
}

// A night-sky multiverse: glowing boards on luminous threads, the present as an aurora.
BoardStyle deepSpace() {
  BoardStyle s{};
  s.id = BoardView::DeepSpace;
  s.background = BoardStyle::Background::Space;
  s.lane = BoardStyle::Lane::Thread;
  s.present = BoardStyle::Present::Aurora;
  s.card = BoardStyle::Card::Glow;
  s.connector = BoardStyle::Connector::Luminous;
  s.bg0 = rgb(29, 33, 102);
  s.bg1 = rgb(16, 21, 74);
  s.bg2 = rgb(6, 8, 36);
  s.grid = rgb(160, 170, 255);
  s.ink = rgb(236, 238, 255);
  s.muted = rgb(150, 158, 232); // lighter than the mockup's #7f86d6: 4.5:1 on the navy
  s.accent = rgb(95, 242, 208);
  s.accent2 = rgb(95, 242, 208);
  s.check = rgb(255, 77, 109);
  s.whiteBranch = rgb(255, 207, 107);
  s.blackBranch = rgb(165, 139, 255);
  s.thread = rgb(140, 160, 255, 150);
  const Color pill[4] = {rgb(95, 242, 208), rgb(255, 207, 107), rgb(143, 137, 255), rgb(165, 139, 255)};
  for (int i = 0; i < 4; ++i) { s.laneTint[i] = {0, 0, 0, 0}; s.lanePill[i] = pill[i]; }
  s.squareLight = rgb(236, 220, 182);
  s.squareDark = rgb(141, 106, 75);
  s.targetDot = rgb(10, 10, 40, 140);
  s.selectTint = rgb(120, 255, 225, 153);
  s.checkTint = rgb(255, 60, 100, 153);
  s.cardWhite = rgb(12, 15, 58);
  s.cardBlack = rgb(12, 15, 58);
  s.cardEdgeWhite = rgb(255, 225, 170, 204);
  s.cardEdgeBlack = rgb(160, 150, 255, 217);
  s.glowWhite = rgb(255, 213, 154);
  s.glowBlack = rgb(143, 137, 255);
  s.mandatory = rgb(95, 242, 208);
  s.optional = rgb(120, 200, 230);
  s.hudFill = rgb(24, 28, 96, 210);
  s.hudBorder = rgb(170, 180, 255, 128);
  s.hudShadow = rgb(120, 130, 255, 115);
  s.hudText = rgb(242, 243, 255);
  s.hudMuted = rgb(199, 203, 255);
  s.hudHint = rgb(95, 242, 208);
  s.skin = {rgb(24, 28, 96, 215), rgb(44, 50, 140, 230), rgb(170, 180, 255, 128), rgb(95, 242, 208), rgb(242, 243, 255),
            rgb(40, 150, 135),    rgb(52, 180, 160),     rgb(34, 124, 112),      rgb(244, 255, 252),
            rgb(60, 70, 170),     rgb(242, 243, 255),    rgb(24, 28, 80, 150),   rgb(120, 126, 190), 1.0f};
  return s;
}

// Monochrome ink on off-white; colour only where attention is needed.
BoardStyle blueprint() {
  BoardStyle s{};
  s.id = BoardView::Blueprint;
  s.background = BoardStyle::Background::Plain;
  s.lane = BoardStyle::Lane::Hairline;
  s.present = BoardStyle::Present::Rule;
  s.card = BoardStyle::Card::Ink;
  s.connector = BoardStyle::Connector::Elbow;
  s.grayPieces = true;
  s.bg0 = rgb(246, 245, 240);
  s.bg1 = s.bg2 = s.bg0;
  s.grid = rgb(231, 228, 219);
  s.ink = rgb(23, 23, 26);
  s.muted = rgb(110, 108, 100); // darker than the mockup's #85837b: 4.5:1 on off-white
  s.accent = rgb(229, 51, 27);
  s.accent2 = rgb(229, 51, 27);
  s.check = rgb(229, 51, 27);
  s.whiteBranch = rgb(23, 23, 26);
  s.blackBranch = rgb(23, 23, 26);
  s.thread = rgb(23, 23, 26);
  for (int i = 0; i < 4; ++i) { s.laneTint[i] = {0, 0, 0, 0}; s.lanePill[i] = s.ink; }
  s.squareLight = rgb(255, 255, 255);
  s.squareDark = rgb(217, 215, 207);
  s.targetDot = rgb(229, 51, 27);
  s.selectTint = rgb(229, 51, 27, 46);
  s.checkTint = rgb(229, 51, 27, 90);
  s.cardWhite = rgb(255, 255, 255);
  s.cardBlack = rgb(23, 23, 26);
  s.cardEdgeWhite = rgb(23, 23, 26);
  s.cardEdgeBlack = rgb(23, 23, 26);
  s.glowWhite = s.glowBlack = rgb(0, 0, 0, 0);
  s.mandatory = rgb(23, 23, 26);
  s.optional = rgb(133, 131, 123);
  s.hudFill = rgb(255, 255, 255);
  s.hudBorder = rgb(23, 23, 26);
  s.hudShadow = rgb(0, 0, 0, 0);
  s.hudText = rgb(23, 23, 26);
  s.hudMuted = rgb(110, 108, 100);
  s.hudHint = rgb(229, 51, 27);
  s.hudSquare = true;
  s.skin = {rgb(255, 255, 255), rgb(236, 235, 229), rgb(23, 23, 26), rgb(229, 51, 27), rgb(23, 23, 26),
            rgb(23, 23, 26),    rgb(60, 60, 66),    rgb(0, 0, 0),    rgb(255, 255, 255),
            rgb(23, 23, 26),    rgb(255, 255, 255), rgb(240, 239, 233), rgb(160, 158, 150), 0.0f};
  return s;
}

} // namespace

const BoardStyle& boardStyle(BoardView view) {
  static const BoardStyle styles[] = {deepSpace(), atlas(), blueprint()}; // BoardView order
  return styles[static_cast<int>(view)];
}

} // namespace play
