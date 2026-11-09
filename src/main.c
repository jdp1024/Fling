/*

Fling in C - DirectX 11 Edition
Written by jdp1024.
Distributed under GPLv3.

*/

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#else
#error "A C11 or later compiler is required to compile this program."
#endif

#define _CRT_RAND_S // must be defined as early as possible
#include <windows.h>
#include <windowsx.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <intrin.h>
#include <immintrin.h>
#include <initguid.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <UIAnimation.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <commdlg.h>
#include <wincodec.h>
#include <mmsystem.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "winmm.lib")

#pragma warning(disable: 4100) // unreferenced formal parameter
#pragma warning(disable: 4702) // unreachable code

// Animation speed in pixels per second at 96 DPI. This will be scaled based
// on the actual DPI to maintain a consistent perceived speed across displays.
#define ANIMATION_PIXELS_PER_SECOND 1000.0f
// Scale factor for the collision "bump" effect when a piece collides with
// another piece during animation. This is multiplied by the cell size to
// determine the maximum offset of the bump.
#define ANIMATION_COLLISION_BUMP_SCALE 0.12f
// Start the move chime slightly before the visual impact to compensate for
// MIDI device startup latency and keep it feeling synchronized.
#define ANIMATION_COLLISION_SOUND_TRIGGER_PROGRESS 0.42f

#define MIN_GENERATE_ANINAL_DURATION_MS 800

#define DEFAULT_GENERATE_PIECE_COUNT 15
#define BOARD_VERTEX_CAPACITY 131072

#define BOARD_ROWS 10
#define BOARD_COLS 7
#define CELL_SIZE 52
#define BOARD_MARGIN 7
#define PANEL_WIDTH 180
#define MIN_PANEL_WIDTH 160
#define TIMER_SOLVE 1
#define TIMER_GENERATE_ANIMATION 2
#define TIMER_VICTORY_MIDI 3
#define TIMER_VICTORY_FIREWORKS 4
#define TIMER_MOVE_MIDI 5
#define TIMER_VICTORY_START 6
#define TIMER_SOLVED_OVERLAY 7
#define TIMER_BACKGROUND_MIDI 8
#define MIDI_BACKGROUND_STEP_MS 10
#define MIDI_BACKGROUND_TICKS_PER_QUARTER 384
#define MIDI_BACKGROUND_TEMPO_US_PER_QUARTER 1090909
#define MIDI_BACKGROUND_EVENT_BITS 11
// Extra pause after each autoplay move finishes sounding.
#define SOLUTION_PLAY_DELAY_MS 800
#define VICTORY_START_DELAY_MS 500
#define MIDI_VICTORY_NOTE_MS 180
#define MIDI_MOVE_NOTE_MIN_MS 75
#define MIDI_MOVE_NOTE_MAX_MS 145
#define MOVE_MIDI_MAX_NOTES 5
#define VICTORY_FIREWORKS_FRAME_MS 16
#define VICTORY_FIREWORK_BURST_INTERVAL_MS 420
#define VICTORY_FIREWORK_BURST_LIFETIME_MS 1100
#define SOLVED_OVERLAY_DURATION_MS 1800
#define SOLVED_OVERLAY_FRAME_MS 16
#define ANIMATION_MIN_DURATION_MS 16
#define MAX_LOADING_BALLS (BOARD_ROWS * BOARD_COLS)
#define LOADING_BALL_SETTLE_DURATION_MS 650

#define IDC_RANDOM 1001
#define IDC_CLEAR 1002
#define IDC_RESTORE 1003
#define IDC_SOLVE 1004
#define IDC_PREVIOUS 1005
#define IDC_NEXT 1006
#define IDC_DIFFICULTY 1007
#define IDC_PLAY 1008
#define IDC_SAVE_PICTURE 1009

#define IDM_SYSTEM_TOGGLE_THEME 0x1200

#define WM_APP_GENERATE_COMPLETE (WM_APP + 1)

#define MAX_SOLUTION_STEPS (BOARD_ROWS * BOARD_COLS)
#define MAX_LEGAL_MOVES (BOARD_ROWS * BOARD_COLS * 4)
#define HASH_CAPACITY 131071
#define SOLVE_NODE_LIMIT 600000

#define kDarkSquareColor RGB(67, 98, 39)
#define kLightSquareColor RGB(89, 125, 51)

// memset and memcpy intrinsics to reduce the code size

#if defined(USE_MEM_INTRINSICS)
#define ZeroMem(ptr, size) __stosb((unsigned char *)(ptr), 0, (size_t)(size))
#define CopyMem(dest, src, size) __movsb((unsigned char *)(dest), (const unsigned char *)(src), (size_t)(size))
#else
#define ZeroMem(ptr, size) memset((ptr), 0, (size))
#define CopyMem(dest, src, size) memcpy((dest), (src), (size))
#endif

#define SAFE_RELEASE(ptr) do { if ((ptr) != NULL) { (ptr)->lpVtbl->Release(ptr); (ptr) = NULL; } } while (0)

// Reduce code size of string literals by packing them into a single array
// and using pointers to reference them.
// This also ensures they are stored in a read-only section of the binary.
#define __packed __declspec(align(1))
#define __packedstr(var, value) __packed static const char var[] = value
#define __packedstrw(var, value) __packed static const WCHAR var[] = value

__packedstr(kEmptyString, "");

__packedstr(FLING_MAINWINDOWCLASS, "FlingDX11");
__packedstr(FLING_BOARDCLASS, "FlingBoardDX11");

__packedstr(kAppTitleText, "Fling in C - DirectX 11 Edition");
__packedstr(kCaptionGenerate, "Generate");
__packedstr(kCaptionClear, "Clear");
__packedstr(kCaptionSavePicture, "Save Picture");
__packedstr(kCaptionRestore, "Restore to initial");
__packedstr(kCaptionSolve, "Solve");
__packedstr(kCaptionPrevious, "Previous");
__packedstr(kCaptionNext, "Next");
__packedstr(kCaptionPlay, "Play");
__packedstr(kCaptionStop, "Stop");
__packedstr(kInfoCaption, "Information");
__packedstr(kSolvedText, "Solved.");
__packedstr(kMoveAppliedText, "Move applied. %d pieces remain.");
__packedstr(kNoValidMoveText, "No valid move.");
__packedstr(kCannotFindSolutionText, "Cannot find a solution.");
__packedstr(kViewingFinalStepText, "Viewing solution final step %d.");
__packedstr(kViewingInitialText, "Viewing the initial position.");
__packedstr(kViewingSolutionStepText, "Viewing solution step %d of %d.");
__packedstr(kAnimatingStepText, "Animating step %d of %d...");
__packedstr(kInvalidMoveText, "Invalid move: cannot fling adjacent pieces.");
__packedstr(kGeneratedBoardText, "Generated a board with %d pieces.");
__packedstr(kGeneratingBoardText, "Generating a board with %d pieces...");
__packedstr(kBoardSolvedText, "Board solved.");
__packedstr(kBoardUpdatedText, "Board updated. %d pieces on the board.");
__packedstr(kGenerateLevelSetText, "Level set to %d.");
__packedstr(kBoardLockedText, "Board locked. Use Generate or Clear for a new setup.");
__packedstr(kBoardClearedText, "Board cleared.");
__packedstr(kBoardRestoredText, "Board restored to its initial setup.");
__packedstr(kPictureSavedText, "Saved board picture to %s.");
__packedstr(kSavePictureFailedText, "Couldn't save the board picture.");
__packedstr(kUnsupportedPictureFormatText, "Choose a file name");
__packedstr(kSavePictureDialogTitle, "Save board picture");
__packedstr(kSavePictureDialogFilter, "PNG Image (*.png)\0*.png\0JPEG Image (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0Bitmap Image (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0");
__packedstr(kConfirmGenerateBoardText, "Board changed. Generate a new board and discard the changes?");
__packedstr(kConfirmClearBoardText, "Board changed. Discard the changes?");
__packedstr(kConfirmQuitText, "Board changed. Quit and discard the changes?");
__packedstr(kInfoTextFormat,
    "Written by [ REDACTED ] who shall not be named.\n\n"
    "Pieces on board: %d\n"
    "Total: %d\n\n"
    "- Click a square to add or remove a piece.\n"
    "- Drag a piece up, down, left, or right to fling it.\n"
    "- Press Q to quit.\n"
    "\n"
    "%s");
