#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include <map>

// hMenu органов управелния
#define CROSSWORD_INPUTBOX 5
#define RESTART_BUT 6


#define HORIZONTAL 0
#define VERTICAL 1
// структура слова {x, y, dir, word, descr}
struct Word {
    UINT x, y;      // начало слова
    BOOL direction; // направление: 0 - горизонталь, 1 - вертикаль
    std::wstring word;        // сама строка
    std::wstring description; // описание слова

    Word() {
        word.reserve(1024);
        description.reserve(1024);
    }

    friend bool operator== (const Word& l, const Word& r) {
        return (l.x == r.x and l.y == r.y) and l.word == r.word;
    }
};


// класс игрового поля
class Table {
public:
    UINT width=0, height=0;         // размеры таблицы

    std::vector<HWND> hEdtTable;    // edit'ты кроссворда
    HWND hPlayerName;               // edit имени игрока, эт для тоблицы рекордов
    HWND hRestart;                  // кнопка перерегистрации 
    HWND hCrosswords;               // список кроссвордов

    std::array<std::wstring, 10> records;    // таблицы рекордов
    std::vector<TCHAR> rightTable, table;   // правильное поле и пользовательское поле
    DWORD minutes=0, seconds=0;             // таймер

    const TCHAR voidLit = L'_';     // пустой литеррал
    const TCHAR sepLit = L'|';      // разделитель

    UINT tile;                      // размерый клетки
    UINT shift_w, shift_h;          // смещение от левого веренго края
    UINT inter_w, inter_h;          // минимальная область кроссворда
    UINT crsNameLen;                // длинна названия кроссворда
    INT scroll_h=0;                 // позиция прокрутки колесика

    std::vector<Word> wordlist;     // список слов

    Table() {};

    Table(std::vector<Word> words) {

        // расчет высоты и ширины
        for (auto& word : words) {

            if (word.direction == HORIZONTAL or word.direction == VERTICAL) {
                UINT x = word.x + (word.direction == HORIZONTAL ? word.word.length() : 0) + 1;
                UINT y = word.y + (word.direction == VERTICAL ? word.word.length() : 0) + 1;
                if (x > width) width = x;
                if (y > height) height = y;
            }
        }
        hEdtTable.resize(height * width);

        // воссоздание таблицы
        rightTable.resize(height * width);
        std::fill(rightTable.begin(), rightTable.end(), voidLit);
        for (auto& word : words) {
            int x(0), y(0);
            for (UINT i = 0; i < word.word.length(); i++) {
                if (word.direction == HORIZONTAL) {
                    x = word.x + i;
                    y = word.y;
                }
                else if (word.direction == VERTICAL) {
                    x = word.x;
                    y = word.y + i;
                }
                rightTable[y * width + x] = word.word[i];
            }
        }

        table.resize(height * width);
        std::fill(table.begin(), table.end(), voidLit);

        tile = 20;

        shift_w = tile * 2;
        shift_h = tile * 2;

        inter_w = tile * (width) + shift_w;
        inter_h = tile * (height) + shift_h;

        crsNameLen = 0;

        scroll_h = 0;

        wordlist = words;
    }

    Table operator=(const Table& tb) {
        width = tb.width;
        height = tb.height;

        hEdtTable = tb.hEdtTable;

        records = tb.records;
        rightTable = tb.rightTable;
        table = tb.table;

        minutes = tb.minutes;
        seconds = tb.seconds;

        tile = tb.tile;

        shift_w = tb.shift_w;
        shift_h = tb.shift_h;

        inter_w = tb.inter_w;
        inter_h = tb.inter_h;

        crsNameLen = tb.crsNameLen;

        scroll_h = tb.scroll_h;

        wordlist = tb.wordlist;

        return *this;
    }

