// Recognises cheat codes typed by the player or found in text files.
#ifndef UI_CHEAT_PARSE_H
#define UI_CHEAT_PARSE_H

#include <string>
#include <vector>

enum UiCodeKind
{
  CODE_GBA_RAW = 0,   // VBA "AAAAAAAA:VV", ":VVVV" or ":VVVVVVVV"
  CODE_GBA_GSA,       // GameShark / Action Replay, 16 hex digits (no space)
  CODE_GBA_CBA,       // CodeBreaker "AAAAAAAA VVVV"
  CODE_GB_GS,         // Game Boy GameShark, 8 hex digits
  CODE_GB_GG          // Game Genie "XXX-XXX" or "XXX-XXX-XXX"
};

struct UiParsedCode
{
  int kind;
  std::string code;   // normalised for the core's parser
};

struct UiCheatEntry
{
  std::string desc;
  std::vector<UiParsedCode> codes;
  bool enabled;
  UiCheatEntry() : enabled(false) {}
};

// Splits a line like "82025BC4 0F42" or "A1B2C3D4 E5F60718 11223344 55667788"
// into codes. Returns false if anything in it is not a valid code.
bool uiParseCheatCodes(const std::string &text, bool gba, std::vector<UiParsedCode> &out);

// Reads a cheat list. Understands RetroArch .cht files (cheatN_desc /
// cheatN_code / cheatN_enable) and plain text where a description line is
// followed by its code lines. Returns false if the file can't be opened.
bool uiParseCheatFile(const std::string &path, bool gba, std::vector<UiCheatEntry> &out);

#endif // UI_CHEAT_PARSE_H
