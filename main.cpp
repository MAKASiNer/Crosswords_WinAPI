#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <vector>
#include <string>
#include <map>

#include "Table.h"


// hMenu ������� ����������
#define CROSSWORD_INPUTBOX 5
#define RESTART_BUT 6


// ���������� ������� ����
Table table(std::vector<Word>(0));

// ������� �������� ������ Arial � �������� h
HFONT myFont(UINT h) {
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfHeight = h;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    memcpy(lf.lfFaceName, L"Arial", sizeof(TCHAR) * 6);
    HFONT font = CreateFontIndirect(&lf);
    return font;
}

// ��� ������� ����� ������ �� ��������� �� �������� ������������ � ���������� �� � ������
std::vector<std::wstring> sepWstring(const std::wstring& wstring, TCHAR separator) {
    std::vector<std::wstring> res;
    res.reserve(wstring.length());
    std::wstring buf;
    buf.reserve(wstring.length());

    for (auto& lit : wstring) {
        // ���� ������ ����� �����������, �� ������ ���������� � �������������� ������
        if (lit == separator) {
            res.push_back(buf);
            buf.clear();
        }
        // ����� ������������ ���������� �������
        else buf += lit;
    }
    if (not buf.empty()) res.push_back(buf);
    return res;
}


// \brief ����������� ����
// \param hwnd ���������� �������� ����
// \param hInst ���������� ���������� 
LONG WINAPI WndReg(HWND hwnd, HINSTANCE hInst) {
    // ��������������� ����������
    RECT rect;
    UINT x(0), y(0);
    
    static std::wstring wbuf; // ��������� ������

    // �����
    static HFONT norm_font = myFont(0.75 * table.tile);

    // ����������� ���� edit ��� ����������
    for (UINT y = 0; y < table.height; y++) {
        for (UINT x = 0; x < table.width; x++) {
            int i = y * table.width + x;
            auto& hEdt = table.hEdtTable[i];
            wchar_t lit = table.rightLetters[i];

            // ��������� ���� edit ���� � ������ �� ����������� �������� ������ �����
            if (lit == table.voidLit) continue;
            hEdt = CreateWindow(L"edit", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_UPPERCASE,
                x * table.tile + table.shift_w, y * table.tile + table.shift_h, table.tile + 1, table.tile + 1,
                hwnd, (HMENU)CROSSWORD_INPUTBOX, hInst, NULL
            );
            // ��������� ������
            SendMessage(hEdt, WM_SETFONT, (WPARAM)norm_font, NULL);
            // ������ ������ ������� edit
            SendMessage(hEdt, EM_LIMITTEXT, (WPARAM)1, NULL);
        }
    }

    // ������� ������� ��������� ����������
    x = table.inter_w + table.shift_w;
    y = table.shift_h * 2 + 8 * table.tile;
    if (table.inter_h < y) table.inter_h = y;


    // ����������� ���� ����� ������
    table.hPlayerName = CreateWindow(L"edit", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
        x, table.shift_h + 2 * table.tile, (table.tile + 1) * 5, table.tile + 1,
        hwnd, NULL, hInst, NULL
    );
    SendMessage(table.hPlayerName, WM_SETFONT, (WPARAM)norm_font, NULL);

    // ����������� ������ �����������
    table.hCrosswords = CreateWindow(L"listbox", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_STANDARD,
        x, table.shift_h + 4 * table.tile, (table.tile + 1) * 5, 3 * table.tile,
        hwnd, (HMENU)CROSSWORD_INPUTBOX, hInst, NULL);
    table.LoadCrosswordsList();

    // ����������� ������ ��������
    table.hRestart = CreateWindow(L"button", NULL,
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        x, table.shift_h + 6.5 * table.tile, (table.tile + 1) * 5, table.tile + 1,
        hwnd, (HMENU)RESTART_BUT, hInst, NULL);

    return NULL;
};


