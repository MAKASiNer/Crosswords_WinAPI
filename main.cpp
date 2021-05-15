#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <vector>
#include <string>
#include <map>

#include "Table.h"


// hMenu органов управелния
#define CROSSWORD_INPUTBOX 5
#define RESTART_BUT 6


// глобальное игровое поле
Table table(std::vector<Word>(0));

// функция создания шрифта Arial с размером h
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

// эта функция делит строку на подстроки по символам разделителям и записывает их в вектор
std::vector<std::wstring> sepWstring(const std::wstring& wstring, TCHAR separator) {
    std::vector<std::wstring> res;
    res.reserve(wstring.length());
    std::wstring buf;
    buf.reserve(wstring.length());

    for (auto& lit : wstring) {
        // если символ равен разделителю, то буффер помещается в результирующий вектор
        if (lit == separator) {
            res.push_back(buf);
            buf.clear();
        }
        // иначе продолжается заполнение буффера
        else buf += lit;
    }
    if (not buf.empty()) res.push_back(buf);
    return res;
}


// \brief Регистрация окон
// \param hwnd дискриптор главного окна
// \param hInst дискриптор приложения 
LONG WINAPI WndReg(HWND hwnd, HINSTANCE hInst) {
    // вспомогательные переменные
    RECT rect;
    UINT x(0), y(0);
    
    static std::wstring wbuf; // строковый буффер

    // шрифт
    static HFONT norm_font = myFont(0.75 * table.tile);

    // регистрация окон edit для кроссворда
    for (UINT y = 0; y < table.height; y++) {
        for (UINT x = 0; x < table.width; x++) {
            int i = y * table.width + x;
            auto& hEdt = table.hEdtTable[i];
            wchar_t lit = table.rightLetters[i];

            // создается окно edit если в ячейка по координатам является частью слова
            if (lit == table.voidLit) continue;
            hEdt = CreateWindow(L"edit", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_UPPERCASE,
                x * table.tile + table.shift_w, y * table.tile + table.shift_h, table.tile + 1, table.tile + 1,
                hwnd, (HMENU)CROSSWORD_INPUTBOX, hInst, NULL
            );
            // установка шрифта
            SendMessage(hEdt, WM_SETFONT, (WPARAM)norm_font, NULL);
            // задает размер буффера edit
            SendMessage(hEdt, EM_LIMITTEXT, (WPARAM)1, NULL);
        }
    }

    // задание области отрисовки кроссворда
    x = table.inter_w + table.shift_w;
    y = table.shift_h * 2 + 8 * table.tile;
    if (table.inter_h < y) table.inter_h = y;


    // регистрация окна имени игрока
    table.hPlayerName = CreateWindow(L"edit", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
        x, table.shift_h + 2 * table.tile, (table.tile + 1) * 5, table.tile + 1,
        hwnd, NULL, hInst, NULL
    );
    SendMessage(table.hPlayerName, WM_SETFONT, (WPARAM)norm_font, NULL);

    // регистрация списка кроссвордов
    table.hCrosswords = CreateWindow(L"listbox", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_STANDARD,
        x, table.shift_h + 4 * table.tile, (table.tile + 1) * 5, 3 * table.tile,
        hwnd, (HMENU)CROSSWORD_INPUTBOX, hInst, NULL);
    table.LoadCrosswordsList();

    // регистрация кнопки рестарта
    table.hRestart = CreateWindow(L"button", NULL,
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        x, table.shift_h + 6.5 * table.tile, (table.tile + 1) * 5, table.tile + 1,
        hwnd, (HMENU)RESTART_BUT, hInst, NULL);

    return NULL;
};


