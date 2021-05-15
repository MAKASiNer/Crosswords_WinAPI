#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <vector>
#include <string>
#include <map>

#include "Table.h"


// hMenu органів управелнія 
#define CROSSWORD_INPUTBOX 5
#define RESTART_BUT 6


// глобальне ігрове поле 
Table table(std::vector<Word>(0));

// функція створення шрифту Arial з розміром х 
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

// ця функція ділить рядок на подстроки по символам розділювачам і записує їх в вектор 
std::vector<std::wstring> sepWstring(const std::wstring& wstring, TCHAR separator) {
    std::vector<std::wstring> res;
    res.reserve(wstring.length());
    std::wstring buf;
    buf.reserve(wstring.length());

    for (auto& lit : wstring) {
        // якщо символ дорівнює разделителю, то буфер поміщається в результуючий вектор 
        if (lit == separator) {
            res.push_back(buf);
            buf.clear();
        }
        // інакше триває заповнення буфера 
        else buf += lit;
    }
    if (not buf.empty()) res.push_back(buf);
    return res;
}


// \brief Реєстрація вікон 
// ЕПАР hwnd дескриптор головного вікна 
// ЕПАР hInst дескриптор додатки 
LONG WINAPI WndReg(HWND hwnd, HINSTANCE hInst) {
    // допоміжні змінні 
    RECT rect;
    UINT x(0), y(0);
    
    static std::wstring wbuf; // строковий буфер 

    // шрифт 
    static HFONT norm_font = myFont(0.75 * table.tile);

    // реєстрація вікон edit для кросворду 
    for (UINT y = 0; y < table.height; y++) {
        for (UINT x = 0; x < table.width; x++) {
            int i = y * table.width + x;
            auto& hEdt = table.hEdtTable[i];
            wchar_t lit = table.rightLetters[i];

            // створюється вікно edit якщо в комірка по координатам є частиною слова 
            if (lit == table.voidLit) continue;
            hEdt = CreateWindow(L"edit", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_UPPERCASE,
                x * table.tile + table.shift_w, y * table.tile + table.shift_h, table.tile + 1, table.tile + 1,
                hwnd, (HMENU)CROSSWORD_INPUTBOX, hInst, NULL
            );
            // установка шрифту 
            SendMessage(hEdt, WM_SETFONT, (WPARAM)norm_font, NULL);
            // задає розмір буфера edit 
            SendMessage(hEdt, EM_LIMITTEXT, (WPARAM)1, NULL);
        }
    }

    // завдання області відтворення кросворду 
    x = table.inter_w + table.shift_w;
    y = table.shift_h * 2 + 8 * table.tile;
    if (table.inter_h < y) table.inter_h = y;


    // реєстрація вікна імені гравця 
    table.hPlayerName = CreateWindow(L"edit", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
        x, table.shift_h + 2 * table.tile, (table.tile + 1) * 5, table.tile + 1,
        hwnd, NULL, hInst, NULL
    );
    SendMessage(table.hPlayerName, WM_SETFONT, (WPARAM)norm_font, NULL);

    // реєстрація списку кросвордів 
    table.hCrosswords = CreateWindow(L"listbox", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_STANDARD,
        x, table.shift_h + 4 * table.tile, (table.tile + 1) * 5, 3 * table.tile,
        hwnd, (HMENU)CROSSWORD_INPUTBOX, hInst, NULL);
    table.LoadCrosswordsList();

    // реєстрація кнопки рестарту 
    table.hRestart = CreateWindow(L"button", NULL,
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        x, table.shift_h + 6.5 * table.tile, (table.tile + 1) * 5, table.tile + 1,
        hwnd, (HMENU)RESTART_BUT, hInst, NULL);

    return NULL;
};