// \brief ������� ��������� ���������
// \param hwnd ���������� �������� ����
// \param message ��������� 
// \param wparam � lparam ��������� ������ �����������
LONG WINAPI WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    // ��������������� ����������
    UINT x(0), y(0);        // ����������
    RECT rect;              // �������

    static HINSTANCE hInst;
    static TCHAR buf[32];            // ������
    static std::wstring wbuf;        // ��������� ������

    static std::wstring selectedCrossword;    // ��������� ��������� (� listbox)

    // ������ ��� ������� ����, ��� ����������, ��� ���������� ������
    static HFONT
        big_font = myFont(table.tile),
        norm_font = myFont(0.75 * table.tile),
        small_font = myFont(table.tile / 2);

    // ������
    static SYSTEMTIME timer;
    static bool lockTimer = true;

    // ���������
    static bool scrollLock(false);

    // ��������� ���������
    switch (message) {

    // ��������� �������� ����
    case WM_CREATE:
    {
        hInst = ((LPCREATESTRUCT)lparam)->hInstance;
        WndReg(hwnd, hInst);
        SetWindowText(table.hPlayerName, L"Noname");
        return NULL;
    }
    break;

    // ��������� ����� ������ � ������� ���� edit-�� ����������
    case WM_CTLCOLOREDIT:
    {
        // ���� ����������� - ���� ����������
        auto iter = std::find(table.hEdtTable.begin(), table.hEdtTable.end(), (HWND)lparam);
        if (iter != table.hEdtTable.end()) {
            int i = iter - table.hEdtTable.begin();
            HWND hEdt = table.hEdtTable[i];

            // ������������� ��, ������� ���������� ������ � ������������ ������ �����
            auto words = table.FindWords(i % table.width, i / table.width);
            for (auto word : words) {
                if (table.CheckWord(word) == 2) {
                    SetTextColor((HDC)wparam, RGB(200, 0, 0));
                    break;
                } else SetTextColor((HDC)wparam, RGB(0, 0, 0));
            }
            // ����������� ���
            SetBkColor((HDC)wparam, RGB(230, 230, 230));
        }
    }
    return (INT_PTR)GetStockObject(NULL_BRUSH);

    // ��������� �� �������
    case WM_TIMER:
    {
        // ���� ������� �� �������, �� ���� ������ �������
        if (lockTimer) return NULL;
        GetSystemTime(&timer);
        if (table.seconds >= 59) {
            table.seconds = 0;
            table.minutes += 1;
        }
        else table.seconds++;

        // ����������� ������� �������
        rect.left = table.shift_w * 2 + table.width * table.tile;
        rect.top = table.shift_h;
        rect.right = rect.left + 5 * table.tile;
        rect.bottom = rect.top + table.shift_h;
        InvalidateRect(hwnd, &rect, true);
    }
    break;

    // ������ �� ������� ����������
    case WM_COMMAND:
    {
        // ����� �� ������ ��������
        if (wparam == RESTART_BUT) {
            // ����������� ����� ������
            if (GetWindowText(table.hPlayerName, buf, 10)) wbuf = buf;
            else wbuf = L"Noname";

            // ���� ������ �����-������ ���������
            if (selectedCrossword.size()) {
                // ���������� �������� ���������� � ������� ��������
                table = Table(table.LoadCrosswordFromFile(selectedCrossword));
                table.loadRecordTable(selectedCrossword);
                // ��������������� ����
                WndReg(hwnd, hInst);
                SetWindowText(table.hPlayerName, wbuf.c_str());
                lockTimer = false;
            };

            InvalidateRect(hwnd, NULL, true);
            return NULL;
        }

        // ����� ������ ���������� 
        if (HIWORD(wparam) == LBN_SELCHANGE) {
            SendMessage(table.hCrosswords, LB_GETTEXT,
                (INT)SendMessage(table.hCrosswords, LB_GETCURSEL, NULL, NULL), (LPARAM)buf);
            selectedCrossword = buf;
            return NULL;
        }


        // ���� ����� �� edit ���� ����������
        // wparam & CROSSWORD_INPUTBOX == CROSSWORD_INPUTBOX
        bool needRedraw(false);
        for (UINT y = 0; y < table.height; y++) {
            for (UINT x = 0; x < table.width; x++) {
                int i = y * table.width + x;
                auto& hEdt = table.hEdtTable[i];
                GetWindowText(hEdt, buf, 2);
                // ��� ���� ���������� edit ����������
                if (buf[0] != table.userLetters[i]) {
                    if (buf[0] == 0) table.userLetters[i] = table.voidLit;
                    else table.userLetters[i] = buf[0];
                    needRedraw = true;
                }
            }
        }
        // ����������� ����������
        if (needRedraw) {
            needRedraw = false;
            rect.left = 0;
            rect.top = 0;
            rect.right = rect.left + table.tile * table.width;
            rect.bottom = rect.top + table.tile * table.height;
            InvalidateRect(hwnd, NULL, false);

            // ���� ����� �������
            if (table.CheckWin()) {
                if (not lockTimer) {
                    lockTimer = true;
                    // ���������� ������ �������
                    if (GetWindowText(table.hPlayerName, buf, 10)) wbuf = buf;
                    else wbuf = L"Noname";
                    table.loadRecordTable(
                        selectedCrossword, wbuf,
                        table.minutes * 60 + table.seconds);
                    InvalidateRect(hwnd, NULL, true);
                    // � ����������� ���������� ����
                    MessageBox(hwnd, L"����������, �� ��������!", L"", MB_OK);
                }
            }
            else InvalidateRect(hwnd, NULL, false);
        }
    }
    break;

    // ������ �������� �����
    case WM_MOUSEWHEEL:
    {
        // ������������ ��������� �� table.tile
        INT scrollDelta = table.tile * (GET_WHEEL_DELTA_WPARAM(wparam) / abs(GET_WHEEL_DELTA_WPARAM(wparam)));
        // ����������� ���������
        if (scrollDelta > 0) {
            if (scrollDelta + table.scroll_h > 0) table.scroll_h = 0;
            else table.scroll_h += scrollDelta;
        }
        else if (not scrollLock) table.scroll_h += scrollDelta;
        // ��������� ������� � ��������� ����
        GetClientRect(hwnd, &rect);
        rect.left = table.shift_w;
        rect.top = table.inter_h + 1;
        InvalidateRect(hwnd, &rect, true);
    }
    break;

    // ��������� ������� ����, ��������, ������� � �.�.
    case WM_PAINT:
    {
        // ������� ���������
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // ��������� ���������� ������ � ��������� ������� ����
        SelectObject(hdc, small_font);
        for (UINT i = 0; i < table.wordlist.size(); i++) {
            wbuf = std::to_wstring(i + 1);
            // ����� ������ ����� �� ��������������� �����������
            if (table.wordlist[i].direction == HORIZONTAL) {
                // ������ � ������ ������ ������ �� ��������
                // ������ ��������� ������������ ������ �� �����������
                if (wbuf.size() == 1)  x = (table.wordlist[i].x * table.tile - table.tile / 4 + table.shift_w);
                else x = (table.wordlist[i].x * table.tile - table.tile / 2 + table.shift_w);
                y = (table.wordlist[i].y * table.tile + table.shift_h + 2);
            }
            else if (table.wordlist[i].direction == VERTICAL) {
                x = (table.wordlist[i].x * table.tile + table.shift_w + 2);
                y = (table.wordlist[i].y * table.tile - table.tile / 2 + table.shift_h);
            }
            TextOut(hdc, x, y, wbuf.c_str(), wbuf.length());
        }

        // ��������� �������� ������
        SelectObject(hdc, big_font);

        // ��������� �������
        x = table.inter_w + table.shift_w;
        y = table.shift_h;
        wbuf = std::to_wstring(table.minutes) + L":" + std::to_wstring(table.seconds);
        TextOut(hdc, x, y, wbuf.c_str(), wbuf.length());

        // ��������� ������� ��������
        x = table.inter_w + table.tile * 5 + 2 * table.shift_w;
        y = table.shift_h;
        for (int i = 0; i < 10; i++) {
            wbuf = std::to_wstring(i + 1) + L": " + table.records[i];
            TextOut(hdc, x, y, wbuf.c_str(), wbuf.length());
            y += table.tile;
            if (y > table.inter_h) {
                y = table.shift_h;
                x += table.shift_w + table.tile * 5;
            }
        }

        // ��������� �������� ���� � ������ ���������
        x = table.shift_w;
        y = table.inter_h + table.tile + table.scroll_h;
        if (y > table.inter_h) {
            wbuf = L"�� �����������:";
            TextOut(hdc, x, y, wbuf.c_str(), wbuf.length());
        }
        for (UINT i = 0; i < table.wordlist.size(); i++) {
            auto& word = table.wordlist[i];
            if (word.direction == HORIZONTAL) {
                wbuf = std::to_wstring(i + 1) + L". " + word.description;
                for (auto wstr : sepWstring(wbuf, table.sepLit)) {
                    y += table.tile;
                    if (y <= table.inter_h) continue;
                    TextOut(hdc, x + table.shift_w, y, wstr.c_str(), wstr.length());
                }
            }
        }

        y += table.shift_h;
        if (y > table.inter_h) {
            wbuf = L"�� ���������:";
            TextOut(hdc, x, y, wbuf.c_str(), wbuf.length());
        }
        for (UINT i = 0; i < table.wordlist.size(); i++) {
            auto& word = table.wordlist[i];
            if (word.direction == VERTICAL) {
                wbuf = std::to_wstring(i + 1) + L". " + word.description;
                for (auto wstr : sepWstring(wbuf, table.sepLit)) {
                    y += table.tile;
                    if (y <= table.inter_h) continue;
                    TextOut(hdc, x + table.shift_w, y, wstr.c_str(), wstr.length());
                }
            }
        }

        // ���� �������� �������� ������ �����������, �� ����������� ��������� �����
        if (y < table.inter_h) scrollLock = true;
        else scrollLock = false;
        EndPaint(hwnd, &ps);
    }
    break;

    // �������� ����
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    }
    break;

    default:
        return DefWindowProc(hwnd, message, wparam, lparam);
    }
    return NULL;
}


// ��������� �������
// � ��� �������� ����������� �������� ����
// � ����� � ��� �������� ��� ���������� mainloop (������� ����)
INT  WINAPI  WinMain(HINSTANCE  hInstance,  HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow) {
    // ����������, ��������� � ������ �������� ����
    HWND hwnd;
    MSG msg;
    WNDCLASS w;

    // �������� � ����������� ������ 
    memset(&w, 0, sizeof(WNDCLASS));
    w.style = CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT;
    w.lpfnWndProc = WndProc;
    w.hInstance = hInstance;
    w.hbrBackground = CreateSolidBrush(0x00FFFFFF);
    w.lpszClassName = L"MainWindow";
    RegisterClass(&w);

    // �������� � ����������� ��������� ����
    hwnd = CreateWindow(L"MainWindow", L"Crossword",
        WS_OVERLAPPEDWINDOW,
        500, 300, 800, 600,
        NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // �������� ������� � ���������� 1 �������
    SetTimer(hwnd, NULL, 1000, NULL);

    // ������� ����
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}