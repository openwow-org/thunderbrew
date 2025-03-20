#include "console/Console.hpp"
#include "console/Command.hpp"
#include "console/Types.hpp"
#include "event/Context.hpp"
#include "event/Event.hpp"
#include "gx/Device.hpp"
#include "gx/Buffer.hpp"
#include "gx/Coordinate.hpp"
#include "gx/Draw.hpp"
#include "gx/Font.hpp"
#include "gx/Gx.hpp"
#include "gx/RenderState.hpp"
#include "gx/Screen.hpp"

#include <bc/Debug.hpp>
#include <storm/List.hpp>
#include <storm/thread/SCritSect.hpp>
#include <storm/String.hpp>
#include <storm/region/Types.hpp>
#include <tempest/Rect.hpp>

#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <algorithm>

#define HIGHLIGHT_COPY_SIZE 128

//=============================================================================
// Console settings
static int   s_active;
static int   s_consoleAccessEnabled;
static KEY   s_consoleKey = KEY_TILDE;
static float s_consoleLines = 10.0f;
static float s_fontHeight = 0.02f;
static float s_consoleHeight = s_consoleLines * s_fontHeight;
static int   s_historyIndex;
static uint32_t s_NumLines;

static CONSOLERESIZESTATE s_consoleResizeState = CS_NONE;

//=============================================================================
static CGxStringBatch* s_batch;
static uint32_t s_baseTextFlags = 0x8;
static int32_t s_caret = 0;
static float s_caretpixwidth;
static float s_caretpixheight;
static float s_charSpacing = 0.0f;
static CGxString* s_inputString = nullptr;

//=============================================================================
static char s_fontName[STORM_MAX_PATH];
static HLAYER s_layerBackground;
static HLAYER s_layerText;
static RECTF s_rect = { 0.0f, 1.0f, 1.0f, 1.0f };
static HTEXTFONT s_textFont;

//=============================================================================
static HIGHLIGHTSTATE s_highlightState = HS_NONE;
static RECTF s_hRect = { 0.0f, 0.0f, 0.0f, 0.0f };
static float s_highlightHStart = 0.0f;
static float s_highlightHEnd = 0.0f;
static uint32_t s_highlightLeftCharIndex = 0;
static uint32_t s_highlightRightCharIndex = 0;
static int32_t s_highlightInput = 0;
static char s_copyText[HIGHLIGHT_COPY_SIZE] = { 0 };

//=============================================================================
static CImVector s_colorArray[] = {
    { 0xFF, 0xFF, 0xFF, 0xFF }, // DEFAULT_COLOR
    { 0xFF, 0xFF, 0xFF, 0xFF }, // INPUT_COLOR
    { 0x80, 0x80, 0x80, 0xFF }, // ECHO_COLOR
    { 0x00, 0x00, 0xFF, 0xFF }, // ERROR_COLOR
    { 0x00, 0xFF, 0xFF, 0xFF }, // WARNING_COLOR
    { 0xFF, 0xFF, 0xFF, 0xFF }, // GLOBAL_COLOR
    { 0xFF, 0xFF, 0xFF, 0xFF }, // ADMIN_COLOR
    { 0xFF, 0xFF, 0xFF, 0x80 }, // HIGHLIGHT_COLOR
    { 0x00, 0x00, 0x00, 0xC0 }, // BACKGROUND_COLOR
};

//=============================================================================
// In this list:
// The head = the input line.
// The tail = the oldest line printed.
static STORM_LIST(CONSOLELINE) s_linelist;

//=============================================================================
// Pointer to the current line:
// Determines what region of the console history gets rendered.
static CONSOLELINE* s_currlineptr;

// Initialize the critical section
SCritSect s_critsect;

//=============================================================================
static void EnforceMaxLines ();
static void MakeCommandCurrent (CONSOLELINE* lineptr, char const* command);
static void MoveLinePtr (int32_t direction, int32_t modifier);
static void PasteInInputLine (char* characters);
static void ReserveInputSpace (CONSOLELINE* line, size_t len);

static CONSOLELINE* GetInputLine ();
static CONSOLELINE* GetLineAtMousePosition (float y);
static CONSOLELINE* GetCurrentLine ();

static void DrawBackground ();
static void DrawHighLight ();
static void DrawCaret (C3Vector& caretpos);
static void PaintBackground (void* param, const RECTF* rect, const RECTF* visible, float elapsedSec);
static void SetInputString (char* buffer);
static void GenerateNodeString (CONSOLELINE* node);
static void PaintText (void* param, const RECTF* rect, const RECTF* visible, float elapsedSec);
static void UpdateHighlight ();
static void ResetHighlight ();
static HIGHLIGHTSTATE GetHighlightState ();
static void SetHighlightState (HIGHLIGHTSTATE hs);
static char* GetHighlightCopyText ();
static void SetHighlightCopyText (char* text);
static void ResetHighlightCopyText ();
static RECTF& GetHighlightRect ();
static void SetHighlightStart (float start);
static void SetHighlightEnd (float end);
static void CutHighlightToClipboard ();
static void PasteClipboardToHighlight ();

