// Achievement pop-up in the Xbox One style, after Andrew Haine's CodePen
// "Xbox One Achievement Popup Animation" (MIT; shared as a gist by bigk98):
// a green circle grows, slides left and a green pill unfolds to its right with
// the text; at the end it all folds back. Drawn with the menu's own canvas,
// a picture per frame. No Xbox logo or gamerscore "G" (they are Microsoft's):
// a trophy and "pts" instead.
#ifndef UI_POPUP_XBOX_H
#define UI_POPUP_XBOX_H

#include "UiCanvas.h"

// How many frames it lasts (60 a second).
int uiXboxPopupFrames();
// The canvas size the pop-up needs for these lines (the pill fits the text).
void uiXboxPopupSize(const char *line1, const char *line2, int &w, int &h);
// Draws frame `frame` into c (made w x h by uiXboxPopupSize), with real
// transparency around and through it (alpha in each pixel).
void uiDrawXboxPopup(UiCanvas &c, int frame, const char *line1, const char *line2);

#endif