// \brief функція обробки повідомлень 
// ЕПАР hwnd дескриптор головного вікна 
// ЕПАР message повідомлення 
// ЕПАР wparam і lparam параметри обміну інформацією 
LONG WINAPI WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    // допоміжний змінні 
    UINT x(0), y(0);        // координати 
    RECT rect;              // область 

    static HINSTANCE hInst;
    static TCHAR buf[32];            // буфер 
    static std::wstring wbuf;        // строковий буфер 

    static std::wstring selectedCrossword;    // обраний кросворд (в listbox) 

    // шрифти для номерів слів, для кросворду, для решти тексту 
    static HFONT
        big_font = myFont(table.tile),
        norm_font = myFont(0.75 * table.tile),
        small_font = myFont(table.tile / 2);

    // таймер 
    static SYSTEMTIME timer;
    static bool lockTimer = true;

    // скролінг 
    static bool scrollLock(false);

    // обробка повідомлень 
    switch (message) {

    // первинне створення вікон 
    case WM_CREATE:
    {
        hInst = ((LPCREATESTRUCT)lparam)->hInstance;
        WndReg(hwnd, hInst);
        SetWindowText(table.hPlayerName, L"Noname");
        return NULL;
    }
    break;

    // налаштування кольору тексту і заднього фону edit-ів кросворду 
    case WM_CTLCOLOREDIT:
    {
        // якщо відправник - поле кросворду 
        auto iter = std::find(table.hEdtTable.begin(), table.hEdtTable.end(), (HWND)lparam);
        if (iter != table.hEdtTable.end()) {
            int i = iter - table.hEdtTable.begin();
            HWND hEdt = table.hEdtTable[i];

            // закрашуються ті, які складають разом з відправником єдине слово 
            auto words = table.FindWords(i % table.width, i / table.width);
            for (auto word : words) {
                if (table.CheckWord(word) == 2) {
                    SetTextColor((HDC)wparam, RGB(200, 0, 0));
                    break;
                } else SetTextColor((HDC)wparam, RGB(0, 0, 0));
            }
            // зафарбовує фон 
            SetBkColor((HDC)wparam, RGB(230, 230, 230));
        }
    }
    return (INT_PTR)GetStockObject(NULL_BRUSH);

    // повідомлення від таймера 
    case WM_TIMER:
    {
        // якщо лічильник НЕ залочений, то йде відлік часу 
        if (lockTimer) return NULL;
        GetSystemTime(&timer);
        if (table.seconds >= 59) {
            table.seconds = 0;
            table.minutes += 1;
        }
        else table.seconds++;

        // перерисовка області таймера 
        rect.left = table.shift_w * 2 + table.width * table.tile;
        rect.top = table.shift_h;
        rect.right = rect.left + 5 * table.tile;
        rect.bottom = rect.top + table.shift_h;
        InvalidateRect(hwnd, &rect, true);
    }
    break;

    // івенти від органів управління 
    case WM_COMMAND:
    {
        // івент від кнопки рестарту 
        if (wparam == RESTART_BUT) {
            // буферизация імені гравця 
            if (GetWindowText(table.hPlayerName, buf, 10)) wbuf = buf;
            else wbuf = L"Noname";

            // якщо обраний який-небудь кросворд 
            if (selectedCrossword.size()) {
                // оновлення головного кросворду і таблиці рекордів 
                table = Table(table.LoadCrosswordFromFile(selectedCrossword));
                table.loadRecordTable(selectedCrossword);
                // перереєстрація вікон 
                WndReg(hwnd, hInst);
                SetWindowText(table.hPlayerName, wbuf.c_str());
                lockTimer = false;
            };

            InvalidateRect(hwnd, NULL, true);
            return NULL;
        }

        // івент вибору кросворду 
        if (HIWORD(wparam) == LBN_SELCHANGE) {
            SendMessage(table.hCrosswords, LB_GETTEXT,
                (INT)SendMessage(table.hCrosswords, LB_GETCURSEL, NULL, NULL), (LPARAM)buf);
            selectedCrossword = buf;
            return NULL;
        }


        // якщо івент від edit вікон кросворду 
        // wparam & CROSSWORD_INPUTBOX == CROSSWORD_INPUTBOX 
        bool needRedraw(false);
        for (UINT y = 0; y < table.height; y++) {
            for (UINT x = 0; x < table.width; x++) {
                int i = y * table.width + x;
                auto& hEdt = table.hEdtTable[i];
                GetWindowText(hEdt, buf, 2);
                // при цьому зміст edit змінилося 
                if (buf[0] != table.userLetters[i]) {
                    if (buf[0] == 0) table.userLetters[i] = table.voidLit;
                    else table.userLetters[i] = buf[0];
                    needRedraw = true;
                }
            }
        }
        // перерисовка кросворду 
        if (needRedraw) {
            needRedraw = false;
            rect.left = 0;
            rect.top = 0;
            rect.right = rect.left + table.tile * table.width;
            rect.bottom = rect.top + table.tile * table.height;
            InvalidateRect(hwnd, NULL, false);

            // якщо гравець переміг 
            if (table.CheckWin()) {
                if (not lockTimer) {
                    lockTimer = true;
                    // відбувається запис рекорду 
                    if (GetWindowText(table.hPlayerName, buf, 10)) wbuf = buf;
                    else wbuf = L"Noname";
                    table.loadRecordTable(
                        selectedCrossword, wbuf,
                        table.minutes * 60 + table.seconds);
                    InvalidateRect(hwnd, NULL, true);
                    // і вискакує діалогове вікно 
                    MessageBox(hwnd, L"Вітаю, ви перемогли!", L"", MB_OK);
                }
            }
            else InvalidateRect(hwnd, NULL, false);
        }
    }
    break;

    // івенти коліщатка мишки 
    case WM_MOUSEWHEEL:
    {
        // нормалізація прокрутки по table.tile 
        INT scrollDelta = table.tile * (GET_WHEEL_DELTA_WPARAM(wparam) / abs(GET_WHEEL_DELTA_WPARAM(wparam)));
        // обмежень прокрутки 
        if (scrollDelta > 0) {
            if (scrollDelta + table.scroll_h > 0) table.scroll_h = 0;
            else table.scroll_h += scrollDelta;
        }
        else if (not scrollLock) table.scroll_h += scrollDelta;
        // отрисовка області з описом слів 
        GetClientRect(hwnd, &rect);
        rect.left = table.shift_w;
        rect.top = table.inter_h + 1;
        InvalidateRect(hwnd, &rect, true);
    }
    break;

    // отрисовка номерів слів, питань, таймера і т.п. 
    case WM_PAINT:
    {
        // область малювання 
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // установка маленького шрифту і отрисовка номерів слів 
        SelectObject(hdc, small_font);
        for (UINT i = 0; i < table.wordlist.size(); i++) {
            wbuf = std::to_wstring(i + 1);
            // висновок номера слова по відповідним координатам 
            if (table.wordlist[i].direction == HORIZONTAL) {
                // висота з зростанням довжини номера НЕ змінюється 
                // тому потрібно вирівнювання тільки по горизонталі 
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

        // установка великого шрифту 
        SelectObject(hdc, big_font);

        // отрисовка таймера 
        x = table.inter_w + table.shift_w;
        y = table.shift_h;
        wbuf = std::to_wstring(table.minutes) + L":" + std::to_wstring(table.seconds);
        TextOut(hdc, x, y, wbuf.c_str(), wbuf.length());

        // отрисовка таблиці рекордів 
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

        // отрисовка опис слів з урахуванням прокрутки 
        x = table.shift_w;
        y = table.inter_h + table.tile + table.scroll_h;
        if (y > table.inter_h) {
            wbuf = L"По горизонталі:";
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
            wbuf = L"По вертикалі:";
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

        // якщо ні одного опису не було отрисовать, то блокується прокрутка вгору 
        if (y < table.inter_h) scrollLock = true;
        else scrollLock = false;
        EndPaint(hwnd, &ps);
    }
    break;

    // закриття вікна 
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


// Стартова функція 
// В ній проходить реєстрація головного вікна 
// А також в ній знаходиться так званий mainloop (головний цикл) 
INT  WINAPI  WinMain(HINSTANCE  hInstance,  HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow) {
    // дескриптор, повідомлення і шаблон головного вікна 
    HWND hwnd;
    MSG msg;
    WNDCLASS w;

    // створення і реєстрація класу 
    memset(&w, 0, sizeof(WNDCLASS));
    w.style = CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT;
    w.lpfnWndProc = WndProc;
    w.hInstance = hInstance;
    w.hbrBackground = CreateSolidBrush(0x00FFFFFF);
    w.lpszClassName = L"MainWindow";
    RegisterClass(&w);

    hwnd = CreateWindow(L"MainWindow", L"Crossword",
        WS_OVERLAPPEDWINDOW,
        500, 300, 800, 600,
        NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    SetTimer(hwnd, NULL, 1000, NULL);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}