static int OnChar (const EVENT_DATA_CHAR* data, void* param);
static int OnIdle (const EVENT_DATA_IDLE* data, void* param);
static int OnKeyDown (const EVENT_DATA_KEY* data, void* param);
static int OnKeyDownRepeat (const EVENT_DATA_KEY* data, void* param);
static int OnKeyUp (const EVENT_DATA_KEY* data, void* param);
static int OnMouseDown (const EVENT_DATA_MOUSE* data, void* param);
static int OnMouseMove (const EVENT_DATA_MOUSE* data, void* param);
static int OnMouseUp (const EVENT_DATA_MOUSE* data, void* param);


/******************************************************************************
*
*   External
*
***/

//=============================================================================
int ConsoleAccessGetEnabled () {
    return s_consoleAccessEnabled;
}

//=============================================================================
void ConsoleAccessSetEnabled (int enable) {
    s_consoleAccessEnabled = enable;
}

//=============================================================================
int ConsoleGetActive () {
    return s_active;
}

//=============================================================================
float ConsoleGetFontHeight () {
    return s_fontHeight;
}

//=============================================================================
float ConsoleGetHeight () {
    return s_consoleHeight;
}

//=============================================================================
float ConsoleGetLines () {
    return s_consoleLines;
}

//=============================================================================
KEY ConsoleGetHotKey () {
    return s_consoleKey;
}

//=============================================================================
CONSOLERESIZESTATE ConsoleGetResizeState () {
    return s_consoleResizeState;
}

//=============================================================================
void ConsoleSetActive (int active) {
    s_active = active;
}

//=============================================================================
void ConsoleSetHotKey (KEY hotkey) {
    s_consoleKey = hotkey;
}

//=============================================================================
void ConsoleSetResizeState (CONSOLERESIZESTATE state) {
    s_consoleResizeState = state;
}

//=============================================================================
void ConsoleSetHeight (float height) {
    s_consoleHeight = height;
}

//=============================================================================
void ConsolePostClose () {
    EventPostCloseEx(EventGetCurrentContext());
}

//=============================================================================
void ConsoleWrite (const char* str, COLOR_T color) {
    if (g_theGxDevicePtr == nullptr || str[0] == '\0') {
        return;
    }

    s_critsect.Enter();

    auto l = reinterpret_cast<char*>(SMemAlloc(sizeof(CONSOLELINE), __FILE__, __LINE__, 0));
    auto lineptr = new (l) CONSOLELINE();

    auto head = s_linelist.Head();

    if (head == nullptr || head->inputpos == 0) {
        // Attach console line to head
        s_linelist.LinkToHead(lineptr);
    } else {
        // Attach console line between head and head-1
        s_linelist.LinkNode(lineptr, 1, head->Prev());
    }

    size_t len = SStrLen(str) + 1;
    lineptr->chars = len;
    lineptr->charsalloc = len;
    lineptr->buffer = reinterpret_cast<char*>(SMemAlloc(len, __FILE__, __LINE__, 0));
    lineptr->colorType = color;

    SStrCopy(lineptr->buffer, str, STORM_MAX_STR);

    GenerateNodeString(lineptr);

    s_NumLines++;

    EnforceMaxLines();
    //

    s_critsect.Leave();
}

//=============================================================================
void ConsoleWriteA (const char* str, COLOR_T color, ...) {
    char buffer[1024] = { 0 };

    if (str != nullptr && str[0] != '\0') {
        va_list list;
        va_start(list, color);
        vsnprintf(buffer, sizeof(buffer), str, list);
        va_end(list);

        ConsoleWrite(buffer, color);
    }
}

//=============================================================================
void ConsoleClear () {
    s_NumLines = 0;

    auto ptr = s_linelist.Head();

    while (ptr) {
        s_linelist.UnlinkNode(ptr);
        s_linelist.DeleteNode(ptr);
        ptr = s_linelist.Head();
    }
}

//=============================================================================
void ConsoleScreenAnimate (float elapsedSec) {
    auto finalPos = ConsoleGetActive() ? std::min(1.0f - ConsoleGetHeight(), 1.0f) : 1.0f;
    finalPos = std::max(finalPos, 0.0f);

    if (s_rect.bottom == finalPos) {
        return;
    }

    auto currentPos = finalPos;

    if (ConsoleGetResizeState() == CS_NONE) {
        auto direction = s_rect.bottom <= finalPos ? 1.0f : -1.0f;

        currentPos = s_rect.bottom + direction * elapsedSec * 5.0f;
        currentPos = ConsoleGetActive() ? std::max(currentPos, finalPos) : std::min(currentPos, finalPos);
    }

    s_rect.bottom = currentPos;

    ScrnLayerSetRect(s_layerBackground, &s_rect);
    ScrnLayerSetRect(s_layerText, &s_rect);
}

