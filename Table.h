#include <algorithm>
#include <windows.h>
#include <string>
#include <vector>
#include <array>

#include "Word.h"

// Структура интерфейса.
// Содержит в себе практически все не статичные элементы на игровом поле.
struct Table {
    UINT width = 0, height = 0;         // размеры таблицы

    std::vector<HWND> hEdtTable;        // окна кроссворда
    HWND hPlayerName, hCrosswords, hRestart;    // имя игрока, список кроссвордов и кнопка рестарта

    std::array<std::wstring, 10> records;           // таблицы рекордов
    std::vector<TCHAR> rightLetters, userLetters;   // поле с правильными буквами и пользовательское поле
    DWORD minutes = 0, seconds = 0;                 // время

    const TCHAR voidLit = L'_';     // пустой литерал
    const TCHAR sepLit = L'|';      // разделитель строки описания

    UINT tile;                      // размеры сетки главного окна
    UINT shift_w, shift_h;          // размер межэлементного сдвига
    UINT inter_w, inter_h;          // минимальная область кроссворда
    INT scroll_h = 0;               // позиция прокрутки колесика

    std::vector<Word> wordlist;     // список слов

    ~Table() {
        // удаление окон
        for (auto& hEdt : hEdtTable) DestroyWindow(hEdt);
        DestroyWindow(hPlayerName);
        DestroyWindow(hRestart);
        DestroyWindow(hCrosswords);
    }
    // \brief Главный конструктор
    // \param words вектор слов
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
        rightLetters.resize(height * width);
        std::fill(rightLetters.begin(), rightLetters.end(), voidLit);
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
                rightLetters[y * width + x] = word.word[i];
            }
        }

        // настройка и инициализации остальных полей
        userLetters.resize(height * width);
        std::fill(userLetters.begin(), userLetters.end(), voidLit);

        tile = 20;

        shift_w = tile * 2;
        shift_h = tile * 2;

        inter_w = tile * (width)+shift_w;
        inter_h = tile * (height)+shift_h;

        scroll_h = 0;

        wordlist = words;
    }
    Table operator=(const Table& tb) {
        this->~Table();
        width = tb.width;
        height = tb.height;
        hEdtTable = tb.hEdtTable;
        records = tb.records;
        rightLetters = tb.rightLetters;
        userLetters = tb.userLetters;
        minutes = tb.minutes;
        seconds = tb.seconds;
        tile = tb.tile;
        shift_w = tb.shift_w;
        shift_h = tb.shift_h;
        inter_w = tb.inter_w;
        inter_h = tb.inter_h;
        scroll_h = tb.scroll_h;
        wordlist = tb.wordlist;
        return *this;
    }

    // \brief Заполняет hCrosswords файлами из папки crosswords
    void LoadCrosswordsList() {
        WIN32_FIND_DATAW fileInfo;
        // открытие папки
        HANDLE dir = FindFirstFile(L"crosswords\\*", &fileInfo);
        if (INVALID_HANDLE_VALUE == dir) return;
        // чтение имен файлов
        do {
            std::wstring name = &fileInfo.cFileName[0];
            if (name[0] == L'.') continue;
            SendMessage(hCrosswords, LB_ADDSTRING, NULL, (LPARAM)name.c_str());
        } while (NULL != FindNextFile(dir, &fileInfo));
        FindClose(dir);
    }

    // \brief Загружает кроссворд из файла
    // \param path Имя файла в папке crosswords
    // \return вектор слов
    std::vector<Word> LoadCrosswordFromFile(const std::wstring& path) {
        std::vector<Word> words;
        std::wstring buf, subbuf;
        // чтение с переводом кодировки
        std::wifstream file(L"crosswords/" + path);
        while (not file.eof()) {
            TCHAR lit = file.get();
            if (lit != file.eof() and lit != L'\0' and 
                lit != UINT16_MAX) buf += ANSItoTCHAR(lit);
            else break;
        }
        file.close();
        // счетчик элемента для парсинга строки
        enum cntr {
            x = 0, y, dir,  wrd, descr
        } counter = x;
        Word word;
        for (INT i = 0; i < buf.size(); i++) {
            // если достигнут определенный символ, инкремирует счетчик
            if (buf[i] == L'\t' or buf[i] == L'\n' or 
                buf[i] == L'\r' or i + 1 == buf.size()) {
                switch (counter) {
                case x: word.x = _wtoi(subbuf.c_str());
                    break;
                case y: word.y = _wtoi(subbuf.c_str());
                    break;
                case dir: word.direction = bool(_wtoi(subbuf.c_str()));
                    break;
                case wrd: word.word = subbuf;
                    break;
                case descr: word.description = subbuf;
                    break;
                }
                counter = cntr(((cntr)counter + 1) % 5);
                // если счетчик обновился, прочтенное слово записывается в результат
                if (counter == x and (
                    word.direction == HORIZONTAL or 
                    word.direction == VERTICAL))
                    words.push_back(word);
                subbuf.clear();
            }
            else subbuf += buf[i];
        }
        loadRecordTable(path);
        return words;
    }

    // \brief Загружает таблицу рекордов из файла
    // \param path имя файла
    // \param playerName имя игрока, если не пустое, то будет добавлен рекорд
    // \param playerSeconds количество секунд затраченное на решение
    void loadRecordTable(const std::wstring& path, std::wstring playerName = L"", UINT playerSeconds = 0) {
        std::vector<std::wstring> recordslist;
        std::wstring buf;
        std::wstring subbuf;
        // создание нового файла по необходимости
        if (not std::wifstream(L"records/" + path).is_open())
            std::wofstream(L"records/" + path);
        // если результат передан, то записывается в файл
        if (playerName.size() > 0) {
            for (auto& lit : playerName) lit = TCHARtoANSI(lit);
            std::wofstream(L"records/" + path, std::ios::app)
                << std::to_wstring(playerSeconds) + L"\t" + playerName << std::endl;
        }

        // чтение из файла
        std::wfstream file(L"records/" + path, std::ios::in);
        while (not file.eof()) {
            recordslist.push_back(L"");
            std::getline(file, recordslist.back());
        };
        // сортировкa прочитанного
        std::sort(recordslist.begin(), recordslist.end(),
            [](const std::wstring& l, const std::wstring& r) {
                return _wtoi(l.c_str()) < _wtoi(r.c_str());});
        // перезапись файла и таблицы рекордов
        file.open(L"records/" + path, std::ios::out);
        for (INT i = 1; i < recordslist.size() and i < records.size() + 1; i++) {
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

    // \brief Ищет слова, которым принадлежат ячейки по координатам
    // \param x координата по x0
    // \param y координата по y
    // \return вектор из максимума двух слов
    std::vector<Word> FindWords(UINT x, UINT y) {
        std::vector<Word> buf;
        buf.reserve(2);
        for (const auto& word : wordlist) {
            // для горизонтали и вертикали разность меж позицией символа и началом строки должна быть меньше длины иначе символ не принадлежит слову
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

    // \brief Проверяет слово
    // \param word слово
    // \return 0 - если слово не собрано, 1 - если верно собрано, 2 - если не верно собрано
    INT CheckWord(const Word& word) {
        int x(0), y(0);
        std::wstring buf;

        // от начала координат слова
        for (UINT l = 0; l < word.word.length(); l++) {
            if (word.direction == HORIZONTAL) {
                x = word.x + l;
                y = word.y;
            }
            else if (word.direction == VERTICAL) {
                x = word.x;
                y = word.y + l;
            }

            // и по направлению слова пускается цикл
            int i = y * width + x;
            if (userLetters[i] == voidLit) return 0;
            else (buf += userLetters[i]);
        }
        // если все соответсвенные элементы пользовательской таблицы равны правильной
        if (buf == word.word) return 1;
        else return 2;
    }

    // \brief Проверяет, победил ли игрок
    // \return true - если победил
    bool CheckWin() {
        // сравнивает только слова по алгоритму из конструктора
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
                // если найдено несовпадение, то игрок еще не победил
                if (userLetters[y * width + x] != word.word[i]) return false;
            }
        }
        return true;
    }

    // \brief Переводит кириллический символ из кодировки ansi в юникод
    // \param ansi_lit символ в кодировке ansi
    // \return символ в кодировке юникод
    wchar_t ANSItoTCHAR(wchar_t ansi_lit) {
        // постоено на кириллическом словаре
        static std::map<wchar_t, wchar_t> alf = {
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
        { 254, 1102 }, { 255, 1103 }, {175, 1031}, {191, 1111},
        {179, 1110}, { 170, 1028}, { 186, 1108} };

        if (alf.find(ansi_lit) != alf.end())return alf[ansi_lit];
        else return ansi_lit;
    }

    // \brief Перевод кириллический символ из юникода в ansi
    // \param ansi_lit символ в кодировке юникода
    // \return символ в кодировке ansi
    wchar_t TCHARtoANSI(wchar_t tchar_lit) {
        // постоено на кириллическом словаре
        static std::map<wchar_t, wchar_t> alf = {
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
        { 1102 , 254 }, { 1103 , 255 },  { 1031 , 175 }, { 1111 , 191 },
        { 1110 , 179 }, { 1028, 170 }, {1108, 186 } };

        if (alf.find(tchar_lit) != alf.end())return alf[tchar_lit];
        else return tchar_lit;
    }
};