    // заполняет список кроссворда с диска
    void LoadCrosswordsList() {
        WIN32_FIND_DATAW fileInfo;
        HANDLE dir = FindFirstFile(L"crosswords\\*", &fileInfo);
        if (INVALID_HANDLE_VALUE == dir) return;

        do {
            std::wstring name = &fileInfo.cFileName[0];
            if (name[0] == L'.') continue;
            SendMessage(hCrosswords, LB_ADDSTRING, NULL, (LPARAM)name.c_str());
        } while (NULL != FindNextFile(dir, &fileInfo));

        FindClose(dir);
    }

    // переводит из кодировки ansi в эту поганую LWSTR
    INT ANSItoTCHAR(INT ansi_lit) {
        static std::map<INT, INT> alf = {
        {192, 1040}, { 193, 1041 }, { 194, 1042 }, { 195, 1043 },
        { 196, 1044 }, { 197, 1045 }, { 168, 1025 }, { 198, 1046 },
        { 199, 1047 }, { 200, 1048 }, { 201, 1049 }, { 202, 1050 },
        { 203, 1051 }, { 204, 1052 }, { 205, 1053 }, { 206, 1054 },
        { 207, 1055 }, { 208, 1056 }, { 209, 1057 }, { 210, 1058 },
        { 211, 1059 }, { 212, 1060 }, { 213, 1061 }, { 214, 1062 },
        { 215, 1063 }, { 216, 1064 }, { 217, 1065 }, { 218, 1066 },
        { 219, 1067 }, { 220, 1068 }, { 221, 1069 }, { 222, 1070 },
        { 223, 1071 }, { 224, 1072 }, { 225, 1073 }, { 226, 1074 },
        { 227, 1075 }, { 228, 1076 }, { 229, 1077 }, { 184, 1105 },
        { 230, 1078 }, { 231, 1079 }, { 232, 1080 }, { 233, 1081 },
        { 234, 1082 }, { 235, 1083 }, { 236, 1084 }, { 237, 1085 },
        { 238, 1086 }, { 239, 1087 }, { 240, 1088 }, { 241, 1089 },
        { 242, 1090 }, { 243, 1091 }, { 244, 1092 }, { 245, 1093 },
        { 246, 1094 }, { 247, 1095 }, { 248, 1096 }, { 249, 1097 },
        { 250, 1098 }, { 251, 1099 }, { 252, 1100 }, { 253, 1101 },
        { 254, 1102 }, { 255, 1103 }, {175, 1031}, {191, 1111}, {179, 1110}, };

        if (alf.find(ansi_lit) != alf.end())return alf[ansi_lit];
        else return ansi_lit;
    }

    INT TCHARtoANSI(INT tchar_lit) {
        static std::map<INT, INT> alf = {
        { 1040 , 192 }, { 1041 , 193 }, { 1042 , 194 }, { 1043 , 195 },
        { 1044 , 196 }, { 1045 , 197 }, { 1025 , 168 }, { 1046 , 198 },
        { 1047 , 199 }, { 1048 , 200 }, { 1049 , 201 }, { 1050 , 202 },
        { 1051 , 203 },{ 1052 , 204 }, { 1053 , 205 }, { 1054 , 206 },
        { 1055 , 207 }, { 1056 , 208 }, { 1057 , 209 }, { 1058 , 210 },
        { 1059 , 211 }, { 1060 , 212 }, { 1061 , 213 }, { 1062 , 214 },
        { 1063 , 215 }, { 1064 , 216 }, { 1065 , 217 }, { 1066 , 218 },
        { 1067 , 219 }, { 1068 , 220 }, { 1069 , 221 }, { 1070 , 222 },
        { 1071 , 223 }, { 1072 , 224 }, { 1073 , 225 }, { 1074 , 226 },
        { 1075, 227 }, { 1076 , 228 }, { 1077 , 229 }, { 1105 , 184 },
        { 1078 , 230 }, { 1079 , 231 }, { 1080 , 232 }, { 1081 , 233 },
        { 1082 , 234 }, { 1083 , 235 }, { 1084 , 236 }, { 1085 , 237 },
        { 1086 , 238 }, { 1087 , 239 }, { 1088 , 240 }, { 1089 , 241 },
        { 1090 , 242 }, { 1091 , 243 }, { 1092 , 244 }, { 1093 , 245 },
        { 1094, 246 }, { 1095 , 247 },  { 1096 , 248 }, { 1097 , 249 },
        { 1098 , 250 }, { 1099 , 251 }, { 1100 , 252 }, { 1101 , 253 },
        { 1102 , 254 }, { 1103 , 255 },  { 1031 , 175 }, { 1111 , 191 }, { 1110 , 179 } };
        
        if (alf.find(tchar_lit) != alf.end())return alf[tchar_lit];
        else return tchar_lit;
    }
    
