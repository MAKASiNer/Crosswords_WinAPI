#include <string>
#include <map>

// макроси для слова 
#define HORIZONTAL 0
#define VERTICAL 1

// структура слова {X, і, dir, word, descr} 
struct Word {
    uint16_t x, y;      // початок слова 
    bool direction;     // напрямок: 0 - горизонталь, 1 - вертикаль 
    std::wstring word;        // сама слово 
    std::wstring description; // опис слова 

    friend bool operator== (const Word& l, const Word& r) {
        return (l.x == r.x and l.y == r.y) and l.word == r.word;
    }
    friend bool operator!= (const Word& l, const Word& r) {
        return l != r;
    }
};