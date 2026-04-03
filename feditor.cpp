#define UNICODE
#define _UNICODE

#include <d2d1.h>
#include <dwrite.h>
#include <shobjidl.h> // for IFileOpenDialog
#include <string>
#include <windows.h>
#include <windowsx.h> // for GET_X_LPARAM, GET_Y_LPARAM

#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "ole32")


// Super global values
float font_size = 14.0f;
const wchar_t *font_type = L"Consolas";
int feditor_window_width = 800;
int feditor_window_height = 800;


// Global variables
ID2D1Factory *g_factory = nullptr;
ID2D1HwndRenderTarget *g_rt = nullptr;
ID2D1SolidColorBrush *g_brush = nullptr;
ID2D1SolidColorBrush *blackBrush = nullptr;
ID2D1SolidColorBrush *greyBrush = nullptr;
IDWriteFactory *g_dwFactory = nullptr;
IDWriteTextFormat *g_textFormat = nullptr;
IDWriteTextLayout *g_textLayout = nullptr;
UINT32 g_textLength = 0;
float g_lineHeight = 0.0f;
std::wstring g_text;            // actual document text
std::wstring g_currentFilePath; // current file path
std::wstring g_currentFileName; // obtained from the file path

float g_scrollY = 0.0f;
float g_contentHeight = 0.0f;

UINT32 g_caretPos = 0; // character index
bool g_caretVisible = true;
float g_caretX = 0.0f;
float g_caretY = 0.0f;
float g_caretHeight = 0.0f;
UINT_PTR CURSOR_TIMER_ID = 1;

bool g_minibufferActive = false;
std::wstring g_minibufferText;
std::wstring g_minibufferPrompt;
bool g_waitingForCtrlX = false;

float g_minibufferHeight = 20.0f;
IDWriteTextFormat *g_minibufferFormat = nullptr;

float g_gutterWidth = 60.0f;
float gutter_padding = 10.0f;
IDWriteTextFormat *g_gutterFormat = nullptr;


// fps calculation
struct FPSCounter {
    LARGE_INTEGER frequency;
    LARGE_INTEGER lastTime;
    int frameCount;
};

FPSCounter g_fps;


void GetFileNameFromPath(const std::wstring& path)
{
    size_t lastSlash = std::wstring::npos;

    // Scan from the end to find the last '/' or '\'
    for (size_t i = path.size(); i-- > 0; ) {
        if (path[i] == L'\\' || path[i] == L'/') {
            lastSlash = i;
            break;
        }
    }

    if (lastSlash == std::wstring::npos) return; // no slashes, whole string is filename
    g_currentFileName = path.substr(lastSlash + 1);
}

void UpdateFPS(HWND hwnd) {
    g_fps.frameCount++;

    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    double elapsed = double(currentTime.QuadPart - g_fps.lastTime.QuadPart) /
                     double(g_fps.frequency.QuadPart);

    if (elapsed >= 1.0) {
        double fps = g_fps.frameCount / elapsed;

        wchar_t buffer[256];
        swprintf_s(buffer, L"feditor (%.0f fps) %s", fps, g_currentFileName.c_str());

        SetWindowText(hwnd, buffer);

        g_fps.frameCount = 0;
        g_fps.lastTime = currentTime;
    }
}

// reading a file using memory mapping
bool LoadFileAndCreateLayout(const wchar_t *filename, float maxWidth,
                             float maxHeight) {
    HANDLE file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD fileSize = GetFileSize(file, nullptr);
    if (fileSize == 0) {
        CloseHandle(file);
        return false;
    }

    HANDLE mapping =
        CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);

    if (!mapping) {
        CloseHandle(file);
        return false;
    }

    void *data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);

    if (!data) {
        CloseHandle(mapping);
        CloseHandle(file);
        return false;
    }

    // Convert UTF-8 -> UTF-16
    int wideLen =
        MultiByteToWideChar(CP_UTF8, 0, (char *)data, fileSize, nullptr, 0);

    if (wideLen <= 0) {
        UnmapViewOfFile(data);
        CloseHandle(mapping);
        CloseHandle(file);
        return false;
    }

    wchar_t *wideText = new wchar_t[wideLen];
    g_text.resize(wideLen); // storing the doc text in a buffer
    g_caretPos = 0;
    g_currentFilePath = filename;
    GetFileNameFromPath(g_currentFilePath);

    MultiByteToWideChar(CP_UTF8, 0, (char *)data, fileSize, &g_text[0],
                        wideLen);

    // Release old layout if exists
    if (g_textLayout)
        g_textLayout->Release();

    HRESULT hr =
        g_dwFactory->CreateTextLayout(g_text.c_str(), wideLen, g_textFormat,
                                      maxWidth, maxHeight, &g_textLayout);

    DWRITE_TEXT_METRICS metrics;
    g_textLayout->GetMetrics(&metrics);

    g_contentHeight = metrics.height;
    g_textLength = wideLen;

    if (metrics.lineCount > 0)
        g_lineHeight = metrics.height / metrics.lineCount;

    delete[] wideText;

    UnmapViewOfFile(data);
    CloseHandle(mapping);
    CloseHandle(file);

    return SUCCEEDED(hr);
}