    // загружает кроссворд из файлов
    std::vector<Word> LoadCrosswordFromFile(const std::wstring& path) {
        std::vector<Word> words(0);
        std::wstring buf;
        std::wstring subbuf;

        // чтение с переводом кодировки
        std::wifstream file(L"crosswords/" + path);
        while (not file.eof()) {
            TCHAR lit = file.get();
            if (lit != file.eof() and lit != L'\0' and lit != UINT16_MAX) buf += ANSItoTCHAR(lit);
            else break;
        }
        file.close();

        // счетчик элемента для парсинга строки
        enum cntr{
            x = 0, y,
            dir,
            wrd,
            descr
        } counter = x;

        Word word;
        for (INT i = 0; i < buf.size(); i++) {
            if (buf[i] == L'\t' or buf[i] == L'\n' or buf[i] == L'\r' or i + 1 == buf.size()) {
                switch (counter)
                {
                case x:
                    word.x = _wtoi(subbuf.c_str());
                        break;
                case y:
                    word.y = _wtoi(subbuf.c_str());
                    break;
                case dir:
                    word.direction = bool(_wtoi(subbuf.c_str()));
                    break;
                case wrd:
                    word.word = subbuf;
                    break;
                case descr:
                    word.description = subbuf;
                    break;
                default:
                    break;
                }

                counter = cntr(((cntr)counter + 1) % 5);

                if (counter == x)
                    if (word.direction == HORIZONTAL or word.direction == VERTICAL) {
                        words.push_back(word);
                    }
                subbuf.clear();
            }
            else subbuf += buf[i];
        }
        
        loadRecordTable(path);

        return words;
    }

    // загружает таблицу рекордов из файла, если передать параметр, то он будет записан в таблицу рекордов
    void loadRecordTable(const std::wstring& path, std::wstring player = L"", UINT score = 0) {
        std::vector<std::wstring> recordslist;
        std::wstring buf;
        std::wstring subbuf;

        if (not std::wifstream(L"records/" + path).is_open())
            std::wofstream(L"records/" + path);

        if (player.size() > 0) {
            std::wofstream(L"records/" + path, std::ios::app)<< std::to_wstring(score) + L"\t" + player << std::endl;
        }
        
        // чтение из файла
        std::wfstream file(L"records/" + path, std::ios::in);
        while (not file.eof()) {
            recordslist.push_back(L"");
            std::getline(file, recordslist.back());
        };

        // сортировкa таблицы
        std::sort(recordslist.begin(), recordslist.end(),
            [](const std::wstring& l, const std::wstring& r) {
                UINT scrl = _wtoi(l.c_str());
                UINT scrr = _wtoi(r.c_str());
                return scrl < scrr;
            });

        // перезапись файла и таблицы рекордов
        file.open(L"records/" + path, std::ios::out); 
        for(INT i = 1; i < recordslist.size() and i < records.size() + 1; i++){
            if (not recordslist[i].empty()) {
                file << recordslist[i];
                UINT point = recordslist[i].find(L"\t");
                buf = recordslist[i].substr(0, point);
                subbuf = recordslist[i].substr(point) + L" - " 
                    + std::to_wstring(_wtoi(buf.c_str()) / 60) + L":" 
                    + std::to_wstring(_wtoi(buf.c_str()) % 60);
                records[i - 1].clear();
                for (auto& lit : subbuf) records[i - 1] += ANSItoTCHAR(lit);
            }
        }
        
        file.close();
    }

