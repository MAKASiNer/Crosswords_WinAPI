#include <string>
#include <map>

// ������� ��� �����
#define HORIZONTAL 0
#define VERTICAL 1

// ��������� ����� {x, y, dir, word, descr}
struct Word {
    uint16_t x, y;      // ������ �����
    bool direction;     // �����������: 0 - �����������, 1 - ���������
    std::wstring word;        // ���� �����
    std::wstring description; // �������� �����

    friend bool operator== (const Word& l, const Word& r) {
        return (l.x == r.x and l.y == r.y) and l.word == r.word;
    }
    friend bool operator!= (const Word& l, const Word& r) {
        return l != r;
    }
};