//=============================================================================
void ConsoleScreenInitialize (const char* title) {
    CRect windowSize;
    GxCapsWindowSize(windowSize);

    auto width = windowSize.maxX - windowSize.minX;
    auto height = windowSize.maxY - windowSize.minY;
    s_caretpixwidth = width == 0.0f ? 1.0f : 1.0f / width;
    s_caretpixheight = height == 0.0f ? 1.0f : 1.0f / height;

    SStrCopy(s_fontName, "Fonts\\ARIALN.ttf", sizeof(s_fontName));
    s_textFont = TextBlockGenerateFont(s_fontName, 0, NDCToDDCHeight(ConsoleGetFontHeight()));

    ScrnLayerCreate(&s_rect, 6.0f, 0x1 | 0x2, nullptr, PaintBackground, &s_layerBackground);
    ScrnLayerCreate(&s_rect, 7.0f, 0x1 | 0x2, nullptr, PaintText, &s_layerText);

    RegisterHandlers();

    // TODO register commands
    ConsoleInitializeScreenCommand();

    // TODO EventSetConfirmCloseCallback(EventCloseCallback, 0);

    ConsoleCommandExecute("ver", 1);

    s_batch = GxuFontCreateBatch(false, false);
}

//=============================================================================
void RegisterHandlers () {
    EventRegisterEx(EVENT_ID_CHAR, reinterpret_cast<EVENTHANDLERFUNC>(OnChar), nullptr, 7.0f);
    EventRegisterEx(EVENT_ID_IDLE, reinterpret_cast<EVENTHANDLERFUNC>(OnIdle), nullptr, 7.0f);
    EventRegisterEx(EVENT_ID_KEYDOWN, reinterpret_cast<EVENTHANDLERFUNC>(OnKeyDown), nullptr, 7.0f);
    EventRegisterEx(EVENT_ID_KEYUP, reinterpret_cast<EVENTHANDLERFUNC>(OnKeyUp), nullptr, 7.0f);
    EventRegisterEx(EVENT_ID_KEYDOWN_REPEATING, reinterpret_cast<EVENTHANDLERFUNC>(OnKeyDownRepeat), nullptr, 7.0f);
    EventRegisterEx(EVENT_ID_MOUSEDOWN, reinterpret_cast<EVENTHANDLERFUNC>(OnMouseDown), nullptr, 7.0f);
    EventRegisterEx(EVENT_ID_MOUSEUP, reinterpret_cast<EVENTHANDLERFUNC>(OnMouseUp), nullptr, 7.0f);
    EventRegisterEx(EVENT_ID_MOUSEMOVE, reinterpret_cast<EVENTHANDLERFUNC>(OnMouseMove), nullptr, 7.0f);
}


/******************************************************************************
*
*   CONSOLELINE
*
***/

//=============================================================================
CONSOLELINE::~CONSOLELINE () {
    if (this->buffer) {
        SMemFree(this->buffer, __FILE__, __LINE__, 0);
    }

    if (this->fontPointer) {
        GxuFontDestroyString(this->fontPointer);
    }
}

//=============================================================================
void CONSOLELINE::Backspace () {
    if (this->inputpos > this->inputstart) {
        if (this->chars <= this->inputpos)
            this->buffer[this->inputpos - 1] = '\0';
        else
            memcpy(&this->buffer[this->inputpos - 1],
                   &this->buffer[this->inputpos],
                   this->chars - this->inputpos + 1);
        --this->inputpos;
        --this->chars;
        SetInputString(this->buffer);
    }
}

//=============================================================================
void CONSOLELINE::Up () {
    if (s_historyIndex != ConsoleCommandHistoryDepth() - 1) {
        int index = s_historyIndex + 1;
        const char * commandHistory = ConsoleCommandHistory(index);
        if (commandHistory) {
            MakeCommandCurrent(this, commandHistory);
            s_historyIndex = index;
            SetInputString(this->buffer);
        }
    }
}

//=============================================================================
void CONSOLELINE::Down () {
    const char * commandHistory = "";
    if (s_historyIndex != -1) {
        int index = s_historyIndex - 1;
        if (s_historyIndex) {
            commandHistory = ConsoleCommandHistory(index);
            if (!commandHistory)
                return;
        }
        MakeCommandCurrent(this, commandHistory);
        s_historyIndex = index;
        SetInputString(this->buffer);
    }
}


/******************************************************************************
*
*   Internal
*
***/