// for users that want to open files using a browse dialog
std::wstring OpenFileDialog(HWND owner) {
    std::wstring result;

    HRESULT hr =
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr))
        return result;

    IFileOpenDialog *pFileOpen = nullptr;

    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                          IID_IFileOpenDialog, (void **)&pFileOpen);

    if (SUCCEEDED(hr)) {
        // Show dialog
        hr = pFileOpen->Show(owner);

        if (SUCCEEDED(hr)) {
            IShellItem *pItem;
            hr = pFileOpen->GetResult(&pItem);

            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                if (SUCCEEDED(hr)) {
                    result = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }

                pItem->Release();
            }
        }

        pFileOpen->Release();
    }

    CoUninitialize();
    return result;
}

// save the file contents back to the file
bool SaveFile() {
    if (g_currentFilePath.empty())
        return false;

    // Convert UTF-16 (wstring) to UTF-8
    int utf8Size =
        WideCharToMultiByte(CP_UTF8, 0, g_text.c_str(), (int)g_text.length(),
                            nullptr, 0, nullptr, nullptr);

    if (utf8Size <= 0)
        return false;

    std::string utf8Text;
    utf8Text.resize(utf8Size);

    WideCharToMultiByte(CP_UTF8, 0, g_text.c_str(), (int)g_text.length(),
                        &utf8Text[0], utf8Size, nullptr, nullptr);

    HANDLE file =
        CreateFileW(g_currentFilePath.c_str(), GENERIC_WRITE, 0, nullptr,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    BOOL result = WriteFile(file, utf8Text.data(), utf8Size, &written, nullptr);

    CloseHandle(file);

    return result && (written == (DWORD)utf8Size);
}

void UpdateCaretPosition() {
    if (!g_textLayout)
        return;

    DWRITE_HIT_TEST_METRICS metrics;
    FLOAT x, y;

    g_textLayout->HitTestTextPosition(g_caretPos, FALSE, &x, &y, &metrics);

    g_caretX = x;
    g_caretY = y;
    g_caretHeight = metrics.height;
}

// anytime the text changes, or window resizes, we need to rebuild the layout
void RebuildTextLayout(float width, float height) {
    if (g_textLayout) {
        g_textLayout->Release();
        g_textLayout = nullptr;
    }

    g_dwFactory->CreateTextLayout(g_text.c_str(), (UINT32)g_text.length(),
                                  g_textFormat, width, height, &g_textLayout);
}

void EnsureCaretVisible(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    float windowHeight = (float)(rc.bottom - rc.top);

    float visibleTop = g_scrollY;
    float visibleBottom = g_scrollY + windowHeight;

    // If caret below bottom
    if (g_caretY + g_caretHeight > visibleBottom) {
        g_scrollY += g_lineHeight;
    }
    // If caret above top
    else if (g_caretY < visibleTop) {
        g_scrollY -= g_lineHeight;

        if (g_scrollY < 0)
            g_scrollY = 0;
    }

    InvalidateRect(hwnd, NULL, FALSE);
}

void UpdateScrollBar(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    float windowHeight = (float)(rc.bottom - rc.top);
    float editorHeight =
        windowHeight - g_minibufferHeight; // actual viewable height

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

    si.nMin = 0;
    si.nMax = (int)g_contentHeight;
    si.nPage = (UINT)editorHeight;
    si.nPos = (int)g_scrollY;

    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

void CreateResources(HWND hwnd) {
    if (g_rt)
        return;

    RECT rc;
    GetClientRect(hwnd, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

    g_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, size), &g_rt);

    g_rt->CreateSolidColorBrush(
        D2D1::ColorF(0.8f, 0.8f, 0.8f), // text color (console gray)
        &g_brush);

    g_rt->CreateSolidColorBrush(
        D2D1::ColorF(0.0f, 0.0f, 0.0f), // minibuffer text color (black)
        &blackBrush);

    g_rt->CreateSolidColorBrush(
        D2D1::ColorF(0.82f, 0.72f, 0.59f), // minibuffer bg color (golden)
        &greyBrush);
}

// to draw the text on the window
void Render(HWND hwnd) {
    CreateResources(hwnd);

    g_rt->BeginDraw();

    g_rt->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    // Apply scroll transform (affects text + caret)
    g_rt->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, -g_scrollY));

    RECT rc;
    GetClientRect(hwnd, &rc);
    float windowHeight = (float)(rc.bottom - rc.top);
    float windowWidth = (float)(rc.right - rc.left);
    float editorHeight = windowHeight - g_minibufferHeight;

    int firstVisibleLine = (int)(g_scrollY / g_lineHeight);
    int lastVisibleLine = (int)((g_scrollY + editorHeight) / g_lineHeight);

    for (int i = firstVisibleLine; i <= lastVisibleLine; i++) {
        float y = i * g_lineHeight;

        std::wstring number = std::to_wstring(i + 1);

        D2D1_RECT_F layoutRect =
            D2D1::RectF(0, y, g_gutterWidth - 5, y + g_lineHeight);

        g_rt->DrawTextW(number.c_str(), number.length(), g_gutterFormat,
                        layoutRect, g_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP,
                        DWRITE_MEASURING_MODE_NATURAL);
    }

    // shift the editor area to the right (for the gutter)
    g_rt->SetTransform(D2D1::Matrix3x2F::Translation(
        g_gutterWidth + gutter_padding, -g_scrollY));

    // Draw text
    if (g_textLayout) {
        g_rt->DrawTextLayout(D2D1::Point2F(0.0f, 0.0f), g_textLayout, g_brush);
    }

    if (g_caretVisible && g_textLayout) {
        // Only draw if caret is inside visible vertical range
        if (g_caretY + g_caretHeight >= g_scrollY &&
            g_caretY <= g_scrollY + windowHeight) {
            D2D1_RECT_F caretRect = D2D1::RectF(
                g_caretX, g_caretY, g_caretX + 2.0f, g_caretY + g_caretHeight);

            g_rt->FillRectangle(&caretRect, g_brush);
        }
    }

    // Reset transform AFTER everything that scrolls
    g_rt->SetTransform(D2D1::Matrix3x2F::Identity());

    // Draw grey background bar
    D2D1_RECT_F barRect = D2D1::RectF(0, windowHeight - g_minibufferHeight,
                                      windowWidth, windowHeight);

    g_rt->FillRectangle(&barRect, greyBrush);

    if (g_minibufferActive) {
        std::wstring fullText = g_minibufferPrompt + g_minibufferText;

        IDWriteTextLayout *layout = nullptr;

        g_dwFactory->CreateTextLayout(
            fullText.c_str(), (UINT32)fullText.length(), g_minibufferFormat,
            windowWidth, g_minibufferHeight, &layout);

        g_rt->DrawTextLayout(
            D2D1::Point2F(4.0f, windowHeight - g_minibufferHeight), layout,
            blackBrush);

        if (layout)
            layout->Release();
    }

    HRESULT hr = g_rt->EndDraw();

    if (hr == D2DERR_RECREATE_TARGET) {
        if (g_rt)
            g_rt->Release();
        if (g_brush)
            g_brush->Release();
    }

    UpdateFPS(hwnd);
}