__packedstr(kGeneratingOverlayText, "Generating board ...");
__packedstr(kSolvedOverlayText, "Board solved!");
__packedstr(kSettingsFolderName, "FlingDX11");
__packedstr(kSettingsFileName, "FlingDX11.ini");
__packedstr(kSettingsSection, "Settings");
__packedstr(kThemeRegistryPath, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
__packedstr(kAppsUseLightThemeValue, "AppsUseLightTheme");
__packedstr(kDarkTheme, "DarkTheme");
__packedstr(kMenuSwitchToDarkTheme, "Switch to &Dark Theme");
__packedstr(kMenuSwitchToLightTheme, "Switch to &Light Theme");
__packedstr(kMenuUseSystemTheme, "Use &System Theme");
__packedstr(kInfoHoverText,
    "Congratulations! You have found a hidden treasure:\n\nAn ancient Chinese calligraphy scroll!\n\n"
    "Let me translate it for you:\n\nThis program is written by jdp1024.");
__packedstr(kNumFmt, "%d");

// high 4 bits are MIDI note (0-127)
// low 4 bits are duration in multiples of MIDI_VICTORY_NOTE_MS
static __packed const unsigned char g_odeToJoyMidi[] = {
    0x42, 0x42, 0x52, 0x72, 0x72, 0x52, 0x42, 0x22, 0x02, 0x02, 0x22, 0x42, 0x44, 0x21, 0x24,
    0x42, 0x42, 0x52, 0x72, 0x72, 0x52, 0x42, 0x22, 0x02, 0x02, 0x22, 0x42, 0x24, 0x01, 0x04
};

typedef struct _Fling_Move {
    int sr;
    int sc;
    int dr;
    int dc;
} Fling_Move;

typedef struct _Fling_BoardState {
    int cells[BOARD_ROWS][BOARD_COLS];
    int pieceCount;
    int nextPieceId;
} Fling_BoardState;

typedef struct _Fling_SolveState {
    uint64_t bitsLow;
    uint64_t bitsHigh;
    int pieceCount;
} Fling_SolveState;

typedef struct _Fling_HashEntry {
    uint64_t bitsLow;
    uint64_t bitsHigh;
    unsigned char used;
} Fling_HashEntry;

typedef enum _Fling_AnimationMode {
    ANIMATION_NONE = 0,
    ANIMATION_SOLUTION,
    ANIMATION_USER_MOVE
} Fling_AnimationMode;

typedef struct _Fling_AnimationSegment {
    int pieceId;
    int startRow;
    int startCol;
    int endRow;
    int endCol;
    int finalRow;
    int finalCol;
} Fling_AnimationSegment;

typedef struct _Fling_DxColor {
    float r;
    float g;
    float b;
    float a;
} Fling_DxColor;

typedef struct _Fling_BoardVertex {
    float x;
    float y;
    float u;
    float v;
    float useTexture;
    float textureIndex;
    float r;
    float g;
    float b;
    float a;
} Fling_BoardVertex;

typedef struct _Fling_GenerateTask {
    HWND hwnd;
    int targetPieces;
} Fling_GenerateTask;

typedef struct _Fling_GenerateResult {
    int succeeded;
    int requestedPieces;
} Fling_GenerateResult;

typedef struct _Fling_AppSettings {
    int windowX;
    int windowY;
    int windowWidth;
    int windowHeight;
    int windowMaximized;
    int showTitleBar;
    int difficulty;
    int useDarkTheme;
    BOOL hasThemePreference;
} Fling_AppSettings;

/*

NOTE
----
All global variables should be declared here, leaving the values to zeros
or uninitialized. You should initialize them in WinMain.

We need to create a zero-sized .data section in the executable file for the
global variables to reduce the code size, and avoid the compiler optimizing
them into registers or function-local statics, which can increase the code
size due to more instructions needed to access them. By declaring them as
static at the file scope, they will be stored in the .data section of the
binary and can be accessed with a single instruction using their address.

*/

static COLORREF g_pieceColors[MAX_LOADING_BALLS];
static int g_pieceColorCount = 0;
static float g_pieceColorSeed = 0.0f;

extern int _fltused = 0;

static COLORREF g_windowBackgroundColor;
static COLORREF g_titleTextColor;
static COLORREF g_infoTextColor;

static BOOL g_useDarkTheme = FALSE;

static HINSTANCE g_instance = NULL;
static HANDLE g_comCtlActivationContext = 0;
static ULONG_PTR g_comCtlActivationCookie = 0;
static HWND g_buttonRandom = NULL;
static HWND g_buttonClear = NULL;
static HWND g_buttonSavePicture = NULL;
static HWND g_buttonRestore = NULL;
static HWND g_buttonSolve = NULL;
static HWND g_buttonPrevious = NULL;
static HWND g_buttonNext = NULL;
static HWND g_buttonPlay = NULL;
static HWND g_comboDifficulty = NULL;
static HWND g_boardWindow = NULL;

static ID3D11Device *g_d3dDevice = NULL;
static ID3D11DeviceContext *g_d3dContext = NULL;
static IDXGISwapChain *g_d3dSwapChain = NULL;
static ID3D11RenderTargetView *g_d3dRenderTargetView = NULL;
static ID3D11VertexShader *g_d3dVertexShader = NULL;
static ID3D11PixelShader *g_d3dPixelShader = NULL;
static ID3D11InputLayout *g_d3dInputLayout = NULL;
static ID3D11Buffer *g_d3dVertexBuffer = NULL;
static ID3D11ShaderResourceView *g_d3dLightSquareTextureView = NULL;
static ID3D11ShaderResourceView *g_d3dDarkSquareTextureView = NULL;
static ID3D11ShaderResourceView *g_d3dGridLineTextureView = NULL;
static ID3D11ShaderResourceView *g_d3dLoadingTextTextureView = NULL;
static ID3D11SamplerState *g_d3dSamplerState = NULL;
static ID3D11BlendState *g_d3dBlendState = NULL;

static IUIAnimationManager *g_animationManager = NULL;
static IUIAnimationTimer *g_animationTimer = NULL;
static IUIAnimationTransitionLibrary *g_transitionLibrary = NULL;
static IUIAnimationVariable *g_animationProgressVariable = NULL;

static Fling_BoardState g_board;
static Fling_BoardState g_initialBoard;
static BOOL g_hasInitialBoard = FALSE;
static BOOL g_initialBoardLocked = FALSE;
static Fling_BoardState g_solutionStates[MAX_SOLUTION_STEPS + 1];
static Fling_Move g_solution[MAX_SOLUTION_STEPS];
static int g_solutionLength = 0;
static int g_solutionStep = 0;
static BOOL g_isAnimating = FALSE;
static BOOL g_solutionReady = FALSE;
static BOOL g_solutionAutoPlaying = FALSE;
static int g_animationPieceId = 0;
static int g_animationOriginPieceId = 0;
static int g_animationStartRow = 0;
static int g_animationStartCol = 0;
static int g_animationEndRow = 0;
static int g_animationEndCol = 0;
static float g_animationProgress = 0.0f;
static DWORD g_animationSegmentStartTime = 0;
static int g_animationSegmentDurationMs = 0;
static int g_animationTargetStep = 0;
static Fling_AnimationMode g_animationMode = ANIMATION_NONE;
static BOOL g_animationMovingForward = FALSE;
static Fling_BoardState g_animationResultBoard;
static int g_collisionAnimationPieceId = 0;
static int g_collisionStartRow = 0;
static int g_collisionStartCol = 0;
static int g_collisionEndRow = 0;
static int g_collisionEndCol = 0;
static BOOL g_moveMidiTriggeredForAnimation = FALSE;
static Fling_AnimationSegment g_animationSegments[MAX_SOLUTION_STEPS];
static Fling_BoardState g_animationPreviewStates[MAX_SOLUTION_STEPS + 1];
static int g_animationSegmentCount = 0;
static int g_animationSegmentIndex = 0;

static BOOL g_mouseDown = FALSE;
static POINT g_mouseStart = {0, 0};
static int g_mouseRow;
static int g_mouseCol;

static Fling_HashEntry g_seen[HASH_CAPACITY];
static unsigned int g_nodesVisited = 0;
static BOOL g_solverAborted = FALSE;
static char g_statusText[1024];
static UINT g_currentDpi = 0;
static int g_cellSize = 0;
static int g_boardMargin = 0;
static int g_panelWidth = 0;
static HFONT g_uiFont = NULL;
static HFONT g_titleFont = NULL;
static BOOL g_boardLocked = FALSE;
static BOOL g_isGeneratingBoard = FALSE;
static BOOL g_infoHighlightHoverActive = FALSE;
static BOOL g_infoTextMouseTracking = FALSE;
static HMIDIOUT g_midiOut = NULL;
static DWORD g_backgroundMidiStartTick = 0;
static DWORD g_backgroundMidiEventTick = 0;
static UINT g_backgroundMidiBitIndex = 0;
static BYTE g_moveMelodyNotes[MOVE_MIDI_MAX_NOTES];
static UINT g_moveMelodyDurations[MOVE_MIDI_MAX_NOTES];
static UINT g_moveMelodyLength = 0;
static UINT g_moveMelodyIndex = 0;
static int g_moveCurrentNote = 0;
static DWORD g_moveMidiPlaybackEndTick = 0;
static UINT g_victoryMelodyIndex = 0;
static int g_victoryCurrentNote = 0;
static BOOL g_victoryMidiStartPending = FALSE;
static BOOL g_victoryDelayArmed = FALSE;
static BOOL g_boardSolvedCelebrated = FALSE;
static BOOL g_showSolvedOverlay = FALSE;
static DWORD g_solvedOverlayStartTick = 0;
static BOOL g_victoryFireworksActive = FALSE;
static DWORD g_victoryFireworksStartTick = 0;

// There is only one generate task and result at a time, so we can use
// static variables for them instead of dynamically allocating memory.
// This also avoids the need for synchronization since the main thread
// will only read the result after the generate thread has completed
// and posted the result message.
static Fling_GenerateTask g_generateTask;
static Fling_GenerateResult g_generateResult;

static HANDLE g_generateThread = NULL;
static DWORD g_generateAnimationStartTick = 0;
static DWORD g_generateAnimationLastTick = 0;
static BOOL g_loadingAnimationInitialized = FALSE;
static BOOL g_loadingBallsSettling = FALSE;
static DWORD g_loadingBallSettleStartTick = 0;
static int g_loadingBallCount = 0;
static int g_loadingBallPieceIds[MAX_LOADING_BALLS];
static float g_loadingBallPosX[MAX_LOADING_BALLS];
static float g_loadingBallPosY[MAX_LOADING_BALLS];
static Fling_AppSettings g_appSettings;
static float g_loadingBallVelX[MAX_LOADING_BALLS];
static float g_loadingBallVelY[MAX_LOADING_BALLS];
static float g_loadingBallStartX[MAX_LOADING_BALLS];
static float g_loadingBallStartY[MAX_LOADING_BALLS];
static float g_loadingBallTargetX[MAX_LOADING_BALLS];
static float g_loadingBallTargetY[MAX_LOADING_BALLS];
static int g_generatePieceCount = 0;

static void LayoutControls(HWND hwnd);
static int InBounds(int row, int col);
static float GetScaledAnimationPixelsPerSecond(void);
static void UpdateThemeColors(void);
static void ApplyControlTheme(void);
static void UpdateSystemMenuThemeItem(HWND hwnd);
static BOOL IsPointOverHighlightedInfoText(HWND hwnd, int x, int y);
static int IsFreeWindowAreaPoint(HWND hwnd, int screenX, int screenY);
static void SetWindowTitleBarVisible(HWND hwnd, BOOL visible, BOOL saveSetting);
static void ToggleWindowTitleBar(HWND hwnd);
static int EnsureSettingsFilePath(char *path, size_t pathSize);
static void LoadAppSettings(void);
static void SaveAppSettings(HWND hwnd);
static int BoardHasPendingChanges(void);
static int ConfirmDiscardBoardChanges(HWND hwnd, const char *message);
static void UpdateRestoreButton(void);
static void UpdateSolutionButtons(void);
static void InvalidateBoardArea(HWND hwnd, BOOL erase);
static void InvalidateInfoArea(HWND hwnd);
static void SetStatus(HWND hwnd, const char *fmt, ...);
static int EnsureVictoryMidiOutput(void);
static void StopMoveMidiPlayback(HWND hwnd);
static void AdvanceMoveMidiPlayback(HWND hwnd);
static int StartMoveMidiPlayback(HWND hwnd);
static int StartSadMoveMidiPlayback(HWND hwnd);
static void StopVictoryMidiPlayback(HWND hwnd);
static int StartVictoryMidiPlaybackWhenReady(HWND hwnd);
static void StopVictoryFireworks(HWND hwnd);
static void SetSolvedOverlayVisible(HWND hwnd, BOOL visible);
static void UpdateSolvedCelebration(HWND hwnd);
static void DestroyVictoryMidiOutput(void);
static void WarnIfBoardHasNoSolution(HWND hwnd);
static void GoToSolutionStep(HWND hwnd, int step);
static void StartSolutionStepAnimation(HWND hwnd, int targetStep);
static void StartSolutionPlaybackFromBeginning(HWND hwnd);
static void StopSolutionAutoPlay(HWND hwnd);
static int AdvanceAnimationSequence(HWND hwnd);
static void GetBoardControlRect(RECT* rc);
static void GetBoardRect(RECT* rc);
static LRESULT CALLBACK FlingBoardWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
static HRESULT EnsureBoardRenderer(HWND hwnd);
static void ResizeBoardRenderer(UINT width, UINT height);
static void DestroyBoardRenderer(void);
static void RenderBoardWindow(void);
static void RenderBoardWindowInternal(BOOL presentSwapChain);
static const char *GetPathExtensionA(const char *path);
static void EnsureSavePicturePathExtension(char *path, size_t pathSize, DWORD filterIndex);
static const GUID *GetWicContainerFormatForPath(const char *path);
static HRESULT SaveBoardImageToFile(const char *path, const GUID *containerFormat);
static void SaveBoardPicture(HWND hwnd);
static HRESULT EnsureWindowsAnimationManager(void);
static HRESULT StartWindowsAnimationSegment(int durationMs);
static int UpdateWindowsAnimationProgress(void);
static void StopWindowsAnimationManager(void);
static void TickAnimation(HWND hwnd);
static void SetBoardGenerating(HWND hwnd, BOOL generating);
static void EnsurePieceColorCount(int count);
static void ResetPieceColors(int count);
static int StartLoadingBallSettlement(void);
static void FinalizeGeneratedBoard(HWND hwnd);
static DWORD WINAPI GenerateBoardThreadProc(LPVOID parameter);

// safer version of rand() that uses rand_s, which is thread-safe and does
// not require calling srand()
static __declspec(noinline) unsigned int randx() {
    unsigned int result;
    rand_s(&result);
    return result;
}

// a "quiet" version of MessageBox that uses MessageBoxIndirect with
// MB_USERICON to avoid the default system sound, and uses custom icons
// for different message types to provide visual feedback without sound.
static int QuietMessageBox(HWND hwnd, LPCTSTR msg, LPCTSTR cap, UINT flags)
{
    MSGBOXPARAMS mbp;

    ZeroMem(&mbp, sizeof(MSGBOXPARAMS));
    mbp.cbSize = sizeof(MSGBOXPARAMS);
    mbp.hwndOwner = hwnd;
    mbp.lpszText = msg;
    mbp.lpszCaption = cap;

    switch (flags & MB_ICONMASK)
    {
    case MB_ICONQUESTION:
        mbp.lpszIcon = IDI_QUESTION;
        break;
    case MB_ICONEXCLAMATION:
        mbp.lpszIcon = IDI_EXCLAMATION;
        break;
    case MB_ICONSTOP:
        mbp.lpszIcon = IDI_ERROR;
        break;
    default:
        mbp.lpszIcon = IDI_INFORMATION;
        break;
    }
    mbp.dwStyle = (flags & ~MB_ICONMASK) | MB_USERICON;
    return MessageBoxIndirect(&mbp);
}

static HRESULT EnsureWindowsAnimationManager(void)
{
    HRESULT hr;
    if (g_animationManager == NULL) {
        hr = CoCreateInstance(
            &CLSID_UIAnimationManager,
            NULL,
            CLSCTX_INPROC_SERVER,
            &IID_IUIAnimationManager,
            (void **)&g_animationManager
        );
    }

    if (g_animationTimer == NULL) {
        hr = CoCreateInstance(
            &CLSID_UIAnimationTimer,
            NULL,
            CLSCTX_INPROC_SERVER,
            &IID_IUIAnimationTimer,
            (void **)&g_animationTimer
        );
    }

    if (g_transitionLibrary == NULL) {
        hr = CoCreateInstance(
            &CLSID_UIAnimationTransitionLibrary,
            NULL,
            CLSCTX_INPROC_SERVER,
            &IID_IUIAnimationTransitionLibrary,
            (void **)&g_transitionLibrary
        );
    }

    if (g_animationProgressVariable == NULL) {
        hr = g_animationManager->lpVtbl->CreateAnimationVariable(g_animationManager, 0.0, &g_animationProgressVariable);
    }

    return S_OK;
}

static HRESULT StartWindowsAnimationSegment(int durationMs)
{
    HRESULT hr = EnsureWindowsAnimationManager();

    hr = g_animationManager->lpVtbl->AbandonAllStoryboards(g_animationManager);

    UI_ANIMATION_SECONDS timeNow = 0.0;
    hr = g_animationTimer->lpVtbl->GetTime(g_animationTimer, &timeNow);

    IUIAnimationStoryboard *storyboard;
    hr = g_animationManager->lpVtbl->CreateStoryboard(g_animationManager, &storyboard);

    IUIAnimationTransition *resetTransition;
    hr = g_transitionLibrary->lpVtbl->CreateInstantaneousTransition(g_transitionLibrary, 0.0, &resetTransition);
    hr = storyboard->lpVtbl->AddTransition(storyboard, g_animationProgressVariable, resetTransition);

    IUIAnimationTransition *moveTransition;
    hr = g_transitionLibrary->lpVtbl->CreateLinearTransition(
        g_transitionLibrary,
        (DOUBLE)durationMs / 1000.0,
        1.0,
        &moveTransition
    );
    hr = storyboard->lpVtbl->AddTransition(storyboard, g_animationProgressVariable, moveTransition);
    hr = storyboard->lpVtbl->Schedule(storyboard, timeNow, NULL);
    hr = g_animationManager->lpVtbl->Update(g_animationManager, timeNow, NULL);

    SAFE_RELEASE(moveTransition);
    SAFE_RELEASE(resetTransition);
    SAFE_RELEASE(storyboard);
    return hr;
}

static int UpdateWindowsAnimationProgress(void)
{
    UI_ANIMATION_SECONDS timeNow = 0.0;
    DOUBLE progressValue = 0.0;
    HRESULT hr;

    hr = g_animationTimer->lpVtbl->GetTime(g_animationTimer, &timeNow);
    hr = g_animationManager->lpVtbl->Update(g_animationManager, timeNow, NULL);
    hr = g_animationProgressVariable->lpVtbl->GetValue(g_animationProgressVariable, &progressValue);

    if (progressValue < 0.0) {
        progressValue = 0.0;
    }
    if (progressValue > 1.0) {
        progressValue = 1.0;
    }

    g_animationProgress = (float)progressValue;
    return 1;
}

static int ShouldTriggerCollisionMoveMidi(Fling_AnimationMode mode)
{
    return !g_moveMidiTriggeredForAnimation &&
        (mode == ANIMATION_USER_MOVE || mode == ANIMATION_SOLUTION) &&
        g_animationMovingForward &&
        g_animationSegmentIndex == 0 &&
        g_collisionAnimationPieceId != 0 &&
        g_animationProgress >= ANIMATION_COLLISION_SOUND_TRIGGER_PROGRESS;
}

static void StopWindowsAnimationManager(void)
{
    if (g_animationManager != NULL) {
        g_animationManager->lpVtbl->AbandonAllStoryboards(g_animationManager);
    }

    SAFE_RELEASE(g_animationProgressVariable);
    SAFE_RELEASE(g_transitionLibrary);
    SAFE_RELEASE(g_animationTimer);
    SAFE_RELEASE(g_animationManager);
}

static void TickAnimation(HWND hwnd)
{
    Fling_AnimationMode completedMode;
    int completedTargetStep;

    if (!g_isAnimating) {
        return;
    }

    completedMode = g_animationMode;
    completedTargetStep = g_animationTargetStep;

    if (!UpdateWindowsAnimationProgress()) {
        DWORD now = (DWORD)GetMessageTime();
        DWORD elapsed = now - g_animationSegmentStartTime;

        if (g_animationSegmentDurationMs < 1) {
            g_animationSegmentDurationMs = 1;
        }

        g_animationProgress = (float)elapsed / (float)g_animationSegmentDurationMs;
        if (g_animationProgress > 1.0f) {
            g_animationProgress = 1.0f;
        }
    }

    if (ShouldTriggerCollisionMoveMidi(completedMode)) {
        StartMoveMidiPlayback(hwnd);
        g_moveMidiTriggeredForAnimation = TRUE;
    }

    if (g_animationProgress >= 1.0f) {
        if (AdvanceAnimationSequence(hwnd)) {
            return;
        }

        g_isAnimating = FALSE;
        g_animationPieceId = 0;
        g_animationOriginPieceId = 0;
        g_collisionAnimationPieceId = 0;
        g_animationMode = ANIMATION_NONE;
        g_animationMovingForward = TRUE;
        g_animationSegmentCount = 0;
        g_animationSegmentIndex = 0;
        UpdateSolutionButtons();

        if (completedMode == ANIMATION_SOLUTION) {
            GoToSolutionStep(hwnd, completedTargetStep);
        } else if (completedMode == ANIMATION_USER_MOVE) {
            g_board = g_animationResultBoard;
            if (g_board.pieceCount == 1) {
                SetStatus(hwnd, kSolvedText);
            } else {
                SetStatus(hwnd, kMoveAppliedText, g_board.pieceCount);
            }
            UpdateSolvedCelebration(hwnd);
            WarnIfBoardHasNoSolution(hwnd);
            InvalidateBoardArea(hwnd, FALSE);
            InvalidateInfoArea(hwnd);
        }
    } else {
        InvalidateBoardArea(hwnd, FALSE);
    }
}

static float HashBoardTextureNoise()
{
    return (float)(randx() & 0x3ff) / 1023.0f;
}

static void UpdateSolutionButtons(void)
{
    int showState = g_solutionReady ? SW_SHOW : SW_HIDE;

    ShowWindow(g_buttonPrevious, showState);
    EnableWindow(g_buttonPrevious, g_solutionReady && !g_isGeneratingBoard && !g_isAnimating && g_solutionStep > 0);
    ShowWindow(g_buttonNext, showState);
    EnableWindow(g_buttonNext, g_solutionReady && !g_isGeneratingBoard && !g_isAnimating && g_solutionStep < g_solutionLength);
    ShowWindow(g_buttonPlay, showState);
    SetWindowTextA(g_buttonPlay, g_solutionAutoPlaying ? kCaptionStop : kCaptionPlay);
    EnableWindow(g_buttonPlay, !g_isGeneratingBoard && g_solutionReady && (g_solutionAutoPlaying || (!g_isAnimating && g_solutionLength > 0)));

    UpdateRestoreButton();
}

static int BoardsEqual(const Fling_BoardState *left, const Fling_BoardState *right)
{
    if (left->pieceCount != right->pieceCount || left->nextPieceId != right->nextPieceId) {
        return 0;
    }

    for (int row = 0; row < BOARD_ROWS; ++row) {
        for (int col = 0; col < BOARD_COLS; ++col) {
            if (left->cells[row][col] != right->cells[row][col]) {
                return 0;
            }
        }
    }

    return 1;
}

static int BoardHasPendingChanges(void)
{
    if (g_hasInitialBoard) {
        return !BoardsEqual(&g_board, &g_initialBoard) || g_boardLocked != g_initialBoardLocked;
    }

    return g_board.pieceCount > 0 || g_boardLocked;
}

static int ConfirmDiscardBoardChanges(HWND hwnd, const char *message)
{
    if (!BoardHasPendingChanges()) {
        return 1;
    }

    return QuietMessageBox(
        hwnd,
        message,
        kInfoCaption,
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2
    ) == IDYES;
}

static __declspec(noinline) void UpdateRestoreButton(void)
{
    EnableWindow(
        g_buttonRestore,
        !g_isGeneratingBoard && !g_solutionAutoPlaying && g_hasInitialBoard && !g_isAnimating && (!BoardsEqual(&g_board, &g_initialBoard) || g_boardLocked != g_initialBoardLocked)
    );
}

static void CaptureInitialBoard(BOOL locked)
{
    g_initialBoard = g_board;
    g_hasInitialBoard = TRUE;
    g_initialBoardLocked = locked;
    UpdateRestoreButton();
}

static void ClearInitialBoard(void)
{
    ZeroMem(&g_initialBoard, sizeof(g_initialBoard));
    g_hasInitialBoard = FALSE;
    g_initialBoardLocked = FALSE;
    UpdateRestoreButton();
}

static int FindPiecePosition(const Fling_BoardState *board, int pieceId, int *outRow, int *outCol)
{
    for (int row = 0; row < BOARD_ROWS; ++row) {
        for (int col = 0; col < BOARD_COLS; ++col) {
            if (board->cells[row][col] == pieceId) {
                *outRow = row;
                *outCol = col;
                return 1;
            }
        }
    }
    return 0;
}

static void ApplyAnimationSegmentResult(Fling_BoardState *board, const Fling_AnimationSegment *segment)
{
    if (InBounds(segment->startRow, segment->startCol) && board->cells[segment->startRow][segment->startCol] == segment->pieceId) {
        board->cells[segment->startRow][segment->startCol] = 0;
    }

    if (InBounds(segment->finalRow, segment->finalCol)) {
        board->cells[segment->finalRow][segment->finalCol] = segment->pieceId;
    } else if (board->pieceCount > 0) {
        board->pieceCount--;
    }
}

static int BuildAnimationSegments(const Fling_BoardState *startBoard, Fling_Move move, const Fling_BoardState *finalBoard)
{
    Fling_BoardState preview;
    int currentRow = move.sr;
    int currentCol = move.sc;
    int currentPieceId;
    int segmentCount = 0;

    if (startBoard == NULL || !InBounds(currentRow, currentCol)) {
        return 0;
    }

    currentPieceId = startBoard->cells[currentRow][currentCol];
    if (currentPieceId == 0) {
        return 0;
    }

    preview = *startBoard;
    g_animationPreviewStates[0] = preview;

    while (segmentCount < MAX_SOLUTION_STEPS) {
        int row = currentRow + move.dr;
        int col = currentCol + move.dc;
        int stopRow = currentRow;
        int stopCol = currentCol;

        while (InBounds(row, col) && startBoard->cells[row][col] == 0) {
            stopRow = row;
            stopCol = col;
            row += move.dr;
            col += move.dc;
        }

        Fling_AnimationSegment segment;
        segment.pieceId = currentPieceId;
        segment.startRow = currentRow;
        segment.startCol = currentCol;
        segment.endRow = row;
        segment.endCol = col;

        if (InBounds(row, col)) {
            segment.finalRow = stopRow;
            segment.finalCol = stopCol;
        } else {
            segment.finalRow = row;
            segment.finalCol = col;
        }

        ApplyAnimationSegmentResult(&preview, &segment);

        if (!InBounds(segment.finalRow, segment.finalCol) ||
            segment.finalRow != segment.startRow ||
            segment.finalCol != segment.startCol) {
            g_animationSegments[segmentCount] = segment;
            g_animationPreviewStates[segmentCount + 1] = preview;
            segmentCount++;
        }

        if (!InBounds(row, col)) {
            break;
        }

        currentRow = row;
        currentCol = col;
        currentPieceId = startBoard->cells[currentRow][currentCol];
        if (currentPieceId == 0) {
            break;
        }
    }

    if (finalBoard != NULL) {
        g_animationResultBoard = *finalBoard;
    } else if (segmentCount > 0) {
        g_animationResultBoard = g_animationPreviewStates[segmentCount];
    } else {
        ZeroMem(&g_animationResultBoard, sizeof(g_animationResultBoard));
    }

    return segmentCount;
}

static int ComputeAnimationSegmentDurationMs(const Fling_AnimationSegment *segment)
{
    if (segment == NULL) {
        return ANIMATION_MIN_DURATION_MS;
    }

    int distanceCells = abs(segment->finalRow - segment->startRow) + abs(segment->finalCol - segment->startCol);
    if (distanceCells < 1) {
        distanceCells = 1;
    }

    int distancePixels = distanceCells * g_cellSize;
    float pixelsPerSecond = GetScaledAnimationPixelsPerSecond();
    if (pixelsPerSecond < 1.0f) {
        pixelsPerSecond = 1.0f;
    }
    int durationMs = (int)((float)distancePixels * 1000.0f / pixelsPerSecond + 0.5f);

    if (durationMs < ANIMATION_MIN_DURATION_MS) {
        durationMs = ANIMATION_MIN_DURATION_MS;
    }

    return durationMs;
}

static void LoadAnimationSegment(int segmentIndex)
{
    const Fling_AnimationSegment *segment = &g_animationSegments[segmentIndex];
    const Fling_BoardState *segmentBoard = &g_animationPreviewStates[segmentIndex];

    g_animationPieceId = segment->pieceId;
    g_animationStartRow = segment->startRow;
    g_animationStartCol = segment->startCol;
    g_animationEndRow = segment->finalRow;
    g_animationEndCol = segment->finalCol;
    g_collisionAnimationPieceId = 0;
    g_collisionStartRow = 0;
    g_collisionStartCol = 0;
    g_collisionEndRow = 0;
    g_collisionEndCol = 0;
    g_moveMidiTriggeredForAnimation = FALSE;

    if (InBounds(segment->endRow, segment->endCol)) {
        g_collisionAnimationPieceId = segmentBoard->cells[segment->endRow][segment->endCol];
        if (g_collisionAnimationPieceId != 0) {
            g_collisionStartRow = segment->endRow;
            g_collisionStartCol = segment->endCol;
            g_collisionEndRow = segment->endRow;
            g_collisionEndCol = segment->endCol;
        }
    }

    if ((g_animationMode == ANIMATION_USER_MOVE || g_animationMode == ANIMATION_SOLUTION) &&
        g_animationMovingForward &&
        segmentIndex == 0 &&
        g_collisionAnimationPieceId != 0) {
        EnsureVictoryMidiOutput();
    }

    g_animationProgress = 0.0f;
    g_animationSegmentDurationMs = ComputeAnimationSegmentDurationMs(segment);
    g_animationSegmentStartTime = (DWORD)GetMessageTime();

    if (SUCCEEDED(StartWindowsAnimationSegment(g_animationSegmentDurationMs))) {
        g_animationSegmentStartTime = 0;
    }
}

static int StartMoveAnimationSequence(HWND hwnd, const Fling_BoardState *startBoard, const Fling_BoardState *finalBoard, Fling_Move move, Fling_AnimationMode mode, int targetStep, BOOL movingForward)
{
    g_animationSegmentCount = BuildAnimationSegments(startBoard, move, finalBoard);
    if (g_animationSegmentCount <= 0) {
        return 0;
    }

    g_animationTargetStep = targetStep;
    g_animationMode = mode;
    g_animationMovingForward = movingForward;
    g_animationOriginPieceId = g_animationSegments[0].pieceId;
    g_isAnimating = TRUE;

    if (movingForward) {
        g_animationSegmentIndex = 0;
        g_board = g_animationPreviewStates[0];
    } else {
        g_animationSegmentIndex = g_animationSegmentCount - 1;
        g_board = g_animationPreviewStates[g_animationSegmentCount];
    }

    LoadAnimationSegment(g_animationSegmentIndex);
    UpdateSolutionButtons();
    InvalidateBoardArea(hwnd, FALSE);
    InvalidateInfoArea(hwnd);
    return 1;
}

static int AdvanceAnimationSequence(HWND hwnd)
{
    if (g_animationSegmentCount <= 0) {
        return 0;
    }

    if (g_animationMovingForward) {
        g_board = g_animationPreviewStates[g_animationSegmentIndex + 1];
        if (g_animationSegmentIndex + 1 < g_animationSegmentCount) {
            g_animationSegmentIndex++;
            LoadAnimationSegment(g_animationSegmentIndex);
            InvalidateBoardArea(hwnd, FALSE);
            InvalidateInfoArea(hwnd);
            return 1;
        }
    } else {
        g_board = g_animationPreviewStates[g_animationSegmentIndex];
        if (g_animationSegmentIndex > 0) {
            g_animationSegmentIndex--;
            LoadAnimationSegment(g_animationSegmentIndex);
            InvalidateBoardArea(hwnd, FALSE);
            InvalidateInfoArea(hwnd);
            return 1;
        }
    }

    return 0;
}

static void SetBoardLocked(BOOL locked)
{
    g_boardLocked = locked;

    EnableWindow(g_buttonRandom, !g_isGeneratingBoard);
    EnableWindow(g_comboDifficulty, !g_isGeneratingBoard);

    UpdateRestoreButton();
}

static void SetBoardGenerating(HWND hwnd, BOOL generating)
{
    g_isGeneratingBoard = generating;

    if (generating) {
        g_generateAnimationStartTick = GetTickCount();
        g_generateAnimationLastTick = 0;
        g_loadingAnimationInitialized = FALSE;
        g_loadingBallsSettling = FALSE;
        g_loadingBallSettleStartTick = 0;
        g_loadingBallCount = g_generateTask.targetPieces;
        if (g_loadingBallCount < 1) {
            g_loadingBallCount = g_generatePieceCount;
        }
        if (g_loadingBallCount < 1) {
            g_loadingBallCount = 1;
        }
        if (g_loadingBallCount > MAX_LOADING_BALLS) {
            g_loadingBallCount = MAX_LOADING_BALLS;
        }

        EnsurePieceColorCount(g_loadingBallCount);
        for (int i = 0; i < g_loadingBallCount; ++i) {
            g_loadingBallPieceIds[i] = i + 1;
        }
        // float values 0
        ZeroMem(g_loadingBallStartX, sizeof(g_loadingBallStartX));
        ZeroMem(g_loadingBallStartY, sizeof(g_loadingBallStartY));
        ZeroMem(g_loadingBallTargetX, sizeof(g_loadingBallTargetX));
        ZeroMem(g_loadingBallTargetY, sizeof(g_loadingBallTargetY));

        SetTimer(hwnd, TIMER_GENERATE_ANIMATION, 16, NULL);
    } else {
        g_generateAnimationLastTick = 0;
        g_loadingAnimationInitialized = FALSE;
        g_loadingBallsSettling = FALSE;
        g_loadingBallSettleStartTick = 0;
        KillTimer(hwnd, TIMER_GENERATE_ANIMATION);
    }

    EnableWindow(g_buttonRandom, !generating);
    EnableWindow(g_comboDifficulty, !generating);
    EnableWindow(g_buttonClear, !generating);
    EnableWindow(g_buttonSavePicture, !generating);
    EnableWindow(g_buttonSolve, !generating && !g_isAnimating);

    UpdateRestoreButton();
    UpdateSolutionButtons();

    if (hwnd != NULL) {
        InvalidateBoardArea(hwnd, FALSE);
        InvalidateInfoArea(hwnd);
    }
}

static inline int ScaleForDpi(int value, UINT dpi)
{
    return value * dpi / USER_DEFAULT_SCREEN_DPI;
}

static int GetDifficultyComboSelectionHeight(UINT dpi)
{
    int controlHeight = ScaleForDpi(32, dpi);
    int edgePadding = GetSystemMetricsForDpi(SM_CYEDGE, dpi) * 2;
    int selectionHeight = controlHeight - edgePadding;

    if (selectionHeight < 1) {
        selectionHeight = 1;
    }
    return selectionHeight;
}

static int GetDifficultyComboItemHeight(UINT dpi)
{
    int selectionHeight = GetDifficultyComboSelectionHeight(dpi);
    int minItemHeight = ScaleForDpi(18, dpi);
    int itemHeight = selectionHeight;

    if (itemHeight < minItemHeight) {
        itemHeight = minItemHeight;
    }
    return itemHeight;
}

static inline float GetScaledAnimationPixelsPerSecond(void)
{
    return (float)(ANIMATION_PIXELS_PER_SECOND * g_currentDpi / USER_DEFAULT_SCREEN_DPI);
}

static float ClampUnitFloat(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float GetCollisionBumpOffset(float animationProgress, int collisionDistanceCells)
{
    float t = ClampUnitFloat((animationProgress - 0.5f) * 2.0f);
    float impactShape = sinf(t * 3.14159265f) * (1.05f - t * 0.25f);
    int gapCells = collisionDistanceCells - 1;
    float proximityBoost;

    if (gapCells < 0) {
        gapCells = 0;
    }

    if (gapCells == 1) {
        proximityBoost = 4.25f;
    } else {
        proximityBoost = 1.25f + 3.0f / (float)(gapCells + 1);
    }

    return impactShape * (float)g_cellSize * ANIMATION_COLLISION_BUMP_SCALE * proximityBoost;
}

static UINT QueryWindowDpi(HWND hwnd)
{
    UINT dpi;

    if (hwnd != NULL) {
        dpi = GetDpiForWindow(hwnd);
        if (dpi != 0) {
            return dpi;
        }
    }

    dpi = GetDpiForSystem();
    return dpi != 0 ? dpi : USER_DEFAULT_SCREEN_DPI;
}

static void ApplyControlFonts(void)
{
    HWND controls[] = { g_buttonRandom, g_comboDifficulty, g_buttonClear, g_buttonSavePicture, g_buttonRestore, g_buttonSolve, g_buttonPrevious, g_buttonNext, g_buttonPlay };
    for (int i = 0; i < (int)_countof(controls); ++i) {
        SendMessageA(controls[i], WM_SETFONT, (WPARAM)g_uiFont, TRUE);
    }
}

__packedstr(kUiFontName, "Segoe UI");

static void RecreateUiFonts(void)
{
    HFONT newUiFont = CreateFontA(
        -9 * (int)g_currentDpi / 72,
        0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        kUiFontName
    );
    HFONT newTitleFont = CreateFontA(
        -13 * (int)g_currentDpi / 72,
        0, 0, 0,
        FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        kUiFontName
    );
    HFONT oldUiFont = g_uiFont;
    HFONT oldTitleFont = g_titleFont;

    g_uiFont = newUiFont;
    g_titleFont = newTitleFont;
    ApplyControlFonts();
    SAFE_RELEASE(g_d3dLoadingTextTextureView);

    if (oldUiFont != NULL) {
        DeleteObject(oldUiFont);
    }
    if (oldTitleFont != NULL) {
        DeleteObject(oldTitleFont);
    }
}

static void UpdateDpiMetrics(HWND hwnd, UINT dpi)
{
    if (dpi == 0) {
        dpi = USER_DEFAULT_SCREEN_DPI;
    }

    g_currentDpi = dpi;
    g_cellSize = ScaleForDpi(CELL_SIZE, dpi);
    g_boardMargin = ScaleForDpi(BOARD_MARGIN, dpi);
    g_panelWidth = ScaleForDpi(PANEL_WIDTH, dpi);
    RecreateUiFonts();

    if (hwnd != NULL) {
        LayoutControls(hwnd);
    }
}

static SIZE GetBoardControlSize(void)
{
    SIZE size;
    size.cx = BOARD_COLS * g_cellSize;
    size.cy = BOARD_ROWS * g_cellSize;
    return size;
}

static SIZE GetDesiredClientSize(void)
{
    SIZE size;
    SIZE boardSize = GetBoardControlSize();
    size.cx = g_boardMargin * 3 + boardSize.cx + g_panelWidth;
    size.cy = g_boardMargin * 2 + boardSize.cy;
    return size;
}

static SIZE GetMinimumClientSize(void)
{
    SIZE size;
    int minCellSize = ScaleForDpi(CELL_SIZE, g_currentDpi);
    int minMargin = ScaleForDpi(BOARD_MARGIN, g_currentDpi);
    int minPanelWidth = ScaleForDpi(PANEL_WIDTH, g_currentDpi);

    size.cx = BOARD_COLS * minCellSize + minMargin * 3 + minPanelWidth;
    size.cy = BOARD_ROWS * minCellSize + minMargin * 2;
    return size;
}

static void GetDesiredWindowRect(RECT *rc, DWORD style, DWORD exStyle)
{
    SIZE clientSize = GetDesiredClientSize();
    UINT dpi = (g_currentDpi != 0) ? g_currentDpi : USER_DEFAULT_SCREEN_DPI;

    rc->left = 0;
    rc->top = 0;
    rc->right = clientSize.cx;
    rc->bottom = clientSize.cy;

    if ((style & WS_CAPTION) != 0) {
        AdjustWindowRectExForDpi(rc, style, FALSE, exStyle, dpi);
    }
}

__packedstr(kAppData, "APPDATA");
__packedstr(kAppDataFmt, "%s\\%s");

static void UpdateThemeColors(void)
{
    if (g_appSettings.hasThemePreference) {
        g_useDarkTheme = g_appSettings.useDarkTheme ? TRUE : FALSE;
    } else {
        DWORD appsUseLightTheme = 1;
        DWORD valueSize = sizeof(appsUseLightTheme);

        if (RegGetValueA(
                HKEY_CURRENT_USER,
                kThemeRegistryPath,
                kAppsUseLightThemeValue,
                RRF_RT_REG_DWORD,
                NULL,
                &appsUseLightTheme,
                &valueSize) == ERROR_SUCCESS) {
            g_useDarkTheme = (appsUseLightTheme == 0);
        } else {
            g_useDarkTheme = FALSE;
        }
    }

    if (g_useDarkTheme) {
        g_windowBackgroundColor = RGB(31, 34, 39);
        g_titleTextColor = RGB(244, 247, 251);
        g_infoTextColor = RGB(217, 223, 230);
    } else {
        g_windowBackgroundColor = RGB(245, 246, 250);
        g_titleTextColor = RGB(25, 25, 25);
        g_infoTextColor = RGB(25, 25, 25);
    }

    ApplyControlTheme();
}

// -------------------------------00000000001111111
// -------------------------------01234567890123456
__packedstrw(kDarkModeExplorer, L"DarkMode_Explorer");
static const WCHAR *kExplorer = kDarkModeExplorer + 11;

static void ApplyControlTheme(void)
{
    HWND buttons[] = {
        g_buttonRandom,
        g_buttonClear,
        g_buttonSavePicture,
        g_buttonRestore,
        g_buttonSolve,
        g_buttonPrevious,
        g_buttonNext,
        g_buttonPlay
    };
    const WCHAR *themeName = g_useDarkTheme ? kDarkModeExplorer : kExplorer;

    for (int i = 0; i < (int)_countof(buttons); ++i) {
        SetWindowTheme(buttons[i], themeName, NULL);
        InvalidateRect(buttons[i], NULL, TRUE);
    }

    COMBOBOXINFO comboInfo;
    ZeroMem(&comboInfo, sizeof(comboInfo));
    comboInfo.cbSize = sizeof(comboInfo);

    SetWindowTheme(g_comboDifficulty, themeName, NULL);
    if (GetComboBoxInfo(g_comboDifficulty, &comboInfo)) {
        if (comboInfo.hwndItem != NULL) {
            SetWindowTheme(comboInfo.hwndItem, themeName, NULL);
        }
        if (comboInfo.hwndList != NULL) {
            SetWindowTheme(comboInfo.hwndList, themeName, NULL);
        }
    }

    InvalidateRect(g_comboDifficulty, NULL, TRUE);
}

static void UpdateSystemMenuThemeItem(HWND hwnd)
{
    HMENU systemMenu = GetSystemMenu(hwnd, FALSE);

    const char *menuText;
    if (!g_appSettings.hasThemePreference) {
        menuText = kMenuSwitchToLightTheme;
    } else if (g_appSettings.useDarkTheme) {
        menuText = kMenuUseSystemTheme;
    } else {
        menuText = kMenuSwitchToDarkTheme;
    }

    ModifyMenuA(
        systemMenu,
        IDM_SYSTEM_TOGGLE_THEME,
        MF_BYCOMMAND | MF_STRING,
        IDM_SYSTEM_TOGGLE_THEME,
        menuText
    );
}

static int IsFreeWindowAreaPoint(HWND hwnd, int screenX, int screenY)
{
    POINT clientPoint = {screenX, screenY};
    RECT clientRect;

    ScreenToClient(hwnd, &clientPoint);
    GetClientRect(hwnd, &clientRect);
    if (!PtInRect(&clientRect, clientPoint)) {
        return 0;
    }

    if (IsPointOverHighlightedInfoText(hwnd, clientPoint.x, clientPoint.y)) {
        return 0;
    }

    HWND child = ChildWindowFromPointEx(hwnd, clientPoint, CWP_SKIPINVISIBLE);
    return child == NULL || child == hwnd;
}

static void SetWindowTitleBarVisible(HWND hwnd, BOOL visible, BOOL saveSetting)
{
    LONG_PTR style = GetWindowLongPtrA(hwnd, GWL_STYLE);
    LONG_PTR newStyle;

    if (visible) {
        newStyle = style | (LONG_PTR)WS_CAPTION;
    } else {
        newStyle = style & ~((LONG_PTR)WS_CAPTION);
    }

    g_appSettings.showTitleBar = visible;

    SetWindowLongPtrA(hwnd, GWL_STYLE, newStyle);
    SetWindowPos(
        hwnd,
        NULL,
        0,
        0,
        0,
        0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
    );

    DWM_WINDOW_CORNER_PREFERENCE cornerPreference = visible ? DWMWCP_DEFAULT : DWMWCP_ROUND;
    HRESULT hr = DwmSetWindowAttribute(
        hwnd,
        DWMWA_WINDOW_CORNER_PREFERENCE,
        &cornerPreference,
        sizeof(cornerPreference)
    );
    // Windows 10 does not support the corner preference attribute.
    if (FAILED(hr) && hr != E_INVALIDARG) {
        //OutputDebugStringA("Fling: failed to set the window corner preference.\n");
    }

    if (saveSetting) {
        SaveAppSettings(hwnd);
    }

    LayoutControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

static void ToggleWindowTitleBar(HWND hwnd)
{
    LONG_PTR style = GetWindowLongPtrA(hwnd, GWL_STYLE);
    SetWindowTitleBarVisible(hwnd, (style & WS_CAPTION) == 0, TRUE);
}

static int EnsureSettingsFilePath(char *path, size_t pathSize)
{
    GetEnvironmentVariableA(kAppData, path, (DWORD)pathSize);

    char folderPath[MAX_PATH];
    sprintf(folderPath, kAppDataFmt, path, kSettingsFolderName);

    CreateDirectoryA(folderPath, NULL);

    sprintf(path, kAppDataFmt, folderPath, kSettingsFileName);
    return 1;
}

__packedstr(kLeft, "Left");
__packedstr(kTop, "Top");
__packedstr(kWidth, "Width");
__packedstr(kHeight, "Height");
__packedstr(kMaximized, "Maximized");
__packedstr(kTitleBarVisible, "TitleBarVisible");
__packedstr(kDifficulty, "Difficulty");

static void LoadAppSettings(void)
{
    char path[MAX_PATH];
    char themeValue[16];

    // g_appSettings set to 0 by global initializer
    g_appSettings.showTitleBar = 1;
    EnsureSettingsFilePath(path, sizeof(path));

    g_appSettings.windowX = GetPrivateProfileIntA(kSettingsSection, kLeft, CW_USEDEFAULT, path);
    g_appSettings.windowY = GetPrivateProfileIntA(kSettingsSection, kTop, CW_USEDEFAULT, path);
    g_appSettings.windowWidth = GetPrivateProfileIntA(kSettingsSection, kWidth, 0, path);
    g_appSettings.windowHeight = GetPrivateProfileIntA(kSettingsSection, kHeight, 0, path);
    g_appSettings.windowMaximized = GetPrivateProfileIntA(kSettingsSection, kMaximized, 0, path);
    g_appSettings.showTitleBar = GetPrivateProfileIntA(kSettingsSection, kTitleBarVisible, 1, path);
    g_appSettings.difficulty = GetPrivateProfileIntA(kSettingsSection, kDifficulty, DEFAULT_GENERATE_PIECE_COUNT, path);

    if (GetPrivateProfileStringA(kSettingsSection, kDarkTheme, kEmptyString, themeValue, (DWORD)sizeof(themeValue), path) > 0) {
        g_appSettings.useDarkTheme = GetPrivateProfileIntA(kSettingsSection, kDarkTheme, 0, path);
        g_appSettings.hasThemePreference = TRUE;
    } else {
        g_appSettings.useDarkTheme = 0;
        g_appSettings.hasThemePreference = FALSE;
    }

    if (g_appSettings.difficulty < 2 || g_appSettings.difficulty > 20) {
        g_appSettings.difficulty = DEFAULT_GENERATE_PIECE_COUNT;
    }
    g_generatePieceCount = g_appSettings.difficulty;
}

static void SaveAppSettings(HWND hwnd)
{
    char path[MAX_PATH];
    EnsureSettingsFilePath(path, sizeof(path));

    WINDOWPLACEMENT placement;
    ZeroMem(&placement, sizeof(placement));
    placement.length = sizeof(placement);
    GetWindowPlacement(hwnd, &placement);

    RECT windowRect = placement.rcNormalPosition;
    int isMaximized = (placement.showCmd == SW_SHOWMAXIMIZED)
        || (placement.showCmd == SW_SHOWMINIMIZED && (placement.flags & WPF_RESTORETOMAXIMIZED) != 0);

    if (!isMaximized && placement.showCmd != SW_SHOWMINIMIZED) {
        GetWindowRect(hwnd, &windowRect);
    }

    char value[32];
    sprintf(value, kNumFmt, windowRect.left);
    WritePrivateProfileStringA(kSettingsSection, kLeft, value, path);
    sprintf(value, kNumFmt, windowRect.top);
    WritePrivateProfileStringA(kSettingsSection, kTop, value, path);
    sprintf(value, kNumFmt, (windowRect.right - windowRect.left));
    WritePrivateProfileStringA(kSettingsSection, kWidth, value, path);
    sprintf(value, kNumFmt, (windowRect.bottom - windowRect.top));
    WritePrivateProfileStringA(kSettingsSection, kHeight, value, path);
    sprintf(value, kNumFmt, isMaximized ? 1 : 0);
    WritePrivateProfileStringA(kSettingsSection, kMaximized, value, path);
    g_appSettings.showTitleBar = (GetWindowLongPtrA(hwnd, GWL_STYLE) & WS_CAPTION);
    sprintf(value, kNumFmt, g_appSettings.showTitleBar);
    WritePrivateProfileStringA(kSettingsSection, kTitleBarVisible, value, path);
    sprintf(value, kNumFmt, g_generatePieceCount);
    WritePrivateProfileStringA(kSettingsSection, kDifficulty, value, path);

    if (g_appSettings.hasThemePreference) {
        sprintf(value, kNumFmt, g_appSettings.useDarkTheme ? 1 : 0);
        WritePrivateProfileStringA(kSettingsSection, kDarkTheme, value, path);
    } else {
        WritePrivateProfileStringA(kSettingsSection, kDarkTheme, NULL, path);
    }
}

static int InBounds(int row, int col)
{
    return row >= 0 && row < BOARD_ROWS && col >= 0 && col < BOARD_COLS;
}

static inline int CellIndex(int row, int col)
{
    return row * BOARD_COLS + col;
}

static void GetBoardControlRect(RECT* rc)
{
    SIZE boardSize = GetBoardControlSize();

    rc->left = g_boardMargin;
    rc->top = g_boardMargin;
    rc->right = rc->left + boardSize.cx;
    rc->bottom = rc->top + boardSize.cy;
}

static void GetBoardRect(RECT *rc)
{
    rc->left = 0;
    rc->top = 0;
    rc->right = BOARD_COLS * g_cellSize;
    rc->bottom = BOARD_ROWS * g_cellSize;
}

static RECT GetInfoTextRect(HWND hwnd)
{
    RECT board;
    GetBoardControlRect(&board);
    int textMargin = ScaleForDpi(20, g_currentDpi);
    int controlTop = board.top + ScaleForDpi(20, g_currentDpi);
    int controlHeight = ScaleForDpi(32, g_currentDpi);
    int controlGap = ScaleForDpi(12, g_currentDpi);
    int navTop = controlTop + 5 * (controlHeight + controlGap);
    RECT textRect;

    RECT client;
    GetClientRect(hwnd, &client);
    textRect.left = board.right + textMargin;
    textRect.top = navTop + controlHeight + ScaleForDpi(18, g_currentDpi);
    textRect.right = client.right - textMargin;
    textRect.bottom = client.bottom - textMargin;
    return textRect;
}

static void InvalidateBoardArea(HWND hwnd, BOOL erase)
{
    if (g_boardWindow != NULL) {
        InvalidateRect(g_boardWindow, NULL, erase);
    } else if (hwnd != NULL) {
        InvalidateRect(hwnd, NULL, erase);
    }
}

static void InvalidateInfoArea(HWND hwnd)
{
    RECT textRect = GetInfoTextRect(hwnd);
    InvalidateRect(hwnd, &textRect, FALSE);
}

static void SetStatus(HWND hwnd, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsprintf(g_statusText, fmt, args);
    va_end(args);

    InvalidateInfoArea(hwnd);
}

static inline void SendMidiMessage(BYTE status, BYTE data1, BYTE data2)
{
    DWORD message = (DWORD)status | ((DWORD)data1 << 8) | ((DWORD)data2 << 16);
    midiOutShortMsg(g_midiOut, message);
}

static int EnsureVictoryMidiOutput(void)
{
    if (g_midiOut != NULL) {
        return 1;
    }

    if (midiOutOpen(&g_midiOut, MIDI_MAPPER, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        g_midiOut = NULL;
        return 0;
    }

    SendMidiMessage(0xC0, 0, 0);
    SendMidiMessage(0xB0, 7, 96);
    return 1;
}

// Mutopia Project four-part score data (CC BY 4.0): 3-bit delta index and
// one packed part/note byte per event, bit-packed to 11 bits per event.
static __packed const BYTE kBackgroundMidiNotes[] = {
    0x26, 0x2A, 0x2B, 0x2D, 0x2F, 0x32, 0x37, 0x39, 0x3B, 
    0x3D, 0x3E, 0x40, 0x42, 0x43, 0x45, 0x47, 0x48, 0x49, 
    0x4A, 0x4C, 0x4E, 0x4F, 0x51, 0x53, 0x54, 0x55, 0x56
};
static __packed const WORD kBackgroundMidiDeltaTicks[] = {0, 48, 96, 192, 288, 384};
static __packed const BYTE kBackgroundMidiEvents[] =
    "\x1C\xB7\x14\x71\xDC\x31\xC9\x71\x07\x0D\xC1\x1C\x57\x08\x70\x5C\x01\xC5\x70\x87\x1D\xC3\x06\x83\x96\x8A\x0C\x50\x66\x38\xE8\x98"
    "\xC3\x06\x43\x92\x89\x0C\x40\x62\x38\x68\x88\xC1\x05\xE3\x8A\x87\x8C\x20\x5C\x38\x28\x70\xC0\x05\xE3\x8A\x87\x8C\x20\x62\x38\xE8"
    "\x88\xC3\x06\x41\xD0\x72\xD1\x20\xA8\x31\x41\x88\x73\x1C\x74\x44\x29\x8C\x30\x5E\x1C\x87\x25\x0F\x0A\x43\x10\x17\x07\x11\xC3\x43"
    "\x82\x88\xC1\x05\xA1\xBC\x71\x50\xD0\x9E\x30\x81\x60\x6E\x1C\x14\x30\x27\x0C\x00\x5A\x1B\xC7\x15\x0D\x09\xE3\x08\x15\x87\x11\xC7"
    "\x42\xC2\x88\xC3\x05\x41\xC8\x5A\x0E\x56\x14\x0B\x18\x60\x52\x12\x83\x14\x17\x07\x11\x66\x38\xD8\x70\x2D\x61\xA1\x44\x49\x8C\x30"
    "\x58\x1B\xC5\x90\xE4\x61\x80\xA9\x85\x04\xF1\x24\x31\x01\x60\x6E\x16\x23\x85\x86\x02\xB6\x16\x13\x84\x88\xC1\x05\x41\xB4\x57\x8E"
    "\x26\x14\x0A\x18\x40\x4D\x11\xE3\x08\x15\x06\xC1\x5C\x38\x18\x50\x2E\x61\xC1\x30\x47\x0C\x00\x5A\x1B\x45\x78\xE2\x61\xA0\xBD\x87"
    "\x84\xD1\x1E\x30\x81\x70\x6B\x16\x23\x8D\x87\x02\xD6\x1A\x12\xC4\x88\xC3\x05\x81\xA8\x59\x0E\x56\x18\x12\x81\x50\x6C\x61\x41\x30"
    "\x49\x0C\x50\x56\x1B\x85\x88\xE3\x61\x61\x38\x18\x86\xD6\x22\x13\x44\x88\xC3\x06\x41\xB0\x57\x8E\x46\x24\x13\x01\xA0\x6A\x62\x81"
    "\x28\x47\x8C\x40\x6C\x1B\x05\x70\xE1\x62\xC1\x30\x17\x06\xB6\x1C\x12\xC4\x70\xC1\x05\xE1\xA8\x56\x8E\x26\x1E\x12\x81\x68\x68\x61"
    "\xA1\x20\x46\x8C\x20\x5C\x1A\x85\x60\xE0\x61\xC1\x28\x16\x06\xE6\x18\x13\x84\x60\xC0\x05\x41\xB4\x56\x8E\x26\x14\x13\x41\x90\x6F"
    "\x62\x41\x3C\x46\x8C\x20\x64\x1B\x85\x58\xE3\x69\xC1\xB5\x09\x03\x14\x22\x13\x44\x58\xC3\x06\x41\xB0\x55\x0E\x54\x24\x0C\x50\x88"
    "\x4C\x11\x40\xC8\x35\x0A\xC4\x24\x0A\x90\x50\x4A\x11\x83\x14\x14\x86\xB1\x5C\x38\xD0\x48\x2E\x41\xC1\x2C\x47\x02\xB0\xE2\x2B\x50"
    "\x58\x2C\x41\x81\x44\x46\x8C\x30\x54\x1C\x85\x60\xE4\x41\x40\xC9\x09\x05\x21\x18\x0C\x43\xA0\xAA\x42\x20\xBD\x07\x85\x41\x14\x31"
    "\x01\x88\x76\x15\x83\x85\x08\x83\x44\x28\x15\x84\x60\x36\x0D\xC2\xAD\x0B\x03\x74\x2E\x13\x84\x58\xC1\x06\xA1\xBC\x55\x0E\x24\x2A"
    "\x0D\x10\xA0\x4F\x11\x40\xCC\x36\x8A\x84\x26\x0D\x50\xA8\x4D\x11\x03\x08\x1A\x06\xE1\x54\x38\x10\xA0\x33\x42\x61\x38\x45\x03\x20"
    "\xD8\x2B\x90\x90\x31\x42\x21\x30\x47\x0C\x00\x5E\x1A\x85\x68\xE2\x41\xE0\xB9\x07\x04\xA1\x1A\x0B\x43\x90\xAF\x41\xA0\xB1\x06\x05"
    "\x21\x1E\x30\x81\x58\x72\x15\xC3\x8D\x05\x82\xD4\x1A\x23\x81\x60\xAD\x41\x81\x48\x15\x87\x14\x16\x14\x44\x68\xC3\x05\x41\xC8\x56"
    "\x0E\x54\x14\x14\x81\x58\x71\x41\x61\x44\x46\x02\xC0\xE4\x2A\x90\x60\x52\x05\xA1\xA9\x06\x84\xA1\x14\x31\x41\x70\x69\x15\x63\x8D"
    "\x07\x04\x90\x56\x1B\x90\x58\x4E\x11\x60\xB8\x35\x8B\x14\x1C\x12\xC1\x68\x6C\x41\xA1\x30\x48\x8C\x30\x58\x1A\x85\x90\xE4\x41\x81"
    "\x28\x17\x87\x24\x1E\x14\x84\x90\x2E\x0E\x22\xD1\x07\x05\x10\x5A\x1B\xD0\x68\x4F\x12\x83\x10\x17\x07\x11\x6C\x38\x50\x70\x51\x05"
    "\xA1\xD1\x06\x85\x41\x2C\x0B\x03\xB0\xAE\x41\x81\x58\x15\x87\x74\x16\x15\xC4\x70\xC1\x05\x41\xD4\x57\x8E\x24\x14\x15\x41\x40\x74"
    "\x41\x01\x50\x47\x82\xF0\xE6\x2B\x50\x78\x53\x06\x21\xD5\x08\x85\x51\x1A\x30\x81\x90\x74\x15\xC3\x81\x09\x05\x40\x62\x1C\xD0\x88"
    "\x53\x11\xC0\xBC\x39\x0A\xC4\x1E\x14\x81\x70\x71\x41\xC1\x44\x46\x0C\x00\x5A\x1B\xC5\x50\xE2\x41\xA1\x3C\x16\x06\xE4\x18\x13\x84"
    "\x50\x2B\x0D\xA2\xC9\x05\x84\xD0\x5E\x1B\x10\x78\x4C\x12\x43\x08\x17\x06\xB1\x64\x38\xD0\x70\x4B\x05\xE1\xB5\x07\x84\xD0\x5C\x1B"
    "\x10\x70\x4C\x12\x40\xB4\x35\x8B\x14\x1A\x12\xC4\x88\xC3\x05\x81\xA8\x59\x0E\x54\x94\x24\x83\x58\xB1\x41\x81\x2C\x48\x83\x40\xD8"
    "\x2C\x92\x60\x92\x0D\xA2\xA9\x0A\x04\xD1\x14\x31\x41\x98\x6E\x15\x23\x8D\x27\x08\x90\xD6\x2B\x92\x58\x8E\x0D\xC2\xAD\x27\x08\xB0"
    "\xDA\x2B\x10\x98\x4D\x11\x83\x0C\x36\x0A\xA1\xC8\x93\x04\x50\x6F\x16\x49\x3C\x49\x03\x20\xDC\x2C\x52\x70\x91\x0D\xA2\xBD\x09\x04"
    "\xD1\x1E\x31\x01\xA0\x6E\x16\x23\x85\x27\x09\x10\xDA\x2D\x12\x68\x94\x0D\x82\xD9\x26\x09\x60\xD6\x2D\xD0\xA0\x4B\x12\xE3\x04\x1B"
    "\x86\xA1\x6A\x38\x92\x50\x95\x0D\x02\xD1\x24\x09\x40\xDE\x2C\xD2\x78\x93\x0E\x22\xD5\x0B\x85\x11\x2A\x30\x81\xB0\x72\x16\x83\x81"
    "\x29\x09\x40\xE2\x2C\xD2\x88\x93\x0D\xE2\xC9\x27\x89\x20\xDC\x2C\x50\xB0\x4E\x12\x23\x00\x1B\x86\xD1\x5E\x38\x92\x68\x8F\x0D\x82"
    "\xB9\x26\x08\xE0\xD6\x2B\x52\x58\x8D\x0D\xE2\xB1\x0B\x84\xF1\x18\x30\x81\xC8\x6E\x15\x63\x8D\x27\x08\xB0\xDE\x2B\x52\x78\x8D\x0D"
    "\xC2\xB1\x27\x08\xC0\xDA\x2A\xD0\xC8\x4D\x11\x63\x0C\x1D\x06\xC1\x54\x39\x54\x50\xAB\x43\x41\x30\x45\x83\x20\xE8\x2B\x14\x60\xAD"
    "\x42\x41\x50\x46\x8C\x50\x62\x1C\xC5\x70\xE3\x51\xC2\xAD\x45\x8A\xE5\x1C\x2B\x50\x88\x53\x11\xA3\x0C\x56\x0E\x45\x18\x2B\xD4\x78"
    "\x2F\x0E\x42\xB9\x47\x0A\xD4\x1E\x14\x84\x68\xC4\x06\x41\xD0\x57\x0E\x15\x1C\x2B\x54\x68\xAC\x51\x82\xAD\x09\x05\x41\x16\x30\x41"
    "\x90\x77\x15\x43\x89\x45\x0A\x85\x10\x2B\xD4\x78\xB1\x4A\xE2\x44\x61\x07\x61\x64\x38\x14\x90\xB1\x42\x42\x44\x19\x0A\xF5\x1E\x2B"
    "\x90\x90\x56\x11\xC3\x00\x19\x07\x71\x5A\x38\x94\x68\xAC\x42\x42\x30\x1A\x8A\xB5\x16\x2B\xD0\xA8\x57\x11\xE3\x08\x19\x87\x91\x5C"
    "\x38\xD4\x70\xAF\x42\x62\x3C\x1B\x0A\xE5\x1C\x2B\x50\xB0\x59\x11\xA3\x0C\x1B\x07\xA1\x58\x39\x50\xB0\x34\x22\x80\xD4\x8A\x85\xA1"
    "\x18\x0D\x83\x90\xB4\x42\xC0\xD0\x8A\x03\x52\x2A\x14\x84\xA0\xC5\x06\xC1\xC4\x59\x8E\x32\x2C\x0B\x88\x70\x2F\x21\xE0\xC4\x88\x83"
    "\x22\x24\x0C\xC8\x98\x34\x22\x80\xD4\x8A\x85\x11\x26\x30\xC1\xA0\xE4\x42\x80\xC8\x89\x03\x32\x26\x0D\x03\x78\xB2\x42\x80\xB0\x86"
    "\x02\xD2\x1A\x13\xC4\x90\xC4\x05\xC1\xC8\x5A\x0E\x12\x1C\x0B\xC8\x78\x2E\x21\xC0\xB4\x86\x82\xE2\x1C\x0B\x08\x60\x2D\x21\xA0\xB8"
    "\x87\x05\x21\x28\x30\x41\x68\x72\x16\xE3\x89\x06\x82\xF2\x1E\x0B\x88\x70\x2D\x41\xA0\xB0\x86\x02\xB2\x16\x25\xC6\x10\x2C\x16\xC3"
    "\x80\x86\x02\xB2\x16\x0A\x88\x50\x2B\x21\x61\x48\x16\x07\x22\x18\x0B\x48\x68\x2E\x21\xC0\xBC\x87\x85\x21\x2C\x30\x01\x68\x72\x16"
    "\xE3\x89\x06\x82\xF2\x1E\x0B\x88\x70\x52\x05\xE1\xD5\x07\x83\x12\x22\x0C\x88\x90\x55\x12\xE3\x08\x17\x07\x31\x72\x38\xC8\x70\x2F"
    "\x21\xE0\xC4\x88\x83\x22\x24\x14\xC1\x98\x76\x22\x60\xD0\x8A\x03\x52\x2A\x0D\x88\xB0\x56\x13\x23\x0C\x1A\x07\x61\x74\x39\x50\xA0"
    "\x56\x06\x41\xD0\x89\x05\x40\x66\x1D\x48\x98\x55\x13\x40\xD0\x3B\x0B\x24\x28\x15\x81\x98\x74\x22\x61\x50\x19\x07\x52\x24\x15\x44"
    "\x90\xC5\x06\x61\xD8\x58\x8E\x32\x26\x15\x81\x88\x6E\x22\x21\x38\x19\x06\xF2\x24\x13\xC1\x98\x71\x22\x61\x44\x1A\x07\x22\x28\x14"
    "\x81\x98\x73\x22\x61\x4C\x19\x07\x42\x24\x15\x01\x88\x75\x22\x21\x54\x48\x8C\x30\x64\x1D\x07\x22\x12\x0A\x80\xBC\x39\x10\xF0\xA4"
    "\x0C\x43\x99\x11\x0A\x60\xC8\x3A\x0A\xF4\x24\x15\x01\x50\x6C\x21\x41\x30\x15\x86\xD2\x16\x13\x44\x78\xC4\x05\x81\xB8\x59\x0E\x12"
    "\x18\x13\x81\x68\x6F\x21\xA1\x3C\x16\x06\xE2\x18\x13\x81\x58\x6D\x21\x61\x34\x16\x06\xE2\x18\x13\x81\x90\x6C\x22\x41\x30\x18\x86"
    "\xD2\x22\x13\x41\x90\x6E\x22\x41\x38\x49\x0C\x10\x5E\x1B\x45\x90\xE2\x41\xE1\x34\x19\x06\xF2\x24\x13\xC1\x88\x6E\x22\x21\x38\x17"
    "\x86\xD4\x1E\x13\x41\x70\x6C\x21\xC1\x30\x16\x86\xB2\x1A\x12\xC6\x10\x2E\x0D\x83\x80\x87\x04\xC0\x5A\x1A\xC8\x68\x4B\x05\x81\xA8"
    "\x86\x04\xA0\x5A\x1A\xC8\x68\x4B\x12\x40\xB8\x36\x0B\x22\x1C\x13\x01\x78\x6D\x21\xE1\x34\x18\x86\xE2\x22\x13\x81\x90\x6F\x22\x41"
    "\x3C\x49\x0C\x00\x5E\x1B\x45\x90\xE2\x41\xE1\x34\x19\x06\xF2\x24\x13\xC1\x88\x6E\x22\x21\x38\x49\x03\x20\xDE\x2D\x50\x90\x4F\x06"
    "\x21\xC4\x88\x85\x10\x5E\x1C\x88\x78\x52\x12\xA3\x08\x18\x86\xE1\x66\x38\xC8\x88\x4E\x06\x41\xBC\x89\x04\xF0\x66\x1C\x48\x98\x51"
    "\x06\x41\xC8\x89\x05\x21\x26\x0C\x43\x98\xB6\x22\x21\x4C\x19\x07\x42\x24\x15\x01\x78\x75\x21\xE1\x54\x18\x87\x62\x22\x15\x84\xB0"
    "\xC3\x06\x41\xD0\x5B\x0E\x54\xA8\x25\x83\x90\xB4\x2A\x42\x50\x39\x8B\x52\x24\x14\xC4\xA8\x74\x16\xC9\x50\x4B\x07\x31\x68\x54\xC4"
    "\xA0\x72\x16\xA5\x48\x4A\x8C\x50\x62\x1C\xC5\xB0\xE3\x2A\x62\x58\x38\x8A\xE2\xA2\x23\x83\x90\xAF\x2A\x42\x3C\x39\x8B\x12\x22\x14"
    "\xC4\x88\x74\x16\x45\x50\x49\x07\x31\x66\x54\xC4\x98\x72\x16\x85\x48\x4A\x07\x11\x6A\x54\x44\xA8\xC3\x05\xE1\xC8\x5A\x0E\x44\xA4"
    "\x25\x03\x78\xB2\x29\xE2\x48\x38\x8B\x32\x1E\x14\x44\x98\x72\x16\x89\x48\x4A\x06\xA1\x58\x52\x84\x60\x6B\x15\xA5\x2C\x46\x8C\x40"
    "\x64\x1B\x05\x70\xE1\x29\x82\x38\x36\x8A\xF2\x9A\x23\xC3\x60\xAE\x29\x82\x38\x35\x8A\xD2\x24\x12\xC4\x68\x6C\x15\xC5\x30\x47\x07"
    "\x21\x58\x54\x84\x60\x71\x15\xA5\x44\x46\x87\x21\x5C\x54\x84\x70\xC1\x05\x41\xBC\x56\x8E\x24\x9E\x23\x43\x90\xAF\x2A\x42\x3C\x38"
    "\x8A\xE2\x14\x14\x44\x70\x6F\x15\xA9\x3C\x46\x86\xE1\x58\x53\x84\x60\x6D\x15\x65\x34\x45\x8C\x20\x54\x1B\x85\x60\xE0\x29\xC2\x30"
    "\x36\x8A\xB2\x9A\x22\xC3\x60\xAA\x29\x82\x28\x36\x8A\xB2\x14\x13\x44\x58\x6E\x15\x85\x38\x46\x06\xF1\x5A\x53\xC4\x68\x71\x15\xC5"
    "\x44\x47\x07\x21\x5E\x54\x84\x78\xC0\x05\x41\xBC\x56\x8E\x24\x9E\x23\x43\x90\xAF\x2A\x42\x3C\x38\x8A\xE2\x14\x14\x44\x70\x72\x15"
    "\xE9\x48\x47\x87\x11\x62\x54\x44\x88\x6F\x16\x45\x3C\x49\x0C\x20\x56\x1C\x45\x70\xE3\x2A\x22\x38\x39\x0A\xF2\xA4\x23\xC3\x98\xB1"
    "\x2A\x62\x44\x39\x0B\x22\x16\x14\x84\x90\x71\x16\x65\x44\x49\x87\x21\x68\x54\x84\xA0\x6F\x16\xA5\x3C\x4A\x87\x11\x6C\x54\x44\xB0"
    "\xC3\x0E\x42\xD0\x72\xA9\x41\x64\x64\x85\x99\x52\x12\x60\xB8\x5A\x29\x41\x66\x64\xC5\x91\x0E\x12\x43\x14\x38\x8B\x31\xC6\x64\xC5"
    "\x89\x91\x16\x46\x48\x59\x95\x11\x26\x0B\x85\xA1\x94\x16\x66\x4C\x59\x19\x21\x62\x43\x84\x88\xC3\x0D\xE2\xC8\x72\x29\x21\x5E\x63"
    "\xC5\x89\x4F\x12\x20\xB0\x59\x29\x21\x54\x62\x85\x59\x0C\x11\x63\x10\x39\x0A\xC1\xC2\x63\x05\x69\x8D\x15\x86\x30\x55\x95\x21\x16"
    "\x0B\x85\x61\x8C\x16\x46\x48\x58\x99\x11\x64\x43\x84\x90\xC1\x0D\x42\xBC\x71\x28\xF1\x64\x64\x85\x89\x4A\x12\x20\xB4\x57\xA8\xF1"
    "\x5C\x63\x85\x69\x0D\x11\xA3\x08\x35\x0A\xE1\xC0\x63\x85\x69\x8D\x15\x86\x30\x56\x94\xA1\x1A\x0B\x05\x71\x8E\x15\xE6\x3C\x58\x99"
    "\x11\x64\x43\x04\x90\xC0\x0D\x42\xBC\x71\x28\xF1\x64\x64\x85\x89\x4A\x12\x20\xB4\x59\x29\x21\x62\x64\x45\x79\x0D\x11\xE3\x08\x35"
    "\x8B\x11\xC6\x64\x45\x91\x92\x16\x66\x4C\x59\x14\xB1\x24\x0C\xC5\x89\x91\x16\x46\x48\x57\x98\xF1\x62\x44\xC4\x88\xC3\x06\x82\xC8"
    "\x72\xA1\x40\x58\x83\x04\x90\x2D\x0D\xC8\x34\x16\x20\xC0\x9C\x31\x41\x58\xB1\x1C\x68\x2C\x19\xA1\x31\x22\x0D\x03\x72\x14\x06\x68"
    "\x4C\x27\x0C\x30\x64\x2B\xC7\x22\x12\x05\x88\x30\x47\x82\xA0\xD8\x82\x81\x7A\x0F\x09\x83\x10\x17\x0B\x21\xC2\x83\x81\x3A\x07\x12"
    "\x40\x98\x37\x20\x60\x4E\x81\xC2\x70\xC1\x05\x02\xA8\x71\x20\x80\x5E\x83\xC4\x50\x31\x0D\xA8\x44\x17\xA0\xF0\x9A\x30\x81\x70\xAA"
    "\x1C\x08\x38\x13\xA0\x71\x14\x09\x83\x62\x06\x04\xE8\x1C\x26\x0C\x00\x50\x2A\x87\x12\x08\x05\xE8\x3C\x45\x02\xE0\xDA\x83\x81\x7A"
    "\x0F\x09\xA3\x08\x18\x8A\xB1\xC6\x84\x41\x4A\x09\x11\x60\xA0\x39\xA0\x80\x52\x82\x42\x98\xC3\x05\x41\xD0\x72\xA0\xA0\xA8\x0C\x83"
    "\x62\x12\x09\x80\xCC\x36\x8A\xE4\x26\x13\x41\x90\x6C\x42\x41\x30\x47\x0C\x50\x62\x1A\xC7\x1A\x11\x09\x60\xA4\x39\xA0\x90\xA6\x0A"
    "\x83\xA0\xAE\x41\x41\x50\x14\x87\x34\x12\x14\xC4\x70\xC3\x05\x01\xC8\x72\x20\x80\xA4\x0B\xC3\x62\x0F\x09\x80\xB8\x35\x0A\xC4\x1C"
    "\x12\x81\x78\x6F\x41\xE1\x3C\x46\x0C\x40\x62\x1B\x87\x0A\x11\x09\xC0\xA4\x33\xA0\x90\x8E\x0B\x03\x30\xAE\x41\x81\x18\x15\x86\x74"
    "\x16\x11\xC4\x70\xC1\x05\x41\xA0\x71\x20\xA0\x90\x0C\x83\x7A\x12\x09\xE0\xCC\x38\x8A\xD4\x26\x14\x41\xA8\x6F\x42\xA1\x3C\x46\x8C"
    "\x20\x68\x1B\x87\x02\x14\x09\xC0\xB0\x33\xA0\xC0\x8E\x0B\x83\x30\xAC\x41\xC1\x18\x1A\x06\x74\x28\x11\xC4\x60\xC0\x06\x41\xA0\x71"
    "\x21\x20\x90\x0D\x43\x7A\x15\x09\xE0\xD0\x37\x0A\xD4\x28\x13\x81\xA8\x6F\x42\xA1\x3C\x46\x8C\x20\x66\x1C\x47\x1A\x13\x0A\x20\xB8"
    "\x34\xA0\xE0\x92\x0B\x43\x40\xB3\x41\xA1\x20\x17\x06\x94\x1C\x12\x44\x98\xC3\x05\x81\xA8\x5A\x0E\x54\x18\x12\x84\xA0\x2E\x0E\x42"
    "\xB1\x07\x05\x21\x18\x0B\x83\x98\xAD\x41\xC1\x4C\x46\x82\xE0\xE4\x2B\x10\x70\x52\x11\x83\x14\x17\x07\x11\x56\x38\xD0\x70\x51\x11"
    "\x60\xB8\x34\x8B\x34\x1C\x12\x44\x98\x2E\x0D\x42\xD1\x07\x04\xA1\x28\x0B\x83\x48\xB3\x41\xC1\x24\x49\x8C\x30\x58\x1A\x05\x90\xE4"
    "\x41\x81\x20\x49\x02\xC0\xDE\x2B\x10\x60\x4F\x11\x80\xB0\x37\x0A\xA4\x18\x13\x84\x50\x2C\x0D\xE2\xBD\x06\x04\xF1\x1E\x31\x01\x60"
    "\x71\x15\xC3\x85\x06\x05\x11\x1C\x0B\x03\x48\xA7\x41\x81\x24\x43\x82\xE0\xD8\x29\x90\x70\x4C\x10\xC0\xB8\x35\x8A\x74\x1C\x12\xC4"
    "\x38\xC1\x05\xA1\xA8\x54\x0E\x24\x1A\x12\x84\x40\x2D\x0E\x42\xBD\x06\x85\x21\x1E\x0B\x43\x98\xB1\x41\xA1\x4C\x48\x83\x20\xEA\x2B"
    "\xD0\x90\x55\x11\xE3\x08\x19\x07\x41\x5C\x38\x10\x90\x54\x11\xC0\xC8\x36\x0A\x74\x24\x13\x04\x38\x32\x0D\xC2\x99\x09\x04\xE1\x0C"
    "\x0C\x83\xA0\xA7\x42\x41\x50\x43\x8C\x00\x64\x1C\x85\x40\xE2\x42\x41\x48\x44\x03\x20\xEA\x2B\xD0\x90\x55\x11\xE0\xBC\x3A\x0A\xE4"
    "\x1E\x15\x04\x70\x2F\x0E\xA2\xBD\x07\x85\x51\x1E\x30\x81\x70\x73\x16\x23\x8D\x07\x05\x31\x22\x0B\x83\x70\xA9\x41\xC1\x38\x44\x83"
    "\x30\xDA\x2A\x10\x98\x4D\x11\x00\xC4\x37\x0A\x94\x22\x13\x84\x48\xC3\x05\xC1\xB0\x55\x0E\x54\x1C\x13\x04\x50\x34\x0D\xC2\xC9\x0A"
    "\x04\xE1\x24\x0D\x03\x70\xB3\x42\x81\x38\x49\x83\x40\xDC\x2C\x90\xA0\x4E\x12\x43\x14\x19\x86\xE1\x62\x38\xD0\x98\x4E\x12\x20\xCC"
    "\x37\x0A\x94\x26\x13\x84\x48\x33\x0D\xC2\xA9\x09\x84\xE1\x14\x0C\xC3\x70\xA9\x42\x61\x38\x44\x8C\x30\x64\x1B\x05\x40\xE4\x42\x41"
    "\x30\x44\x03\x20\xD8\x2B\xD0\x90\x4C\x11\xE0\xC8\x36\x0A\xE4\x24\x13\x04\x70\x32\x0D\x82\xBD\x09\x04\xC1\x1E\x31\x01\xB0\x6C\x16"
    "\x23\x85\x0B\x04\xC1\x22\x0D\x83\x60\xA9\x42\xC1\x30\x44\x83\x60\xDC\x2B\x10\xB0\x4E\x11\x80\xD8\x37\x0A\xB4\x2C\x13\x84\x58\xC1"
    "\x06\xE1\xB4\x55\x0E\x24\x2E\x13\x44\x50\x37\x0D\xA2\xC9\x0B\x84\xD1\x24\x0D\xC3\x68\xB3\x42\xE1\x34\x49\x83\x70\xE4\x2D\x50\xB8"
    "\x52\x12\xA3\x08\x1B\x07\x21\x68\x38\x10\xB0\x52\x12\x80\xD8\x39\x0A\xC4\x2C\x14\x84\x60\x36\x0E\x42\xB9\x0B\x05\x21\x1C\x0D\x83"
    "\x90\xB4\x42\xC1\x48\x4A\x0C\x00\x6E\x1C\x85\x90\xE2\x42\xE1\x48\x49\x03\x70\xE4\x2D\x50\xB8\x52\x12\xA0\xDC\x37\x8B\x44\x2E\x13"
    "\xC4\xA0\x37\x0D\xE2\xD5\x0B\x84\xF1\x2A\x30\x81\xC8\x6E\x16\x63\x8D\x0C\x84\xE1\x26\x0C\x43\x70\xAE\x42\x21\x38\x47\x03\x10\xE6"
    "\x2B\x50\x88\x53\x11\xA0\xC4\x38\x8A\xE4\x22\x14\x44\x70\xC3\x06\x41\xB8\x56\x0E\x54\x24\x13\x84\x60\x2A\x0E\x82\xB8\x85\x02\xB2"
    "\x16\x15\x04\x70\x2C\x0E\x82\xB9\x06\x05\x41\x1C\x0A\x83\xA0\xAE\x41\x41\x50\x47\x0C\x50\x52\x1C\xC5\x70\xE3\x41\x21\x4C\x47\x03"
    "\x10\xE6\x2B\x88\x88\x32\x22\x41\x4C\x47\x03\x30\xE6\x2B\x90\x98\x53\x11\xC0\xC4\x39\x8A\xE4\x22\x14\xC4\x70\xC3\x05\xE1\xC8\x56"
    "\x0E\x44\x1E\x14\x84\x60\x28\x0E\x42\xB0\x84\x02\x92\x12\x14\x84\x60\x2A\x0E\x42\xB1\x05\x05\x21\x18\x0A\x03\x90\xAC\x41\x01\x48"
    "\x46\x0C\x40\x52\x1D\x85\x60\xE1\x41\x21\x58\x46\x02\xE0\xEC\x2B\x08\x70\x2D\x21\xA1\x58\x46\x02\xC0\xEC\x2B\x90\x60\x56\x11\xC0"
    "\xAC\x3B\x0A\xE4\x16\x15\x84\x70\xC1\x05\x41\xDC\x56\x8E\x24\x14\x15\xC4\x68\x2D\x0E\xE2\xB4\x86\x82\xC2\x18\x15\xC4\x68\x2B\x0E"
    "\xE2\xB5\x05\x85\x71\x1A\x0B\x43\xB8\xB2\x41\xA1\x5C\x49\x0C\x20\x58\x1D\x85\x90\xE0\x41\x81\x58\x49\x02\xA0\xEC\x2C\x88\x50\x2B"
    "\x21\x61\x58\x49\x02\xC0\xEC\x2C\x90\x60\x56\x12\x40\xB8\x3B\x0B\x24\x1C\x15\x84\x90\xC0\x05\xA1\xDC\x59\x0E\x24\x1A\x15\xC4\x90"
    "\x2F\x0E\xE2\xC8\x87\x82\xE2\x1C\x15\xC4\x90\x2D\x0E\xE2\xBD\x06\x85\x71\x1E\x0B\x03\xB8\xAF\x41\x81\x5C\x47\x8C\x20\x56\x1E\x45"
    "\x70\xE3\x41\x61\x64\x47\x02\xE0\xE2\x2B\x88\x70\x2D\x21\xA1\x44\x47\x02\xC0\xE2\x2C\xD0\x60\x51\x12\x60\xAC\x38\x8B\x14\x16\x14"
    "\x44\x88\xC3\x05\x81\xC8\x57\x0E\x54\x18\x14\x84\x70\x32\x0D\x42\xD0\x89\x04\xA0\x62\x1A\xC8\x88\x4B\x12\x80\xC8\x36\x0B\x44\x24"
    "\x13\x04\xA0\x2C\x0D\x42\xD1\x06\x04\xA1\x28\x31\x41\x70\x69\x16\x63\x8D\x07\x04\x91\x26\x0B\x83\x88\xB3\x21\xC1\x44\x17\x87\x22"
    "\x1E\x14\x84\x98\x31\x0E\x62\xCD\x08\x85\x31\x26\x0B\x83\x88\xB3\x41\xC1\x44\x49\x8C\x30\x58\x1B\xC5\x90\xE4\x41\x81\x3C\x49\x03"
    "\x20\xD0\x2C\x88\x90\x48\x06\x61\xA4\x89\x84\x91\x24\x0D\x03\x50\xB2\x42\x81\x28\x49\x03\x20\xD0\x2C\x90\x90\x48\x12\x43\x10\x1A"
    "\x06\x91\x6C\x38\x50\xA0\x49\x12\xC0\xD0\x37\x0B\x62\x28\x13\x81\x98\x6D\x22\x61\x34\x4B\x03\x20\xD8\x2D\x90\x90\x4C\x12\xC0\xC4"
    "\x35\x8B\x64\x22\x12\xC4\xB0\xC1\x05\xE1\xA8\x5B\x8E\x24\x1E\x12\x84\xB8\x2F\x0D\xA2\xDC\x87\x84\xD0\x5C\x1B\x08\x70\x4C\x12\xE0"
    "\xBC\x35\x8B\x74\x1E\x12\xC4\xB8\x31\x0D\xA2\xDD\x08\x84\xD1\x2E\x30\x81\x90\x6C\x16\xC3\x81\x09\x04\xC1\x2C\x0D\x03\x50\xB6\x22"
    "\x81\x28\x19\x86\xB2\x26\x12\xC4\xB0\x32\x0D\x82\xD9\x09\x04\xC1\x2C\x0D\x03\x70\xB6\x42\x81\x38\x4B\x0C\x00\x6A\x1B\x45\xB8\xE2"
    "\x42\xA1\x34\x4B\x83\x20\xDE\x2D\xC8\x90\x4F\x06\x21\xB8\x88\x84\xE1\x2E\x0B\xC3\x68\xB7\x41\xE1\x34\x4B\x82\xF0\xD8\x2D\xD0\x78"
    "\x4C\x12\xE3\x08\x17\x06\xB1\x72\x38\xD0\x70\x4B\x13\x20\xAC\x37\x0B\x12\x9C\x1B\x48\x58\x4D\x12\x20\xB8\x36\x0B\x14\x1C\x13\x04"
    "\x88\x2E\x0D\x62\xC5\x07\x04\xB1\x22\x30\xC1\x70\x6C\x16\x43\x95\x26\x09\x20\xE4\x2A\x8A\x90\x8A\x0E\x22\xAC\xA8\x88\xB0\xE4\x2B"
    "\x12\x90\x8C\x0D\x82\xA9\x26\x08\xA1\x8A\x1B\x85\x48\xE3\x49\xC2\x24\x37\x0B\x12\x9C\x24\x43\x78\xB2\x21\xC1\x3C\x49\x02\xE0\xE2"
    "\x2C\xD2\x88\x93\x0D\xC2\xC5\x07\x04\xE1\x22\x30\xC1\x50\x6C\x15\xE3\x91\x26\x08\xF0\xE4\x2A\x0A\x90\x88\x0E\x62\xA4\xA9\x88\x90"
    "\xE8\x2A\x92\xA0\x8A\x0E\x42\xA1\x29\x08\x81\x88\x1D\x05\x48\xE1\x4A\x82\x24\x3A\x0A\xE2\xA8\x23\x83\x98\xAD\x21\x41\x4C\x46\x82"
    "\xE0\xE4\x2B\x12\x90\x8C\x0E\x22\xAD\x07\x05\x11\x16\x30\x41\x68\x6F\x15\x43\x89\x27\x88\xA0\xDE\x2B\x4A\x78\x8D\x0D\xC2\xB0\xA7"
    "\x08\xC0\xDE\x2A\xD2\x78\x8B\x0E\x22\xB5\x06\x85\x11\x1A\x30\x81\x70\x72\x15\x83\x81\x29\x08\xC0\xE8\x2A\x8A\xA0\x8A\x0E\x62\xAC"
    "\xA9\x88\xB0\xE4\x2B\x12\x90\x8C\x0E\x82\xB9\x07\x05\x41\x1C\x30\x01\x68\x75\x15\xA3\x89\x2A\x88\xD0\xE4\x2B\xCA\x90\x8F\x0E\x22"
    "\xB8\x86\x85\x11\x1C\x0A\x83\x78\xAD\x49\xE2\x34\x37\x8A\xC4\x14\x13\xC4\x60\xC2\x05\x41\xB8\x55\x8E\x34\x9C\x22\xC3\x58\xAE\x31"
    "\xC2\xB4\xA5\x88\xD0\xDC\x2B\x10\x50\x4E\x11\x80\xA4\x37\x0A\xB4\x12\x13\x84\x58\xC3\x05\x41\xB8\x56\x0E\x55\x18\x2C\x8C\x90\xB1"
    "\x21\x42\x44\x19\x0B\x25\x24\x2B\x10\x90\x8C\x18\xA0\xC4\x57\x0E\x35\x1C\x2B\x8C\x70\xAF\x29\xC2\x3C\x37\x0B\x15\x22\x2B\x90\x88"
    "\x4E\x11\xC3\x0C\x17\x86\xA1\x58\x39\x14\x60\xB2\x32\x42\xCC\xC9\x8B\x45\x28\x2C\x90\x78\x92\x18\x80\xB8\x5A\x0E\x15\x28\x2D\x0C"
    "\xA0\xB3\x29\x42\x4C\x37\x0B\x25\x24\x2C\x50\x70\x4E\x12\x23\x04\x15\x06\xD1\x5E\x38\x94\x78\xAF\x31\xE2\xB8\xC7\x0A\xF4\x14\x23"
    "\xC1\x58\xB1\x41\x61\x34\x48\x8C\x20\x58\x1B\x85\x90\xE0\x52\x42\xD0\xCA\x0B\x33\x26\x2C\x94\x90\xB4\x41\x81\x38\x4A\x0C\x00\x5E"
    "\x1B\x45\xA8\xE2\x52\xA2\xC8\xC9\x0B\x12\x9A\x24\x43\x50\xAF\x51\xE2\xBD\x07\x84\xA1\x1E\x30\x81\x58\x6A\x15\xC3\x8D\x47\x0A\xB5"
    "\x16\x2B\x90\x58\x4A\x11\xC0\xAC\x34\x8A\xE4\x16\x12\x44\x70\xC3\x05\x81\xA8\x57\x0E\x56\x94\x1C\x90\x60\x34\x42\x81\x48\x62\x83"
    "\x40\xE2\x38\xD0\xA0\x35\x42\xA2\x38\x1A\x0A\xE4\x28\x0C\xD0\x98\x51\x11\xC3\x0C\x19\x06\xF1\x54\x39\x20\x90\x32\x42\x41\x3C\x62"
    "\x03\x20\xDC\x38\x50\x90\x33\x42\x62\x28\x19\x0A\xE4\x24\x0C\x50\x88\x4E\x11\xC3\x04\x17\x86\xA1\x5A\x38\xA2\x50\x6B\x41\xE1\x2C"
    "\x46\x8C\x20\x64\x1B\x05\x70\xE0\xA2\x41\x30\x47\x0C\x00\x64\x1B\xC5\x68\xE2\x42\x40\xC1\x08\x08\xD0\x5E\x2A\x90\x78\x30\x42\x01"
    "\x3C\x45\x0C\x20\x5C\x1A\xC5\x50\xE3\x81\xC1\x2C\x45\x02\xE0\xD6\x2A\x50\x70\x4B\x11\x23\x0C\x17\x06\xC1\x54\x39\x5C\x50\xB2\x41"
    "\xC1\x30\x1B\x07\x44\x2C\x15\x04\x90\xC5\x06\xC1\xD0\x58\x8E\x34\x2C\x15\x01\xB8\x75\x42\xE1\x54\x1B\x07\x44\x2C\x15\x01\xA8\x73"
    "\x42\xA1\x4C\x48\x8C\x30\x68\x1C\x85\x78\xE4\x82\x81\x48\x1A\x07\x24\x28\x14\x84\x78\xC4\x06\x81\xC8\x57\x0E\x14\x28\x14\x81\xA8"
    "\x73\x42\xA1\x4C\x1A\x07\x24\x28\x14\x81\x98\x71\x42\x61\x44\x47\x0C\x10\x64\x1B\xC5\x50\xE2\x42\x40\xC1\x08\x02\xF4\x1E\x22\x81"
    "\x80\xAB\x42\x01\x3C\x45\x8C\x20\x5C\x1C\x85\x60\xE0\x81\xC0\xB9\x07\x05\x21\x18\x30\x01\x68\x72\x15\xE3\x89\x29\x07\x04\x1A\x14"
    "\x01\x90\x6F\x49\xE1\xC1\x09\x05\x01\x1E\x30\x81\x88\x6E\x15\x63\x8E\x08\x84\xE1\x16\x0C\x43\x70\xAB\x42\x21\x38\x45\x8C\x30\x64"
    "\x1B\x85\x60\xE5\x62\x40\xC9\x27\x08\xC0\xEC\x2D\x12\xB0\x94\x18\xA1\xD8\x5A\x0E\x34\xAC\x25\x03\xB8\xB5\x42\x41\x5C\x4A\x83\x10"
    "\xEC\x2D\x12\xB0\x94\x0E\xA2\xCD\x2A\x89\x31\x86\x1D\x05\x90\xE4\x62\x20\xBD\x2A\x09\x20\xE8\x2C\x92\xA0\x92\x18\x81\xD0\x59\x0E"
    "\x14\xA8\x24\x83\xA8\xB3\x41\xE1\x54\x49\x82\xE0\xE8\x2C\x92\xA0\x92\x0E\x62\xC5\x29\x89\x11\x82\x1C\x85\x78\xE2\x4A\x41\xC1\x07"
    "\x05\x00\x5A\x1B\xD2\x78\x70\x4A\x02\x3C\x61\x06\xE1\x64\x38\x18\x68\x2C\x49\xC1\xB9\x27\x09\x21\x80\x1B\x45\x90\xE2\x52\x42\xC1"
    "\x26\x89\x00\xE4\x2B\xD0\x60\x8F\x05\x62\xC1\x05\x85\x21\x20\x30\x81\x58\x71\x15\xC3\x8E\x28\x88\xE0\xE2\x2B\x90\x58\x51\x11\xC3"
    "\x0C\x16\x07\x21\x5C\x39\x58\x60\x52\x06\x81\xC9\x47\x0B\x65\x2C\x31\x45\xB0\xE3\x52\xC2\xDD\x0A\x05\x21\x2E\x0C\xC3\x88\xB6\x52"
    "\xC2\xD5\x09\x89\x51\x86\x0C\x85\xA0\xE4\x62\x41\x44\x1D\x06\xF5\x28\x2D\x14\xA0\xC4\x16\x83\x85\x4A\x0B\x54\x34\x13\xC4\xA8\x38"
    "\x0D\xC2\xD1\x4A\x0B\x34\x30\x24\xC6\x08\x37\x16\x43\x89\x49\x0B\x04\x9C\x24\x03\x68\xAF\x51\xE2\xC1\x0B\x89\x01\x84\x0E\x85\x70"
    "\xE0\x63\x41\x34\x1B\x06\xC5\x1C\x2B\x90\xB0\x8E\x18\x00\xDC\x56\x8E\x27\x1A\x2C\x92\x60\x6B\x42\xE1\x2C\x49\x0C\x20\x6C\x1A\xC5"
    "\x88\xE3\x92\x22\xC5\x0B\x04\xB1\x22\x30\xC1\xB0\x6C\x16\x43\x95\xA6\x09\x20\xE8\x2C\x98\xB0\xC5\x05\xC3\x8D\xAA\x09\x20\xE6\x2C"
    "\x50\x70\x2D\x41\xA1\x4C\x61\x82\xC0\xE4\x39\x1A\x90\x91\x0F\x42\xBD\x86\x0C\x40\x68\x38\x5A\xD0\x8F\x0F\x02\xB9\x0A\x03\x34\x26"
    "\x16\x06\x08\x32\x0E\xE3\x89\xC7\x0A\xD6\xAE\x30\x83\xD0\xE0\x62\x41\x68\x46\x83\x20\xEC\x2B\x18\x90\x56\x18\x00\xC8\x3B\x8E\x29"
    "\x18\x2A\xD0\x90\x57\x11\x63\x08\x18\x87\x61\x56\x38\xE8\x88\x56\x11\x63\x0C\x19\x07\x61\x58\x39\x58\x90\x8C\x05\x42\xD1\x85\x05"
    "\x61\x8A\x0A\x43\x70\xE3\x61\x22\x50\x18\x8B\x34\x9C\x1B\x50\x88\x4D\x12\x63\x0C\x17\x86\xC1\x64\x39\x18\x78\x92\x05\x02\xE9\x84"
    "\x04\xC1\x88\x09\xC3\xA0\xE1\x60\xE2\x68\x17\x0B\x84\xA8\x1C\xD0\x70\x53\x13\x03\x04\x16\x87\x21\x6E\x38\x98\x68\x35\x62\xA2\x5C"
    "\x61\x03\x41\x74\x38\x18\xA0\x52\x13\x40\xB0\x39\x0B\x66\x18\x14\x84\xB0\xC0\x05\x61\xC8\x5B\x8E\x26\x16\x0B\xD8\x78\x52\x12\xE3"
    "\x08\x15\x87\x11\x6C\x38\xD8\x58\x33\x62\x61\x44\x4B\x0C\x30\x68\x1C\x85\xB0\xE5\x62\x81\x48\x16\x06\xA6\x18\x12\x84\xB0\xC5\x05"
    "\x61\xA4\x57\x0E\x36\x16\x12\x41\x98\x71\x51\xC2\xB5\x09\x85\x11\x1A\x30\xC1\x90\x6F\x15\x83\x91\x89\x04\xF0\x54\x1A\x18\x50\x48"
    "\x11\x83\x10\x14\x86\x71\x68\x38\x58\x48\x47\x06\x21\xB9\x4A\x0B\x34\x22\x13\x84\x98\xC1\x05\xE1\xB4\x59\x0E\x26\x1E\x13\x41\xB8"
    "\x75\x62\xE1\x54\x61\x03\x60\xE8\x38\x18\xB0\x54\x12\x40\xB8\x36\x0B\x26\x1C\x13\x04\x90\xC0\x05\xA1\xAC\x59\x0E\x26\x96\x1B\xD0"
    "\x68\x33\x42\x61\x3C\x49\x0C\x20\x5C\x1A\xC5\x88\xE3\x61\xC1\x2C\x17\x07\x36\x1C\x14\xC4\x88\xC3\x05\xC1\xD0\x59\x0E\x5A\x1C\x15"
    "\x04\x90\xC5";

static void StopBackgroundMidiPlayback(HWND hwnd)
{
    KillTimer(hwnd, TIMER_BACKGROUND_MIDI);
    if (g_midiOut != NULL) {
        SendMidiMessage(0xB2, 123, 0);
        SendMidiMessage(0xB3, 123, 0);
        SendMidiMessage(0xB4, 123, 0);
        SendMidiMessage(0xB5, 123, 0);
    }
    g_backgroundMidiBitIndex = 0;
    g_backgroundMidiEventTick = 0;
}

static void AdvanceBackgroundMidiPlayback(HWND hwnd)
{
    DWORD elapsedMs = GetTickCount() - g_backgroundMidiStartTick;
    ULONGLONG targetTick = (ULONGLONG)elapsedMs *
        MIDI_BACKGROUND_TICKS_PER_QUARTER * 1000 /
        MIDI_BACKGROUND_TEMPO_US_PER_QUARTER;

    while (g_backgroundMidiBitIndex < (sizeof(kBackgroundMidiEvents) - 1) * 8) {
        UINT byteIndex = g_backgroundMidiBitIndex >> 3;
        UINT shift = 24 - (g_backgroundMidiBitIndex & 7) - MIDI_BACKGROUND_EVENT_BITS;
        DWORD bits = ((DWORD)kBackgroundMidiEvents[byteIndex] << 16) |
            ((DWORD)kBackgroundMidiEvents[byteIndex + 1] << 8) |
            kBackgroundMidiEvents[byteIndex + 2];
        UINT packedEvent = (bits >> shift) & ((1U << MIDI_BACKGROUND_EVENT_BITS) - 1);
        DWORD delta = kBackgroundMidiDeltaTicks[packedEvent >> 8];
        BYTE eventCode = (BYTE)packedEvent;
        DWORD eventTick = g_backgroundMidiEventTick + delta;
        if (eventTick > targetTick) {
            break;
        }

        g_backgroundMidiEventTick = eventTick;
        g_backgroundMidiBitIndex += MIDI_BACKGROUND_EVENT_BITS;
        UINT part = eventCode >> 5;
        BYTE channel = (BYTE)(2 + (part >> 1));
        BYTE isNoteOn = (BYTE)(part & 1);
        BYTE note = kBackgroundMidiNotes[eventCode & 0x1F];
        BYTE velocity = isNoteOn ? (BYTE)(channel == 5 ? 79 : 97) : 0;
        SendMidiMessage((BYTE)((isNoteOn ? 0x90 : 0x80) | channel), note, velocity);
    }

    if (g_backgroundMidiBitIndex >= (sizeof(kBackgroundMidiEvents) - 1) * 8) {
        StopBackgroundMidiPlayback(hwnd);
    }
}

static void StartBackgroundMidiPlayback(HWND hwnd)
{
    if (!EnsureVictoryMidiOutput()) {
        return;
    }

    StopBackgroundMidiPlayback(hwnd);

    SendMidiMessage(0xC2, 40, 0);
    SendMidiMessage(0xC3, 40, 0);
    SendMidiMessage(0xC4, 40, 0);
    SendMidiMessage(0xC5, 42, 0);
    SendMidiMessage(0xB2, 7, 48);
    SendMidiMessage(0xB3, 7, 68);
    SendMidiMessage(0xB4, 7, 68);
    SendMidiMessage(0xB5, 7, 68);
    g_backgroundMidiStartTick = GetTickCount();

    if (SetTimer(hwnd, TIMER_BACKGROUND_MIDI, MIDI_BACKGROUND_STEP_MS, NULL) == 0) {
        StopBackgroundMidiPlayback(hwnd);
        return;
    }
    AdvanceBackgroundMidiPlayback(hwnd);
}

static void StopMoveMidiPlayback(HWND hwnd)
{
    if (hwnd != NULL) {
        KillTimer(hwnd, TIMER_MOVE_MIDI);
    }

    if (g_moveCurrentNote >= 0) {
        SendMidiMessage(0x81, (BYTE)g_moveCurrentNote, 0);
        g_moveCurrentNote = -1;
    }

    g_moveMelodyLength = 0;
    g_moveMelodyIndex = 0;
    g_moveMidiPlaybackEndTick = 0;
}

static DWORD GetRemainingMoveMidiPlaybackMs(void)
{
    DWORD now;

    if (g_moveMidiPlaybackEndTick == 0) {
        return 0;
    }

    now = GetTickCount();
    if ((LONG)(g_moveMidiPlaybackEndTick - now) <= 0) {
        return 0;
    }

    return g_moveMidiPlaybackEndTick - now;
}

static int IsMoveMidiPlaybackActive(void)
{
    return g_moveCurrentNote >= 0 ||
        g_moveMelodyIndex < g_moveMelodyLength ||
        GetRemainingMoveMidiPlaybackMs() > 0;
}

static void AdvanceMoveMidiPlayback(HWND hwnd)
{
    if (g_moveCurrentNote >= 0) {
        SendMidiMessage(0x81, (BYTE)g_moveCurrentNote, 0);
        g_moveCurrentNote = -1;
    }

    if (g_moveMelodyIndex >= g_moveMelodyLength) {
        StopMoveMidiPlayback(hwnd);
        return;
    }

    BYTE note = g_moveMelodyNotes[g_moveMelodyIndex];
    UINT noteDurationMs = g_moveMelodyDurations[g_moveMelodyIndex];
    BYTE velocity = (BYTE)(84 + (randx() % 20));
    g_moveMelodyIndex++;

    SendMidiMessage(0x91, note, velocity);
    g_moveCurrentNote = (int)note;
    if (hwnd != NULL) {
        SetTimer(hwnd, TIMER_MOVE_MIDI, noteDurationMs, NULL);
    }
}

static __packed const BYTE kMoveScale[] = {0, 2, 4, 7, 9, 12, 14};
static __packed const BYTE kMoveMotifs[][MOVE_MIDI_MAX_NOTES] = {
    {0, 2, 4, 2, 6},
    {0, 3, 2, 4, 1},
    {2, 4, 1, 3, 0},
    {4, 2, 5, 1, 3}
};

static int StartMoveMidiPlayback(HWND hwnd)
{
    if (hwnd == NULL || g_board.pieceCount <= 1) {
        return 0;
    }

    if (!EnsureVictoryMidiOutput()) {
        return 0;
    }

    StopMoveMidiPlayback(hwnd);
    SendMidiMessage(0xC1, (BYTE)(9 + (randx() % 5)), 0);
    SendMidiMessage(0xB1, 7, 78);

    UINT motifIndex = randx() % _countof(kMoveMotifs);
    int transpose = 60 + (int)((randx() % 2) * 12);
    UINT noteCount = 4 + (randx() % 2);
    UINT totalDurationMs = 0;
    g_moveMelodyLength = noteCount;
    g_moveMelodyIndex = 0;

    for (UINT i = 0; i < noteCount; ++i) {
        int scaleIndex = (int)kMoveMotifs[motifIndex][i];
        if (randx() % 4 == 0) {
            scaleIndex = (scaleIndex + 1) % (int)_countof(kMoveScale);
        }
        g_moveMelodyNotes[i] = (BYTE)(transpose + kMoveScale[scaleIndex]);
        g_moveMelodyDurations[i] = (i + 1 == noteCount)
            ? (UINT)(MIDI_MOVE_NOTE_MAX_MS + 30 + (randx() % 30))
            : (UINT)(MIDI_MOVE_NOTE_MIN_MS + (randx() % (MIDI_MOVE_NOTE_MAX_MS - MIDI_MOVE_NOTE_MIN_MS + 1)));
        totalDurationMs += g_moveMelodyDurations[i];
    }

    g_moveMidiPlaybackEndTick = GetTickCount() + totalDurationMs;
    AdvanceMoveMidiPlayback(hwnd);
    return 1;
}

static int StartSadMoveMidiPlayback(HWND hwnd)
{
    if (!EnsureVictoryMidiOutput()) {
        return 0;
    }

    StopMoveMidiPlayback(hwnd);
    SendMidiMessage(0xC1, 42, 0);
    SendMidiMessage(0xB1, 7, 72);

    BYTE root = (BYTE)(48 + (randx() % 5));
    UINT totalDurationMs;
    g_moveMelodyLength = 4;
    g_moveMelodyIndex = 0;
    g_moveMelodyNotes[0] = (BYTE)(root + 12);
    g_moveMelodyNotes[1] = (BYTE)(root + 8);
    g_moveMelodyNotes[2] = (BYTE)(root + 5);
    g_moveMelodyNotes[3] = root;
    g_moveMelodyDurations[0] = 120;
    g_moveMelodyDurations[1] = 120;
    g_moveMelodyDurations[2] = 150;
    g_moveMelodyDurations[3] = 240;
    totalDurationMs = g_moveMelodyDurations[0] + g_moveMelodyDurations[1] + g_moveMelodyDurations[2] + g_moveMelodyDurations[3];

    g_moveMidiPlaybackEndTick = GetTickCount() + totalDurationMs;
    AdvanceMoveMidiPlayback(hwnd);
    return 1;
}

static void StopVictoryMidiPlayback(HWND hwnd)
{
    if (hwnd != NULL) {
        KillTimer(hwnd, TIMER_VICTORY_START);
        KillTimer(hwnd, TIMER_VICTORY_MIDI);
    }

    g_victoryMidiStartPending = FALSE;
    g_victoryDelayArmed = FALSE;

    if (g_victoryCurrentNote >= 0) {
        SendMidiMessage(0x80, (BYTE)g_victoryCurrentNote, 0);
        g_victoryCurrentNote = -1;
    }

    g_victoryMelodyIndex = 0;
    StopVictoryFireworks(hwnd);
}

static void StopVictoryFireworks(HWND hwnd)
{
    if (hwnd != NULL) {
        KillTimer(hwnd, TIMER_VICTORY_FIREWORKS);
    }

    g_victoryFireworksActive = FALSE;
    g_victoryFireworksStartTick = 0;

    if (hwnd != NULL) {
        InvalidateBoardArea(hwnd, FALSE);
    }
}

static void SetSolvedOverlayVisible(HWND hwnd, BOOL visible)
{
    BOOL showOverlay = visible ? TRUE : FALSE;

    if (g_showSolvedOverlay == showOverlay) {
        return;
    }

    g_showSolvedOverlay = showOverlay;
    g_solvedOverlayStartTick = showOverlay ? GetTickCount() : 0;
    SAFE_RELEASE(g_d3dLoadingTextTextureView);

    if (hwnd != NULL) {
        if (showOverlay) {
            SetTimer(hwnd, TIMER_SOLVED_OVERLAY, SOLVED_OVERLAY_FRAME_MS, NULL);
        } else {
            KillTimer(hwnd, TIMER_SOLVED_OVERLAY);
        }
        InvalidateBoardArea(hwnd, FALSE);
    }
}

static void StartVictoryFireworks(HWND hwnd)
{
    g_victoryFireworksActive = TRUE;
    g_victoryFireworksStartTick = GetTickCount();

    if (hwnd != NULL) {
        SetTimer(hwnd, TIMER_VICTORY_FIREWORKS, VICTORY_FIREWORKS_FRAME_MS, NULL);
        InvalidateBoardArea(hwnd, FALSE);
    }
}

static void AdvanceVictoryMidiPlayback(HWND hwnd)
{
    if (g_victoryCurrentNote >= 0) {
        SendMidiMessage(0x80, (BYTE)g_victoryCurrentNote, 0);
        g_victoryCurrentNote = -1;
    }

    if (g_victoryMelodyIndex >= _countof(g_odeToJoyMidi)) {
        StopVictoryMidiPlayback(hwnd);
        return;
    }

    int note = 60 + (g_odeToJoyMidi[g_victoryMelodyIndex] >> 4);
    UINT noteDurationMs = (UINT)(g_odeToJoyMidi[g_victoryMelodyIndex] & 0x0F) * MIDI_VICTORY_NOTE_MS;
    g_victoryMelodyIndex++;

    SendMidiMessage(0x90, (BYTE)note, 100);
    g_victoryCurrentNote = (int)note;
    if (hwnd != NULL) {
        SetTimer(hwnd, TIMER_VICTORY_MIDI, noteDurationMs, NULL);
    }
}

static int StartVictoryMidiPlayback(HWND hwnd)
{
    StopMoveMidiPlayback(hwnd);
    StopVictoryMidiPlayback(hwnd);
    if (!EnsureVictoryMidiOutput()) {
        return 0;
    }

    AdvanceVictoryMidiPlayback(hwnd);
    return 1;
}

static int StartVictoryMidiPlaybackWhenReady(HWND hwnd)
{
    StopVictoryMidiPlayback(hwnd);
    if (hwnd != NULL) {
        g_victoryMidiStartPending = TRUE;
        g_victoryDelayArmed = !IsMoveMidiPlaybackActive();
        SetTimer(hwnd, TIMER_VICTORY_START, g_victoryDelayArmed ? (UINT)VICTORY_START_DELAY_MS : 120u, NULL);
        return 0;
    }

    return StartVictoryMidiPlayback(hwnd);
}

static inline int BoardIsSolved(void)
{
    return g_boardLocked && g_board.pieceCount == 1;
}

static void UpdateSolvedCelebration(HWND hwnd)
{
    if (BoardIsSolved()) {
        if (!g_boardSolvedCelebrated) {
            g_boardSolvedCelebrated = TRUE;
            SetSolvedOverlayVisible(hwnd, TRUE);
            if (StartVictoryMidiPlaybackWhenReady(hwnd)) {
                StartVictoryFireworks(hwnd);
            }
        }
    } else {
        g_boardSolvedCelebrated = FALSE;
        SetSolvedOverlayVisible(hwnd, FALSE);
        StopVictoryMidiPlayback(hwnd);
        StopVictoryFireworks(hwnd);
    }
}

static void DestroyVictoryMidiOutput(void)
{
    StopMoveMidiPlayback(NULL);
    StopVictoryMidiPlayback(NULL);
    StopVictoryFireworks(NULL);
    if (g_midiOut != NULL) {
        midiOutReset(g_midiOut);
        midiOutClose(g_midiOut);
        g_midiOut = NULL;
    }
}

static __packed const COLORREF g_pieceColorsPreset[] = {
    RGB(231, 76, 60), RGB(52, 152, 219), RGB(46, 204, 113), RGB(241, 196, 15),
    RGB(155, 89, 182), RGB(230, 126, 34), RGB(26, 188, 156), RGB(149, 165, 166),
    RGB(192, 57, 43), RGB(41, 128, 185), RGB(39, 174, 96), RGB(142, 68, 173)
};

static __declspec(noinline) COLORREF ComputePieceColor(int index)
{
    return g_pieceColorsPreset[index % _countof(g_pieceColorsPreset)];
}

static void EnsurePieceColorCount(int count)
{
    if (count < 0) {
        count = 0;
    }
    if (count > MAX_LOADING_BALLS) {
        count = MAX_LOADING_BALLS;
    }

    while (g_pieceColorCount < count) {
        g_pieceColors[g_pieceColorCount] = ComputePieceColor(g_pieceColorCount);
        g_pieceColorCount++;
    }
}

static void ResetPieceColors(int count)
{
    g_pieceColorSeed = (float)(randx() % 360) / 360.0f;
    g_pieceColorCount = 0;
    EnsurePieceColorCount(count);
}

static COLORREF GetPieceColor(int pieceId)
{
    if (pieceId <= 0) {
        return RGB(80, 80, 80);
    }

    EnsurePieceColorCount(pieceId);
    if (g_pieceColorCount <= 0) {
        return RGB(80, 80, 80);
    }

    return g_pieceColors[(pieceId - 1) % g_pieceColorCount];
}

static int StartLoadingBallSettlement(void)
{
    int ballCount = g_loadingBallCount;

    if (ballCount > g_board.pieceCount) {
        ballCount = g_board.pieceCount;
    }
    if (ballCount <= 0) {
        return 0;
    }

    RECT boardRect;
    GetBoardRect(&boardRect);

    for (int i = 0; i < ballCount; ++i) {
        g_loadingBallStartX[i] = g_loadingBallPosX[i];
        g_loadingBallStartY[i] = g_loadingBallPosY[i];

        int row = -1;
        int col = -1;
        if (FindPiecePosition(&g_board, g_loadingBallPieceIds[i], &row, &col)) {
            g_loadingBallTargetX[i] = (float)(boardRect.left + col * g_cellSize) + (float)g_cellSize * 0.5f;
            g_loadingBallTargetY[i] = (float)(boardRect.top + row * g_cellSize) + (float)g_cellSize * 0.5f;
        } else {
            g_loadingBallTargetX[i] = g_loadingBallStartX[i];
            g_loadingBallTargetY[i] = g_loadingBallStartY[i];
        }
    }

    g_loadingBallCount = ballCount;
    g_loadingBallsSettling = TRUE;
    g_loadingBallSettleStartTick = GetTickCount();
    return 1;
}

static void FinalizeGeneratedBoard(HWND hwnd)
{
    g_loadingBallsSettling = FALSE;
    SetBoardLocked(TRUE);
    SetStatus(hwnd, kGeneratedBoardText, g_board.pieceCount);
    CaptureInitialBoard(TRUE);
    SetBoardGenerating(hwnd, FALSE);
}

#pragma region Board State Management

static __declspec(noinline) void ClearBoard(Fling_BoardState *board)
{
    ZeroMem(board->cells, sizeof(board->cells));
    board->pieceCount = 0;
    board->nextPieceId = 1;
}

static void AddPiece(Fling_BoardState *board, int row, int col)
{
    if (!InBounds(row, col) || board->cells[row][col] != 0) {
        return;
    }

    const int pieceId = board->nextPieceId++;
    EnsurePieceColorCount(pieceId);
    board->cells[row][col] = pieceId;
    board->pieceCount++;
}

static void RemovePiece(Fling_BoardState *board, int row, int col)
{
    if (!InBounds(row, col) || board->cells[row][col] == 0) {
        return;
    }

    board->cells[row][col] = 0;
    board->pieceCount--;
}

static void TogglePiece(Fling_BoardState *board, int row, int col)
{
    if (!InBounds(row, col)) {
        return;
    }

    if (board->cells[row][col] == 0) {
        AddPiece(board, row, col);
    } else {
        RemovePiece(board, row, col);
    }
}

#pragma endregion

#pragma region Board and Solve State Conversion

static int CanBoardChainMove(const Fling_BoardState *board, int row, int col, int dr, int dc, int requireTarget)
{
    int nextRow = row + dr;
    int nextCol = col + dc;
    int sawEmpty = 0;

    while (InBounds(nextRow, nextCol)) {
        if (board->cells[nextRow][nextCol] != 0) {
            if (!sawEmpty && requireTarget) {
                return 0;
            }
            return CanBoardChainMove(board, nextRow, nextCol, dr, dc, 0);
        }
        sawEmpty = 1;
        nextRow += dr;
        nextCol += dc;
    }

    return requireTarget ? 0 : 1;
}

static void PushBoardChain(Fling_BoardState *board, int row, int col, int dr, int dc)
{
    int pieceId = board->cells[row][col];
    int nextRow = row + dr;
    int nextCol = col + dc;
    int stopRow = row;
    int stopCol = col;

    while (InBounds(nextRow, nextCol) && board->cells[nextRow][nextCol] == 0) {
        stopRow = nextRow;
        stopCol = nextCol;
        nextRow += dr;
        nextCol += dc;
    }

    board->cells[row][col] = 0;

    if (!InBounds(nextRow, nextCol)) {
        board->pieceCount--;
        return;
    }

    PushBoardChain(board, nextRow, nextCol, dr, dc);
    board->cells[stopRow][stopCol] = pieceId;
}

static int ApplyBoardMove(Fling_BoardState *board, Fling_Move move)
{
    if (!InBounds(move.sr, move.sc) || board->cells[move.sr][move.sc] == 0) {
        return 0;
    }

    if (!CanBoardChainMove(board, move.sr, move.sc, move.dr, move.dc, 1)) {
        return 0;
    }

    PushBoardChain(board, move.sr, move.sc, move.dr, move.dc);
    return 1;
}

static void BoardToSolveState(const Fling_BoardState *board, Fling_SolveState *state)
{
    state->bitsLow = 0;
    state->bitsHigh = 0;
    state->pieceCount = board->pieceCount;

    for (int row = 0; row < BOARD_ROWS; ++row) {
        for (int col = 0; col < BOARD_COLS; ++col) {
            if (board->cells[row][col] != 0) {
                int idx = CellIndex(row, col);
                if (idx < 64) {
                    state->bitsLow |= (1ULL << idx);
                } else {
                    state->bitsHigh |= (1ULL << (idx - 64));
                }
            }
        }
    }
}

static int GetSolveCell(const Fling_SolveState *state, int row, int col)
{
    if (!InBounds(row, col)) {
        return 0;
    }

    int idx = CellIndex(row, col);
    if (idx < 64) {
        return (int)((state->bitsLow >> idx) & 1ULL);
    }

    return (int)((state->bitsHigh >> (idx - 64)) & 1ULL);
}

static void SetSolveCell(Fling_SolveState *state, int row, int col, int value)
{
    if (!InBounds(row, col)) {
        return;
    }

    int idx = CellIndex(row, col);
    if (idx < 64) {
        uint64_t mask = 1ULL << idx;
        if (value) {
            state->bitsLow |= mask;
        } else {
            state->bitsLow &= ~mask;
        }
    } else {
        uint64_t mask = 1ULL << (idx - 64);
        if (value) {
            state->bitsHigh |= mask;
        } else {
            state->bitsHigh &= ~mask;
        }
    }
}

static int CanSolveChainMove(const Fling_SolveState *state, int row, int col, int dr, int dc, int requireTarget)
{
    int nextRow = row + dr;
    int nextCol = col + dc;
    int sawEmpty = 0;

    while (InBounds(nextRow, nextCol)) {
        if (GetSolveCell(state, nextRow, nextCol)) {
            if (!sawEmpty && requireTarget) {
                return 0;
            }
            return CanSolveChainMove(state, nextRow, nextCol, dr, dc, 0);
        }
        sawEmpty = 1;
        nextRow += dr;
        nextCol += dc;
    }

    return requireTarget ? 0 : 1;
}

static void PushSolveChain(Fling_SolveState *state, int row, int col, int dr, int dc)
{
    int nextRow = row + dr;
    int nextCol = col + dc;
    int stopRow = row;
    int stopCol = col;

    while (InBounds(nextRow, nextCol) && !GetSolveCell(state, nextRow, nextCol)) {
        stopRow = nextRow;
        stopCol = nextCol;
        nextRow += dr;
        nextCol += dc;
    }

    SetSolveCell(state, row, col, 0);

    if (!InBounds(nextRow, nextCol)) {
        state->pieceCount--;
        return;
    }

    PushSolveChain(state, nextRow, nextCol, dr, dc);
    SetSolveCell(state, stopRow, stopCol, 1);
}

static int ApplySolveMove(const Fling_SolveState *source, Fling_Move move, Fling_SolveState *dest)
{
    *dest = *source;

    if (!GetSolveCell(dest, move.sr, move.sc)) {
        return 0;
    }

    if (!CanSolveChainMove(dest, move.sr, move.sc, move.dr, move.dc, 1)) {
        return 0;
    }

    PushSolveChain(dest, move.sr, move.sc, move.dr, move.dc);
    return 1;
}

static inline void ResetSeenStates(void)
{
    ZeroMem(g_seen, sizeof(g_seen));
}

// Fibonacci hashing with Knuth's multiplicative constant, adapted for 128-bit
// state and reduced to fit HASH_CAPACITY
// See https://asecuritysite.com/hash/smh_fib
static unsigned int HashSolveState(const Fling_SolveState *state)
{
    uint64_t x = state->bitsLow * 11400714819323198485ULL;
    x ^= state->bitsHigh + 0x9E3779B97F4A7C15ULL + (x << 6) + (x >> 2);
    return (unsigned int)(x % HASH_CAPACITY);
}

static int MarkSolveStateSeen(const Fling_SolveState *state)
{
    unsigned int start = HashSolveState(state);

    for (unsigned int offset = 0; offset < HASH_CAPACITY; ++offset) {
        Fling_HashEntry *entry = &g_seen[(start + offset) % HASH_CAPACITY];
        if (!entry->used) {
            entry->used = 1;
            entry->bitsLow = state->bitsLow;
            entry->bitsHigh = state->bitsHigh;
            return 0;
        }

        if (entry->bitsLow == state->bitsLow && entry->bitsHigh == state->bitsHigh) {
            return 1;
        }
    }

    return 0;
}

static int MoveScore(const Fling_SolveState *state, const Fling_Move *move)
{
    int row = move->sr + move->dr;
    int col = move->sc + move->dc;
    int emptyCount = 0;
    const int centerRow = BOARD_ROWS / 2;
    const int centerCol = BOARD_COLS / 2;

    while (InBounds(row, col) && !GetSolveCell(state, row, col)) {
        emptyCount++;
        row += move->dr;
        col += move->dc;
    }

    if (!InBounds(row, col)) {
        return -1000;
    }

    return emptyCount * 5 - (abs(row - centerRow) + abs(col - centerCol));
}

static void SortMoves(const Fling_SolveState *state, Fling_Move *moves, int moveCount)
{
    for (int i = 0; i < moveCount - 1; ++i) {
        int bestIndex = i;
        int bestScore = MoveScore(state, &moves[i]);

        for (int j = i + 1; j < moveCount; ++j) {
            int score = MoveScore(state, &moves[j]);
            if (score > bestScore) {
                bestScore = score;
                bestIndex = j;
            }
        }

        if (bestIndex != i) {
            Fling_Move temp = moves[i];
            moves[i] = moves[bestIndex];
            moves[bestIndex] = temp;
        }
    }
}

static __packed const char g_Fling_MoveDirections[4][2] = {
    {-1, 0}, {1, 0}, {0, -1}, {0, 1}
};

static int CollectLegalMoves(const Fling_SolveState *state, Fling_Move *moves)
{
    int count = 0;

    for (int row = 0; row < BOARD_ROWS; ++row) {
        for (int col = 0; col < BOARD_COLS; ++col) {
            if (!GetSolveCell(state, row, col)) {
                continue;
            }

            for (int dir = 0; dir < 4; ++dir) {
                int dr = (int) g_Fling_MoveDirections[dir][0];
                int dc = (int) g_Fling_MoveDirections[dir][1];
                if (CanSolveChainMove(state, row, col, dr, dc, 1)) {
                    moves[count].sr = row;
                    moves[count].sc = col;
                    moves[count].dr = dr;
                    moves[count].dc = dc;
                    count++;
                }
            }
        }
    }

    SortMoves(state, moves, count);
    return count;
}

static int SolveDepthFirst(const Fling_SolveState *state, Fling_Move *path, int depth)
{
    if (state->pieceCount <= 1) {
        return 1;
    }

    if (g_nodesVisited++ >= SOLVE_NODE_LIMIT) {
        g_solverAborted = TRUE;
        return 0;
    }

    if (MarkSolveStateSeen(state)) {
        return 0;
    }

    Fling_Move moves[MAX_LEGAL_MOVES];
    int moveCount = CollectLegalMoves(state, moves);
    for (int i = 0; i < moveCount; ++i) {
        Fling_SolveState nextState;
        if (ApplySolveMove(state, moves[i], &nextState)) {
            path[depth] = moves[i];
            if (SolveDepthFirst(&nextState, path, depth + 1)) {
                return 1;
            }
            if (g_solverAborted) {
                return 0;
            }
        }
    }

    return 0;
}

static int FindSolutionForBoard(const Fling_BoardState *board, Fling_Move *outMoves, int *outMoveCount)
{
    Fling_SolveState startState;
    BoardToSolveState(board, &startState);

    *outMoveCount = 0;

    if (startState.pieceCount < 1) {
        return 0;
    }

    if (startState.pieceCount == 1) {
        return 1;
    }

    ResetSeenStates();
    g_nodesVisited = 0;
    g_solverAborted = FALSE;

    if (SolveDepthFirst(&startState, outMoves, 0)) {
        *outMoveCount = startState.pieceCount - 1;
        return 1;
    }

    if (g_solverAborted) {
        return -1;
    }

    return 0;
}

static int BoardHasAnyLegalMove(const Fling_BoardState *board)
{
    if (board == NULL || board->pieceCount <= 1) {
        return 0;
    }

    Fling_SolveState state;
    Fling_Move moves[MAX_LEGAL_MOVES];

    BoardToSolveState(board, &state);
    return CollectLegalMoves(&state, moves) > 0;
}

#pragma endregion

static void WarnIfBoardHasNoSolution(HWND hwnd)
{
    if (hwnd == NULL || g_board.pieceCount <= 1) {
        return;
    }

    if (!BoardHasAnyLegalMove(&g_board)) {
        StartSadMoveMidiPlayback(hwnd);
        QuietMessageBox(hwnd, kNoValidMoveText, kInfoCaption, MB_OK | MB_ICONWARNING);
        return;
    }

    Fling_Move tempMoves[MAX_SOLUTION_STEPS];
    int tempMoveCount = 0;
    int result = FindSolutionForBoard(&g_board, tempMoves, &tempMoveCount);
    if (result == 0) {
        // QuietMessageBox(hwnd, "No valid solution exists for the current board.", "No Valid Solution", MB_OK | MB_ICONWARNING);
    } else if (result < 0) {
        //QuietMessageBox(hwnd, kCannotFindSolutionText, kInfoCaption, MB_OK | MB_ICONWARNING);
    }
}

static void StopSolutionPlayback(HWND hwnd)
{
    if (g_isAnimating) {
        g_isAnimating = FALSE;
    }

    KillTimer(hwnd, TIMER_SOLVE);

    StopVictoryMidiPlayback(hwnd);
    StopVictoryFireworks(hwnd);
    SetSolvedOverlayVisible(hwnd, FALSE);
    g_boardSolvedCelebrated = FALSE;

    if (g_animationManager != NULL) {
        g_animationManager->lpVtbl->AbandonAllStoryboards(g_animationManager);
    }

    g_solutionStep = 0;
    g_solutionLength = 0;
    g_solutionReady = FALSE;
    g_solutionAutoPlaying = FALSE;
    g_animationPieceId = 0;
    g_animationOriginPieceId = 0;
    g_animationProgress = 0.0f;
    g_animationSegmentStartTime = 0;
    g_animationTargetStep = 0;
    g_animationMode = ANIMATION_NONE;
    g_animationMovingForward = TRUE;
    g_collisionAnimationPieceId = 0;
    g_collisionStartRow = 0;
    g_collisionStartCol = 0;
    g_collisionEndRow = 0;
    g_collisionEndCol = 0;
    g_animationSegmentCount = 0;
    g_animationSegmentIndex = 0;
    ZeroMem(&g_animationResultBoard, sizeof(g_animationResultBoard));
    UpdateSolutionButtons();

    if (hwnd != NULL) {
        InvalidateBoardArea(hwnd, FALSE);
        InvalidateInfoArea(hwnd);
    }
}

static void StopSolutionAutoPlay(HWND hwnd)
{
    KillTimer(hwnd, TIMER_SOLVE);

    if (!g_solutionAutoPlaying && !(g_isAnimating && g_animationMode == ANIMATION_SOLUTION)) {
        UpdateSolutionButtons();
        return;
    }

    g_solutionAutoPlaying = FALSE;

    if (g_isAnimating && g_animationMode == ANIMATION_SOLUTION) {
        g_isAnimating = FALSE;
        if (g_animationManager != NULL) {
            g_animationManager->lpVtbl->AbandonAllStoryboards(g_animationManager);
        }
        g_animationPieceId = 0;
        g_animationOriginPieceId = 0;
        g_animationProgress = 0.0f;
        g_animationSegmentStartTime = 0;
        g_animationTargetStep = 0;
        g_animationMode = ANIMATION_NONE;
        g_animationMovingForward = TRUE;
        g_collisionAnimationPieceId = 0;
        g_collisionStartRow = 0;
        g_collisionStartCol = 0;
        g_collisionEndRow = 0;
        g_collisionEndCol = 0;
        g_animationSegmentCount = 0;
        g_animationSegmentIndex = 0;
    }

    if (g_solutionReady) {
        GoToSolutionStep(hwnd, 0);
    } else {
        UpdateSolutionButtons();
    }
}

static void GoToSolutionStep(HWND hwnd, int step)
{
    if (!g_solutionReady) {
        return;
    }

    if (step < 0) {
        step = 0;
    }
    if (step > g_solutionLength) {
        step = g_solutionLength;
    }

    g_solutionStep = step;
    g_board = g_solutionStates[step];
    UpdateSolutionButtons();

    if (g_solutionLength == 0) {
        SetStatus(hwnd, kSolvedText);
    } else if (g_solutionStep == g_solutionLength) {
        SetStatus(hwnd, kViewingFinalStepText, g_solutionStep);
    } else {
        if (g_solutionStep == 0)
            SetStatus(hwnd, kViewingInitialText);
        else
            SetStatus(hwnd, kViewingSolutionStepText, g_solutionStep, g_solutionLength);
    }

    UpdateSolvedCelebration(hwnd);
    InvalidateBoardArea(hwnd, FALSE);
    InvalidateInfoArea(hwnd);

    if (g_solutionAutoPlaying && !g_isAnimating) {
        if (hwnd != NULL) {
            KillTimer(hwnd, TIMER_SOLVE);
        }

        if (g_solutionLength > 0 && g_solutionStep < g_solutionLength) {
            if (hwnd != NULL) {
                DWORD remainingMoveMidiMs = GetRemainingMoveMidiPlaybackMs();
                UINT delayMs;

                if (g_solutionStep == 0 && remainingMoveMidiMs == 0) {
                    delayMs = 1u;
                } else {
                    delayMs = (UINT)(remainingMoveMidiMs + (DWORD)SOLUTION_PLAY_DELAY_MS);
                }

                SetTimer(hwnd, TIMER_SOLVE, delayMs, NULL);
            } else {
                StartSolutionStepAnimation(hwnd, g_solutionStep + 1);
            }
        } else {
            g_solutionAutoPlaying = FALSE;
            UpdateSolutionButtons();
        }
    }
}

static void StartSolutionStepAnimation(HWND hwnd, int targetStep)
{
    int fromStep;

    if (!g_solutionReady || g_isAnimating) {
        return;
    }

    if (targetStep < 0) {
        targetStep = 0;
    }
    if (targetStep > g_solutionLength) {
        targetStep = g_solutionLength;
    }
    if (targetStep == g_solutionStep) {
        return;
    }

    fromStep = g_solutionStep;
    g_collisionAnimationPieceId = 0;
    SetStatus(hwnd, kAnimatingStepText, (targetStep > fromStep) ? targetStep : fromStep, g_solutionLength);

    if (targetStep > fromStep) {
        if (!StartMoveAnimationSequence(
                hwnd,
                &g_solutionStates[fromStep],
                &g_solutionStates[targetStep],
                g_solution[fromStep],
                ANIMATION_SOLUTION,
                targetStep,
                TRUE)) {
            GoToSolutionStep(hwnd, targetStep);
        }
    } else {
        if (!StartMoveAnimationSequence(
                hwnd,
                &g_solutionStates[targetStep],
                &g_solutionStates[fromStep],
                g_solution[targetStep],
                ANIMATION_SOLUTION,
                targetStep,
                FALSE)) {
            GoToSolutionStep(hwnd, targetStep);
        }
    }
}

static void StartSolutionPlaybackFromBeginning(HWND hwnd)
{
    if (!g_solutionReady || g_isAnimating || g_solutionLength <= 0) {
        return;
    }

    if (g_hasInitialBoard && (!BoardsEqual(&g_board, &g_initialBoard) || g_boardLocked != g_initialBoardLocked)) {
        g_board = g_initialBoard;
        SetBoardLocked(g_initialBoardLocked);
        InvalidateBoardArea(hwnd, FALSE);
        InvalidateInfoArea(hwnd);
    }

    KillTimer(hwnd, TIMER_SOLVE);
    g_solutionAutoPlaying = TRUE;
    GoToSolutionStep(hwnd, 0);
}

static void StartBoardMoveAnimation(HWND hwnd, Fling_Move move)
{
    if (g_isAnimating) {
        return;
    }

    int pieceId = g_board.cells[move.sr][move.sc];
    Fling_BoardState nextBoard = g_board;
    if (pieceId == 0 || !ApplyBoardMove(&nextBoard, move)) {
        SetStatus(hwnd, kInvalidMoveText);
        StartSadMoveMidiPlayback(hwnd);
        return;
    }

    g_collisionAnimationPieceId = 0;

    if (!StartMoveAnimationSequence(hwnd, &g_board, &nextBoard, move, ANIMATION_USER_MOVE, 0, TRUE)) {
        g_board = nextBoard;
        if (g_board.pieceCount == 1) {
            SetStatus(hwnd, kSolvedText);
        } else {
            SetStatus(hwnd, kMoveAppliedText, g_board.pieceCount);
        }
        StartMoveMidiPlayback(hwnd);
        UpdateSolvedCelebration(hwnd);
        WarnIfBoardHasNoSolution(hwnd);
        UpdateRestoreButton();
        InvalidateBoardArea(hwnd, FALSE);
        InvalidateInfoArea(hwnd);
    }
}

static void PopulateDifficultyComboBox(void)
{
    SendMessageA(g_comboDifficulty, CB_RESETCONTENT, 0, 0);
    for (int pieces = 2; pieces <= 20; ++pieces) {
        char label[16];
        sprintf(label, kNumFmt, pieces);
        SendMessageA(g_comboDifficulty, CB_ADDSTRING, 0, (LPARAM)label);
    }

    if (g_generatePieceCount < 2 || g_generatePieceCount > 20) {
        g_generatePieceCount = DEFAULT_GENERATE_PIECE_COUNT;
    }

    SendMessageA(g_comboDifficulty, CB_SETCURSEL, (WPARAM)(g_generatePieceCount - 2), 0);
}

static int GetSelectedGeneratePieceCount(void)
{
    LRESULT selectedIndex = SendMessageA(g_comboDifficulty, CB_GETCURSEL, 0, 0);
    if (selectedIndex == CB_ERR) {
        return g_generatePieceCount;
    }

    return (int)selectedIndex + 2;
}

static int GenerateRandomSolvableBoard(Fling_BoardState *board, int targetPieces)
{
    if (targetPieces < 2) {
        targetPieces = 2;
    }
    if (targetPieces > 20) {
        targetPieces = 20;
    }

    for (int attempt = 0; attempt < 500; ++attempt) {
        Fling_BoardState candidate;
        Fling_Move tempPath[MAX_SOLUTION_STEPS];
        int tempLength = 0;
        int placed = 0;

        ClearBoard(&candidate);

        while (placed < targetPieces) {
            int row = randx() % BOARD_ROWS;
            int col = randx() % BOARD_COLS;
            if (candidate.cells[row][col] == 0) {
                AddPiece(&candidate, row, col);
                placed++;
            }
        }

        if (FindSolutionForBoard(&candidate, tempPath, &tempLength) == 1 && tempLength > 0) {
            *board = candidate;
            return 1;
        }
    }

    return 0;
}

static DWORD WINAPI GenerateBoardThreadProc(LPVOID parameter)
{
    DWORD startTick = GetTickCount();

    g_generateResult.requestedPieces = g_generateTask.targetPieces;
    g_generateResult.succeeded = GenerateRandomSolvableBoard(&g_board, g_generateTask.targetPieces);

    DWORD elapsedMs = GetTickCount() - startTick;
    if (elapsedMs < MIN_GENERATE_ANINAL_DURATION_MS) {
        Sleep(MIN_GENERATE_ANINAL_DURATION_MS - elapsedMs);
    }

    // note: here we are safe to use SendMessage (instead of PostMessage)
    // just to detach the handle because we are 100% sure that the main
    // thread doesn't need to care about this thread.
    SendMessageA(g_generateTask.hwnd, WM_APP_GENERATE_COMPLETE, 0, 0);
    return 0;
}

static void LayoutControls(HWND hwnd)
{
    int minPanelWidth = ScaleForDpi(PANEL_WIDTH, g_currentDpi);
    int height = ScaleForDpi(32, g_currentDpi);
    int gap = ScaleForDpi(12, g_currentDpi);

    RECT client;
    GetClientRect(hwnd, &client);

    g_boardMargin = ScaleForDpi(BOARD_MARGIN, g_currentDpi);
    int bottomPadding = g_boardMargin;

    int sizeByWidth = (client.right - minPanelWidth - g_boardMargin * 3) / BOARD_COLS;
    int sizeByHeight = (client.bottom - g_boardMargin - bottomPadding) / BOARD_ROWS;
    g_cellSize = (sizeByWidth < sizeByHeight) ? sizeByWidth : sizeByHeight;
    if (g_cellSize < ScaleForDpi(CELL_SIZE, g_currentDpi)) {
        g_cellSize = ScaleForDpi(CELL_SIZE, g_currentDpi);
    }

    g_panelWidth = client.right - (BOARD_COLS * g_cellSize + g_boardMargin * 3);
    if (g_panelWidth < minPanelWidth) {
        g_panelWidth = minPanelWidth;
    }

    RECT board;
    GetBoardControlRect(&board);
    int left = board.right + ScaleForDpi(20, g_currentDpi);
    int top = board.top + ScaleForDpi(20, g_currentDpi);
    int width = client.right - left - g_boardMargin;
    int comboWidth = ScaleForDpi(56, g_currentDpi);
    int comboItemHeight = GetDifficultyComboItemHeight(g_currentDpi);
    int comboSelectionHeight = GetDifficultyComboSelectionHeight(g_currentDpi);

    if (width < ScaleForDpi(MIN_PANEL_WIDTH, g_currentDpi)) {
        width = ScaleForDpi(MIN_PANEL_WIDTH, g_currentDpi);
    }
    if (comboWidth > width / 2) {
        comboWidth = width / 2;
    }
    if (comboWidth < ScaleForDpi(48, g_currentDpi)) {
        comboWidth = ScaleForDpi(48, g_currentDpi);
    }
    if (comboItemHeight < 1) {
        comboItemHeight = 1;
    }
    if (comboSelectionHeight < comboItemHeight) {
        comboSelectionHeight = comboItemHeight;
    }

    int buttonWidth = width - comboWidth - gap;
    if (buttonWidth < ScaleForDpi(80, g_currentDpi)) {
        buttonWidth = ScaleForDpi(80, g_currentDpi);
    }

    int clearWidth = (width - gap) / 2;
    if (clearWidth < ScaleForDpi(76, g_currentDpi)) {
        clearWidth = ScaleForDpi(76, g_currentDpi);
    }
    int saveWidth = width - clearWidth - gap;
    if (saveWidth < ScaleForDpi(76, g_currentDpi)) {
        saveWidth = ScaleForDpi(76, g_currentDpi);
        clearWidth = width - saveWidth - gap;
    }
    if (clearWidth < 1) {
        clearWidth = 1;
    }
    if (saveWidth < 1) {
        saveWidth = 1;
    }

    int navWidth = (width - gap * 2) / 3;
    int navTop = top + 4 * (height + gap);

    SendMessageA(g_comboDifficulty, CB_SETITEMHEIGHT, (WPARAM)-1, (LPARAM)comboSelectionHeight);
    SendMessageA(g_comboDifficulty, CB_SETITEMHEIGHT, 0, (LPARAM)comboItemHeight);

    HDWP defer = BeginDeferWindowPos(10);
    {
        const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;

        defer = DeferWindowPos(defer, g_boardWindow, NULL, board.left, board.top, board.right - board.left, board.bottom - board.top, flags);
        defer = DeferWindowPos(defer, g_buttonRandom, NULL, left, top, buttonWidth, height, flags);
        defer = DeferWindowPos(defer, g_comboDifficulty, NULL, left + buttonWidth + gap, top, comboWidth, comboSelectionHeight + comboItemHeight * 8, flags);
        defer = DeferWindowPos(defer, g_buttonClear, NULL, left, top + 1 * (height + gap), clearWidth, height, flags);
        defer = DeferWindowPos(defer, g_buttonSavePicture, NULL, left + clearWidth + gap, top + 1 * (height + gap), saveWidth, height, flags);
        defer = DeferWindowPos(defer, g_buttonRestore, NULL, left, top + 2 * (height + gap), width, height, flags);
        defer = DeferWindowPos(defer, g_buttonSolve, NULL, left, top + 3 * (height + gap), width, height, flags);
        defer = DeferWindowPos(defer, g_buttonPrevious, NULL, left, navTop, navWidth, height, flags);
        defer = DeferWindowPos(defer, g_buttonNext, NULL, left + navWidth + gap, navTop, navWidth, height, flags);
        defer = DeferWindowPos(defer, g_buttonPlay, NULL, left + (navWidth + gap) * 2, navTop, navWidth, height, flags);
    }
    EndDeferWindowPos(defer);
}

static int HitTestBoardCell(int x, int y, int *outRow, int *outCol)
{
    RECT board;
    GetBoardRect(&board);

    if (x < board.left || x >= board.right || y < board.top || y >= board.bottom) {
        return 0;
    }

    *outCol = (x - board.left) / g_cellSize;
    *outRow = (y - board.top) / g_cellSize;
    return 1;
}

static void UpdateBoardCursorAtPoint(int x, int y)
{
    int row, col;
    HCURSOR cursor;

    if (!g_isGeneratingBoard && HitTestBoardCell(x, y, &row, &col) && g_board.cells[row][col] != 0) {
        cursor = LoadCursor(NULL, IDC_HAND);
    } else {
        cursor = LoadCursor(NULL, IDC_ARROW);
    }

    SetCursor(cursor);
}

static void StartSolveAnimation(HWND hwnd)
{
    //wchar_t message[128];

    StopSolutionPlayback(hwnd);

    if (g_hasInitialBoard && (!BoardsEqual(&g_board, &g_initialBoard) || g_boardLocked != g_initialBoardLocked)) {
        g_board = g_initialBoard;
        SetBoardLocked(g_initialBoardLocked);
        InvalidateBoardArea(hwnd, FALSE);
        InvalidateInfoArea(hwnd);
    }

    int result = FindSolutionForBoard(&g_board, g_solution, &g_solutionLength);

    if (result == 1) {
        if (!g_boardLocked) {
            CaptureInitialBoard(TRUE);
        }
        SetBoardLocked(TRUE);
        g_solutionStates[0] = g_board;

        for (int stepIndex = 0; stepIndex < g_solutionLength; ++stepIndex) {
            g_solutionStates[stepIndex + 1] = g_solutionStates[stepIndex];
            ApplyBoardMove(&g_solutionStates[stepIndex + 1], g_solution[stepIndex]);
        }

        g_solutionReady = TRUE;
        GoToSolutionStep(hwnd, 0);
    } else if (result < 0) {
        SetStatus(hwnd, kCannotFindSolutionText);
    } else {
        SetStatus(hwnd, kCannotFindSolutionText);
        QuietMessageBox(hwnd, kCannotFindSolutionText, kInfoCaption, MB_OK | MB_ICONINFORMATION);
    }
}

static void LoadGeneratedBoard(HWND hwnd)
{
    if (g_isGeneratingBoard) {
        return;
    }

    g_generatePieceCount = GetSelectedGeneratePieceCount();
    StopSolutionPlayback(hwnd);
    ResetPieceColors(g_generatePieceCount);

    g_generateTask.hwnd = hwnd;
    g_generateTask.targetPieces = g_generatePieceCount;

    SetBoardGenerating(hwnd, TRUE);
    SetStatus(hwnd, kGeneratingBoardText, g_generatePieceCount);

    g_generateThread = CreateThread(NULL, 0, GenerateBoardThreadProc, NULL, 0, NULL);
}

static void OnBoardEdited(HWND hwnd)
{
    if (BoardIsSolved()) {
        SetStatus(hwnd, kBoardSolvedText);
    } else {
        SetStatus(hwnd, kBoardUpdatedText, g_board.pieceCount);
    }

    UpdateSolvedCelebration(hwnd);
}

static Fling_DxColor ColorrefToDxColor(COLORREF color, float alpha)
{
    Fling_DxColor dxColor;
    dxColor.r = (float)GetRValue(color) / 255.0f;
    dxColor.g = (float)GetGValue(color) / 255.0f;
    dxColor.b = (float)GetBValue(color) / 255.0f;
    dxColor.a = alpha;
    return dxColor;
}

static COLORREF BlendPieceColor(COLORREF base, COLORREF target, float amount)
{
    int r = (int)((float)GetRValue(base) + ((float)GetRValue(target) - (float)GetRValue(base)) * amount + 0.5f);
    int g = (int)((float)GetGValue(base) + ((float)GetGValue(target) - (float)GetGValue(base)) * amount + 0.5f);
    int b = (int)((float)GetBValue(base) + ((float)GetBValue(target) - (float)GetBValue(base)) * amount + 0.5f);
    return RGB(r, g, b);
}

static COLORREF GetBoardSquareColorAtPoint(float x, float y)
{
    int col = (int)(x / (float)g_cellSize);
    int row = (int)(y / (float)g_cellSize);

    if (!InBounds(row, col)) {
        return kLightSquareColor;
    }

    return ((row + col) % 2 == 0) ? kLightSquareColor : kDarkSquareColor;
}

static void AppendCircle(Fling_BoardVertex *vertices, int *count, int maxCount, float centerX, float centerY, float radius, float surfaceWidth, float surfaceHeight, Fling_DxColor* color);
static void AppendVictoryFireworks(Fling_BoardVertex *vertices, int *count, int maxCount, RECT board, float surfaceWidth, float surfaceHeight);

static void AppendPieceBall(
    Fling_BoardVertex *vertices,
    int *count,
    int maxCount,
    float centerX,
    float centerY,
    float radius,
    float surfaceWidth,
    float surfaceHeight,
    COLORREF baseColor,
    COLORREF shadowBaseColor)
{
    float shadowRadius = radius * 1.03f;
    float rimRadius = radius * 0.99f;
    float shadowOffsetX = radius * 0.10f;
    float shadowOffsetY = radius * 0.14f;
    float glossRadius = radius * 0.20f;
    float sparkleRadius = radius * 0.03f;
    float glossOffsetX = -radius * 0.22f;
    float glossOffsetY = -radius * 0.24f;
    float sparkleOffsetX = -radius * 0.12f;
    float sparkleOffsetY = -radius * 0.14f;
    COLORREF shadowColor = BlendPieceColor(shadowBaseColor, RGB(116, 122, 132), 0.18f);
    COLORREF rimColor = BlendPieceColor(baseColor, RGB(32, 36, 44), 0.52f);
    COLORREF glossColor = BlendPieceColor(baseColor, RGB(255, 255, 255), 0.45f);
    COLORREF sparkleColor = BlendPieceColor(baseColor, RGB(255, 255, 255), 0.75f);

    Fling_DxColor clrShadow = ColorrefToDxColor(shadowColor, 0.58f);
    Fling_DxColor clrRim = ColorrefToDxColor(rimColor, 0.98f);
    AppendCircle(vertices, count, maxCount, centerX + shadowOffsetX, centerY + shadowOffsetY, shadowRadius, surfaceWidth, surfaceHeight, &clrShadow);
    AppendCircle(vertices, count, maxCount, centerX, centerY, rimRadius, surfaceWidth, surfaceHeight, &clrRim);

    // make the ball out of several layers of blended colors to give it a
    // smooth gradient look without needing a shader
    for (int layer = 0; layer < 7; ++layer) {
        float t = (float)layer / 6.0f;
        float layerRadius = radius * (0.93f - t * 0.38f);
        float offsetX = -radius * (0.02f + t * 0.12f);
        float offsetY = -radius * (0.03f + t * 0.13f);
        float lightAmount = 0.10f + t * 0.07f;
        float alpha = 0.95f - t * 0.08f;
        COLORREF layerColor = BlendPieceColor(baseColor, RGB(255, 255, 255), lightAmount);
        Fling_DxColor clrLayer = ColorrefToDxColor(layerColor, alpha);
        AppendCircle(vertices, count, maxCount, centerX + offsetX, centerY + offsetY, layerRadius, surfaceWidth, surfaceHeight, &clrLayer);
    }

    Fling_DxColor clrGloss = ColorrefToDxColor(glossColor, 0.56f);
    Fling_DxColor clrSparkle = ColorrefToDxColor(sparkleColor, 0.74f);
    AppendCircle(vertices, count, maxCount, centerX + glossOffsetX, centerY + glossOffsetY, glossRadius, surfaceWidth, surfaceHeight, &clrGloss);
    AppendCircle(vertices, count, maxCount, centerX + sparkleOffsetX, centerY + sparkleOffsetY, sparkleRadius, surfaceWidth, surfaceHeight, &clrSparkle);
}

static void AppendVertexEx(
    Fling_BoardVertex *vertices,
    int *count,
    int maxCount,
    float x,
    float y,
    float u,
    float v,
    float useTexture,
    float textureIndex,
    float surfaceWidth,
    float surfaceHeight,
    Fling_DxColor* color)
{
    Fling_BoardVertex *vertex;

    if (*count >= maxCount) {
        return;
    }

    vertex = &vertices[*count];
    vertex->x = x / surfaceWidth * 2.0f - 1.0f;
    vertex->y = 1.0f - y / surfaceHeight * 2.0f;
    vertex->u = u;
    vertex->v = v;
    vertex->useTexture = useTexture;
    vertex->textureIndex = textureIndex;
    vertex->r = color->r;
    vertex->g = color->g;
    vertex->b = color->b;
    vertex->a = color->a;
    (*count)++;
}

static void AppendVertex(Fling_BoardVertex *vertices, int *count, int maxCount, float x, float y, float surfaceWidth, float surfaceHeight, Fling_DxColor* color)
{
    AppendVertexEx(vertices, count, maxCount, x, y, 0.0f, 0.0f, 0.0f, 0.0f, surfaceWidth, surfaceHeight, color);
}

static void AppendRect(Fling_BoardVertex *vertices, int *count, int maxCount, float left, float top, float right, float bottom, float surfaceWidth, float surfaceHeight, Fling_DxColor* color)
{
    AppendVertex(vertices, count, maxCount, left, top, surfaceWidth, surfaceHeight, color);
    AppendVertex(vertices, count, maxCount, right, top, surfaceWidth, surfaceHeight, color);
    AppendVertex(vertices, count, maxCount, left, bottom, surfaceWidth, surfaceHeight, color);
    AppendVertex(vertices, count, maxCount, left, bottom, surfaceWidth, surfaceHeight, color);
    AppendVertex(vertices, count, maxCount, right, top, surfaceWidth, surfaceHeight, color);
    AppendVertex(vertices, count, maxCount, right, bottom, surfaceWidth, surfaceHeight, color);
}

static void AppendTexturedRect(Fling_BoardVertex *vertices, int *count, int maxCount, float left, float top, float right, float bottom, float surfaceWidth, float surfaceHeight, float textureIndex, Fling_DxColor* color)
{
    AppendVertexEx(vertices, count, maxCount, left, top, 0.0f, 0.0f, 1.0f, textureIndex, surfaceWidth, surfaceHeight, color);
    AppendVertexEx(vertices, count, maxCount, right, top, 1.0f, 0.0f, 1.0f, textureIndex, surfaceWidth, surfaceHeight, color);
    AppendVertexEx(vertices, count, maxCount, left, bottom, 0.0f, 1.0f, 1.0f, textureIndex, surfaceWidth, surfaceHeight, color);
    AppendVertexEx(vertices, count, maxCount, left, bottom, 0.0f, 1.0f, 1.0f, textureIndex, surfaceWidth, surfaceHeight, color);
    AppendVertexEx(vertices, count, maxCount, right, top, 1.0f, 0.0f, 1.0f, textureIndex, surfaceWidth, surfaceHeight, color);
    AppendVertexEx(vertices, count, maxCount, right, bottom, 1.0f, 1.0f, 1.0f, textureIndex, surfaceWidth, surfaceHeight, color);
}

#define kSinCosTableSegments        (128)
#define kAngleStep                  (6.28318530718f / (float)kSinCosTableSegments)

static int g_SinCosTableComputed = 0;
static float g_SinTable[kSinCosTableSegments + 1];
static float g_CosTable[kSinCosTableSegments + 1];

static void AppendCircle(Fling_BoardVertex *vertices, int *count, int maxCount, float centerX, float centerY, float radius, float surfaceWidth, float surfaceHeight, Fling_DxColor* color)
{
    if (g_SinCosTableComputed == 0) {
        for (int i = 0; i <= kSinCosTableSegments; ++i) {
            float angle = kAngleStep * (float)i;
            g_SinTable[i] = sinf(angle);
            g_CosTable[i] = cosf(angle);
        }
        g_SinCosTableComputed = 1;
    }

    for (int i = 0; i < kSinCosTableSegments; ++i) {
        float x0 = centerX + g_CosTable[i] * radius;
        float y0 = centerY + g_SinTable[i] * radius;
        float x1 = centerX + g_CosTable[i + 1] * radius;
        float y1 = centerY + g_SinTable[i + 1] * radius;

        AppendVertex(vertices, count, maxCount, centerX, centerY, surfaceWidth, surfaceHeight, color);
        AppendVertex(vertices, count, maxCount, x0, y0, surfaceWidth, surfaceHeight, color);
        AppendVertex(vertices, count, maxCount, x1, y1, surfaceWidth, surfaceHeight, color);
    }
}

static void AppendVictoryFireworks(Fling_BoardVertex *vertices, int *count, int maxCount, RECT board, float surfaceWidth, float surfaceHeight)
{
    DWORD elapsedMs;
    float boardWidth;
    float boardHeight;
    int latestBurstIndex;

    if (!g_victoryFireworksActive || g_victoryFireworksStartTick == 0) {
        return;
    }

    elapsedMs = GetTickCount() - g_victoryFireworksStartTick;
    boardWidth = (float)(board.right - board.left);
    boardHeight = (float)(board.bottom - board.top);
    latestBurstIndex = (int)(elapsedMs / VICTORY_FIREWORK_BURST_INTERVAL_MS);

    for (int burstOffset = 0; burstOffset < 4; ++burstOffset) {
        int burstIndex = latestBurstIndex - burstOffset;
        int burstDelayMs;
        float burstAgeMs;
        float seed;
        float baseX;
        float baseY;
        float t;
        float fade;
        COLORREF burstColor;

        if (burstIndex < 0) {
            continue;
        }

        burstDelayMs = burstIndex * VICTORY_FIREWORK_BURST_INTERVAL_MS;
        burstAgeMs = (float)((int)elapsedMs - burstDelayMs);
        if (burstAgeMs < 0.0f || burstAgeMs > (float)VICTORY_FIREWORK_BURST_LIFETIME_MS) {
            continue;
        }

        seed = 1.3f + (float)burstIndex * 1.618f;
        t = burstAgeMs / (float)VICTORY_FIREWORK_BURST_LIFETIME_MS;
        fade = 1.0f - t;
        baseX = (float)board.left + boardWidth * (0.14f + 0.72f * (0.5f + 0.5f * sinf(seed * 2.13f)));
        baseY = (float)board.top + boardHeight * (0.10f + 0.90f * (0.5f + 0.5f * cosf(seed * 1.71f)));
        burstColor = ComputePieceColor(burstIndex * 3 + 1);

        Fling_DxColor burstDxColor1 = ColorrefToDxColor(BlendPieceColor(burstColor, RGB(255, 255, 255), 0.45f), 0.18f * fade);
        AppendCircle(
            vertices,
            count,
            maxCount,
            baseX,
            baseY,
            (float)ScaleForDpi(20, g_currentDpi) * (1.0f + t * 1.6f),
            surfaceWidth,
            surfaceHeight,
            &burstDxColor1
        );

        for (int particleIndex = 0; particleIndex < 18; ++particleIndex) {
            float angle = ((float)particleIndex / 18.0f) * 6.28318530718f + seed;
            float speedScale = 0.75f + 0.35f * (0.5f + 0.5f * sinf(seed + (float)particleIndex * 2.17f));
            float distance = (float)g_cellSize * (0.35f + 2.45f * t) * speedScale;
            float sparkX = baseX + cosf(angle) * distance;
            float sparkY = baseY + sinf(angle) * distance + t * t * (float)ScaleForDpi(42, g_currentDpi);
            float sparkRadius = (float)ScaleForDpi(4, g_currentDpi) * (1.0f - t * 0.55f);

            if (sparkRadius < 1.0f) {
                sparkRadius = 1.0f;
            }

            Fling_DxColor burstDxColor2 = ColorrefToDxColor(BlendPieceColor(burstColor, RGB(255, 255, 255), 0.30f), 0.90f * fade);
            AppendCircle(
                vertices,
                count,
                maxCount,
                sparkX,
                sparkY,
                sparkRadius,
                surfaceWidth,
                surfaceHeight,
                &burstDxColor2
            );
        }
    }
}

static void AppendOutlineRect(Fling_BoardVertex *vertices, int *count, int maxCount, float left, float top, float right, float bottom, float thickness, float surfaceWidth, float surfaceHeight, Fling_DxColor* color)
{
    AppendRect(vertices, count, maxCount, left, top, right, top + thickness, surfaceWidth, surfaceHeight, color);
    AppendRect(vertices, count, maxCount, left, bottom - thickness, right, bottom, surfaceWidth, surfaceHeight, color);
    AppendRect(vertices, count, maxCount, left, top + thickness, left + thickness, bottom - thickness, surfaceWidth, surfaceHeight, color);
    AppendRect(vertices, count, maxCount, right - thickness, top + thickness, right, bottom - thickness, surfaceWidth, surfaceHeight, color);
}

// A simulated Brownian motion loading animation
static void AppendBoardLoadingAnimation(Fling_BoardVertex *vertices, int *count, int maxCount, RECT board, float surfaceWidth, float surfaceHeight)
{
    float width = (float)(board.right - board.left);
    float height = (float)(board.bottom - board.top);
    float minDimension = (width < height) ? width : height;
    float radius = (float)(g_cellSize / 2 - ScaleForDpi(10, g_currentDpi));
    float minX = (float)board.left + radius;
    float maxX = (float)board.right - radius;
    float minY = (float)board.top + radius;
    float maxY = (float)board.bottom - radius;
    DWORD now = GetTickCount();
    int ballCount = g_loadingBallCount;

    if (radius < minDimension * 0.028f) {
        radius = minDimension * 0.028f;
    }
    if (radius > minDimension * 0.065f) {
        radius = minDimension * 0.065f;
    }

    if (ballCount < 0) {
        ballCount = 0;
    }
    if (ballCount > MAX_LOADING_BALLS) {
        ballCount = MAX_LOADING_BALLS;
    }
    if (ballCount <= 0) {
        return;
    }

    if (maxX < minX) {
        maxX = minX;
    }
    if (maxY < minY) {
        maxY = minY;
    }

    if (g_loadingBallsSettling) {
        float t = 1.0f;

        if (LOADING_BALL_SETTLE_DURATION_MS > 0) {
            t = ClampUnitFloat((float)(now - g_loadingBallSettleStartTick) / (float)LOADING_BALL_SETTLE_DURATION_MS);
        }

        const float easedT = t * t * (3.0f - 2.0f * t);
        for (int i = 0; i < ballCount; ++i) {
            g_loadingBallPosX[i] = g_loadingBallStartX[i] + (g_loadingBallTargetX[i] - g_loadingBallStartX[i]) * easedT;
            g_loadingBallPosY[i] = g_loadingBallStartY[i] + (g_loadingBallTargetY[i] - g_loadingBallStartY[i]) * easedT;
        }
    } else if (!g_loadingAnimationInitialized || g_generateAnimationLastTick == 0) {
        for (int i = 0; i < ballCount; ++i) {
            float xT = 0.08f + 0.84f * HashBoardTextureNoise();
            float yT = 0.08f + 0.84f * HashBoardTextureNoise();

            unsigned int seed;
            seed = randx() & 1023u;
            float dirX = ((float)seed / 511.5f) - 1.0f;
            seed = randx() & 1023u;
            float dirY = ((float)seed / 511.5f) - 1.0f;

            if (fabsf(dirX) + fabsf(dirY) < 0.25f) {
                dirX = 0.82f;
                dirY = -0.57f;
            }

            g_loadingBallPieceIds[i] = i + 1;
            g_loadingBallPosX[i] = minX + (maxX - minX) * xT;
            g_loadingBallPosY[i] = minY + (maxY - minY) * yT;
            g_loadingBallVelX[i] = dirX * (minDimension * (0.34f + 0.03f * (float)i));
            g_loadingBallVelY[i] = dirY * (minDimension * (0.34f + 0.03f * (float)i));
        }

        g_loadingAnimationInitialized = TRUE;
        g_generateAnimationLastTick = now;
    } else {
        float dt = (float)(now - g_generateAnimationLastTick) / 1000.0f;
        g_generateAnimationLastTick = now;

        if (dt < 0.001f) {
            dt = 0.001f;
        }
        if (dt > 0.05f) {
            dt = 0.05f;
        }

        for (int i = 0; i < ballCount; ++i) {
            float minSpeed = minDimension * 0.54f;
            float maxSpeed = minDimension * 0.95f;

            unsigned int noise;
            noise = randx() & 1023u;
            float jitterX = ((float)noise / 511.5f) - 1.0f;
            noise = randx() & 1023u;
            float jitterY = ((float)noise / 511.5f) - 1.0f;

            jitterX += 0.22f * sinf((float)now * 0.010f + (float)i * 0.91f);
            jitterY += 0.22f * cosf((float)now * 0.012f + (float)i * 1.17f);

            g_loadingBallVelX[i] += jitterX * minDimension * 5.10f * dt;
            g_loadingBallVelY[i] += jitterY * minDimension * 5.10f * dt;
            g_loadingBallVelX[i] *= 0.978f;
            g_loadingBallVelY[i] *= 0.978f;

            float speed = sqrtf(g_loadingBallVelX[i] * g_loadingBallVelX[i] + g_loadingBallVelY[i] * g_loadingBallVelY[i]);
            if (speed < minSpeed) {
                if (speed > 0.0001f) {
                    float scale = minSpeed / speed;
                    g_loadingBallVelX[i] *= scale;
                    g_loadingBallVelY[i] *= scale;
                } else {
                    g_loadingBallVelX[i] = minSpeed;
                    g_loadingBallVelY[i] = 0.0f;
                }
            } else if (speed > maxSpeed) {
                float scale = maxSpeed / speed;
                g_loadingBallVelX[i] *= scale;
                g_loadingBallVelY[i] *= scale;
            }

            g_loadingBallPosX[i] += g_loadingBallVelX[i] * dt;
            g_loadingBallPosY[i] += g_loadingBallVelY[i] * dt;

            if (g_loadingBallPosX[i] < minX) {
                g_loadingBallPosX[i] = minX + (minX - g_loadingBallPosX[i]);
                g_loadingBallVelX[i] = fabsf(g_loadingBallVelX[i]) * 0.94f;
            } else if (g_loadingBallPosX[i] > maxX) {
                g_loadingBallPosX[i] = maxX - (g_loadingBallPosX[i] - maxX);
                g_loadingBallVelX[i] = -fabsf(g_loadingBallVelX[i]) * 0.94f;
            }

            if (g_loadingBallPosY[i] < minY) {
                g_loadingBallPosY[i] = minY + (minY - g_loadingBallPosY[i]);
                g_loadingBallVelY[i] = fabsf(g_loadingBallVelY[i]) * 0.94f;
            } else if (g_loadingBallPosY[i] > maxY) {
                g_loadingBallPosY[i] = maxY - (g_loadingBallPosY[i] - maxY);
                g_loadingBallVelY[i] = -fabsf(g_loadingBallVelY[i]) * 0.94f;
            }
        }
    }

    EnsurePieceColorCount(ballCount);
    for (int i = 0; i < ballCount; ++i) {
        AppendPieceBall(
            vertices,
            count,
            maxCount,
            g_loadingBallPosX[i],
            g_loadingBallPosY[i],
            radius,
            surfaceWidth,
            surfaceHeight,
            GetPieceColor(g_loadingBallPieceIds[i]),
            GetBoardSquareColorAtPoint(g_loadingBallPosX[i], g_loadingBallPosY[i])
        );
    }
}

static void ApplyLoadingTextAlphaMask(void *bits, size_t pixelCount)
{
    unsigned char *bytePixels = (unsigned char *)bits;
    size_t i = 0;

#if defined(__AVX2__) || defined(_M_AVX2)
    {
        const __m256i colorMask = _mm256_set1_epi32(0x00FFFFFF);
        const __m256i byteMask = _mm256_set1_epi32(0x000000FF);
        const __m256 alphaScale = _mm256_set1_ps(0.8f);

        for (; i + 8 <= pixelCount; i += 8) {
            __m256i pixels = _mm256_loadu_si256((const __m256i *)(bytePixels + i * 4));
            __m256i blue = _mm256_and_si256(pixels, byteMask);
            __m256i green = _mm256_and_si256(_mm256_srli_epi32(pixels, 8), byteMask);
            __m256i red = _mm256_and_si256(_mm256_srli_epi32(pixels, 16), byteMask);
            __m256i alpha = _mm256_max_epi32(_mm256_max_epi32(blue, green), red);
            __m256i scaledAlpha = _mm256_cvtps_epi32(_mm256_mul_ps(_mm256_cvtepi32_ps(alpha), alphaScale));
            __m256i packed = _mm256_or_si256(_mm256_and_si256(pixels, colorMask), _mm256_slli_epi32(scaledAlpha, 24));

            _mm256_storeu_si256((__m256i *)(bytePixels + i * 4), packed);
        }
    }
#endif

    for (; i < pixelCount; ++i) {
        unsigned char *pixel = &bytePixels[i * 4];
        unsigned int alpha = pixel[0];

        if (pixel[1] > alpha) {
            alpha = pixel[1];
        }
        if (pixel[2] > alpha) {
            alpha = pixel[2];
        }

        pixel[3] = (unsigned char)((alpha * 204u + 127u) / 255u);
    }
}

#define kBoardSquareTextureSize (24)

static HRESULT CreateBoardSquareTextureView(COLORREF baseColor, BOOL darkVariant, ID3D11ShaderResourceView **outTextureView)
{
    BYTE pixels[kBoardSquareTextureSize * kBoardSquareTextureSize * 4];
    COLORREF lightTint = BlendPieceColor(baseColor, RGB(255, 255, 255), darkVariant ? 0.18f : 0.16f);
    COLORREF darkTint = BlendPieceColor(baseColor, RGB(14, 24, 14), darkVariant ? 0.16f : 0.18f);
    D3D11_TEXTURE2D_DESC textureDesc;
    D3D11_SUBRESOURCE_DATA textureData;
    HRESULT hr;

    *outTextureView = NULL;

    for (int y = 0; y < kBoardSquareTextureSize; ++y) {
        for (int x = 0; x < kBoardSquareTextureSize; ++x) {
            int pixelIndex = (y * kBoardSquareTextureSize + x) * 4;
            float u = (float)x / (float)(kBoardSquareTextureSize - 1);
            float v = (float)y / (float)(kBoardSquareTextureSize - 1);
            float lightAmount = (1.0f - (u * 0.62f + v * 0.38f)) * (darkVariant ? 0.14f : 0.12f);
            float shadowAmount = (u * 0.30f + v * 0.70f) * (darkVariant ? 0.03f : 0.04f);
            float speckle = HashBoardTextureNoise();
            COLORREF pixelColor = BlendPieceColor(baseColor, lightTint, 0.03f + lightAmount);

            pixelColor = BlendPieceColor(pixelColor, darkTint, shadowAmount);

            if (speckle > 0.80f) {
                pixelColor = BlendPieceColor(pixelColor, lightTint, 0.28f + (speckle - 0.80f) * 0.40f);
            } else if (speckle < 0.18f) {
                pixelColor = BlendPieceColor(pixelColor, darkTint, 0.12f + (0.18f - speckle) * 0.28f);
            }

            if (((x + y + (darkVariant ? 1 : 0)) % 9) == 0) {
                pixelColor = BlendPieceColor(pixelColor, darkTint, 0.03f);
            }

            *(DWORD*)(pixels + pixelIndex) = 0xFF000000 | pixelColor;
        }
    }

    ZeroMem(&textureDesc, sizeof(textureDesc));
    textureDesc.Width = kBoardSquareTextureSize;
    textureDesc.Height = kBoardSquareTextureSize;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureData.pSysMem = pixels;
    textureData.SysMemPitch = kBoardSquareTextureSize * 4;
    textureData.SysMemSlicePitch = 0;

    ID3D11Texture2D *texture;
    hr = g_d3dDevice->lpVtbl->CreateTexture2D(g_d3dDevice, &textureDesc, &textureData, &texture);
    hr = g_d3dDevice->lpVtbl->CreateShaderResourceView(g_d3dDevice, (ID3D11Resource *)texture, NULL, outTextureView);

    SAFE_RELEASE(texture);
    return hr;
}

static HRESULT CreateLoadingTextTextureView(ID3D11ShaderResourceView **outTextureView)
{
    RECT board;
    GetBoardRect(&board);
    const char *overlayText = g_showSolvedOverlay ? kSolvedOverlayText : kGeneratingOverlayText;
    int width = board.right - board.left;
    int height = ScaleForDpi(54, g_currentDpi);

    BITMAPINFO bitmapInfo;
    ZeroMem(&bitmapInfo, sizeof(bitmapInfo));
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    HDC memDC = CreateCompatibleDC(NULL);
    void *bits = NULL;
    HBITMAP dib = CreateDIBSection(memDC, &bitmapInfo, DIB_RGB_COLORS, &bits, NULL, 0);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, dib);
    HFONT oldFont = (HFONT)SelectObject(memDC, g_titleFont);
    SetBkMode(memDC, TRANSPARENT);

    RECT drawRect;
    drawRect.left = 0;
    drawRect.top = 0;
    drawRect.right = width;
    drawRect.bottom = height;

    int shadowOffset = ScaleForDpi(2, g_currentDpi);

    SetTextColor(memDC, RGB(70, 55, 0));

    drawRect.left += shadowOffset;
    drawRect.top += shadowOffset;
    DrawTextA(memDC, overlayText, -1, &drawRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    drawRect.left -= shadowOffset;
    drawRect.top -= shadowOffset;
    SetTextColor(memDC, g_showSolvedOverlay ? RGB(128, 235, 255) : RGB(255, 235, 0));
    DrawTextA(memDC, overlayText, -1, &drawRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    ApplyLoadingTextAlphaMask(bits, (size_t)width * (size_t)height);

    D3D11_TEXTURE2D_DESC textureDesc;
    ZeroMem(&textureDesc, sizeof(textureDesc));
    textureDesc.Width = (UINT)width;
    textureDesc.Height = (UINT)height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA textureData;
    textureData.pSysMem = bits;
    textureData.SysMemPitch = (UINT)(width * 4);
    textureData.SysMemSlicePitch = 0;

    HRESULT hr;
    ID3D11Texture2D *texture;
    hr = g_d3dDevice->lpVtbl->CreateTexture2D(g_d3dDevice, &textureDesc, &textureData, &texture);
    hr = g_d3dDevice->lpVtbl->CreateShaderResourceView(g_d3dDevice, (ID3D11Resource *)texture, NULL, outTextureView);

    SAFE_RELEASE(texture);
    SelectObject(memDC, oldFont);
    SelectObject(memDC, oldBmp);
    DeleteObject(dib);
    DeleteDC(memDC);
    return hr;
}

static HRESULT EnsureBoardTextures(void)
{
    HRESULT hr;

    if (g_d3dLightSquareTextureView != NULL && g_d3dDarkSquareTextureView != NULL && g_d3dGridLineTextureView != NULL && g_d3dLoadingTextTextureView != NULL && g_d3dSamplerState != NULL && g_d3dBlendState != NULL) {
        return S_OK;
    }

    if (g_d3dLightSquareTextureView == NULL) {
        hr = CreateBoardSquareTextureView(kLightSquareColor, FALSE, &g_d3dLightSquareTextureView);
    }

    if (g_d3dDarkSquareTextureView == NULL) {
        hr = CreateBoardSquareTextureView(kDarkSquareColor, TRUE, &g_d3dDarkSquareTextureView);
    }

    if (g_d3dGridLineTextureView == NULL) {
        hr = CreateBoardSquareTextureView(RGB(52, 70, 40), TRUE, &g_d3dGridLineTextureView);
    }

    if (g_d3dLoadingTextTextureView == NULL) {
        hr = CreateLoadingTextTextureView(&g_d3dLoadingTextTextureView);
    }

    if (g_d3dSamplerState == NULL) {
        D3D11_SAMPLER_DESC samplerDesc;
        ZeroMem(&samplerDesc, sizeof(samplerDesc));
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = g_d3dDevice->lpVtbl->CreateSamplerState(g_d3dDevice, &samplerDesc, &g_d3dSamplerState);
    }

    if (g_d3dBlendState == NULL) {
        D3D11_BLEND_DESC blendDesc;
        ZeroMem(&blendDesc, sizeof(blendDesc));
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = g_d3dDevice->lpVtbl->CreateBlendState(g_d3dDevice, &blendDesc, &g_d3dBlendState);
    }

    return S_OK;
}

static HRESULT CreateBoardRenderTarget(void)
{
    ID3D11Texture2D *backBuffer;
    HRESULT hr;

    hr = g_d3dSwapChain->lpVtbl->GetBuffer(g_d3dSwapChain, 0, &IID_ID3D11Texture2D, (void **)&backBuffer);
    hr = g_d3dDevice->lpVtbl->CreateRenderTargetView(g_d3dDevice, (ID3D11Resource *)backBuffer, NULL, &g_d3dRenderTargetView);
    SAFE_RELEASE(backBuffer);
    return hr;
}

__packedstr(kBoardShader, "BoardShader");
__packedstr(kVSMain, "VSMain");
__packedstr(kPSMain, "PSMain");
__packedstr(kVs40, "vs_4_0");
__packedstr(kPs40, "ps_4_0");
__packedstr(kPosition, "POSITION");
__packedstr(kTexCoord, "TEXCOORD");
__packedstr(kColor, "COLOR");

static HRESULT EnsureBoardShaders(void)
{
    static __packed const char shaderSource[] =
        "struct VSInput { float2 pos : POSITION; float2 uv : TEXCOORD0; float2 texInfo : TEXCOORD1; float4 color : COLOR; };"
        "struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float2 texInfo : TEXCOORD1; float4 color : COLOR; };"
        "Texture2D lightTexture : register(t0);"
        "Texture2D darkTexture : register(t1);"
        "Texture2D gridTexture : register(t2);"
        "Texture2D loadingTextTexture : register(t3);"
        "SamplerState boardSampler : register(s0);"
        "PSInput VSMain(VSInput input) { PSInput output; output.pos = float4(input.pos, 0.0, 1.0); output.uv = input.uv; output.texInfo = input.texInfo; output.color = input.color; return output; }"
        "float4 PSMain(PSInput input) : SV_TARGET { if (input.texInfo.x > 0.5) { float4 texel; if (input.texInfo.y > 2.5) texel = loadingTextTexture.Sample(boardSampler, input.uv); else if (input.texInfo.y > 1.5) texel = gridTexture.Sample(boardSampler, input.uv); else if (input.texInfo.y > 0.5) texel = darkTexture.Sample(boardSampler, input.uv); else texel = lightTexture.Sample(boardSampler, input.uv); return texel * input.color; } return input.color; }";
    HRESULT hr;

    if (g_d3dVertexShader != NULL && g_d3dPixelShader != NULL && g_d3dInputLayout != NULL && g_d3dVertexBuffer != NULL && g_d3dLightSquareTextureView != NULL && g_d3dDarkSquareTextureView != NULL && g_d3dGridLineTextureView != NULL && g_d3dLoadingTextTextureView != NULL && g_d3dSamplerState != NULL && g_d3dBlendState != NULL) {
        return S_OK;
    }

    ID3DBlob *vertexShaderBlob;
    hr = D3DCompile(
        shaderSource,
        sizeof(shaderSource) - 1,
        kBoardShader,
        NULL,
        NULL,
        kVSMain,
        kVs40,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        &vertexShaderBlob,
        NULL
    );

    ID3DBlob *pixelShaderBlob;
    hr = D3DCompile(
        shaderSource,
        sizeof(shaderSource) - 1,
        kBoardShader,
        NULL,
        NULL,
        kPSMain,
        kPs40,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        &pixelShaderBlob,
        NULL
    );

    if (g_d3dVertexShader == NULL) {
        hr = g_d3dDevice->lpVtbl->CreateVertexShader(
            g_d3dDevice,
            vertexShaderBlob->lpVtbl->GetBufferPointer(vertexShaderBlob),
            vertexShaderBlob->lpVtbl->GetBufferSize(vertexShaderBlob),
            NULL,
            &g_d3dVertexShader
        );
    }

    if (g_d3dPixelShader == NULL) {
        hr = g_d3dDevice->lpVtbl->CreatePixelShader(
            g_d3dDevice,
            pixelShaderBlob->lpVtbl->GetBufferPointer(pixelShaderBlob),
            pixelShaderBlob->lpVtbl->GetBufferSize(pixelShaderBlob),
            NULL,
            &g_d3dPixelShader
        );
    }

    if (g_d3dInputLayout == NULL) {
        D3D11_INPUT_ELEMENT_DESC inputElements[4];
        inputElements[0].SemanticName = kPosition;
        inputElements[0].SemanticIndex = 0;
        inputElements[0].Format = DXGI_FORMAT_R32G32_FLOAT;
        inputElements[0].InputSlot = 0;
        inputElements[0].AlignedByteOffset = 0;
        inputElements[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        inputElements[0].InstanceDataStepRate = 0;

        inputElements[1].SemanticName = kTexCoord;
        inputElements[1].SemanticIndex = 0;
        inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
        inputElements[1].InputSlot = 0;
        inputElements[1].AlignedByteOffset = sizeof(float) * 2;
        inputElements[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        inputElements[1].InstanceDataStepRate = 0;

        inputElements[2].SemanticName = kTexCoord;
        inputElements[2].SemanticIndex = 1;
        inputElements[2].Format = DXGI_FORMAT_R32G32_FLOAT;
        inputElements[2].InputSlot = 0;
        inputElements[2].AlignedByteOffset = sizeof(float) * 4;
        inputElements[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        inputElements[2].InstanceDataStepRate = 0;

        inputElements[3].SemanticName = kColor;
        inputElements[3].SemanticIndex = 0;
        inputElements[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        inputElements[3].InputSlot = 0;
        inputElements[3].AlignedByteOffset = sizeof(float) * 6;
        inputElements[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        inputElements[3].InstanceDataStepRate = 0;

        hr = g_d3dDevice->lpVtbl->CreateInputLayout(
            g_d3dDevice,
            inputElements,
            4,
            vertexShaderBlob->lpVtbl->GetBufferPointer(vertexShaderBlob),
            vertexShaderBlob->lpVtbl->GetBufferSize(vertexShaderBlob),
            &g_d3dInputLayout
        );
    }

    SAFE_RELEASE(vertexShaderBlob);
    SAFE_RELEASE(pixelShaderBlob);

    if (g_d3dVertexBuffer == NULL) {
        D3D11_BUFFER_DESC bufferDesc;
        ZeroMem(&bufferDesc, sizeof(bufferDesc));
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.ByteWidth = sizeof(Fling_BoardVertex) * BOARD_VERTEX_CAPACITY;
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = g_d3dDevice->lpVtbl->CreateBuffer(g_d3dDevice, &bufferDesc, NULL, &g_d3dVertexBuffer);
    }

    return EnsureBoardTextures();
}

static HRESULT EnsureBoardRenderer(HWND hwnd)
{
    if (g_d3dDevice != NULL && g_d3dSwapChain != NULL) {
        if (g_d3dRenderTargetView == NULL) {
            HRESULT hr = CreateBoardRenderTarget();
            if (FAILED(hr)) {
                return hr;
            }
        }
        return EnsureBoardShaders();
    }

    RECT client;
    GetClientRect(hwnd, &client);

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ZeroMem(&swapChainDesc, sizeof(swapChainDesc));
    swapChainDesc.BufferDesc.Width = client.right;
    swapChainDesc.BufferDesc.Height = client.bottom;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.Windowed = TRUE;
    // board presentation separated from the parent's GDI redirection surface
    // required by borderless + DWM rounded corner
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    const D3D_FEATURE_LEVEL wantedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        0,
        &wantedFeatureLevel,
        1,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &g_d3dSwapChain,
        &g_d3dDevice,
        NULL,
        &g_d3dContext
    );
    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            NULL,
            D3D_DRIVER_TYPE_WARP,
            NULL,
            0,
            &wantedFeatureLevel,
            1,
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &g_d3dSwapChain,
            &g_d3dDevice,
            NULL,
            &g_d3dContext
        );
    }

    hr = CreateBoardRenderTarget();
    return EnsureBoardShaders();
}

static void ResizeBoardRenderer(UINT width, UINT height)
{
    g_d3dContext->lpVtbl->OMSetRenderTargets(g_d3dContext, 0, NULL, NULL);
    SAFE_RELEASE(g_d3dRenderTargetView);
    SAFE_RELEASE(g_d3dLoadingTextTextureView);

    g_d3dSwapChain->lpVtbl->ResizeBuffers(g_d3dSwapChain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateBoardRenderTarget();
}

static void DestroyBoardRenderer(void)
{
    if (g_d3dContext != NULL) {
        g_d3dContext->lpVtbl->ClearState(g_d3dContext);
    }

    SAFE_RELEASE(g_d3dBlendState);
    SAFE_RELEASE(g_d3dSamplerState);
    SAFE_RELEASE(g_d3dLoadingTextTextureView);
    SAFE_RELEASE(g_d3dGridLineTextureView);
    SAFE_RELEASE(g_d3dDarkSquareTextureView);
    SAFE_RELEASE(g_d3dLightSquareTextureView);
    SAFE_RELEASE(g_d3dVertexBuffer);
    SAFE_RELEASE(g_d3dInputLayout);
    SAFE_RELEASE(g_d3dPixelShader);
    SAFE_RELEASE(g_d3dVertexShader);
    SAFE_RELEASE(g_d3dRenderTargetView);
    SAFE_RELEASE(g_d3dSwapChain);
    SAFE_RELEASE(g_d3dContext);
    SAFE_RELEASE(g_d3dDevice);
}

static __packed float blendFactor[4];// automatically zeroed = { 0.0f, 0.0f, 0.0f, 0.0f };
static __packed const float clearColor[4] = { 245.0f / 255.0f, 246.0f / 255.0f, 250.0f / 255.0f, 1.0f };
static Fling_BoardVertex vertices[BOARD_VERTEX_CAPACITY];

static void RenderBoardWindowInternal(BOOL presentSwapChain)
{

    EnsureBoardRenderer(g_boardWindow);

    RECT client;
    GetClientRect(g_boardWindow, &client);
    float surfaceWidth = (float)((client.right > 0) ? client.right : 1);
    float surfaceHeight = (float)((client.bottom > 0) ? client.bottom : 1);

    D3D11_VIEWPORT viewport;
    ZeroMem(&viewport, sizeof(viewport));
    viewport.Width = surfaceWidth;
    viewport.Height = surfaceHeight;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    g_d3dContext->lpVtbl->RSSetViewports(g_d3dContext, 1, &viewport);
    g_d3dContext->lpVtbl->OMSetRenderTargets(g_d3dContext, 1, &g_d3dRenderTargetView, NULL);
    g_d3dContext->lpVtbl->ClearRenderTargetView(g_d3dContext, g_d3dRenderTargetView, clearColor);

    RECT board;
    GetBoardRect(&board);
    int piecePadding = ScaleForDpi(10, g_currentDpi);
    float gridThickness = (float)ScaleForDpi(3, g_currentDpi);
    int vertexCount = 0;

    Fling_DxColor clrWhite = ColorrefToDxColor(RGB(255, 255, 255), 1.0f);
    for (int row = 0; row < BOARD_ROWS; ++row) {
        for (int col = 0; col < BOARD_COLS; ++col) {
            float left = (float)(board.left + col * g_cellSize);
            float top = (float)(board.top + row * g_cellSize);
            float right = left + (float)g_cellSize;
            float bottom = top + (float)g_cellSize;
            float insetLeft = (col == 0) ? gridThickness : gridThickness * 0.5f;
            float insetTop = (row == 0) ? gridThickness : gridThickness * 0.5f;
            float insetRight = (col == BOARD_COLS - 1) ? gridThickness : gridThickness * 0.5f;
            float insetBottom = (row == BOARD_ROWS - 1) ? gridThickness : gridThickness * 0.5f;
            COLORREF cellColor = ((row + col) % 2 == 0) ? kLightSquareColor : kDarkSquareColor;

            AppendTexturedRect(vertices, &vertexCount, BOARD_VERTEX_CAPACITY, left, top, right, bottom, surfaceWidth, surfaceHeight, 2.0f, &clrWhite);
            AppendTexturedRect(vertices, &vertexCount, BOARD_VERTEX_CAPACITY, left + insetLeft, top + insetTop, right - insetRight, bottom - insetBottom, surfaceWidth, surfaceHeight, ((row + col) % 2 == 0) ? 0.0f : 1.0f, &clrWhite);

            if (!g_isGeneratingBoard && g_board.cells[row][col] != 0 && !(g_isAnimating && (g_board.cells[row][col] == g_animationPieceId || g_board.cells[row][col] == g_collisionAnimationPieceId))) {
                float centerX = left + (float)g_cellSize * 0.5f;
                float centerY = top + (float)g_cellSize * 0.5f;
                float outerRadius = (float)(g_cellSize / 2 - piecePadding);

                AppendPieceBall(vertices, &vertexCount, BOARD_VERTEX_CAPACITY, centerX, centerY, outerRadius, surfaceWidth, surfaceHeight, GetPieceColor(g_board.cells[row][col]), cellColor);
            }
        }
    }

    if (g_isGeneratingBoard || g_showSolvedOverlay) {
        float loadingTextHeight = (float)ScaleForDpi(54, g_currentDpi);
        float loadingTextWidth = (float)(board.right - board.left);
        float loadingTextCenterX = (float)(board.left + board.right) * 0.5f;
        float loadingTextCenterY = (float)(board.top + board.bottom) * 0.5f;
        float loadingTextScale = 1.0f;
        float loadingTextAlpha = 1.0f;

        if (g_isGeneratingBoard) {
            AppendBoardLoadingAnimation(vertices, &vertexCount, BOARD_VERTEX_CAPACITY, board, surfaceWidth, surfaceHeight);
        }

        if (g_isGeneratingBoard && g_loadingBallsSettling) {
            float t = 1.0f;
            float easedT;

            if (LOADING_BALL_SETTLE_DURATION_MS > 0) {
                t = ClampUnitFloat((float)(GetTickCount() - g_loadingBallSettleStartTick) / (float)LOADING_BALL_SETTLE_DURATION_MS);
            }

            easedT = t * t * (3.0f - 2.0f * t);

            // Scale from 1.0 to 6.0 over the course of the animation, while fading out to 0 alpha.
            loadingTextScale = 1.0f + 5.0f * easedT;
            loadingTextAlpha = 1.0f - t * t;
        } else if (g_showSolvedOverlay && g_solvedOverlayStartTick != 0) {
            float t = 1.0f;
            float easedT;

            if (SOLVED_OVERLAY_DURATION_MS > 0) {
                t = ClampUnitFloat((float)(GetTickCount() - g_solvedOverlayStartTick) / (float)SOLVED_OVERLAY_DURATION_MS);
            }

            easedT = t * t * (3.0f - 2.0f * t);
            loadingTextScale = 1.0f + 5.0f * easedT;
            loadingTextAlpha = 1.0f - t * t;
        }

        if (loadingTextAlpha > 0.01f) {
            float drawWidth = loadingTextWidth * loadingTextScale;
            float drawHeight = loadingTextHeight * loadingTextScale;

            Fling_DxColor clrLoadingText = ColorrefToDxColor(RGB(255, 255, 255), loadingTextAlpha);
            AppendTexturedRect(
                vertices,
                &vertexCount,
                BOARD_VERTEX_CAPACITY,
                loadingTextCenterX - drawWidth * 0.5f,
                loadingTextCenterY - drawHeight * 0.5f,
                loadingTextCenterX + drawWidth * 0.5f,
                loadingTextCenterY + drawHeight * 0.5f,
                surfaceWidth,
                surfaceHeight,
                3.0f,
                &clrLoadingText
            );
        }
    }

    if (!g_isGeneratingBoard && g_isAnimating && g_animationPieceId != 0) {
        float startLeft = (float)(board.left + g_animationStartCol * g_cellSize);
        float startTop = (float)(board.top + g_animationStartRow * g_cellSize);
        float endLeft = (float)(board.left + g_animationEndCol * g_cellSize);
        float endTop = (float)(board.top + g_animationEndRow * g_cellSize);
        float currentLeft;
        float currentTop;
        float fromLeft = g_animationMovingForward ? startLeft : endLeft;
        float fromTop = g_animationMovingForward ? startTop : endTop;
        float toLeft = g_animationMovingForward ? endLeft : startLeft;
        float toTop = g_animationMovingForward ? endTop : startTop;
        float drawProgress = ClampUnitFloat(g_animationProgress);
        float impactBumpOffset = 0.0f;
        float contactPush = 0.0f;
        float moveDirX = 0.0f;
        float moveDirY = 0.0f;
        int collisionDistanceCells = 1;
        float pieceRadius = (float)(g_cellSize / 2 - piecePadding);
        float markerOuterRadius = (float)ScaleForDpi(10, g_currentDpi);
        float markerInnerRadius = (float)ScaleForDpi(9, g_currentDpi);

        if (markerOuterRadius > (float)g_cellSize * 0.22f) {
            markerOuterRadius = (float)g_cellSize * 0.22f;
        }
        if (markerInnerRadius > markerOuterRadius - 1.0f) {
            markerInnerRadius = markerOuterRadius - 1.0f;
        }
        if (markerInnerRadius < 1.0f) {
            markerInnerRadius = 1.0f;
        }

        if (g_collisionAnimationPieceId != 0) {
            float contactOffset = (float)g_cellSize - pieceRadius * 2.0f;
            float contactProgress = g_animationMovingForward ? g_animationProgress : (1.0f - g_animationProgress);

            collisionDistanceCells = abs(g_collisionStartRow - g_animationStartRow) + abs(g_collisionStartCol - g_animationStartCol);
            if (g_animationMovingForward) {
                drawProgress = ClampUnitFloat(g_animationProgress * 2.0f);
                impactBumpOffset = GetCollisionBumpOffset(g_animationProgress, collisionDistanceCells);
            } else {
                drawProgress = ClampUnitFloat((g_animationProgress - 0.5f) * 2.0f);
                impactBumpOffset = GetCollisionBumpOffset(1.0f - g_animationProgress, collisionDistanceCells);
            }

            if (contactProgress < 0.5f) {
                contactPush = ClampUnitFloat(contactProgress * 2.0f) * contactOffset;
            } else {
                contactPush = (1.0f - ClampUnitFloat((contactProgress - 0.5f) * 2.0f)) * contactOffset;
            }
        }

        if (toLeft > fromLeft) {
            moveDirX = 1.0f;
        } else if (toLeft < fromLeft) {
            moveDirX = -1.0f;
        }
        if (toTop > fromTop) {
            moveDirY = 1.0f;
        } else if (toTop < fromTop) {
            moveDirY = -1.0f;
        }

        currentLeft = fromLeft + (toLeft - fromLeft) * drawProgress + moveDirX * (contactPush - impactBumpOffset);
        currentTop = fromTop + (toTop - fromTop) * drawProgress + moveDirY * (contactPush - impactBumpOffset);

        AppendPieceBall(
            vertices,
            &vertexCount,
            BOARD_VERTEX_CAPACITY,
            currentLeft + (float)g_cellSize * 0.5f,
            currentTop + (float)g_cellSize * 0.5f,
            pieceRadius,
            surfaceWidth,
            surfaceHeight,
            GetPieceColor(g_animationPieceId),
            GetBoardSquareColorAtPoint(currentLeft + (float)g_cellSize * 0.5f, currentTop + (float)g_cellSize * 0.5f)
        );
        if (g_animationPieceId == g_animationOriginPieceId) {
            Fling_DxColor clrMarkerOuter = ColorrefToDxColor(RGB(60, 60, 60), 0.65f);
            Fling_DxColor clrMarkerInner = ColorrefToDxColor(RGB(200, 200, 200), 0.8f);
            AppendCircle(vertices, &vertexCount, BOARD_VERTEX_CAPACITY, currentLeft + (float)g_cellSize * 0.5f, currentTop + (float)g_cellSize * 0.5f, markerOuterRadius, surfaceWidth, surfaceHeight, &clrMarkerOuter);
            AppendCircle(vertices, &vertexCount, BOARD_VERTEX_CAPACITY, currentLeft + (float)g_cellSize * 0.5f, currentTop + (float)g_cellSize * 0.5f, markerInnerRadius, surfaceWidth, surfaceHeight, &clrMarkerInner);
        }
    }

    if (!g_isGeneratingBoard && g_isAnimating && g_collisionAnimationPieceId != 0) {
        float startLeft = (float)(board.left + g_collisionStartCol * g_cellSize);
        float startTop = (float)(board.top + g_collisionStartRow * g_cellSize);
        float endLeft = (float)(board.left + g_collisionEndCol * g_cellSize);
        float endTop = (float)(board.top + g_collisionEndRow * g_cellSize);
        float currentLeft;
        float currentTop;
        float fromLeft = g_animationMovingForward ? startLeft : endLeft;
        float fromTop = g_animationMovingForward ? startTop : endTop;
        float toLeft = g_animationMovingForward ? endLeft : startLeft;
        float toTop = g_animationMovingForward ? endTop : startTop;
        float drawProgress;

        if (g_animationMovingForward) {
            drawProgress = (g_animationProgress - 0.5f) * 2.0f;
        } else {
            drawProgress = g_animationProgress * 2.0f;
        }

        if (drawProgress < 0.0f) {
            drawProgress = 0.0f;
        }
        if (drawProgress > 1.0f) {
            drawProgress = 1.0f;
        }

        currentLeft = fromLeft + (toLeft - fromLeft) * drawProgress;
        currentTop = fromTop + (toTop - fromTop) * drawProgress;

        AppendPieceBall(
            vertices,
            &vertexCount,
            BOARD_VERTEX_CAPACITY,
            currentLeft + (float)g_cellSize * 0.5f,
            currentTop + (float)g_cellSize * 0.5f,
            (float)(g_cellSize / 2 - piecePadding),
            surfaceWidth,
            surfaceHeight,
            GetPieceColor(g_collisionAnimationPieceId),
            GetBoardSquareColorAtPoint(currentLeft + (float)g_cellSize * 0.5f, currentTop + (float)g_cellSize * 0.5f)
        );
    }

#if 0
    if (!g_isGeneratingBoard && g_mouseDown && InBounds(g_mouseRow, g_mouseCol) && g_board.cells[g_mouseRow][g_mouseCol] != 0) {
        int highlightInset = ScaleForDpi(3, g_currentDpi);
        int highlightWidth = ScaleForDpi(3, g_currentDpi);
        float left = (float)(board.left + g_mouseCol * g_cellSize + highlightInset);
        float top = (float)(board.top + g_mouseRow * g_cellSize + highlightInset);
        float right = left + (float)(g_cellSize - highlightInset * 2);
        float bottom = top + (float)(g_cellSize - highlightInset * 2);

        if (highlightWidth < 1) {
            highlightWidth = 1;
        }

        AppendOutlineRect(vertices, &vertexCount, BOARD_VERTEX_CAPACITY, left, top, right, bottom, (float)highlightWidth, surfaceWidth, surfaceHeight, ColorrefToDxColor(RGB(255, 255, 255), 1.0f));
    }
#endif

    if (!g_isGeneratingBoard) {
        AppendVictoryFireworks(vertices, &vertexCount, BOARD_VERTEX_CAPACITY, board, surfaceWidth, surfaceHeight);
    }

    if (vertexCount > 0) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        UINT stride = sizeof(Fling_BoardVertex);
        UINT offset = 0;

        if (SUCCEEDED(g_d3dContext->lpVtbl->Map(g_d3dContext, (ID3D11Resource *)g_d3dVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            ID3D11ShaderResourceView *boardTextures[4] = { g_d3dLightSquareTextureView, g_d3dDarkSquareTextureView, g_d3dGridLineTextureView, g_d3dLoadingTextTextureView };

            CopyMem(mapped.pData, vertices, sizeof(Fling_BoardVertex) * (size_t)vertexCount);
            g_d3dContext->lpVtbl->Unmap(g_d3dContext, (ID3D11Resource *)g_d3dVertexBuffer, 0);

            g_d3dContext->lpVtbl->IASetInputLayout(g_d3dContext, g_d3dInputLayout);
            g_d3dContext->lpVtbl->IASetPrimitiveTopology(g_d3dContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_d3dContext->lpVtbl->IASetVertexBuffers(g_d3dContext, 0, 1, &g_d3dVertexBuffer, &stride, &offset);
            g_d3dContext->lpVtbl->VSSetShader(g_d3dContext, g_d3dVertexShader, NULL, 0);
            g_d3dContext->lpVtbl->PSSetShader(g_d3dContext, g_d3dPixelShader, NULL, 0);
            g_d3dContext->lpVtbl->OMSetBlendState(g_d3dContext, g_d3dBlendState, blendFactor, 0xFFFFFFFFu);
            g_d3dContext->lpVtbl->PSSetShaderResources(g_d3dContext, 0, 4, boardTextures);
            g_d3dContext->lpVtbl->PSSetSamplers(g_d3dContext, 0, 1, &g_d3dSamplerState);
            g_d3dContext->lpVtbl->Draw(g_d3dContext, (UINT)vertexCount, 0);
        }
    }

    if (presentSwapChain) {
        g_d3dSwapChain->lpVtbl->Present(g_d3dSwapChain, 1, 0);
    }
}

static void RenderBoardWindow(void)
{
    RenderBoardWindowInternal(TRUE);
}

static BOOL ProcessBracketStyledText(HDC hdc, const RECT *rect, const char *text, COLORREF normalTextColor, COLORREF normalBackgroundColor, const POINT *hitPoint)
{
    RECT bounds = *rect;
    TEXTMETRICA metrics;
    int x = bounds.left;
    int y = bounds.top;
    int highlight = 0;
    int lineHeight;
    const BOOL hitTestOnly = (hitPoint != NULL);

    if (hitTestOnly && !PtInRect(&bounds, *hitPoint)) {
        return FALSE;
    }

    GetTextMetricsA(hdc, &metrics);
    lineHeight = metrics.tmHeight + metrics.tmExternalLeading;
    if (lineHeight < 1) {
        lineHeight = 1;
    }

    while (*text != '\0' && y < bounds.bottom) {
        while (*text == '[') {
            highlight = 1;
            ++text;
        }
        while (*text == ']') {
            highlight = 0;
            ++text;
        }

        if (*text == '\0') {
            break;
        }
        if (*text == '\r') {
            ++text;
            continue;
        }
        if (*text == '\n') {
            x = bounds.left;
            y += lineHeight;
            ++text;
            continue;
        }

        const int isWhitespace = (*text == ' ' || *text == '\t');
        const int tokenHighlight = highlight;
        char token[256];
        int tokenLength = 0;
        SIZE tokenSize;

        while (*text != '\0') {
            if (*text == '[' || *text == ']' || *text == '\r' || *text == '\n') {
                break;
            }
            if (isWhitespace) {
                if (*text != ' ' && *text != '\t') {
                    break;
                }
            } else if (*text == ' ' || *text == '\t') {
                break;
            }

            if (tokenLength + 1 < (int)sizeof(token)) {
                token[tokenLength++] = (*text == '\t') ? ' ' : *text;
            }
            ++text;
        }

        token[tokenLength] = '\0';
        if (tokenLength == 0) {
            continue;
        }

        GetTextExtentPoint32A(hdc, token, tokenLength, &tokenSize);

        if (!isWhitespace && x > bounds.left && x + tokenSize.cx > bounds.right) {
            x = bounds.left;
            y += lineHeight;
            if (y >= bounds.bottom) {
                break;
            }
        }

        if (isWhitespace && x == bounds.left) {
            continue;
        }

        if (hitTestOnly) {
            if (tokenHighlight && !isWhitespace) {
                RECT tokenRect;
                tokenRect.left = x;
                tokenRect.top = y;
                tokenRect.right = x + tokenSize.cx;
                tokenRect.bottom = y + lineHeight;

                if (PtInRect(&tokenRect, *hitPoint)) {
                    return TRUE;
                }
            }
        } else if (tokenHighlight) {
            const int oldBkMode = SetBkMode(hdc, OPAQUE);
            const COLORREF oldTextColor = SetTextColor(hdc, normalBackgroundColor);
            const COLORREF oldBkColor = SetBkColor(hdc, normalTextColor);

            TextOutA(hdc, x, y, token, tokenLength);

            SetBkColor(hdc, oldBkColor);
            SetTextColor(hdc, oldTextColor);
            SetBkMode(hdc, oldBkMode);
        } else {
            const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
            const COLORREF oldTextColor = SetTextColor(hdc, normalTextColor);

            TextOutA(hdc, x, y, token, tokenLength);

            SetTextColor(hdc, oldTextColor);
            SetBkMode(hdc, oldBkMode);
        }

        x += tokenSize.cx;
    }

    return FALSE;
}

static void DrawBracketStyledText(HDC hdc, const RECT *rect, const char *text, COLORREF normalTextColor, COLORREF normalBackgroundColor)
{
    ProcessBracketStyledText(hdc, rect, text, normalTextColor, normalBackgroundColor, NULL);
}

static BOOL IsPointOverBracketStyledHighlight(HDC hdc, const RECT *rect, const char *text, POINT point)
{
    return ProcessBracketStyledText(hdc, rect, text, 0, 0, &point);
}

static BOOL IsPointOverHighlightedInfoText(HWND hwnd, int x, int y)
{
    POINT point = {x, y};
    HDC hdc = GetDC(hwnd);
    char infoText[1024];

    RECT textRect = GetInfoTextRect(hwnd);
    if (!PtInRect(&textRect, point)) {
        ReleaseDC(hwnd, hdc);
        return FALSE;
    }

    RECT titleRect = textRect;
    sprintf(infoText, kInfoTextFormat, g_board.pieceCount, g_generatePieceCount, g_statusText);

    HFONT oldFont = (HFONT)SelectObject(hdc, g_titleFont);
    DrawTextA(hdc, kAppTitleText, -1, &titleRect, DT_TOP | DT_WORDBREAK | DT_CALCRECT);
    textRect.top = titleRect.bottom + ScaleForDpi(12, g_currentDpi);

    SelectObject(hdc, g_uiFont);
    BOOL hit = IsPointOverBracketStyledHighlight(hdc, &textRect, infoText, point);
    SelectObject(hdc, oldFont);
    ReleaseDC(hwnd, hdc);
    return hit;
}

static int g_treasureChestHoverCount = 0;

static void HandleInfoTextHover(HWND hwnd, int x, int y, BOOL screenCoordinates)
{
    POINT point = {x, y};
    TRACKMOUSEEVENT trackMouseEvent;
    DWORD trackFlags = TME_LEAVE;
    BOOL isOverHighlight;

    if (screenCoordinates) {
        ScreenToClient(hwnd, &point);
        trackFlags |= TME_NONCLIENT;
    }

    if (!g_infoTextMouseTracking) {
        trackMouseEvent.cbSize = sizeof(trackMouseEvent);
        trackMouseEvent.dwFlags = trackFlags;
        trackMouseEvent.hwndTrack = hwnd;
        trackMouseEvent.dwHoverTime = HOVER_DEFAULT;
        if (TrackMouseEvent(&trackMouseEvent)) {
            g_infoTextMouseTracking = TRUE;
        }
    }

    isOverHighlight = IsPointOverHighlightedInfoText(hwnd, point.x, point.y);

    if (!screenCoordinates) {
        SetCursor(LoadCursor(NULL, isOverHighlight ? IDC_HAND : IDC_ARROW));
    }

    if (isOverHighlight && !g_infoHighlightHoverActive) {
        g_infoHighlightHoverActive = TRUE;
        if (++g_treasureChestHoverCount % 19 == 0) {
            QuietMessageBox(hwnd, kInfoHoverText, kInfoCaption, MB_OK | MB_ICONINFORMATION);
        }
    } else if (!isOverHighlight) {
        g_infoHighlightHoverActive = FALSE;
    }
}

static void PaintScene(HWND hwnd, HDC hdc)
{
    RECT client;
    GetClientRect(hwnd, &client);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);

    HBRUSH bgBrush = CreateSolidBrush(g_windowBackgroundColor);
    FillRect(memDC, &client, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, g_infoTextColor);

    {
        RECT textRect = GetInfoTextRect(hwnd);
        RECT titleRect = textRect;
        const COLORREF infoTextColor = g_infoTextColor;
        const COLORREF infoBackgroundColor = g_windowBackgroundColor;
        const COLORREF oldTextColor = SetTextColor(memDC, g_titleTextColor);

        char infoText[1024];
        sprintf(
            infoText,
            kInfoTextFormat,
            g_board.pieceCount,
            g_generatePieceCount,
            g_statusText
        );

        HFONT oldFont = (HFONT)SelectObject(memDC, g_titleFont);
        DrawTextA(memDC, kAppTitleText, -1, &titleRect, DT_TOP | DT_WORDBREAK | DT_CALCRECT);
        DrawTextA(memDC, kAppTitleText, -1, &textRect, DT_TOP | DT_WORDBREAK);
        textRect.top = titleRect.bottom + ScaleForDpi(12, g_currentDpi);
        SetTextColor(memDC, infoTextColor);
        SelectObject(memDC, g_uiFont);
        DrawBracketStyledText(memDC, &textRect, infoText, infoTextColor, infoBackgroundColor);
        SetTextColor(memDC, oldTextColor);
        SelectObject(memDC, oldFont);
    }

    BitBlt(hdc, 0, 0, client.right, client.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

__packedstr(kButtonClass, "BUTTON");
__packedstr(kComboBoxClass, "COMBOBOX");

static __declspec(noinline) HWND CreateButton(HWND hParent, const char* caption, UINT_PTR id)
{
    return CreateWindowA(kButtonClass, caption, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hParent, (HMENU)id, g_instance, NULL);
}

static inline HWND CreateDifficultyComboBox(HWND hParent, UINT_PTR id)
{
    return CreateWindowA(
        kComboBoxClass,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
        0, 0, 0, 0,
        hParent,
        (HMENU)id,
        g_instance,
        NULL
    );
}

static inline const char *GetPathExtensionA(const char *path)
{
    return strrchr(path, '.');
}

__packedstr(kPngExtension, ".png");
__packedstr(kJpgExtension, ".jpg");
__packedstr(kJpegExtension, ".jpeg");
__packedstr(kBmpExtension, ".bmp");

static void EnsureSavePicturePathExtension(char *path, size_t pathSize, DWORD filterIndex)
{
    if (GetPathExtensionA(path) == NULL) {
        const char *defaultExtension = kPngExtension;
        size_t pathLength;
        size_t extensionLength;

        if (filterIndex == 2) {
            defaultExtension = kJpgExtension;
        } else if (filterIndex == 3) {
            defaultExtension = kBmpExtension;
        }

        pathLength = strlen(path);
        extensionLength = strlen(defaultExtension);
        if ((size_t)(pathLength + extensionLength + 1) <= pathSize) {
            CopyMem(path + pathLength, defaultExtension, (size_t)extensionLength + 1);
        }
    }
}

static const GUID *GetWicContainerFormatForPath(const char *path)
{
    const char *extension = GetPathExtensionA(path);

    if (extension == NULL) {
        return NULL;
    }
    if (_strcmpi(extension, kPngExtension) == 0) {
        return &GUID_ContainerFormatPng;
    }
    if (_strcmpi(extension, kBmpExtension) == 0) {
        return &GUID_ContainerFormatBmp;
    }
    if (_strcmpi(extension, kJpgExtension) == 0 || _strcmpi(extension, kJpegExtension) == 0) {
        return &GUID_ContainerFormatJpeg;
    }
    return NULL;
}

static HRESULT SaveBoardImageToFile(const char *path, const GUID *containerFormat)
{
    HRESULT hr = EnsureBoardRenderer(g_boardWindow);

    TickAnimation(GetParent(g_boardWindow));
    RenderBoardWindowInternal(FALSE);

    ID3D11Texture2D *backBuffer;
    hr = g_d3dSwapChain->lpVtbl->GetBuffer(g_d3dSwapChain, 0, &IID_ID3D11Texture2D, (void **)&backBuffer);

    D3D11_TEXTURE2D_DESC textureDesc;
    backBuffer->lpVtbl->GetDesc(backBuffer, &textureDesc);
    textureDesc.Usage = D3D11_USAGE_STAGING;
    textureDesc.BindFlags = 0;
    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    textureDesc.MiscFlags = 0;

    ID3D11Texture2D *stagingTexture;
    hr = g_d3dDevice->lpVtbl->CreateTexture2D(g_d3dDevice, &textureDesc, NULL, &stagingTexture);

    g_d3dContext->lpVtbl->CopyResource(g_d3dContext, (ID3D11Resource *)stagingTexture, (ID3D11Resource *)backBuffer);
    D3D11_MAPPED_SUBRESOURCE mapped;
    ZeroMem(&mapped, sizeof(mapped));
    hr = g_d3dContext->lpVtbl->Map(g_d3dContext, (ID3D11Resource *)stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);

    IWICImagingFactory *wicFactory;
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&wicFactory);
    IWICStream *stream;
    hr = wicFactory->lpVtbl->CreateStream(wicFactory, &stream);

    WCHAR widePath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, path, -1, widePath, (int)_countof(widePath));
    hr = stream->lpVtbl->InitializeFromFilename(stream, widePath, GENERIC_WRITE);
    IWICBitmapEncoder *encoder;
    hr = wicFactory->lpVtbl->CreateEncoder(wicFactory, containerFormat, NULL, &encoder);
    hr = encoder->lpVtbl->Initialize(encoder, (IStream *)stream, WICBitmapEncoderNoCache);
    IWICBitmapFrameEncode *frame;
    IPropertyBag2 *propertyBag;
    hr = encoder->lpVtbl->CreateNewFrame(encoder, &frame, &propertyBag);
    hr = frame->lpVtbl->Initialize(frame, propertyBag);
    hr = frame->lpVtbl->SetSize(frame, textureDesc.Width, textureDesc.Height);

    GUID sourcePixelFormat = GUID_WICPixelFormat32bppRGBA;
    GUID framePixelFormat = (containerFormat == &GUID_ContainerFormatJpeg) ? GUID_WICPixelFormat24bppBGR : GUID_WICPixelFormat32bppBGRA;
    hr = frame->lpVtbl->SetPixelFormat(frame, &framePixelFormat);

    IWICBitmap *bitmap;
    hr = wicFactory->lpVtbl->CreateBitmapFromMemory(
        wicFactory,
        textureDesc.Width,
        textureDesc.Height,
        &sourcePixelFormat,
        mapped.RowPitch,
        mapped.RowPitch * textureDesc.Height,
        (BYTE *)mapped.pData,
        &bitmap
    );

    IWICFormatConverter *converter;
    hr = wicFactory->lpVtbl->CreateFormatConverter(wicFactory, &converter);
    hr = converter->lpVtbl->Initialize(
        converter,
        (IWICBitmapSource *)bitmap,
        &framePixelFormat,
        WICBitmapDitherTypeNone,
        NULL,
        0.0f,
        WICBitmapPaletteTypeCustom
    );

    hr = frame->lpVtbl->WriteSource(frame, (IWICBitmapSource *)converter, NULL);
    hr = frame->lpVtbl->Commit(frame);
    hr = encoder->lpVtbl->Commit(encoder);

    g_d3dContext->lpVtbl->Unmap(g_d3dContext, (ID3D11Resource *)stagingTexture, 0);
    if (g_d3dSwapChain != NULL) {
        g_d3dSwapChain->lpVtbl->Present(g_d3dSwapChain, 1, 0);
    }

    SAFE_RELEASE(converter);
    SAFE_RELEASE(bitmap);
    SAFE_RELEASE(propertyBag);
    SAFE_RELEASE(frame);
    SAFE_RELEASE(encoder);
    SAFE_RELEASE(stream);
    SAFE_RELEASE(wicFactory);
    SAFE_RELEASE(stagingTexture);
    SAFE_RELEASE(backBuffer);
    return hr;
}

__packedstr(kDefaultFileName, "Flingboard.png");
__packedstr(kDefaultFileExt, "png");

static void SaveBoardPicture(HWND hwnd)
{
    OPENFILENAMEA ofn;
    char path[MAX_PATH];
    HRESULT hr;

    strcpy(path, kDefaultFileName);

    ZeroMem(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = kSavePictureDialogFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = path;
    ofn.nMaxFile = (DWORD)_countof(path);
    ofn.lpstrDefExt = kDefaultFileExt;
    ofn.lpstrTitle = kSavePictureDialogTitle;
    ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameA(&ofn)) {
        return;
    }

    EnsureSavePicturePathExtension(path, sizeof(path), ofn.nFilterIndex);
    const GUID *containerFormat = GetWicContainerFormatForPath(path);
    if (containerFormat == NULL) {
        QuietMessageBox(hwnd, kUnsupportedPictureFormatText, kInfoCaption, MB_OK | MB_ICONWARNING);
        SetStatus(hwnd, kUnsupportedPictureFormatText);
        return;
    }

    hr = SaveBoardImageToFile(path, containerFormat);
    if (SUCCEEDED(hr)) {
        SetStatus(hwnd, kPictureSavedText, path);
    } else {
        QuietMessageBox(hwnd, kSavePictureFailedText, kInfoCaption, MB_OK | MB_ICONERROR);
        SetStatus(hwnd, kSavePictureFailedText);
    }
}

static LRESULT CALLBACK FlingBoardWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        EnsureBoardRenderer(hwnd);
        return 0;

    case WM_SIZE:
        ResizeBoardRenderer((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE:
        UpdateBoardCursorAtPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT cursorPoint;

            GetCursorPos(&cursorPoint);
            ScreenToClient(hwnd, &cursorPoint);
            UpdateBoardCursorAtPoint(cursorPoint.x, cursorPoint.y);
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN:
        {
            int row = -1;
            int col = -1;

            if (g_isAnimating || g_isGeneratingBoard) {
                return 0;
            }

            if (HitTestBoardCell(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &row, &col)) {
                StopSolutionPlayback(GetParent(hwnd));
                g_mouseDown = TRUE;
                g_mouseStart.x = GET_X_LPARAM(lParam);
                g_mouseStart.y = GET_Y_LPARAM(lParam);
                g_mouseRow = row;
                g_mouseCol = col;
                SetCapture(hwnd);
                InvalidateBoardArea(GetParent(hwnd), FALSE);
            }
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_mouseDown) {
            HWND parent = GetParent(hwnd);
            int dx = GET_X_LPARAM(lParam) - g_mouseStart.x;
            int dy = GET_Y_LPARAM(lParam) - g_mouseStart.y;
            int handledMove = 0;
            int wasDragAttempt = (abs(dx) >= 18 || abs(dy) >= 18);

            ReleaseCapture();

            if (InBounds(g_mouseRow, g_mouseCol) && g_board.cells[g_mouseRow][g_mouseCol] != 0 && wasDragAttempt) {
                Fling_Move move;
                move.sr = g_mouseRow;
                move.sc = g_mouseCol;

                if (abs(dx) > abs(dy)) {
                    move.dr = 0;
                    move.dc = (dx > 0) ? 1 : -1;
                } else {
                    move.dr = (dy > 0) ? 1 : -1;
                    move.dc = 0;
                }

                if (CanBoardChainMove(&g_board, move.sr, move.sc, move.dr, move.dc, 1)) {
                    handledMove = 1;
                    if (!g_boardLocked) {
                        CaptureInitialBoard(TRUE);
                    }
                    SetBoardLocked(TRUE);
                    StartBoardMoveAnimation(parent, move);
                } else {
                    SetStatus(parent, kInvalidMoveText);
                    StartSadMoveMidiPlayback(parent);
                }
            }

            if (!handledMove && !wasDragAttempt && InBounds(g_mouseRow, g_mouseCol)) {
                if (g_boardLocked) {
                    SetStatus(parent, kBoardLockedText);
                } else {
                    TogglePiece(&g_board, g_mouseRow, g_mouseCol);
                    OnBoardEdited(parent);
                }
            }

            g_mouseDown = FALSE;
            g_mouseRow = -1;
            g_mouseCol = -1;
            UpdateBoardCursorAtPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            InvalidateBoardArea(parent, FALSE);
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HWND parent = GetParent(hwnd);
            BeginPaint(hwnd, &ps);
            TickAnimation(parent != NULL ? parent : hwnd);
            RenderBoardWindow();
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_DESTROY:
        DestroyBoardRenderer();
        g_boardWindow = NULL;
        return 0;
    }

    return DefWindowProcA(hwnd, message, wParam, lParam);
}

static LRESULT CALLBACK FlingMainWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_KEYDOWN:
        if (wParam == 'Q') {
            SendMessageA(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;

    case WM_CREATE:
        g_boardWindow = CreateWindowA(
            FLING_BOARDCLASS,
            NULL,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, 0, 0,
            hwnd,
            NULL,
            g_instance,
            NULL
        );
        g_buttonRandom = CreateButton(hwnd, kCaptionGenerate, IDC_RANDOM);
        g_comboDifficulty = CreateDifficultyComboBox(hwnd, IDC_DIFFICULTY);
        PopulateDifficultyComboBox();
        g_buttonClear = CreateButton(hwnd, kCaptionClear, IDC_CLEAR);
        g_buttonSavePicture = CreateButton(hwnd, kCaptionSavePicture, IDC_SAVE_PICTURE);
        g_buttonRestore = CreateButton(hwnd, kCaptionRestore, IDC_RESTORE);
        g_buttonSolve = CreateButton(hwnd, kCaptionSolve, IDC_SOLVE);
        g_buttonPrevious = CreateButton(hwnd, kCaptionPrevious, IDC_PREVIOUS);
        g_buttonNext = CreateButton(hwnd, kCaptionNext, IDC_NEXT);
        g_buttonPlay = CreateButton(hwnd, kCaptionPlay, IDC_PLAY);
        {
            HMENU systemMenu = GetSystemMenu(hwnd, FALSE);
            AppendMenuA(systemMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuA(systemMenu, MF_STRING, IDM_SYSTEM_TOGGLE_THEME, kMenuSwitchToLightTheme);
            UpdateSystemMenuThemeItem(hwnd);
        }
        ApplyControlTheme();
        UpdateDpiMetrics(hwnd, QueryWindowDpi(hwnd));
        SetWindowTitleBarVisible(hwnd, g_appSettings.showTitleBar, FALSE);
        UpdateSolutionButtons();
        LoadGeneratedBoard(hwnd);
        StartBackgroundMidiPlayback(hwnd);
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == IDM_SYSTEM_TOGGLE_THEME) {
            if (!g_appSettings.hasThemePreference) {
                g_appSettings.hasThemePreference = TRUE;
                g_appSettings.useDarkTheme = 0;
            } else if (!g_appSettings.useDarkTheme) {
                g_appSettings.useDarkTheme = 1;
            } else {
                g_appSettings.hasThemePreference = FALSE;
                g_appSettings.useDarkTheme = 0;
            }

            UpdateThemeColors();
            UpdateSystemMenuThemeItem(hwnd);
            SaveAppSettings(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        break;

    case WM_NCACTIVATE:
        if ((GetWindowLongPtrA(hwnd, GWL_STYLE) & WS_CAPTION) == 0) {
            // Preserve activation handling without repainting the native frame.
            return DefWindowProcA(hwnd, message, wParam, -1);
        }
        break;

    case WM_NCPAINT:
        if ((GetWindowLongPtrA(hwnd, GWL_STYLE) & WS_CAPTION) == 0) {
            return 0;
        }
        break;

    case WM_NCCALCSIZE:
        if ((GetWindowLongPtrA(hwnd, GWL_STYLE) & WS_CAPTION) == 0) {
            if (IsZoomed(hwnd)) {
                RECT *clientRect = wParam
                    ? &((NCCALCSIZE_PARAMS *)lParam)->rgrc[0]
                    : (RECT *)lParam;
                MONITORINFO monitorInfo;
                ZeroMem(&monitorInfo, sizeof(monitorInfo));
                monitorInfo.cbSize = sizeof(monitorInfo);

                // Exclude the off-screen resize frame and taskbars on any edge.
                if (!GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitorInfo)) {
                    break;
                }
                IntersectRect(clientRect, clientRect, &monitorInfo.rcWork);
            }
            return 0;
        }
        break;

    case WM_NCHITTEST:
        {
            LONG_PTR style = GetWindowLongPtrA(hwnd, GWL_STYLE);
            LRESULT hit;
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

            if ((style & WS_CAPTION) == 0) {
                RECT windowRect;
                GetWindowRect(hwnd, &windowRect);
                hit = PtInRect(&windowRect, point) ? HTCLIENT : HTNOWHERE;

                if (hit == HTCLIENT && (style & WS_THICKFRAME) != 0 && !IsZoomed(hwnd)) {
                    UINT dpi = QueryWindowDpi(hwnd);
                    int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                    int borderX = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) + padding;
                    int borderY = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) + padding;
                    BOOL left = point.x < windowRect.left + borderX;
                    BOOL right = point.x >= windowRect.right - borderX;
                    BOOL top = point.y < windowRect.top + borderY;
                    BOOL bottom = point.y >= windowRect.bottom - borderY;

                    if (top) {
                        return left ? HTTOPLEFT : (right ? HTTOPRIGHT : HTTOP);
                    }
                    if (bottom) {
                        return left ? HTBOTTOMLEFT : (right ? HTBOTTOMRIGHT : HTBOTTOM);
                    }
                    if (left || right) {
                        return left ? HTLEFT : HTRIGHT;
                    }
                }
            } else {
                hit = DefWindowProcA(hwnd, message, wParam, lParam);
            }

            if (hit == HTCLIENT && IsFreeWindowAreaPoint(hwnd, point.x, point.y)) {
                return HTCAPTION;
            }
            return hit;
        }

    case WM_NCLBUTTONDBLCLK:
        if (wParam == HTCAPTION && IsFreeWindowAreaPoint(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))) {
            ToggleWindowTitleBar(hwnd);
            return 0;
        }
        break;

    case WM_SIZE:
        LayoutControls(hwnd);
        if ((GetWindowLongPtrA(hwnd, GWL_STYLE) & WS_CAPTION) == 0) {
            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        } else {
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_GETMINMAXINFO:
        {
            MINMAXINFO *minMaxInfo = (MINMAXINFO *)lParam;
            RECT minWindowRect;
            SIZE minClientSize = GetMinimumClientSize();

            if (minMaxInfo != NULL) {
                UINT dpi = (g_currentDpi != 0) ? g_currentDpi : QueryWindowDpi(hwnd);

                minWindowRect.left = 0;
                minWindowRect.top = 0;
                minWindowRect.right = minClientSize.cx;
                minWindowRect.bottom = minClientSize.cy;
                DWORD style = (DWORD)GetWindowLongPtrA(hwnd, GWL_STYLE);
                if ((style & WS_CAPTION) != 0) {
                    AdjustWindowRectExForDpi(
                        &minWindowRect,
                        style,
                        FALSE,
                        (DWORD)GetWindowLongPtrA(hwnd, GWL_EXSTYLE),
                        dpi
                    );
                }
                minMaxInfo->ptMinTrackSize.x = minWindowRect.right - minWindowRect.left;
                minMaxInfo->ptMinTrackSize.y = minWindowRect.bottom - minWindowRect.top;
            }
        }
        return 0;

    case WM_DPICHANGED:
        {
            RECT *suggested = (RECT *)lParam;
            UpdateDpiMetrics(hwnd, HIWORD(wParam));
            SetWindowPos(
                hwnd,
                NULL,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_THEMECHANGED:
    case WM_SETTINGCHANGE:
        UpdateThemeColors();
        UpdateSystemMenuThemeItem(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case WM_MEASUREITEM:
        {
            MEASUREITEMSTRUCT *measure = (MEASUREITEMSTRUCT *)lParam;
            if (measure != NULL && measure->CtlType == ODT_COMBOBOX && measure->CtlID == IDC_DIFFICULTY) {
                // the selection field height is driven by the combo item height, not by SetWindowPos
                measure->itemHeight = GetDifficultyComboItemHeight(g_currentDpi);
                measure->itemWidth = ScaleForDpi(56, g_currentDpi);
                return TRUE;
            }
        }
        break;

    case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT *draw = (DRAWITEMSTRUCT *)lParam;
            if (draw != NULL && draw->CtlType == ODT_COMBOBOX && draw->CtlID == IDC_DIFFICULTY) {
                COLORREF bg;
                COLORREF fg;

                if (g_useDarkTheme) {
                    bg = (draw->itemState & ODS_SELECTED) ? RGB(74, 96, 126) : RGB(45, 49, 55);
                    fg = RGB(244, 247, 251);
                } else {
                    bg = GetSysColor((draw->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHT : COLOR_WINDOW);
                    fg = GetSysColor((draw->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT);
                }

                HBRUSH brush = CreateSolidBrush(bg);

                RECT* rc = &draw->rcItem;
                FillRect(draw->hDC, rc, brush);
                DeleteObject(brush);

                char label[64];
                label[0] = '\0';
                if ((int)draw->itemID >= 0) {
                    SendMessageA(draw->hwndItem, CB_GETLBTEXT, draw->itemID, (LPARAM)label);
                }

                HFONT oldFont = (HFONT)SelectObject(draw->hDC, g_uiFont);
                COLORREF oldTextColor = SetTextColor(draw->hDC, fg);
                int oldBkMode = SetBkMode(draw->hDC, TRANSPARENT);
                DrawTextA(draw->hDC, label, -1, rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SetBkMode(draw->hDC, oldBkMode);
                SetTextColor(draw->hDC, oldTextColor);
                SelectObject(draw->hDC, oldFont);

                if (draw->itemState & ODS_FOCUS) {
                    DrawFocusRect(draw->hDC, rc);
                }
                return TRUE;
            }
        }
        break;

    case WM_ERASEBKGND:
        if ((GetWindowLongPtrA(hwnd, GWL_STYLE) & WS_CAPTION) == 0) {
            PaintScene(hwnd, (HDC)wParam);
        }
        return 1;

    case WM_SETCURSOR:
        if ((HWND)wParam == hwnd && LOWORD(lParam) == HTCLIENT) {
            POINT cursorPoint;

            GetCursorPos(&cursorPoint);
            ScreenToClient(hwnd, &cursorPoint);
            SetCursor(LoadCursor(NULL, IsPointOverHighlightedInfoText(hwnd, cursorPoint.x, cursorPoint.y) ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_MOUSEMOVE:
        HandleInfoTextHover(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), FALSE);
        return 0;

    case WM_NCMOUSEMOVE:
        HandleInfoTextHover(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), TRUE);
        return 0;

    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
        g_infoHighlightHoverActive = FALSE;
        g_infoTextMouseTracking = FALSE;
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_BACKGROUND_MIDI) {
            AdvanceBackgroundMidiPlayback(hwnd);
            return 0;
        }
        if (wParam == TIMER_SOLVE) {
            KillTimer(hwnd, TIMER_SOLVE);
            if (g_solutionAutoPlaying && g_solutionReady && !g_isAnimating && g_solutionStep < g_solutionLength) {
                StartSolutionStepAnimation(hwnd, g_solutionStep + 1);
            }
            return 0;
        }
        if (wParam == TIMER_VICTORY_START) {
            KillTimer(hwnd, TIMER_VICTORY_START);
            if (g_victoryMidiStartPending && IsMoveMidiPlaybackActive()) {
                g_victoryDelayArmed = FALSE;
                SetTimer(hwnd, TIMER_VICTORY_START, 120u, NULL);
                return 0;
            }
            if (g_victoryMidiStartPending && !g_victoryDelayArmed) {
                g_victoryDelayArmed = TRUE;
                SetTimer(hwnd, TIMER_VICTORY_START, (UINT)VICTORY_START_DELAY_MS, NULL);
                return 0;
            }
            g_victoryMidiStartPending = FALSE;
            g_victoryDelayArmed = FALSE;
            if (StartVictoryMidiPlayback(hwnd)) {
                StartVictoryFireworks(hwnd);
            }
            return 0;
        }
        if (wParam == TIMER_VICTORY_MIDI) {
            KillTimer(hwnd, TIMER_VICTORY_MIDI);
            AdvanceVictoryMidiPlayback(hwnd);
            return 0;
        }
        if (wParam == TIMER_MOVE_MIDI) {
            KillTimer(hwnd, TIMER_MOVE_MIDI);
            AdvanceMoveMidiPlayback(hwnd);
            return 0;
        }
        if (wParam == TIMER_VICTORY_FIREWORKS) {
            if (g_victoryFireworksActive) {
                InvalidateBoardArea(hwnd, FALSE);
            } else {
                StopVictoryFireworks(hwnd);
            }
            return 0;
        }
        if (wParam == TIMER_SOLVED_OVERLAY) {
            if (!g_showSolvedOverlay || g_solvedOverlayStartTick == 0) {
                KillTimer(hwnd, TIMER_SOLVED_OVERLAY);
                return 0;
            }

            if (SOLVED_OVERLAY_DURATION_MS > 0 && (GetTickCount() - g_solvedOverlayStartTick) >= SOLVED_OVERLAY_DURATION_MS) {
                KillTimer(hwnd, TIMER_SOLVED_OVERLAY);
            }

            InvalidateBoardArea(hwnd, FALSE);
            return 0;
        }
        if (wParam == TIMER_GENERATE_ANIMATION) {
            if (g_isGeneratingBoard) {
                if (g_loadingBallsSettling && GetTickCount() - g_loadingBallSettleStartTick >= LOADING_BALL_SETTLE_DURATION_MS) {
                    FinalizeGeneratedBoard(hwnd);
                } else {
                    InvalidateBoardArea(hwnd, FALSE);
                }
            } else {
                KillTimer(hwnd, TIMER_GENERATE_ANIMATION);
            }
            return 0;
        }
        break;

    case WM_APP_GENERATE_COMPLETE:
        {
            if (g_generateThread != NULL) {
                CloseHandle(g_generateThread);
                g_generateThread = NULL;
            }

            if (g_generateResult.succeeded) {
                if (!StartLoadingBallSettlement()) {
                    FinalizeGeneratedBoard(hwnd);
                }
            } else {
                SetStatus(hwnd, kCannotFindSolutionText);
                SetBoardGenerating(hwnd, FALSE);
            }

            InvalidateBoardArea(hwnd, TRUE);
            InvalidateInfoArea(hwnd);

        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_DIFFICULTY:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                g_generatePieceCount = GetSelectedGeneratePieceCount();
                SetStatus(hwnd, kGenerateLevelSetText, g_generatePieceCount);
                return 0;
            }
            break;
        case IDC_RANDOM:
            if (!ConfirmDiscardBoardChanges(hwnd, kConfirmGenerateBoardText)) {
                return 0;
            }
            LoadGeneratedBoard(hwnd);
            return 0;
        case IDC_CLEAR:
            if (!ConfirmDiscardBoardChanges(hwnd, kConfirmClearBoardText)) {
                return 0;
            }
            StopSolutionPlayback(hwnd);
            ClearBoard(&g_board);
            SetBoardLocked(FALSE);
            ClearInitialBoard();
            SetStatus(hwnd, kBoardClearedText);
            InvalidateBoardArea(hwnd, TRUE);
            InvalidateInfoArea(hwnd);
            return 0;
        case IDC_SAVE_PICTURE:
            SaveBoardPicture(hwnd);
            return 0;
        case IDC_RESTORE:
            if (!g_hasInitialBoard) {
                //SetStatus(hwnd, "No saved starting layout is available yet.");
                return 0;
            }
            StopSolutionPlayback(hwnd);
            g_board = g_initialBoard;
            SetBoardLocked(g_initialBoardLocked);
            SetStatus(hwnd, kBoardRestoredText);
            InvalidateBoardArea(hwnd, TRUE);
            InvalidateInfoArea(hwnd);
            return 0;
        case IDC_SOLVE:
            StartSolveAnimation(hwnd);
            return 0;
        case IDC_PREVIOUS:
            KillTimer(hwnd, TIMER_SOLVE);
            g_solutionAutoPlaying = FALSE;
            StartSolutionStepAnimation(hwnd, g_solutionStep - 1);
            return 0;
        case IDC_NEXT:
            KillTimer(hwnd, TIMER_SOLVE);
            g_solutionAutoPlaying = FALSE;
            StartSolutionStepAnimation(hwnd, g_solutionStep + 1);
            return 0;
        case IDC_PLAY:
            if (g_solutionAutoPlaying) {
                StopSolutionAutoPlay(hwnd);
            } else {
                StartSolutionPlaybackFromBeginning(hwnd);
            }
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        {
            int row = -1;
            int col = -1;
            if (g_isAnimating || g_isGeneratingBoard) {
                return 0;
            }
            if (HitTestBoardCell(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &row, &col)) {
                StopSolutionPlayback(hwnd);
                g_mouseDown = TRUE;
                g_mouseStart.x = GET_X_LPARAM(lParam);
                g_mouseStart.y = GET_Y_LPARAM(lParam);
                g_mouseRow = row;
                g_mouseCol = col;
                SetCapture(hwnd);
                InvalidateBoardArea(hwnd, FALSE);
            }
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_mouseDown) {
            int dx = GET_X_LPARAM(lParam) - g_mouseStart.x;
            int dy = GET_Y_LPARAM(lParam) - g_mouseStart.y;
            int handledMove = 0;
            int wasDragAttempt = (abs(dx) >= 18 || abs(dy) >= 18);

            ReleaseCapture();

            if (InBounds(g_mouseRow, g_mouseCol) && g_board.cells[g_mouseRow][g_mouseCol] != 0 && wasDragAttempt) {
                Fling_Move move;
                move.sr = g_mouseRow;
                move.sc = g_mouseCol;

                if (abs(dx) > abs(dy)) {
                    move.dr = 0;
                    move.dc = (dx > 0) ? 1 : -1;
                } else {
                    move.dr = (dy > 0) ? 1 : -1;
                    move.dc = 0;
                }

                if (CanBoardChainMove(&g_board, move.sr, move.sc, move.dr, move.dc, 1)) {
                    handledMove = 1;
                    if (!g_boardLocked) {
                        CaptureInitialBoard(TRUE);
                    }
                    SetBoardLocked(TRUE);
                    StartBoardMoveAnimation(hwnd, move);
                } else {
                    SetStatus(hwnd, kInvalidMoveText);
                    StartSadMoveMidiPlayback(hwnd);
                }
            }

            if (!handledMove && !wasDragAttempt && InBounds(g_mouseRow, g_mouseCol)) {
                if (g_boardLocked) {
                    SetStatus(hwnd, kBoardLockedText);
                } else {
                    TogglePiece(&g_board, g_mouseRow, g_mouseCol);
                    OnBoardEdited(hwnd);
                }
            }

            g_mouseDown = FALSE;
            g_mouseRow = -1;
            g_mouseCol = -1;
            InvalidateBoardArea(hwnd, FALSE);
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintScene(hwnd, hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_CLOSE:
        if (!ConfirmDiscardBoardChanges(hwnd, kConfirmQuitText)) {
            return 0;
        }
        break;

    case WM_DESTROY:
        SaveAppSettings(hwnd);
        StopBackgroundMidiPlayback(hwnd);
        StopSolutionPlayback(hwnd);
        DestroyVictoryMidiOutput();
        StopWindowsAnimationManager();
        if (g_generateThread != NULL) {
            CloseHandle(g_generateThread);
            g_generateThread = NULL;
        }
        DestroyBoardRenderer();
        DeleteObject(g_uiFont);
        DeleteObject(g_titleFont);
        // if (g_comCtlActivationCookie != 0) {
        //     DeactivateActCtx(0, g_comCtlActivationCookie);
        //     g_comCtlActivationCookie = 0;
        // }
        // if (g_comCtlActivationContext != INVALID_HANDLE_VALUE) {
        //     ReleaseActCtx(g_comCtlActivationContext);
        //     g_comCtlActivationContext = INVALID_HANDLE_VALUE;
        // }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, message, wParam, lParam);
}

#define MAIN_WINDOW_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_THICKFRAME)

static WNDCLASSA g_wc;
static WNDCLASSA g_boardWc;

static __forceinline void EnableComCtlV6(void)
{
    char actCtxSource[MAX_PATH];
    ACTCTXA activationContext;
    UINT pathLength;

    pathLength = GetWindowsDirectoryA(actCtxSource, (UINT)_countof(actCtxSource));
    CopyMem(actCtxSource + pathLength, "\\explorer.exe", 14);

    ZeroMem(&activationContext, sizeof(activationContext));
    activationContext.cbSize = sizeof(activationContext);
    activationContext.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID;
    activationContext.lpSource = actCtxSource;
    activationContext.lpResourceName = MAKEINTRESOURCEA(1);

    g_comCtlActivationContext = CreateActCtxA(&activationContext);
    if (g_comCtlActivationContext == INVALID_HANDLE_VALUE) {
        return;
    }

    if (!ActivateActCtx(g_comCtlActivationContext, &g_comCtlActivationCookie)) {
        ReleaseActCtx(g_comCtlActivationContext);
        g_comCtlActivationContext = INVALID_HANDLE_VALUE;
        g_comCtlActivationCookie = 0;
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int showCmd)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    EnableComCtlV6();
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    g_currentDpi = USER_DEFAULT_SCREEN_DPI;
    g_mouseRow = -1;
    g_mouseCol = -1;
    g_windowBackgroundColor = RGB(245, 246, 250);
    g_titleTextColor = RGB(25, 25, 25);
    g_infoTextColor = RGB(25, 25, 25);
    g_cellSize = CELL_SIZE;
    g_boardMargin = BOARD_MARGIN;
    g_panelWidth = PANEL_WIDTH;
    g_animationMovingForward = TRUE;
    g_generatePieceCount = DEFAULT_GENERATE_PIECE_COUNT;
    g_moveCurrentNote = -1;
    g_victoryCurrentNote = -1;
    g_instance = instance;
    LoadAppSettings();
    UpdateThemeColors();

    g_wc.lpfnWndProc = FlingMainWindowProc;
    g_wc.hInstance = instance;
    g_wc.lpszClassName = FLING_MAINWINDOWCLASS;
    g_wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    g_wc.hbrBackground = NULL;
    RegisterClassA(&g_wc);

    g_boardWc.lpfnWndProc = FlingBoardWindowProc;
    g_boardWc.hInstance = instance;
    g_boardWc.lpszClassName = FLING_BOARDCLASS;
    g_boardWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    g_boardWc.hbrBackground = NULL;
    RegisterClassA(&g_boardWc);

    UpdateDpiMetrics(NULL, QueryWindowDpi(NULL));

    RECT windowRect;
    int windowX = CW_USEDEFAULT;
    int windowY = CW_USEDEFAULT;
    int windowWidth;
    int windowHeight;
    int initialShowCmd = SW_SHOWNORMAL;
    DWORD windowStyle = MAIN_WINDOW_STYLE;

    if (!g_appSettings.showTitleBar) {
        windowStyle &= ~((DWORD)WS_CAPTION);
    }

    GetDesiredWindowRect(&windowRect, windowStyle, 0);
    windowWidth = windowRect.right - windowRect.left;
    windowHeight = windowRect.bottom - windowRect.top;

    windowX = g_appSettings.windowX;
    windowY = g_appSettings.windowY;
    windowWidth = g_appSettings.windowWidth;
    windowHeight = g_appSettings.windowHeight;
    initialShowCmd = g_appSettings.windowMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;

    HWND hwnd = CreateWindowA(
        g_wc.lpszClassName,
        kAppTitleText,
        windowStyle,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        NULL,
        NULL,
        instance,
        NULL
    );

    ShowWindow(hwnd, initialShowCmd);
    UpdateWindow(hwnd);

    MSG message;
    while (GetMessageA(&message, NULL, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    CoUninitialize();

#ifdef USE_MINICRT
    _exit((int) message.wParam);
    return 0;
#endif
    return (int) message.wParam;
}
