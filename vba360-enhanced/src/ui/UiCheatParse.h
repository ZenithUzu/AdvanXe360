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
  CODE_GB_GG,         // Game Genie "XXX-XXX" or "XXX-XXX-XXX"
  CODE_GBA_GSA12,     // GameShark / Action Replay v1 / v2 (chosen by the player)
  CODE_GBA_GSA3       // GameShark v3 / Action Replay v3 (chosen by the player)
};

// The code types the player can pick when adding a cheat.
enum UiCheatType
{
  CT_AUTO = 0,        // recognised from how the code looks
  CT_GBA_GSA12,       // GBA GameShark / Action Replay v1, v2
  CT_GBA_GSA3,        // GBA GameShark v3 / Action Replay v3
  CT_GBA_CBA,         // GBA CodeBreaker
  CT_GBA_RAW,         // VBA "address:value"
  CT_GB_GS,           // Game Boy / Color GameShark
  CT_GB_GG,           // Game Boy / Color Game Genie
  CT_COUNT
};
const char *uiCheatTypeKey(int type);           // "auto", "gsa", ... (files)
int uiCheatTypeFromKey(const std::string &key);  // CT_AUTO if unknown
// Example of how a code of this type is written ("XXX-XXX-XXX"...).
const char *uiCheatTypeExample(int type);

struct UiParsedCode
{
  int kind;
  std::string code;   // normalised for the core's parser
};

struct UiCheatEntry
{
  std::string desc;
  std::vector<UiParsedCode> codes;
  std::string blank;   // the code as written when it has a value to fill in (see below)
  bool enabled;
  UiCheatEntry() : enabled(false) {}
};

// Codes from cheat lists that leave a value to the player ("33002708+00??"
// score, "320091CF+00XX" item...): '?' and 'X' mark the digits to fill in.
bool uiCheatHasBlank(const std::string &code);
// Fills each run of blanks with value (hex), right-aligned and padded with
// zeros to the run's length: "33002708+00??" with "E4" -> "33002708+00E4".
std::string uiCheatFillBlank(const std::string &code, const std::string &value);

// Splits a line like "82025BC4 0F42" or "A1B2C3D4 E5F60718 11223344 55667788"
// into codes. Returns false if anything in it is not a valid code.
bool uiParseCheatCodes(const std::string &text, bool gba, std::vector<UiParsedCode> &out);
// The same, but every code must be of the given UiCheatType.
bool uiParseCheatCodesAs(const std::string &text, bool gba, int type, std::vector<UiParsedCode> &out);

// The cheats saved for one game (cheats\<game>.vch, a text file): each one
// with its type, codes and description, on or off.
struct UiStoredCheat
{
  bool enabled;
  int type;            // UiCheatType
  std::string code;    // as typed (several codes separated by spaces)
  std::string desc;
  UiStoredCheat() : enabled(true), type(CT_AUTO) {}
};
// system: 1 = Game Boy Advance, 2 = Game Boy / Color, 0 = not known.
bool uiLoadCheatStore(const std::string &path, std::vector<UiStoredCheat> &out, int &system);
bool uiSaveCheatStore(const std::string &path, const std::vector<UiStoredCheat> &list, int system);

// Reads a cheat list. Understands RetroArch .cht files (cheatN_desc /
// cheatN_code / cheatN_enable) and plain text where a description line is
// followed by its code lines. Returns false if the file can't be opened.
bool uiParseCheatFile(const std::string &path, bool gba, std::vector<UiCheatEntry> &out);

#endif // UI_CHEAT_PARSE_H