    // возращает слова, которым пренадлежит буква с данными координатами
    std::vector<Word> FindWords(UINT x, UINT y) {
        std::vector<Word> buf;
        buf.reserve(2);

        for (const auto& word : wordlist) {
            // для горизонтали и вертикали разность меж позицией и началом должна быть меньше длины
            if (word.direction == HORIZONTAL and y == word.y) {
                if (x - word.x <= word.word.length()) buf.push_back(word);
            }
            else if ((word.direction == VERTICAL and x == word.x)) {
                if (y - word.y <= word.word.length()) buf.push_back(word);
            }
            // не более 2 слов может иметь общую букву
            if (buf.size() == 2) break;
        }
        return buf;
    }

    // 0 - если слово не заполненно, 1 - если верно 2, - если не верно
    INT CheckWord(const Word& word) {
        int x(0), y(0);
        std::wstring buf;

        for (UINT l = 0; l < word.word.length(); l++) {
            if (word.direction == HORIZONTAL) {
                x = word.x + l;
                y = word.y;
            }
            else if (word.direction == VERTICAL) {
                x = word.x;
                y = word.y + l;
            }
            int i = y * width + x;
            if (table[i] == voidLit) return 0;
            else (buf += table[i]);
        }

        if (buf == word.word) return 1;
        else return 2;
    }

    // проверяет победил ли игрок
    bool CheckWin() {
        for (auto& word : wordlist) {
            int x(0), y(0);
            for (UINT i = 0; i < word.word.length(); i++) {
                if (word.direction == HORIZONTAL) {
                    x = word.x + i;
                    y = word.y;
                }
                else if (word.direction == VERTICAL) {
                    x = word.x;
                    y = word.y + i;
                }
                if (table[y * width + x] != word.word[i]) return false;
            }
        }
        return true;
    }
} table(std::vector<Word>(0));



// функция обработки сообщений окна
LONG WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);

// функция регистрации окон
LONG WINAPI WndReg(HWND, HINSTANCE);

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

// эта функция делит строку на подстроки по символам разделителям
std::vector<std::wstring> sepWstring(const std::wstring& wstr, TCHAR lit) {
    std::vector<std::wstring> res;
    res.reserve(wstr.length());
    std::wstring buf;
    buf.reserve(wstr.length());

    for (auto& wchr: wstr) {
        if (wchr == lit) {
            res.push_back(buf);
            buf.clear();
        }
        else buf += wchr;
    }
    if (not buf.empty()) res.push_back(buf);
    return res;
}