//=============================================================================
void EnforceMaxLines () {
    if (s_NumLines <= CONSOLE_LINES_MAX) {
        return;
    }

    // Pop oldest line off the list
    auto lineptr = s_linelist.Tail();

    if (lineptr == nullptr) {
        lineptr = s_currlineptr;
    }

    if (lineptr == nullptr) {
        return;
    }

    // Clean up oldest line.
    s_linelist.UnlinkNode(lineptr);
    s_linelist.DeleteNode(lineptr);

    s_NumLines--;
}

void MakeCommandCurrent(CONSOLELINE * lineptr, char const * command) {
    auto len = lineptr->inputstart;
    lineptr->inputpos = len;
    lineptr->chars = len;
    lineptr->buffer[len] = '\0';

    len = SStrLen(command);
    ReserveInputSpace(lineptr, len);

    SStrCopy(lineptr->buffer + lineptr->inputpos, command, STORM_MAX_STR);

    len = lineptr->inputpos + len;
    lineptr->inputpos = len;
    lineptr->chars = len;
}

//=============================================================================
void MoveLinePtr (int32_t direction, int32_t modifier) {
    CONSOLELINE* lineptr = s_currlineptr;

    auto anyControl = (1 << KEY_LCONTROL) | (1 << KEY_RCONTROL);

    if (modifier & anyControl) {
        for (int32_t i = 0; i < 10 && lineptr != nullptr; i++) {
            CONSOLELINE* next;

            if (direction == 1) {
                next = lineptr->m_link.Next();
            } else {
                next = lineptr->m_link.Prev();
            }

            if (next != nullptr) {
                lineptr = next;
            }
        }
    } else {
        // if (s_currlineptr == s_linelist.Head()) {
        //     s_currlineptr = s_currlineptr->Prev();
        // }

        if (direction == 1) {
            lineptr = lineptr->m_link.Next();
        } else {
            lineptr = lineptr->m_link.Prev();
        }
    }

    if (lineptr) {
        s_currlineptr = lineptr;
    }
}

//=============================================================================
void PasteInInputLine (char* characters) {
    auto len = SStrLen(characters);

    if (!len) {
        return;
    }

    auto line = GetInputLine();

    ReserveInputSpace(line, len);

    if (line->inputpos < line->chars) {
        if (len <= 1) {
            memmove(&line->buffer[line->inputpos + 1], &line->buffer[line->inputpos], line->chars - (line->inputpos + 1));

            line->buffer[line->inputpos] = *characters;

            line->inputpos++;
            line->chars++;
        } else {
            auto input = reinterpret_cast<char*>(SMemAlloc(line->charsalloc, __FILE__, __LINE__, 0x0));
            SStrCopy(input, &line->buffer[line->inputpos], STORM_MAX_STR);

            auto buffer = reinterpret_cast<char*>(SMemAlloc(line->charsalloc, __FILE__, __LINE__, 0x0));
            SStrCopy(buffer, line->buffer, STORM_MAX_STR);
            buffer[line->inputpos] = '\0';

            SStrPack(buffer, characters, line->charsalloc);

            auto len = SStrLen(buffer);

            line->inputpos = len;

            SStrPack(buffer, input, line->charsalloc);
            SStrCopy(line->buffer, buffer, STORM_MAX_STR);

            line->chars = SStrLen(line->buffer);

            if (input) {
                SMemFree(input, __FILE__, __LINE__, 0);
            }

            if (buffer) {
                SMemFree(input, __FILE__, __LINE__, 0);
            }
        }
    } else {
        for (int32_t i = 0; i < len; i++) {
            line->buffer[line->inputpos++] = characters[i];
        }

        line->buffer[line->inputpos] = '\0';
        line->chars = line->inputpos;
    }

    SetInputString(line->buffer);
}

//=============================================================================
void ReserveInputSpace (CONSOLELINE* line, size_t len) {
    size_t newsize = line->chars + len;
    if (newsize >= line->charsalloc) {
        while (line->charsalloc <= newsize) {
            line->charsalloc += CONSOLE_LINE_PREALLOC;
        }

        auto buffer = reinterpret_cast<char*>(SMemAlloc(line->charsalloc, __FILE__, __LINE__, 0));
        SStrCopy(buffer, line->buffer, line->charsalloc);
        SMemFree(line->buffer, __FILE__, __LINE__, 0x0);
        line->buffer = buffer;
    }
}