// \brief Функция обработки сообщений
// \param hwnd дискриптор главного окна
// \param message сообщение 
// \param wparam и lparam параметры обмена информацией
LONG WINAPI WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    // вспомогательный переменные
    UINT x(0), y(0);        // координаты
    RECT rect;              // область

    static HINSTANCE hInst;
    static TCHAR buf[32];            // буффер
    static std::wstring wbuf;        // строковый буффер

    static std::wstring selectedCrossword;    // выбранный кроссворд (в listbox)

    // шрифты для номеров слов, для кроссворда, для остального текста
    static HFONT
        big_font = myFont(table.tile),
        norm_font = myFont(0.75 * table.tile),
        small_font = myFont(table.tile / 2);

    // таймер
    static SYSTEMTIME timer;
    static bool lockTimer = true;

    // скроллинг
    static bool scrollLock(false);

    // обработка сообщений
    switch (message) {

    // первичное создание окон
    case WM_CREATE:
    {
        hInst = ((LPCREATESTRUCT)lparam)->hInstance;
        WndReg(hwnd, hInst);
        SetWindowText(table.hPlayerName, L"Noname");
        return NULL;
    }
    break;

    // настройка цвета текста и заднего фона edit-ов кроссворда
    case WM_CTLCOLOREDIT:
    {
        // если отправитель - поле кроссворда
        auto iter = std::find(table.hEdtTable.begin(), table.hEdtTable.end(), (HWND)lparam);
        if (iter != table.hEdtTable.end()) {
            int i = iter - table.hEdtTable.begin();
            HWND hEdt = table.hEdtTable[i];

            // закрашиваются те, которые составляют вместе с отправителем единое слово
            auto words = table.FindWords(i % table.width, i / table.width);
            for (auto word : words) {
                if (table.CheckWord(word) == 2) {
                    SetTextColor((HDC)wparam, RGB(200, 0, 0));
                    break;
                } else SetTextColor((HDC)wparam, RGB(0, 0, 0));
            }
            // закрашивает фон
            SetBkColor((HDC)wparam, RGB(230, 230, 230));
        }
    }
    return (INT_PTR)GetStockObject(NULL_BRUSH);

    // сообщение от таймера
    case WM_TIMER:
    {
        // если счетчик не залочен, то идет отсчет времени
        if (lockTimer) return NULL;
        GetSystemTime(&timer);
        if (table.seconds >= 59) {
            table.seconds = 0;
            table.minutes += 1;
        }
        else table.seconds++;

        // перерисовка области таймера
        rect.left = table.shift_w * 2 + table.width * table.tile;
        rect.top = table.shift_h;
        rect.right = rect.left + 5 * table.tile;
        rect.bottom = rect.top + table.shift_h;
        InvalidateRect(hwnd, &rect, true);
    }
    break;

    // ивенты от органов управления
    case WM_COMMAND:
    {
        // ивент от кнопки рестарта
        if (wparam == RESTART_BUT) {
            // буферизация имени игрока
            if (GetWindowText(table.hPlayerName, buf, 10)) wbuf = buf;
            else wbuf = L"Noname";

            // если выбран какой-нибудь кроссворд
            if (selectedCrossword.size()) {
                // обновление главного кроссворда и таблицы рекордов
                table = Table(table.LoadCrosswordFromFile(selectedCrossword));
                table.loadRecordTable(selectedCrossword);
                // перерегистрация окон
                WndReg(hwnd, hInst);
                SetWindowText(table.hPlayerName, wbuf.c_str());
                lockTimer = false;
            };

            InvalidateRect(hwnd, NULL, true);
            return NULL;
        }

        // ивент выбора кроссворда 
        if (HIWORD(wparam) == LBN_SELCHANGE) {
            SendMessage(table.hCrosswords, LB_GETTEXT,
                (INT)SendMessage(table.hCrosswords, LB_GETCURSEL, NULL, NULL), (LPARAM)buf);
            selectedCrossword = buf;
            return NULL;
        }


        // если ивент от edit окон кроссворда
        // wparam & CROSSWORD_INPUTBOX == CROSSWORD_INPUTBOX
        bool needRedraw(false);
        for (UINT y = 0; y < table.height; y++) {
            for (UINT x = 0; x < table.width; x++) {
                int i = y * table.width + x;
                auto& hEdt = table.hEdtTable[i];
                GetWindowText(hEdt, buf, 2);
                // при этом содержание edit изменилось
                if (buf[0] != table.userLetters[i]) {
                    if (buf[0] == 0) table.userLetters[i] = table.voidLit;
                    else table.userLetters[i] = buf[0];
                    needRedraw = true;
                }
            }
        }
        // перерисовка кроссворда
        if (needRedraw) {
            needRedraw = false;
            rect.left = 0;
            rect.top = 0;
            rect.right = rect.left + table.tile * table.width;
            rect.bottom = rect.top + table.tile * table.height;
            InvalidateRect(hwnd, NULL, false);

            // если игрок победил
            if (table.CheckWin()) {
                if (not lockTimer) {
                    lockTimer = true;
                    // происходит запись рекорда
                    if (GetWindowText(table.hPlayerName, buf, 10)) wbuf = buf;
                    else wbuf = L"Noname";
                    table.loadRecordTable(
                        selectedCrossword, wbuf,
                        table.minutes * 60 + table.seconds);
                    InvalidateRect(hwnd, NULL, true);
                    // и выскакивает диалоговое окно
                    MessageBox(hwnd, L"Поздравляю, вы победили!", L"", MB_OK);
                }
            }
            else InvalidateRect(hwnd, NULL, false);
        }
    }
    break;

    // ивенты колесика мышки
    case WM_MOUSEWHEEL:
    {
        // нормализация прокрутки по table.tile
        INT scrollDelta = table.tile * (GET_WHEEL_DELTA_WPARAM(wparam) / abs(GET_WHEEL_DELTA_WPARAM(wparam)));
        // ограничений прокрутки
        if (scrollDelta > 0) {
            if (scrollDelta + table.scroll_h > 0) table.scroll_h = 0;
            else table.scroll_h += scrollDelta;
        }
        else if (not scrollLock) table.scroll_h += scrollDelta;
        // отрисовка области с описанием слов
        GetClientRect(hwnd, &rect);
        rect.left = table.shift_w;
        rect.top = table.inter_h + 1;
        InvalidateRect(hwnd, &rect, true);
    }
    break;

    // отрисовка номеров слов, вопросов, таймера и т.п.
    case WM_PAINT:
    {
        // область рисования
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // установка маленького шрифта и отрисовка номеров слов
        SelectObject(hdc, small_font);
        for (UINT i = 0; i < table.wordlist.size(); i++) {
            wbuf = std::to_wstring(i + 1);
            // вывод номера слова по соответствующим координатам
            if (table.wordlist[i].direction == HORIZONTAL) {
                // высота с ростом длинны номера не меняется
                // потому требуется выравнивание только по горизонтали
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

        // установка большого шрифта
        SelectObject(hdc, big_font);

        // отрисовка таймера
        x = table.inter_w + table.shift_w;
        y = table.shift_h;
        wbuf = std::to_wstring(table.minutes) + L":" + std::to_wstring(table.seconds);
        TextOut(hdc, x, y, wbuf.c_str(), wbuf.length());

        // отрисовка таблицы рекордов
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

        // отрисовка описание слов с учетом прокрутки
        x = table.shift_w;
        y = table.inter_h + table.tile + table.scroll_h;
        if (y > table.inter_h) {
            wbuf = L"По горизонтали:";
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
            wbuf = L"По вертикали:";
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

        // если ниодного описания небыло отрисованно, то блокируется прокрутка вверх
        if (y < table.inter_h) scrollLock = true;
        else scrollLock = false;
        EndPaint(hwnd, &ps);
    }
    break;

    // закрытие окна
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


// Стартовая функция
// В ней проходит регистрация главного окна
// А также в ней находися так называемый mainloop (главный цикл)
INT  WINAPI  WinMain(HINSTANCE  hInstance,  HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow) {
    // дискриптор, сообщение и шаблон главного окна
    HWND hwnd;
    MSG msg;
    WNDCLASS w;

    // создание и регистрация класса 
    memset(&w, 0, sizeof(WNDCLASS));
    w.style = CS_HREDRAW | CS_VREDRAW | CS_BYTEALIGNCLIENT;
    w.lpfnWndProc = WndProc;
    w.hInstance = hInstance;
    w.hbrBackground = CreateSolidBrush(0x00FFFFFF);
    w.lpszClassName = L"MainWindow";
    RegisterClass(&w);

    // создание и регистрация основного окна
    hwnd = CreateWindow(L"MainWindow", L"Crossword",
        WS_OVERLAPPEDWINDOW,
        500, 300, 800, 600,
        NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // создание таймера с интервалом 1 секунда
    SetTimer(hwnd, NULL, 1000, NULL);

    // главный цикл
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}