// Стартовая функция
INT  WINAPI  WinMain(HINSTANCE  hInstance,
    HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{

    HWND hwnd;
    MSG msg;
    WNDCLASS w;

    //создание и регистация класса
    memset(&w, 0, sizeof(WNDCLASS));
    w.style = CS_HREDRAW | CS_VREDRAW;
    w.lpfnWndProc = WndProc;
    w.hInstance = hInstance;
    w.hbrBackground = CreateSolidBrush(0x00FFFFFF);
    w.lpszClassName = L"MyClass";
    RegisterClass(&w);

    // создание и регистрация основного окна
    hwnd = CreateWindow(L"MyClass", L"Кроссворд",
        WS_OVERLAPPEDWINDOW,
        500, 300, 800, 600,
        NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // создание таймера, тик раз в 1000мс
    SetTimer(hwnd, 1, 1000, NULL);

    // цикл окна
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}


// Регистрация окон
LONG WINAPI WndReg(HWND hwnd, HINSTANCE hInst) {
    static std::wstring wbuf;
    static RECT rect;
    UINT x(0), y(0);
    static HFONT
        big_font = myFont(table.tile),
        norm_font = myFont(0.75 * table.tile),
        small_font = myFont(table.tile / 2);

    

    // регистрация окон edit для кроссворда
    for (UINT y = 0; y < table.height; y++) {
        for (UINT x = 0; x < table.width; x++) {
            int i = y * table.width + x;
            auto& hEdt = table.hEdtTable[i];
            wchar_t lit = table.rightTable[i];

            table.table[i] = table.rightTable[i];

            // создается окно edit
            if (lit == table.voidLit) continue;
            hEdt = CreateWindow( L"edit", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_UPPERCASE,
                x * table.tile + table.shift_w, y * table.tile + table.shift_h, table.tile + 1, table.tile + 1,
                hwnd, (HMENU)CROSSWORD_INPUTBOX, hInst, NULL
            );
            // шрифт, размеры шрифта должны быть на 20% меньше edit
            // иначе такие буквы как 'y' будут вылезать за границы 
            SendMessage(hEdt, WM_SETFONT, (WPARAM)norm_font, NULL);
            // лимит буффера edit
            SendMessage(hEdt, EM_LIMITTEXT, (WPARAM)1, NULL);
        }
    }

    x = table.inter_w + table.shift_w;

    y = table.shift_h * 2 + 8 * table.tile;
    if (table.inter_h < y) table.inter_h = y;
    

    // регистрация окна имени игрока
    table.hPlayerName = CreateWindow( L"edit", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
        x, table.shift_h + 2 * table.tile, (table.tile + 1) * 5, table.tile + 1,
        hwnd, NULL, hInst, NULL
    );
    SendMessage(table.hPlayerName, WM_SETFONT, (WPARAM)norm_font, NULL);
    
    // регистрация окна выбора кроссворда
    table.hCrosswords = CreateWindow(L"listbox", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_STANDARD ,
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


// Функция обработки сообщений
LONG WINAPI WndProc(HWND hwnd, UINT Message,
    WPARAM wparam, LPARAM lparam) {

    static HDC hdc;           // "холст"
    static HBRUSH hbrBkgnd;   // кисть
    UINT x(0), y(0);          // вспомогательные ккординаты
    bool needRedraw(false);
    static PAINTSTRUCT ps;
    static HINSTANCE hInst;
    static RECT rect;

    static TCHAR buf[32];            // буффер
    static std::wstring wbuf;        // строковый буффер

    static std::wstring selectedCrossword;    // активный кроссворд

    // задает 3 шрифта, один для номеров, другой для кроссворда, третий для остального текста
    static HFONT
        big_font = myFont(table.tile),
        norm_font = myFont(0.75 * table.tile),
        small_font = myFont(table.tile / 2);

    // таймер
    static SYSTEMTIME timer;
    static bool lockTimer = true;

    // скроллинг
    static bool scrollLim(false);
    static bool scrollLock(false);
    static INT scrollDelta(0);

    switch (Message) {
    // инициализация окон
    case WM_CREATE:
    {
        hInst = ((LPCREATESTRUCT)lparam)->hInstance;
        WndReg(hwnd, hInst);
        SetWindowText(table.hPlayerName, L"Noname");
        return NULL;
    }
    break;

    // настройка цвета текста внутри edits
    case WM_CTLCOLOREDIT:
    {
        // костыль, чтоб не инициализировать iter вне swith
        if (true) {
            // сначала находим сендера, и если он является полем кроссворда
            auto iter = std::find(table.hEdtTable.begin(), table.hEdtTable.end(), (HWND)lparam);
            if (iter != table.hEdtTable.end()) {
                int i = iter - table.hEdtTable.begin();
                HWND hEdt = table.hEdtTable[i];

                // находим, каким словам он пренадлежит
                auto words = table.FindWords(i % table.width, i / table.width);

                // раскрашиваем текст
                for (auto word : words) {
                    int check = table.CheckWord(word);
                    if (check == 2) {
                        SetTextColor((HDC)wparam, RGB(200, 0, 0));
                        break;
                    }
                    else SetTextColor((HDC)wparam, RGB(0, 0, 0));
                }
                SetBkColor((HDC)wparam, RGB(230, 230, 230));
            }
        }
        return (INT_PTR)GetStockObject(NULL_BRUSH);
    }
    break;

    // тик таймера
    case WM_TIMER:
    {
        // счетчик времени(timer)
        if (lockTimer) return NULL;
        GetSystemTime(&timer);
        if (table.seconds >= 59) {
            table.seconds = 0;
            table.minutes += 1;
        }
        else table.seconds++;

        // перерисовка таймера
        rect.left = table.shift_w * 2 + table.width * table.tile;
        rect.top = table.shift_h;
        rect.right = rect.left + 5 * table.tile;
        rect.bottom = rect.top + table.shift_h;
        InvalidateRect(hwnd, &rect, true);
    }
    break;

    // ивенты 
    case WM_COMMAND:
    {
        // ивент от кнопки рестарта
        if (wparam == RESTART_BUT) {
            // перенос данных
            if (GetWindowText(table.hPlayerName, buf, 10)) wbuf = buf;
            else wbuf = L"Noname";          

            // чтоб не переписывать диструктор(по умолчанию) Table
            if (selectedCrossword.size()) {
                for (auto& hEdt : table.hEdtTable) DestroyWindow(hEdt);
                DestroyWindow(table.hPlayerName);
                DestroyWindow(table.hRestart);
                DestroyWindow(table.hCrosswords);

                // перерегистрация
                lockTimer = false;
                table = Table(table.LoadCrosswordFromFile(selectedCrossword));
                table.loadRecordTable(selectedCrossword);
                WndReg(hwnd, hInst);
                SetWindowText(table.hPlayerName, wbuf.c_str());
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
        needRedraw = false;
        for (UINT y = 0; y < table.height; y++) {
            for (UINT x = 0; x < table.width; x++) {
                int i = y * table.width + x;
                auto& hEdt = table.hEdtTable[i];
                GetWindowText(hEdt, buf, 2);
                // при этом содержание edit изменилось
                if (buf[0] != table.table[i]) {
                    if (buf[0] == 0) table.table[i] = table.voidLit;
                    else table.table[i] = buf[0];
                    needRedraw = true;
                }
            }
        }
        // перерисовка кроссворда
        if (needRedraw) {
            rect.left = 0;
            rect.top = 0;
            rect.right = rect.left + table.tile * table.width;
            rect.bottom = rect.top + table.tile * table.height;
            InvalidateRect(hwnd, NULL, false);

            if (table.CheckWin()) {
                if (not lockTimer) {
                    lockTimer = true;

                    if (GetWindowText(table.hPlayerName, buf, 10)) wbuf = buf;
                    else wbuf = L"Noname";
                    for (auto& lit : wbuf) lit = table.TCHARtoANSI(lit);
                    table.loadRecordTable(
                        selectedCrossword, wbuf, 
                        table.minutes * 60 + table.seconds);
                    InvalidateRect(hwnd, NULL, true);
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
        // нормалицация прокрутки
        scrollDelta = table.tile * (GET_WHEEL_DELTA_WPARAM(wparam) / abs(GET_WHEEL_DELTA_WPARAM(wparam)));

        if (scrollDelta > 0) {
            if (scrollDelta + table.scroll_h > 0) table.scroll_h = 0;
            else table.scroll_h += scrollDelta;
        }
        else {
            if (not scrollLock) table.scroll_h += scrollDelta;
        }

        GetClientRect(hwnd, &rect);
        rect.left = table.shift_w;
        rect.top = table.inter_h + 1;

        InvalidateRect(hwnd, &rect, true);
    }
    break;

    // отрисовка номеров слов, вопросов, таймера и т.п.
    case WM_PAINT:
    {
        hdc = BeginPaint(hwnd, &ps);
        // установка маленького шрифта
        SelectObject(hdc, small_font);

        // отрисовка номеров слов
        for (UINT i = 0; i < table.wordlist.size(); i++) {

            wbuf = std::to_wstring(i + 1);
            // вывод номера слова по соответствующим координатам
            if (table.wordlist[i].direction == HORIZONTAL) {
                // высота с ростом длинны строки не меняется
                // потому требуется выравнивание по горизонтали
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

        // таблицы рекордов
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

        // описание слов 
        scrollLim = true;
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
        return DefWindowProc(hwnd, Message, wparam, lparam);
    }
    return NULL;
}