void ShowCursorAndResetBlink(HWND hwnd)
{
    g_caretVisible = true;

    // reset timer to start blinking from now
    KillTimer(hwnd, CURSOR_TIMER_ID);
    SetTimer(hwnd, CURSOR_TIMER_ID, 500, NULL);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        SetTimer(hwnd, CURSOR_TIMER_ID, 500, nullptr);
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        Render(hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE: {
        if (g_rt) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            g_rt->Resize(D2D1::SizeU(rc.right, rc.bottom));
            UpdateScrollBar(hwnd);
        }
        return 0;
    }

    case WM_VSCROLL: {
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);

        int yPos = si.nPos;

        switch (LOWORD(wParam)) {
        case SB_LINEUP:
            yPos -= 20;
            break;
        case SB_LINEDOWN:
            yPos += 20;
            break;
        case SB_PAGEUP:
            yPos -= si.nPage;
            break;
        case SB_PAGEDOWN:
            yPos += si.nPage;
            break;
        case SB_THUMBTRACK:
            yPos = si.nTrackPos;
            break;
        }

        yPos = max(0, min(yPos, si.nMax - (int)si.nPage));

        si.nPos = yPos;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

        g_scrollY = (float)yPos;

	ShowCursorAndResetBlink(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        g_scrollY -= delta / 4.0f;

        g_scrollY = max(0.0f, min(g_scrollY, g_contentHeight));

        UpdateScrollBar(hwnd);
	ShowCursorAndResetBlink(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    // for cursor (caret) blinking
    case WM_TIMER: {
        g_caretVisible = !g_caretVisible;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    // to handle typing into the editor
    case WM_CHAR: {
        wchar_t ch = (wchar_t)wParam;
	ShowCursorAndResetBlink(hwnd);

        // to handle typing inside the minibuffer
        if (g_minibufferActive) {
            if (ch == VK_RETURN) {
                RECT rc;
                GetClientRect(hwnd, &rc);

                LoadFileAndCreateLayout(
                    g_minibufferText.c_str(), (float)(rc.right - rc.left),
                    (float)(rc.bottom - rc.top - g_minibufferHeight));

                g_minibufferActive = false;
                g_minibufferText.clear();
            } else if (ch == VK_BACK) {
                if (!g_minibufferText.empty())
                    g_minibufferText.pop_back();
            } else if (ch >= 32) {
                g_minibufferText.push_back(ch);
            }

            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        // Ignore control chars except newline
        if (ch >= 32 || ch == VK_RETURN) {
            if (ch == L'\r')
                ch = L'\n'; // normalize new line characters

            if (g_caretPos > g_text.length())
                g_caretPos = (UINT32)g_text.length();

            // Insert at caret
            g_text.insert(g_caretPos, 1, ch);
            g_caretPos++;

            RECT rc;
            GetClientRect(hwnd, &rc);

            RebuildTextLayout((float)(rc.right - rc.left),
                              (float)(rc.bottom - rc.top));

            InvalidateRect(hwnd, NULL, FALSE);
            UpdateCaretPosition();
        }
        return 0;
    }

    case WM_KEYDOWN: {
        // handle ctrl key commands
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        if (ctrl && wParam == 'X') {
            g_waitingForCtrlX = true;
            return 0;
        }

        if (ctrl && wParam == 'S') // Save File (C-x C-s)
        {
            SaveFile();

            g_minibufferPrompt = L"Saved File";
            g_minibufferActive = true;
            g_minibufferText.clear();
            g_waitingForCtrlX = false;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        if (ctrl && wParam == 'G') // Quit minibuffer (C-g)
        {
            g_waitingForCtrlX = false;
            g_minibufferActive = false;
            g_minibufferText.clear();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        if (ctrl && wParam == 'O') // Open file browse dialog (C-o)
        {
            std::wstring filePath = OpenFileDialog(hwnd);
            if (!filePath.empty()) {
                RECT rc;
                GetClientRect(hwnd, &rc);

                LoadFileAndCreateLayout(
                    filePath.c_str(), (float)(rc.right - rc.left),
                    (float)(rc.bottom - rc.top - g_minibufferHeight));
            }

            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        if (g_waitingForCtrlX && ctrl && wParam == 'F') // Find File (C-x C-f)
        {
            g_minibufferPrompt = L"Find File: ";
            g_minibufferActive = true;
            g_minibufferText.clear();
            g_waitingForCtrlX = false;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        g_waitingForCtrlX = false;

        if (g_minibufferActive)
            return 0;

	ShowCursorAndResetBlink(hwnd);

        switch (wParam) {
        case VK_LEFT:
            if (g_caretPos > 0)
                g_caretPos--;
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateCaretPosition();
            return 0;

        case VK_RIGHT: {
            if (g_caretPos < g_text.length())
                g_caretPos++;
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateCaretPosition();
            return 0;
        }

        case VK_BACK: {
            if (g_caretPos > 0) {
                g_text.erase(g_caretPos - 1, 1);
                g_caretPos--;

                RECT rc;
                GetClientRect(hwnd, &rc);
                RebuildTextLayout((float)(rc.right - rc.left),
                                  (float)(rc.bottom - rc.top));

                InvalidateRect(hwnd, NULL, FALSE);
                UpdateCaretPosition();
            }
            return 0;
        }

        case VK_DELETE: {
            if (g_caretPos < g_text.length()) {
                g_text.erase(g_caretPos, 1);

                RECT rc;
                GetClientRect(hwnd, &rc);
                RebuildTextLayout((float)(rc.right - rc.left),
                                  (float)(rc.bottom - rc.top));

                InvalidateRect(hwnd, NULL, FALSE);
                UpdateCaretPosition();
            }
            return 0;
        }
        }

        UINT32 textLen = g_textLength;

        if (wParam == VK_RIGHT && g_caretPos < textLen)
            g_caretPos++;

        if (wParam == VK_LEFT && g_caretPos > 0)
            g_caretPos--;

        if (wParam == VK_UP || wParam == VK_DOWN) {
            DWRITE_HIT_TEST_METRICS ht;
            FLOAT x, y;

            g_textLayout->HitTestTextPosition(g_caretPos, FALSE, &x, &y, &ht);

            FLOAT newY = y + (wParam == VK_DOWN ? ht.height : -ht.height);

            BOOL isTrailing;
            BOOL inside;
            UINT32 newPos;

            g_textLayout->HitTestPoint(x - g_gutterWidth - gutter_padding, newY,
                                       &isTrailing, &inside, &ht);

            newPos = ht.textPosition;
            if (isTrailing)
                newPos++;

            g_caretPos = newPos;
        }

        UpdateCaretPosition();
        InvalidateRect(hwnd, nullptr, FALSE);

        EnsureCaretVisible(hwnd);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        float x = (float)GET_X_LPARAM(lParam);
        float y = (float)GET_Y_LPARAM(lParam);

        // adjust for scroll
        y += g_scrollY;

        BOOL trailing;
        BOOL inside;
        DWRITE_HIT_TEST_METRICS metrics;

        g_textLayout->HitTestPoint(x - g_gutterWidth - gutter_padding, y,
                                   &trailing, &inside, &metrics);

        g_caretPos = metrics.textPosition;
        if (trailing)
            g_caretPos++;

        UpdateCaretPosition();
	ShowCursorAndResetBlink(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_DESTROY: {
        if (g_brush)
            g_brush->Release();
        if (greyBrush)
            greyBrush->Release();
        if (blackBrush)
            blackBrush->Release();
        if (g_rt)
            g_rt->Release();
        if (g_textFormat)
            g_textFormat->Release();
        if (g_textLayout)
            g_textLayout->Release();
        if (g_dwFactory)
            g_dwFactory->Release();
        if (g_factory)
            g_factory->Release();
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    CoInitialize(nullptr);

    // initialize the fps counter
    QueryPerformanceFrequency(&g_fps.frequency);
    QueryPerformanceCounter(&g_fps.lastTime);
    g_fps.frameCount = 0;

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_factory);

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown **>(&g_dwFactory));

    g_dwFactory->CreateTextFormat(
        font_type, nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-us", &g_textFormat);

    g_dwFactory->CreateTextFormat(
        font_type, nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, // minibuffer text is heavier
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, font_size,
        L"en-us", &g_minibufferFormat);

    g_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    g_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    g_dwFactory->CreateTextFormat(
        font_type, nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, font_size, L"en-us", &g_gutterFormat);

    // Right align inside layout rectangle
    g_gutterFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    g_gutterFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DWWindow";

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, L"DWWindow", L"feditor", WS_OVERLAPPEDWINDOW | WS_VSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, feditor_window_width,
        feditor_window_height, nullptr, nullptr, hInstance, nullptr);

    RECT rc;
    GetClientRect(hwnd, &rc);

    g_dwFactory->CreateTextLayout(g_text.c_str(), 0, g_textFormat,
                                  (float)(rc.right - rc.left),
                                  (float)(rc.bottom - rc.top), &g_textLayout);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return 0;
}