//=============================================================================
CONSOLELINE* GetInputLine () {
    auto head = s_linelist.Head();

    // If the list is empty, or the list's head is an entered input-line,
    // Create a fresh input line, with "> " prefixed before the caret.
    if (!head || head->inputpos == 0) {
        auto l = SMemAlloc(sizeof(CONSOLELINE), __FILE__, __LINE__, 0);
        auto line = new (l) CONSOLELINE();
        line->buffer = reinterpret_cast<char*>(SMemAlloc(CONSOLE_LINE_PREALLOC, __FILE__, __LINE__, 0));
        line->charsalloc = CONSOLE_LINE_PREALLOC;

        s_linelist.LinkToHead(line);

        SStrCopy(line->buffer, "> ", line->charsalloc);
        SetInputString(line->buffer);
        auto chars = SStrLen(line->buffer);
        s_NumLines++;
        line->inputstart = chars;
        line->inputpos = chars;
        line->chars = chars;
        line->colorType = INPUT_COLOR;

        s_currlineptr = line;

        EnforceMaxLines();

        return line;
    }

    return head;
}

//=============================================================================
CONSOLELINE* GetLineAtMousePosition (float y) {
    // Loop through linelist to find line at mouse position
    int32_t linePos = static_cast<int32_t>((ConsoleGetHeight() - (1.0 - y)) / ConsoleGetFontHeight());

    if (linePos == 1) {
        return s_linelist.Head();
    }

    if (s_currlineptr != s_linelist.Head()) {
        linePos--;
    }

    CONSOLELINE* line = s_currlineptr;

    while (linePos > 1) {
        linePos--;

        if (!line) {
            line = s_linelist.Head();
        }

        if (line == nullptr) {
            return nullptr;
        }

        line = line->Next();
    }

    return line;
}

//=============================================================================
CONSOLELINE* GetCurrentLine () {
    return s_currlineptr;
}

//=============================================================================
void DrawBackground () {
    uint16_t indices[] = {
        0, 1, 2, 3
    };

    C3Vector position[] = {
        { s_rect.left,  s_rect.bottom,  0.0f },
        { s_rect.right, s_rect.bottom,  0.0f },
        { s_rect.left,  s_rect.top,     0.0f },
        { s_rect.right, s_rect.top,     0.0f }
    };

    GxRsPush();

    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_DepthTest, 0);
    GxRsSet(GxRs_DepthWrite, 0);
    GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_PolygonOffset, 0.0f);
    GxRsSet(GxRs_BlendingMode, GxBlend_Alpha);
    GxRsSet(GxRs_AlphaRef, CGxDevice::s_alphaRef[GxBlend_Alpha]);

    GxPrimLockVertexPtrs(4, position, sizeof(C3Vector), nullptr, 0, &s_colorArray[BACKGROUND_COLOR], 0, nullptr, 0, nullptr, 0, nullptr, 0);
    GxDrawLockedElements(GxPrim_TriangleStrip, 4, indices);
    GxPrimUnlockVertexPtrs();

    GxRsPop();
}

//=============================================================================
void DrawHighLight () {
    uint16_t indices[] = {
        0, 1, 2, 3
    };

    C3Vector position[] = {
        { s_hRect.left,  s_hRect.bottom,  0.0f },
        { s_hRect.right, s_hRect.bottom,  0.0f },
        { s_hRect.left,  s_hRect.top,     0.0f },
        { s_hRect.right, s_hRect.top,     0.0f }
    };

    GxRsPush();

    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_BlendingMode, GxBlend_Alpha);
    GxRsSet(GxRs_AlphaRef, CGxDevice::s_alphaRef[GxBlend_Alpha]);

    GxPrimLockVertexPtrs(4, position, sizeof(C3Vector), nullptr, 0, &s_colorArray[HIGHLIGHT_COLOR], 0, nullptr, 0, nullptr, 0, nullptr, 0);
    GxDrawLockedElements(GxPrim_TriangleStrip, 4, indices);
    GxPrimUnlockVertexPtrs();

    GxRsPop();
}

//=============================================================================
void DrawCaret (C3Vector& caretpos) {
    uint16_t indices[] = {
        0, 1, 2, 3
    };

    float minX = caretpos.x;
    float minY = caretpos.y;

    float maxX = caretpos.x + (s_caretpixwidth * 2);
    float maxY = caretpos.y + ConsoleGetFontHeight();

    C3Vector position[] = {
        { minX, minY, 0.0f },
        { maxX, minY, 0.0f },
        { minX, maxY, 0.0f },
        { maxX, maxY, 0.0f }
    };

    GxRsPush();

    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_DepthTest, 0);
    GxRsSet(GxRs_DepthWrite, 0);
    GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_PolygonOffset, 0.0f);
    GxRsSet(GxRs_BlendingMode, GxBlend_Alpha);
    GxRsSet(GxRs_AlphaRef, CGxDevice::s_alphaRef[GxBlend_Alpha]);

    GxPrimLockVertexPtrs(4, position, sizeof(C3Vector), nullptr, 0, &s_colorArray[INPUT_COLOR], 0, nullptr, 0, nullptr, 0, nullptr, 0);
    GxDrawLockedElements(GxPrim_TriangleStrip, 4, indices);
    GxPrimUnlockVertexPtrs();

    GxRsPop();
}

