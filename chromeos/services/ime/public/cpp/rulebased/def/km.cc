// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/rulebased/def/km.h"

namespace km {

const char* kId = "km";
bool kIs102 = false;
const char* kNormal[] = {
    u8"\u00ab",        // BackQuote
    u8"\u17e1",        // Digit1
    u8"\u17e2",        // Digit2
    u8"\u17e3",        // Digit3
    u8"\u17e4",        // Digit4
    u8"\u17e5",        // Digit5
    u8"\u17e6",        // Digit6
    u8"\u17e7",        // Digit7
    u8"\u17e8",        // Digit8
    u8"\u17e9",        // Digit9
    u8"\u17e0",        // Digit0
    u8"\u17a5",        // Minus
    u8"\u17b2",        // Equal
    u8"\u1786",        // KeyQ
    u8"\u17b9",        // KeyW
    u8"\u17c1",        // KeyE
    u8"\u179a",        // KeyR
    u8"\u178f",        // KeyT
    u8"\u1799",        // KeyY
    u8"\u17bb",        // KeyU
    u8"\u17b7",        // KeyI
    u8"\u17c4",        // KeyO
    u8"\u1795",        // KeyP
    u8"\u17c0",        // BracketLeft
    u8"\u17aa",        // BracketRight
    u8"\u17ae",        // Backslash
    u8"\u17b6",        // KeyA
    u8"\u179f",        // KeyS
    u8"\u178a",        // KeyD
    u8"\u1790",        // KeyF
    u8"\u1784",        // KeyG
    u8"\u17a0",        // KeyH
    u8"\u17d2",        // KeyJ
    u8"\u1780",        // KeyK
    u8"\u179b",        // KeyL
    u8"\u17be",        // Semicolon
    u8"\u17cb",        // Quote
    u8"\u178b",        // KeyZ
    u8"\u1781",        // KeyX
    u8"\u1785",        // KeyC
    u8"\u179c",        // KeyV
    u8"\u1794",        // KeyB
    u8"\u1793",        // KeyN
    u8"\u1798",        // KeyM
    u8"\u17bb\u17c6",  // Comma
    u8"\u17d4",        // Period
    u8"\u17ca",        // Slash
    u8"\u200b",        // Space
};
const char* kShift[] = {
    u8"\u00bb",        // BackQuote
    u8"!",             // Digit1
    u8"\u17d7",        // Digit2
    u8"\"",            // Digit3
    u8"\u17db",        // Digit4
    u8"%",             // Digit5
    u8"\u17cd",        // Digit6
    u8"\u17d0",        // Digit7
    u8"\u17cf",        // Digit8
    u8"(",             // Digit9
    u8")",             // Digit0
    u8"\u17cc",        // Minus
    u8"=",             // Equal
    u8"\u1788",        // KeyQ
    u8"\u17ba",        // KeyW
    u8"\u17c2",        // KeyE
    u8"\u17ac",        // KeyR
    u8"\u1791",        // KeyT
    u8"\u17bd",        // KeyY
    u8"\u17bc",        // KeyU
    u8"\u17b8",        // KeyI
    u8"\u17c5",        // KeyO
    u8"\u1797",        // KeyP
    u8"\u17bf",        // BracketLeft
    u8"\u17a7",        // BracketRight
    u8"\u17ad",        // Backslash
    u8"\u17b6\u17c6",  // KeyA
    u8"\u17c3",        // KeyS
    u8"\u178c",        // KeyD
    u8"\u1792",        // KeyF
    u8"\u17a2",        // KeyG
    u8"\u17c7",        // KeyH
    u8"\u1789",        // KeyJ
    u8"\u1782",        // KeyK
    u8"\u17a1",        // KeyL
    u8"\u17c4\u17c7",  // Semicolon
    u8"\u17c9",        // Quote
    u8"\u178d",        // KeyZ
    u8"\u1783",        // KeyX
    u8"\u1787",        // KeyC
    u8"\u17c1\u17c7",  // KeyV
    u8"\u1796",        // KeyB
    u8"\u178e",        // KeyN
    u8"\u17c6",        // KeyM
    u8"\u17bb\u17c7",  // Comma
    u8"\u17d5",        // Period
    u8"?",             // Slash
    u8"\u0020",        // Space
};
const char* kAltGr[] = {
    u8"\u200d",  // BackQuote
    u8"\u200c",  // Digit1
    u8"@",       // Digit2
    u8"\u17d1",  // Digit3
    u8"$",       // Digit4
    u8"\u20ac",  // Digit5
    u8"\u17d9",  // Digit6
    u8"\u17da",  // Digit7
    u8"*",       // Digit8
    u8"{",       // Digit9
    u8"}",       // Digit0
    u8"\u00d7",  // Minus
    u8"\u17ce",  // Equal
    u8"\u17dc",  // KeyQ
    u8"\u17dd",  // KeyW
    u8"\u17af",  // KeyE
    u8"\u17ab",  // KeyR
    u8"\u17a8",  // KeyT
    u8"",        // KeyY
    u8"",        // KeyU
    u8"\u17a6",  // KeyI
    u8"\u17b1",  // KeyO
    u8"\u17b0",  // KeyP
    u8"\u17a9",  // BracketLeft
    u8"\u17b3",  // BracketRight
    u8"\\",      // Backslash
    u8"+",       // KeyA
    u8"-",       // KeyS
    u8"",        // KeyD
    u8"",        // KeyF
    u8"",        // KeyG
    u8"",        // KeyH
    u8"",        // KeyJ
    u8"\u179d",  // KeyK
    u8"",        // KeyL
    u8"\u17d6",  // Semicolon
    u8"\u17c8",  // Quote
    u8"",        // KeyZ
    u8"",        // KeyX
    u8"",        // KeyC
    u8"",        // KeyV
    u8"\u179e",  // KeyB
    u8"",        // KeyN
    u8"",        // KeyM
    u8",",       // Comma
    u8".",       // Period
    u8"/",       // Slash
    u8"\u0020",  // Space
};
const char* kCapslock[] = {
    u8"\u00bb",        // BackQuote
    u8"!",             // Digit1
    u8"\u17d7",        // Digit2
    u8"\"",            // Digit3
    u8"\u17db",        // Digit4
    u8"%",             // Digit5
    u8"\u17cd",        // Digit6
    u8"\u17d0",        // Digit7
    u8"\u17cf",        // Digit8
    u8"(",             // Digit9
    u8")",             // Digit0
    u8"\u17cc",        // Minus
    u8"=",             // Equal
    u8"\u1788",        // KeyQ
    u8"\u17ba",        // KeyW
    u8"\u17c2",        // KeyE
    u8"\u17ac",        // KeyR
    u8"\u1791",        // KeyT
    u8"\u17bd",        // KeyY
    u8"\u17bc",        // KeyU
    u8"\u17b8",        // KeyI
    u8"\u17c5",        // KeyO
    u8"\u1797",        // KeyP
    u8"\u17bf",        // BracketLeft
    u8"\u17a7",        // BracketRight
    u8"\u17ad",        // Backslash
    u8"\u17b6\u17c6",  // KeyA
    u8"\u17c3",        // KeyS
    u8"\u178c",        // KeyD
    u8"\u1792",        // KeyF
    u8"\u17a2",        // KeyG
    u8"\u17c7",        // KeyH
    u8"\u1789",        // KeyJ
    u8"\u1782",        // KeyK
    u8"\u17a1",        // KeyL
    u8"\u17c4\u17c7",  // Semicolon
    u8"\u17c9",        // Quote
    u8"\u178d",        // KeyZ
    u8"\u1783",        // KeyX
    u8"\u1787",        // KeyC
    u8"\u17c1\u17c7",  // KeyV
    u8"\u1796",        // KeyB
    u8"\u178e",        // KeyN
    u8"\u17c6",        // KeyM
    u8"\u17bb\u17c7",  // Comma
    u8"\u17d5",        // Period
    u8"?",             // Slash
    u8"\u0020",        // Space
};
const char* kShiftAltGr[] = {
    u8"",        // BackQuote
    u8"\u17f1",  // Digit1
    u8"\u17f2",  // Digit2
    u8"\u17f3",  // Digit3
    u8"\u17f4",  // Digit4
    u8"\u17f5",  // Digit5
    u8"\u17f6",  // Digit6
    u8"\u17f7",  // Digit7
    u8"\u17f8",  // Digit8
    u8"\u17f9",  // Digit9
    u8"\u17f0",  // Digit0
    u8"",        // Minus
    u8"",        // Equal
    u8"\u19e0",  // KeyQ
    u8"\u19e1",  // KeyW
    u8"\u19e2",  // KeyE
    u8"\u19e3",  // KeyR
    u8"\u19e4",  // KeyT
    u8"\u19e5",  // KeyY
    u8"\u19e6",  // KeyU
    u8"\u19e7",  // KeyI
    u8"\u19e8",  // KeyO
    u8"\u19e9",  // KeyP
    u8"\u19ea",  // BracketLeft
    u8"\u19eb",  // BracketRight
    u8"",        // Backslash
    u8"\u19ec",  // KeyA
    u8"\u19ed",  // KeyS
    u8"\u19ee",  // KeyD
    u8"\u19ef",  // KeyF
    u8"\u19f0",  // KeyG
    u8"\u19f1",  // KeyH
    u8"\u19f2",  // KeyJ
    u8"\u19f3",  // KeyK
    u8"\u19f4",  // KeyL
    u8"\u19f5",  // Semicolon
    u8"\u19f6",  // Quote
    u8"\u19f7",  // KeyZ
    u8"\u19f8",  // KeyX
    u8"\u19f9",  // KeyC
    u8"\u19fa",  // KeyV
    u8"\u19fb",  // KeyB
    u8"\u19fc",  // KeyN
    u8"\u19fd",  // KeyM
    u8"\u19fe",  // Comma
    u8"\u19ff",  // Period
    u8"",        // Slash
    u8"\u0020",  // Space
};
const char* kAltgrCapslock[] = {
    u8"",        // BackQuote
    u8"\u17f1",  // Digit1
    u8"\u17f2",  // Digit2
    u8"\u17f3",  // Digit3
    u8"\u17f4",  // Digit4
    u8"\u17f5",  // Digit5
    u8"\u17f6",  // Digit6
    u8"\u17f7",  // Digit7
    u8"\u17f8",  // Digit8
    u8"\u17f9",  // Digit9
    u8"\u17f0",  // Digit0
    u8"",        // Minus
    u8"",        // Equal
    u8"\u19e0",  // KeyQ
    u8"\u19e1",  // KeyW
    u8"\u19e2",  // KeyE
    u8"\u19e3",  // KeyR
    u8"\u19e4",  // KeyT
    u8"\u19e5",  // KeyY
    u8"\u19e6",  // KeyU
    u8"\u19e7",  // KeyI
    u8"\u19e8",  // KeyO
    u8"\u19e9",  // KeyP
    u8"\u19ea",  // BracketLeft
    u8"\u19eb",  // BracketRight
    u8"",        // Backslash
    u8"\u19ec",  // KeyA
    u8"\u19ed",  // KeyS
    u8"\u19ee",  // KeyD
    u8"\u19ef",  // KeyF
    u8"\u19f0",  // KeyG
    u8"\u19f1",  // KeyH
    u8"\u19f2",  // KeyJ
    u8"\u19f3",  // KeyK
    u8"\u19f4",  // KeyL
    u8"\u19f5",  // Semicolon
    u8"\u19f6",  // Quote
    u8"\u19f7",  // KeyZ
    u8"\u19f8",  // KeyX
    u8"\u19f9",  // KeyC
    u8"\u19fa",  // KeyV
    u8"\u19fb",  // KeyB
    u8"\u19fc",  // KeyN
    u8"\u19fd",  // KeyM
    u8"\u19fe",  // Comma
    u8"\u19ff",  // Period
    u8"",        // Slash
    u8"\u0020",  // Space
};
const char* kShiftCapslock[] = {
    u8"\u00ab",        // BackQuote
    u8"\u17e1",        // Digit1
    u8"\u17e2",        // Digit2
    u8"\u17e3",        // Digit3
    u8"\u17e4",        // Digit4
    u8"\u17e5",        // Digit5
    u8"\u17e6",        // Digit6
    u8"\u17e7",        // Digit7
    u8"\u17e8",        // Digit8
    u8"\u17e9",        // Digit9
    u8"\u17e0",        // Digit0
    u8"\u17a5",        // Minus
    u8"\u17b2",        // Equal
    u8"\u1786",        // KeyQ
    u8"\u17b9",        // KeyW
    u8"\u17c1",        // KeyE
    u8"\u179a",        // KeyR
    u8"\u178f",        // KeyT
    u8"\u1799",        // KeyY
    u8"\u17bb",        // KeyU
    u8"\u17b7",        // KeyI
    u8"\u17c4",        // KeyO
    u8"\u1795",        // KeyP
    u8"\u17c0",        // BracketLeft
    u8"\u17aa",        // BracketRight
    u8"\u17ae",        // Backslash
    u8"\u17b6",        // KeyA
    u8"\u179f",        // KeyS
    u8"\u178a",        // KeyD
    u8"\u1790",        // KeyF
    u8"\u1784",        // KeyG
    u8"\u17a0",        // KeyH
    u8"\u17d2",        // KeyJ
    u8"\u1780",        // KeyK
    u8"\u179b",        // KeyL
    u8"\u17be",        // Semicolon
    u8"\u17cb",        // Quote
    u8"\u178b",        // KeyZ
    u8"\u1781",        // KeyX
    u8"\u1785",        // KeyC
    u8"\u179c",        // KeyV
    u8"\u1794",        // KeyB
    u8"\u1793",        // KeyN
    u8"\u1798",        // KeyM
    u8"\u17bb\u17c6",  // Comma
    u8"\u17d4",        // Period
    u8"\u17ca",        // Slash
    u8"\u200b",        // Space
};
const char* kShiftAltGrCapslock[] = {
    u8"\u200d",  // BackQuote
    u8"\u200c",  // Digit1
    u8"@",       // Digit2
    u8"\u17d1",  // Digit3
    u8"$",       // Digit4
    u8"\u20ac",  // Digit5
    u8"\u17d9",  // Digit6
    u8"\u17da",  // Digit7
    u8"*",       // Digit8
    u8"{",       // Digit9
    u8"}",       // Digit0
    u8"\u00d7",  // Minus
    u8"\u17ce",  // Equal
    u8"\u17dc",  // KeyQ
    u8"\u17dd",  // KeyW
    u8"\u17af",  // KeyE
    u8"\u17ab",  // KeyR
    u8"\u17a8",  // KeyT
    u8"",        // KeyY
    u8"",        // KeyU
    u8"\u17a6",  // KeyI
    u8"\u17b1",  // KeyO
    u8"\u17b0",  // KeyP
    u8"\u17a9",  // BracketLeft
    u8"\u17b3",  // BracketRight
    u8"\\",      // Backslash
    u8"+",       // KeyA
    u8"-",       // KeyS
    u8"",        // KeyD
    u8"",        // KeyF
    u8"",        // KeyG
    u8"",        // KeyH
    u8"",        // KeyJ
    u8"\u179d",  // KeyK
    u8"",        // KeyL
    u8"\u17d6",  // Semicolon
    u8"\u17c8",  // Quote
    u8"",        // KeyZ
    u8"",        // KeyX
    u8"",        // KeyC
    u8"",        // KeyV
    u8"\u179e",  // KeyB
    u8"",        // KeyN
    u8"",        // KeyM
    u8",",       // Comma
    u8".",       // Period
    u8"/",       // Slash
    u8"\u0020",  // Space
};
const char** kKeyMap[8] = {
    kNormal,   kShift,         kAltGr,         kShiftAltGr,
    kCapslock, kShiftCapslock, kAltgrCapslock, kShiftAltGrCapslock};

}  // namespace km
