// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/rulebased/def/th_tis.h"

namespace th_tis {

const char* kId = "th_tis";
bool kIs102 = false;
const char* kNormal[] = {
    u8"_",       // BackQuote
    u8"\u0e45",  // Digit1
    u8"/",       // Digit2
    u8"-",       // Digit3
    u8"\u0e20",  // Digit4
    u8"\u0e16",  // Digit5
    u8"\u0e38",  // Digit6
    u8"\u0e36",  // Digit7
    u8"\u0e04",  // Digit8
    u8"\u0e15",  // Digit9
    u8"\u0e08",  // Digit0
    u8"\u0e02",  // Minus
    u8"\u0e0a",  // Equal
    u8"\u0e46",  // KeyQ
    u8"\u0e44",  // KeyW
    u8"\u0e33",  // KeyE
    u8"\u0e1e",  // KeyR
    u8"\u0e30",  // KeyT
    u8"\u0e31",  // KeyY
    u8"\u0e35",  // KeyU
    u8"\u0e23",  // KeyI
    u8"\u0e19",  // KeyO
    u8"\u0e22",  // KeyP
    u8"\u0e1a",  // BracketLeft
    u8"\u0e25",  // BracketRight
    u8"\u0e03",  // Backslash
    u8"\u0e1f",  // KeyA
    u8"\u0e2b",  // KeyS
    u8"\u0e01",  // KeyD
    u8"\u0e14",  // KeyF
    u8"\u0e40",  // KeyG
    u8"\u0e49",  // KeyH
    u8"\u0e48",  // KeyJ
    u8"\u0e32",  // KeyK
    u8"\u0e2a",  // KeyL
    u8"\u0e27",  // Semicolon
    u8"\u0e07",  // Quote
    u8"\u0e1c",  // KeyZ
    u8"\u0e1b",  // KeyX
    u8"\u0e41",  // KeyC
    u8"\u0e2d",  // KeyV
    u8"\u0e34",  // KeyB
    u8"\u0e37",  // KeyN
    u8"\u0e17",  // KeyM
    u8"\u0e21",  // Comma
    u8"\u0e43",  // Period
    u8"\u0e1d",  // Slash
    u8"\u0020",  // Space
};
const char* kShift[] = {
    u8"%",       // BackQuote
    u8"+",       // Digit1
    u8"\u0e51",  // Digit2
    u8"\u0e52",  // Digit3
    u8"\u0e53",  // Digit4
    u8"\u0e54",  // Digit5
    u8"\u0e39",  // Digit6
    u8"\u0e3f",  // Digit7
    u8"\u0e55",  // Digit8
    u8"\u0e56",  // Digit9
    u8"\u0e57",  // Digit0
    u8"\u0e58",  // Minus
    u8"\u0e59",  // Equal
    u8"\u0e50",  // KeyQ
    u8"\"",      // KeyW
    u8"\u0e0e",  // KeyE
    u8"\u0e11",  // KeyR
    u8"\u0e18",  // KeyT
    u8"\u0e4d",  // KeyY
    u8"\u0e4a",  // KeyU
    u8"\u0e13",  // KeyI
    u8"\u0e2f",  // KeyO
    u8"\u0e0d",  // KeyP
    u8"\u0e10",  // BracketLeft
    u8",",       // BracketRight
    u8"\u0e05",  // Backslash
    u8"\u0e24",  // KeyA
    u8"\u0e06",  // KeyS
    u8"\u0e0f",  // KeyD
    u8"\u0e42",  // KeyF
    u8"\u0e0c",  // KeyG
    u8"\u0e47",  // KeyH
    u8"\u0e4b",  // KeyJ
    u8"\u0e29",  // KeyK
    u8"\u0e28",  // KeyL
    u8"\u0e0b",  // Semicolon
    u8".",       // Quote
    u8"(",       // KeyZ
    u8")",       // KeyX
    u8"\u0e09",  // KeyC
    u8"\u0e2e",  // KeyV
    u8"\u0e3a",  // KeyB
    u8"\u0e4c",  // KeyN
    u8"?",       // KeyM
    u8"\u0e12",  // Comma
    u8"\u0e2c",  // Period
    u8"\u0e26",  // Slash
    u8"\u0020",  // Space
};
const char* kAltGr[] = {
    u8"_",       // BackQuote
    u8"\u0e45",  // Digit1
    u8"/",       // Digit2
    u8"-",       // Digit3
    u8"\u0e20",  // Digit4
    u8"\u0e16",  // Digit5
    u8"\u0e38",  // Digit6
    u8"\u0e36",  // Digit7
    u8"\u0e04",  // Digit8
    u8"\u0e15",  // Digit9
    u8"\u0e08",  // Digit0
    u8"\u0e02",  // Minus
    u8"\u0e0a",  // Equal
    u8"\u0e46",  // KeyQ
    u8"\u0e44",  // KeyW
    u8"\u0e33",  // KeyE
    u8"\u0e1e",  // KeyR
    u8"\u0e30",  // KeyT
    u8"\u0e31",  // KeyY
    u8"\u0e35",  // KeyU
    u8"\u0e23",  // KeyI
    u8"\u0e19",  // KeyO
    u8"\u0e22",  // KeyP
    u8"\u0e1a",  // BracketLeft
    u8"\u0e25",  // BracketRight
    u8"\u0e03",  // Backslash
    u8"\u0e1f",  // KeyA
    u8"\u0e2b",  // KeyS
    u8"\u0e01",  // KeyD
    u8"\u0e14",  // KeyF
    u8"\u0e40",  // KeyG
    u8"\u0e49",  // KeyH
    u8"\u0e48",  // KeyJ
    u8"\u0e32",  // KeyK
    u8"\u0e2a",  // KeyL
    u8"\u0e27",  // Semicolon
    u8"\u0e07",  // Quote
    u8"\u0e1c",  // KeyZ
    u8"\u0e1b",  // KeyX
    u8"\u0e41",  // KeyC
    u8"\u0e2d",  // KeyV
    u8"\u0e34",  // KeyB
    u8"\u0e37",  // KeyN
    u8"\u0e17",  // KeyM
    u8"\u0e21",  // Comma
    u8"\u0e43",  // Period
    u8"\u0e1d",  // Slash
    u8"\u0020",  // Space
};
const char* kCapslock[] = {
    u8"%",       // BackQuote
    u8"+",       // Digit1
    u8"\u0e51",  // Digit2
    u8"\u0e52",  // Digit3
    u8"\u0e53",  // Digit4
    u8"\u0e54",  // Digit5
    u8"\u0e39",  // Digit6
    u8"\u0e3f",  // Digit7
    u8"\u0e55",  // Digit8
    u8"\u0e56",  // Digit9
    u8"\u0e57",  // Digit0
    u8"\u0e58",  // Minus
    u8"\u0e59",  // Equal
    u8"\u0e50",  // KeyQ
    u8"\"",      // KeyW
    u8"\u0e0e",  // KeyE
    u8"\u0e11",  // KeyR
    u8"\u0e18",  // KeyT
    u8"\u0e4d",  // KeyY
    u8"\u0e4a",  // KeyU
    u8"\u0e13",  // KeyI
    u8"\u0e2f",  // KeyO
    u8"\u0e0d",  // KeyP
    u8"\u0e10",  // BracketLeft
    u8",",       // BracketRight
    u8"\u0e05",  // Backslash
    u8"\u0e24",  // KeyA
    u8"\u0e06",  // KeyS
    u8"\u0e0f",  // KeyD
    u8"\u0e42",  // KeyF
    u8"\u0e0c",  // KeyG
    u8"\u0e47",  // KeyH
    u8"\u0e4b",  // KeyJ
    u8"\u0e29",  // KeyK
    u8"\u0e28",  // KeyL
    u8"\u0e0b",  // Semicolon
    u8".",       // Quote
    u8"(",       // KeyZ
    u8")",       // KeyX
    u8"\u0e09",  // KeyC
    u8"\u0e2e",  // KeyV
    u8"\u0e3a",  // KeyB
    u8"\u0e4c",  // KeyN
    u8"?",       // KeyM
    u8"\u0e12",  // Comma
    u8"\u0e2c",  // Period
    u8"\u0e26",  // Slash
    u8"\u0020",  // Space
};
const char* kShiftAltGr[] = {
    u8"%",       // BackQuote
    u8"+",       // Digit1
    u8"\u0e51",  // Digit2
    u8"\u0e52",  // Digit3
    u8"\u0e53",  // Digit4
    u8"\u0e54",  // Digit5
    u8"\u0e39",  // Digit6
    u8"\u0e3f",  // Digit7
    u8"\u0e55",  // Digit8
    u8"\u0e56",  // Digit9
    u8"\u0e57",  // Digit0
    u8"\u0e58",  // Minus
    u8"\u0e59",  // Equal
    u8"\u0e50",  // KeyQ
    u8"\"",      // KeyW
    u8"\u0e0e",  // KeyE
    u8"\u0e11",  // KeyR
    u8"\u0e18",  // KeyT
    u8"\u0e4d",  // KeyY
    u8"\u0e4a",  // KeyU
    u8"\u0e13",  // KeyI
    u8"\u0e2f",  // KeyO
    u8"\u0e0d",  // KeyP
    u8"\u0e10",  // BracketLeft
    u8",",       // BracketRight
    u8"\u0e05",  // Backslash
    u8"\u0e24",  // KeyA
    u8"\u0e06",  // KeyS
    u8"\u0e0f",  // KeyD
    u8"\u0e42",  // KeyF
    u8"\u0e0c",  // KeyG
    u8"\u0e47",  // KeyH
    u8"\u0e4b",  // KeyJ
    u8"\u0e29",  // KeyK
    u8"\u0e28",  // KeyL
    u8"\u0e0b",  // Semicolon
    u8".",       // Quote
    u8"(",       // KeyZ
    u8")",       // KeyX
    u8"\u0e09",  // KeyC
    u8"\u0e2e",  // KeyV
    u8"\u0e3a",  // KeyB
    u8"\u0e4c",  // KeyN
    u8"?",       // KeyM
    u8"\u0e12",  // Comma
    u8"\u0e2c",  // Period
    u8"\u0e26",  // Slash
    u8"\u0020",  // Space
};
const char* kAltgrCapslock[] = {
    u8"%",       // BackQuote
    u8"+",       // Digit1
    u8"\u0e51",  // Digit2
    u8"\u0e52",  // Digit3
    u8"\u0e53",  // Digit4
    u8"\u0e54",  // Digit5
    u8"\u0e39",  // Digit6
    u8"\u0e3f",  // Digit7
    u8"\u0e55",  // Digit8
    u8"\u0e56",  // Digit9
    u8"\u0e57",  // Digit0
    u8"\u0e58",  // Minus
    u8"\u0e59",  // Equal
    u8"\u0e50",  // KeyQ
    u8"\"",      // KeyW
    u8"\u0e0e",  // KeyE
    u8"\u0e11",  // KeyR
    u8"\u0e18",  // KeyT
    u8"\u0e4d",  // KeyY
    u8"\u0e4a",  // KeyU
    u8"\u0e13",  // KeyI
    u8"\u0e2f",  // KeyO
    u8"\u0e0d",  // KeyP
    u8"\u0e10",  // BracketLeft
    u8",",       // BracketRight
    u8"\u0e05",  // Backslash
    u8"\u0e24",  // KeyA
    u8"\u0e06",  // KeyS
    u8"\u0e0f",  // KeyD
    u8"\u0e42",  // KeyF
    u8"\u0e0c",  // KeyG
    u8"\u0e47",  // KeyH
    u8"\u0e4b",  // KeyJ
    u8"\u0e29",  // KeyK
    u8"\u0e28",  // KeyL
    u8"\u0e0b",  // Semicolon
    u8".",       // Quote
    u8"(",       // KeyZ
    u8")",       // KeyX
    u8"\u0e09",  // KeyC
    u8"\u0e2e",  // KeyV
    u8"\u0e3a",  // KeyB
    u8"\u0e4c",  // KeyN
    u8"?",       // KeyM
    u8"\u0e12",  // Comma
    u8"\u0e2c",  // Period
    u8"\u0e26",  // Slash
    u8"\u0020",  // Space
};
const char* kShiftCapslock[] = {
    u8"_",       // BackQuote
    u8"\u0e45",  // Digit1
    u8"/",       // Digit2
    u8"-",       // Digit3
    u8"\u0e20",  // Digit4
    u8"\u0e16",  // Digit5
    u8"\u0e38",  // Digit6
    u8"\u0e36",  // Digit7
    u8"\u0e04",  // Digit8
    u8"\u0e15",  // Digit9
    u8"\u0e08",  // Digit0
    u8"\u0e02",  // Minus
    u8"\u0e0a",  // Equal
    u8"\u0e46",  // KeyQ
    u8"\u0e44",  // KeyW
    u8"\u0e33",  // KeyE
    u8"\u0e1e",  // KeyR
    u8"\u0e30",  // KeyT
    u8"\u0e31",  // KeyY
    u8"\u0e35",  // KeyU
    u8"\u0e23",  // KeyI
    u8"\u0e19",  // KeyO
    u8"\u0e22",  // KeyP
    u8"\u0e1a",  // BracketLeft
    u8"\u0e25",  // BracketRight
    u8"\u0e03",  // Backslash
    u8"\u0e1f",  // KeyA
    u8"\u0e2b",  // KeyS
    u8"\u0e01",  // KeyD
    u8"\u0e14",  // KeyF
    u8"\u0e40",  // KeyG
    u8"\u0e49",  // KeyH
    u8"\u0e48",  // KeyJ
    u8"\u0e32",  // KeyK
    u8"\u0e2a",  // KeyL
    u8"\u0e27",  // Semicolon
    u8"\u0e07",  // Quote
    u8"\u0e1c",  // KeyZ
    u8"\u0e1b",  // KeyX
    u8"\u0e41",  // KeyC
    u8"\u0e2d",  // KeyV
    u8"\u0e34",  // KeyB
    u8"\u0e37",  // KeyN
    u8"\u0e17",  // KeyM
    u8"\u0e21",  // Comma
    u8"\u0e43",  // Period
    u8"\u0e1d",  // Slash
    u8"\u0020",  // Space
};
const char* kShiftAltGrCapslock[] = {
    u8"_",       // BackQuote
    u8"\u0e45",  // Digit1
    u8"/",       // Digit2
    u8"-",       // Digit3
    u8"\u0e20",  // Digit4
    u8"\u0e16",  // Digit5
    u8"\u0e38",  // Digit6
    u8"\u0e36",  // Digit7
    u8"\u0e04",  // Digit8
    u8"\u0e15",  // Digit9
    u8"\u0e08",  // Digit0
    u8"\u0e02",  // Minus
    u8"\u0e0a",  // Equal
    u8"\u0e46",  // KeyQ
    u8"\u0e44",  // KeyW
    u8"\u0e33",  // KeyE
    u8"\u0e1e",  // KeyR
    u8"\u0e30",  // KeyT
    u8"\u0e31",  // KeyY
    u8"\u0e35",  // KeyU
    u8"\u0e23",  // KeyI
    u8"\u0e19",  // KeyO
    u8"\u0e22",  // KeyP
    u8"\u0e1a",  // BracketLeft
    u8"\u0e25",  // BracketRight
    u8"\u0e03",  // Backslash
    u8"\u0e1f",  // KeyA
    u8"\u0e2b",  // KeyS
    u8"\u0e01",  // KeyD
    u8"\u0e14",  // KeyF
    u8"\u0e40",  // KeyG
    u8"\u0e49",  // KeyH
    u8"\u0e48",  // KeyJ
    u8"\u0e32",  // KeyK
    u8"\u0e2a",  // KeyL
    u8"\u0e27",  // Semicolon
    u8"\u0e07",  // Quote
    u8"\u0e1c",  // KeyZ
    u8"\u0e1b",  // KeyX
    u8"\u0e41",  // KeyC
    u8"\u0e2d",  // KeyV
    u8"\u0e34",  // KeyB
    u8"\u0e37",  // KeyN
    u8"\u0e17",  // KeyM
    u8"\u0e21",  // Comma
    u8"\u0e43",  // Period
    u8"\u0e1d",  // Slash
    u8"\u0020",  // Space
};
const char** kKeyMap[8] = {
    kNormal,   kShift,         kAltGr,         kShiftAltGr,
    kCapslock, kShiftCapslock, kAltgrCapslock, kShiftAltGrCapslock};

}  // namespace th_tis