//=============================================================================
void PaintBackground (void* param, const RECTF* rect, const RECTF* visible, float elapsedSec) {
    if (s_rect.bottom < 1.0f) {
        DrawBackground();

        if (s_highlightState) {
            DrawHighLight();
        }
    }
}

//=============================================================================
void SetInputString (char* buffer) {
    // s_highlightState = HS_NONE;
    // s_hRect = { 0.0f, 0.0f, 0.0f, 0.0f };
    // s_highlightLeftCharIndex = 0;
    // s_highlightRightCharIndex = 0;
    // s_highlightInput = 0;

    if (s_inputString) {
        GxuFontDestroyString(s_inputString);
    }

    s_inputString = nullptr;

    auto fontHeight = ConsoleGetFontHeight();

    if (buffer && buffer[0] != '\0') {
        C3Vector pos = { 0.0f, 0.0f, 1.0f };

        auto font = TextBlockGetFontPtr(s_textFont);

        GxuFontCreateString(font, buffer, fontHeight, pos, 1.0f, fontHeight, 0.0f, s_inputString, GxVJ_Middle, GxHJ_Left, s_baseTextFlags, s_colorArray[INPUT_COLOR], s_charSpacing, 1.0f);
    }
}

//=============================================================================
void GenerateNodeString (CONSOLELINE* node) {
    auto font = TextBlockGetFontPtr(s_textFont);

    if (font && node && node->buffer && node->buffer[0] != '\0') {
        if (node->fontPointer) {
            GxuFontDestroyString(node->fontPointer);
        }

        C3Vector pos = {
            0.0f, 0.0f, 1.0f
        };

        auto fontHeight = ConsoleGetFontHeight();

        GxuFontCreateString(font, node->buffer, fontHeight, pos, 1.0f, fontHeight, 0.0f, node->fontPointer, GxVJ_Middle, GxHJ_Left, s_baseTextFlags, s_colorArray[node->colorType], s_charSpacing, 1.0f);
        BLIZZARD_ASSERT(node->fontPointer);
    }
}

//=============================================================================
void PaintText (void* param, const RECTF* rect, const RECTF* visible, float elapsedSec) {
    if (s_rect.bottom >= 1.0f) {
        return;
    }

    static float carettime = 0.0f;
    static C3Vector caretpos = { 0.0f, 0.0f, 0.0f };

    //
    carettime += elapsedSec;
    if ((!s_caret && carettime > 0.2) || (carettime > 0.3)) {
        s_caret = !s_caret;
        carettime = 0;
    }

    auto line = GetInputLine();

    C3Vector pos = {
        s_rect.left,
        (ConsoleGetFontHeight() * 0.75f) + s_rect.bottom,
        1.0f
    };

    GxuFontClearBatch(s_batch);

    if (s_inputString) {
        GxuFontSetStringPosition(s_inputString, pos);
        GxuFontAddToBatch(s_batch, s_inputString);
    }

    auto font = TextBlockGetFontPtr(s_textFont);

    if (line->inputpos) {
        caretpos = pos;

        GxuFontGetTextExtent(font, line->buffer, line->inputpos, ConsoleGetFontHeight(), &caretpos.x, 0.0f, 1.0f, s_charSpacing, s_baseTextFlags);

        DrawCaret(caretpos);
    }

    pos.y += ConsoleGetFontHeight();

    for (auto lineptr = GetCurrentLine(); (lineptr && pos.y < 1.0); lineptr = lineptr->Next()) {
        if (lineptr != line) {
            if (lineptr->fontPointer == nullptr) {
                GenerateNodeString(lineptr);
            }

            GxuFontSetStringPosition(lineptr->fontPointer, pos);
            GxuFontAddToBatch(s_batch, lineptr->fontPointer);
            pos.y += ConsoleGetFontHeight();
        }
    }

    GxuFontRenderBatch(s_batch);
}

//=============================================================================
void UpdateHighlight () {
    auto font = TextBlockGetFontPtr(s_textFont);
    BLIZZARD_ASSERT(font);

    auto len = SStrLen(s_copyText);

    float left = std::min(s_highlightHStart, s_highlightHEnd);
    float right = std::max(s_highlightHStart, s_highlightHEnd);

    auto chars = GxuFontGetMaxCharsWithinWidth(font, s_copyText, ConsoleGetFontHeight(), left, len, &s_hRect.left, 0.0f, 1.0f, s_charSpacing, s_baseTextFlags);

    s_highlightLeftCharIndex = chars;

    if (chars) {
        s_highlightRightCharIndex = chars - 1;
    }

    if (s_hRect.left < 0.015f) {
        s_hRect.left = 0.0f;
    }

    s_highlightRightCharIndex = GxuFontGetMaxCharsWithinWidth(font, s_copyText, ConsoleGetFontHeight(), right, len, &s_hRect.right, 0.0f, 1.0f, s_charSpacing, s_baseTextFlags);
}

//=============================================================================
void ResetHighlight () {
    s_highlightState = HS_NONE;
    s_hRect = { 0.0f, 0.0f, 0.0f, 0.0f };
}

//=============================================================================
HIGHLIGHTSTATE GetHighlightState () {
    return s_highlightState;
}

//=============================================================================
void SetHighlightState (HIGHLIGHTSTATE hs) {
    s_highlightState = hs;
}

//=============================================================================
char* GetHighlightCopyText () {
    return s_copyText;
}

//=============================================================================
void SetHighlightCopyText (char* text) {
    SStrCopy(s_copyText, text, HIGHLIGHT_COPY_SIZE);
}

//=============================================================================
void ResetHighlightCopyText () {
    s_copyText[0] = '\0';
}

//=============================================================================
RECTF& GetHighlightRect () {
    return s_hRect;
}

//=============================================================================
void SetHighlightStart (float start) {
    s_highlightHStart = start;
}

//=============================================================================
void SetHighlightEnd (float end) {
    s_highlightHEnd = end;
}

//=============================================================================
void CutHighlightToClipboard () {
    char buffer[HIGHLIGHT_COPY_SIZE];

    if (s_copyText[0] != '\0') {
        uint32_t size = s_highlightRightCharIndex - s_highlightLeftCharIndex;
        uint32_t capsize = HIGHLIGHT_COPY_SIZE-1;
        size = std::min(size, capsize);

        SStrCopy(buffer, &s_copyText[s_highlightLeftCharIndex], size);

        buffer[size] = '\0';

        // OsClipboardPutString(buffer);
    }

    ResetHighlight();
}

//=============================================================================
void PasteClipboardToHighlight () {
    // auto buffer = OsClipboardGetString();
    // PasteInInputLine(buffer);
    // SMemFree(buffer, __FILE__, __LINE__, 0);
    // ResetHighlight();
}

//=============================================================================
int OnChar (const EVENT_DATA_CHAR* data, void* param) {
    char character[2];

    if (ConsoleAccessGetEnabled() && EventIsKeyDown(ConsoleGetHotKey())) {
        return 0;
    }

    if (ConsoleGetActive()) {
        character[0] = char(data->ch);
        character[1] = 0;

        PasteInInputLine(character);
        ResetHighlight();
        return 0;
    }

    // SUniSPutUTF8(data->ch, character);


    return 1;
}

//=============================================================================
int OnIdle (const EVENT_DATA_IDLE* data, void* param) {
    // TODO repeat buffer logic

    ConsoleScreenAnimate(data->elapsedSec);

    return 1;
}

//=============================================================================
int OnKeyDown (const EVENT_DATA_KEY* data, void* param) {
    if (data->key == ConsoleGetHotKey() && ConsoleAccessGetEnabled()) {
        // Toggle the console on/off if the console hotkey is pressed down
        // and the console access is enabled for the client
        ConsoleSetActive(!ConsoleGetActive());

        // Reset the highlight when toggled off
        if (!ConsoleGetActive()) {
            ResetHighlight();
        }

        return 0;
    }

    if (EventIsKeyDown(ConsoleGetHotKey()) || !ConsoleGetActive()) {
        return 1;
    }

    auto anyControl = (1 << KEY_LCONTROL) | (1 << KEY_RCONTROL);

    auto line = GetInputLine();

    switch (data->key) {
    case KEY_ESCAPE:
        if (line->inputpos < line->inputstart || line->inputpos == line->inputstart) {
            ConsoleSetActive(0);
        } else {
            line->inputpos = line->inputstart;
            line->chars = line->inputstart;
            line->buffer[line->inputstart] = '\0';
            SetInputString(line->buffer);
        }
        break;
    case KEY_PAGEUP:
        MoveLinePtr(1, data->metaKeyState);
        break;
    case KEY_PAGEDOWN:
        MoveLinePtr(0, data->metaKeyState);
        break;
    case KEY_ENTER:
        if (line->inputstart <= line->inputpos && line->inputpos != line->inputstart) {
            line->inputpos = 0;
            GenerateNodeString(line);
            ConsoleCommandExecute(line->buffer + line->inputstart, 1);
            s_historyIndex = -1;
        }
        break;
    case KEY_HOME:
        break;
    case KEY_END:
        break;
    case KEY_C:
        if (data->metaKeyState & anyControl) {
            CutHighlightToClipboard();
        }
        break;
    case KEY_V:
        if (data->metaKeyState & anyControl) {
            PasteClipboardToHighlight();
        }
        break;
    case KEY_LEFT:
        if (line->inputstart <= line->inputpos && line->inputpos != line->inputstart) {
            line->inputpos--;
        }
        break;
    case KEY_UP:
        line->Up();
        break;
    case KEY_RIGHT:
        if (line->inputpos < line->chars) {
            line->inputpos++;
        }
        break;
    case KEY_DOWN:
        line->Down();
        break;
    case KEY_BACKSPACE:
        line->Backspace();
        break;

    default:
        break;
    }

    if (data->key != KEY_TAB && data->key != KEY_LSHIFT && data->key != KEY_RSHIFT && data->key != KEY_LALT && data->key != KEY_RALT && !(data->metaKeyState & anyControl)) {
        // s_completionMode = 0;
        ResetHighlight();
    }

    // TODO
    return 0;
}

//=============================================================================
int OnKeyDownRepeat(const EVENT_DATA_KEY* data, void* param) {
    if (data->key == ConsoleGetHotKey() && ConsoleAccessGetEnabled()) {
        ConsoleSetActive(!ConsoleGetActive());
        return 0;
    }

    auto anyControl = (1 << KEY_LCONTROL) | (1 << KEY_RCONTROL);

    auto line = GetInputLine();

    switch (data->key) {
    case KEY_PAGEUP:
        MoveLinePtr(1, data->metaKeyState);
        break;
    case KEY_PAGEDOWN:
        MoveLinePtr(0, data->metaKeyState);
        break;
    case KEY_LEFT:
        if (line->inputstart <= line->inputpos && line->inputpos != line->inputstart) {
            line->inputpos--;
        }
        break;
    case KEY_RIGHT:
        if (line->inputpos < line->chars) {
            line->inputpos++;
        }
        break;
    case KEY_BACKSPACE:
        line->Backspace();
        break;
    }

    if (data->key != KEY_TAB && data->key != KEY_LSHIFT && data->key != KEY_RSHIFT && data->key != KEY_LALT && data->key != KEY_RALT && !(data->metaKeyState & anyControl)) {
        // s_completionMode = 0;
        ResetHighlight();
    }

    return 1;
}

//=============================================================================
int OnKeyUp (const EVENT_DATA_KEY* data, void* param) {
    // TODO
    return 1;
}

//=============================================================================
int OnMouseDown (const EVENT_DATA_MOUSE* data, void* param) {
    auto consoleHeight = ConsoleGetHeight();
    auto fontHeight = ConsoleGetFontHeight();

    if (EventIsKeyDown(ConsoleGetHotKey()) || !ConsoleGetActive() || (1.0f - consoleHeight) > data->y) {
        return 1;
    }

    float clickPos = 1.0f - data->y;

    if (clickPos < (std::min(consoleHeight, 1.0f) - (fontHeight * 0.75f)) || clickPos > consoleHeight) {
        ResetHighlight();

        auto line = GetLineAtMousePosition(data->y);

        if (line) {
            SetHighlightCopyText(line->buffer);
            SetHighlightState(HS_HIGHLIGHTING);

            float v7 = 1.0f - (consoleHeight - (fontHeight * 0.75f) - (fontHeight) - ((consoleHeight - clickPos) / fontHeight - 1.0) * fontHeight);

            auto hRect = GetHighlightRect();

            hRect.bottom = v7;
            hRect.top = v7 - fontHeight;

            SetHighlightStart(v7);
            SetHighlightEnd(v7);

            UpdateHighlight();

            return 0;
        }

        ResetHighlightCopyText();
        return 0;
    }

    ResetHighlight();

    ConsoleSetResizeState(CS_STRETCH);

    return 1;
}

//=============================================================================
int OnMouseMove (const EVENT_DATA_MOUSE* data, void* param) {
    if (EventIsKeyDown(ConsoleGetHotKey()) || !ConsoleGetActive()) {
        return 1;
    }

    if (ConsoleGetResizeState() == CS_STRETCH) {
        auto newHeight = std::max(1.0f - data->y, ConsoleGetFontHeight());
        ConsoleSetHeight(newHeight);
    } else if ((1.0f - ConsoleGetHeight()) > data->y) {
        return 1;
    }

    SetHighlightEnd(data->x);

    if (GetHighlightState() == HS_HIGHLIGHTING) {
        UpdateHighlight();
    }

    return 1;
}

//=============================================================================
int OnMouseUp (const EVENT_DATA_MOUSE* data, void* param) {
    if (EventIsKeyDown(ConsoleGetHotKey()) || !ConsoleGetActive()) {
        return 1;
    }

    SetHighlightState(HS_ENDHIGHLIGHT);
    ConsoleSetResizeState(CS_NONE);

    return 1;
}
