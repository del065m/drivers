#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <cstdint>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <codecvt>

class CommandHelper {
public:
	CommandHelper(const std::string &s) : str(s), pos(0), hexOffset(true), offsetCharLen(8), bytesPerLine(16) {}

    bool hasNext() const {
        size_t p = pos;
        while (p < str.size() && std::isspace(static_cast<unsigned char>(str[p]))) ++p;
        return p < str.size();
    }

    bool isNextWord() const {
        return hasNext();
    }
    bool isNextWordFancy() const {
        return hasNext();
    }

    bool isNextInt() const { return isNextInteger<int64_t>(); }
    bool isNextInt8() const { return isNextInteger<int8_t>(); }
    bool isNextInt16() const { return isNextInteger<int16_t>(); }
    bool isNextInt32() const { return isNextInteger<int32_t>(); }
    bool isNextInt64() const { return isNextInteger<int64_t>(); }
    bool isNextUInt8() const { return isNextInteger<uint8_t>(); }
    bool isNextUInt16() const { return isNextInteger<uint16_t>(); }
    bool isNextUInt32() const { return isNextInteger<uint32_t>(); }
    bool isNextUInt64() const { return isNextInteger<uint64_t>(); }

    std::string getNextWord() {
        skipSpaces();
        if (pos >= str.size()) throw std::runtime_error("no next word");
        size_t start = pos;
        while (pos < str.size() && !std::isspace(static_cast<unsigned char>(str[pos]))) ++pos;
        return str.substr(start, pos - start);
    }

    std::string getNextWordFancy() {
        skipSpaces();
        if (pos >= str.size()) throw std::runtime_error("no next word fancy");
        if (str[pos] == '"') return parseQuoted();
        size_t start = pos;
        while (pos < str.size() && !std::isspace(static_cast<unsigned char>(str[pos]))) ++pos;
        return unescapeSimple(str.substr(start, pos - start));
    }

    int64_t getNextInt() { return getNextInteger<int64_t>(); }
    int8_t getNextInt8() { return getNextInteger<int8_t>(); }
    int16_t getNextInt16() { return getNextInteger<int16_t>(); }
    int32_t getNextInt32() { return getNextInteger<int32_t>(); }
    int64_t getNextInt64() { return getNextInteger<int64_t>(); }
    uint8_t getNextUInt8() { return getNextInteger<uint8_t>(); }
    uint16_t getNextUInt16() { return getNextInteger<uint16_t>(); }
    uint32_t getNextUInt32() { return getNextInteger<uint32_t>(); }
    uint64_t getNextUInt64() { return getNextInteger<uint64_t>(); }

    void setPrintHexOffsetCharLen(uint8_t len) { offsetCharLen = len; }
    uint8_t getPrintHexOffsetCharLen() const { return offsetCharLen; }
    void setPrintHexOffsetType(bool hex) { hexOffset = hex; }
    bool getPrintHexOffsetType() const { return hexOffset; }

    void printHex(const uint8_t* data, uint64_t length, uint64_t baseOffset) const {
        if (bytesPerLine == 0) return;
        if (!data || length == 0) {
            uint64_t displayStart = (baseOffset / bytesPerLine) * bytesPerLine;
            if (hexOffset) std::cout << std::setw(offsetCharLen) << std::right << toHexStr(displayStart) << "  ";
            else std::cout << std::setw(offsetCharLen) << std::right << displayStart << "  ";
            for (size_t i = 0; i < bytesPerLine; ++i) std::cout << "   ";
            std::cout << "  ";
            for (size_t i = 0; i < bytesPerLine; ++i) std::cout << ' ';
            std::cout << '\n';
            return;
        }
        uint64_t startRow = (baseOffset / bytesPerLine) * bytesPerLine;
        uint64_t endOffset = baseOffset + length - 1;
        uint64_t endRow = (endOffset / bytesPerLine) * bytesPerLine;
        for (uint64_t row = startRow; row <= endRow; row += bytesPerLine) {
            if (hexOffset) std::cout << std::setw(offsetCharLen) << std::right << toHexStr(row) << "  ";
            else std::cout << std::setw(offsetCharLen) << std::right << row << "  ";
            for (uint64_t col = 0; col < bytesPerLine; ++col) {
                uint64_t global = row + col;
                if (global < baseOffset || global >= baseOffset + length) {
                    std::cout << "   ";
                } else {
                    uint64_t idx = global - baseOffset;
                    uint8_t b = data[idx];
                    std::ostringstream oss;
                    oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(b);
                    std::cout << oss.str() << std::dec << std::setfill(' ') << ' ';
                }
            }
            std::cout << " ";
            for (uint64_t col = 0; col < bytesPerLine; ++col) {
                uint64_t global = row + col;
                if (global < baseOffset || global >= baseOffset + length) {
                    std::cout << ' ';
                } else {
                    uint8_t ch = data[global - baseOffset];
                    if (ch == '\t') std::cout << "\u2409";
                    else if (ch == '\n') std::cout << "\u240A";
                    else if (ch == '\r') std::cout << "\u240D";
                    else if (std::isprint(static_cast<unsigned char>(ch))) std::cout << static_cast<char>(ch);
                    else std::cout << '.';
                }
            }
            std::cout << '\n';
        }
    }

    static CommandHelper fromCin() {
        std::string line;
        std::getline(std::cin, line);
        return CommandHelper(line);
    }

    bool isNextWWord() const { return isNextWord(); }
    std::wstring getNextWWord() { return utf8ToWstring(getNextWord()); }

    bool isNextWWordFancy() const { return isNextWordFancy(); }
    std::wstring getNextWWordFancy() { return utf8ToWstring(getNextWordFancy()); }

private:
    std::string str;
    size_t pos;
    bool hexOffset;
    uint8_t offsetCharLen;
    const uint8_t bytesPerLine;

    void skipSpaces() { 
        while (pos < str.size() && std::isspace(static_cast<unsigned char>(str[pos]))) ++pos; 
    }

    static std::wstring utf8ToWstring(const std::string& s) {
        if (s.empty())
            return std::wstring();

        int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int) s.size(), nullptr, 0);
        if (size_needed == 0)
            return std::wstring(); // conversion failed, return empty string or handle error

        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int) s.size(), &wstr[0], size_needed);
        return wstr;
    }

    std::string parseQuoted() {
        if (pos >= str.size() || str[pos] != '"') throw std::runtime_error("parseQuoted error");
        ++pos;
        std::string out;
        while (pos < str.size()) {
            char c = str[pos++];
            if (c == '"') return out;
            if (c == '\\') {
                if (pos >= str.size()) break;
                char e = str[pos++];
                switch (e) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case 'x':
                {
                    int hi = hexDigit(peekChar()); if (hi < 0) { out.push_back('x'); break; }
                    ++pos;
                    int lo = hexDigit(peekChar()); if (lo < 0) { out.push_back(static_cast<char>(hi)); break; }
                    ++pos;
                    out.push_back(static_cast<char>((hi << 4) | lo));
                    break;
                }
                default: out.push_back(e); break;
                }
            } else out.push_back(c);
        }
        throw std::runtime_error("unterminated quote");
    }

    char peekChar() const { return pos < str.size() ? str[pos] : '\0'; }

    static int hexDigit(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static std::string unescapeSimple(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '\\' && i + 1 < s.size()) {
                char e = s[++i];
                switch (e) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                default: out.push_back(e); break;
                }
            } else out.push_back(c);
        }
        return out;
    }

    template<typename T>
    bool isNextInteger() const {
        size_t p = pos;
        while (p < str.size() && std::isspace(static_cast<unsigned char>(str[p]))) ++p;
        if (p >= str.size()) return false;

        size_t start = p;

        if constexpr (std::is_signed<T>::value) {
            if (str[p] == '+' || str[p] == '-') ++p;
        } else {
            if (str[p] == '+') ++p;
            if (str[p] == '-') return false;
        }

        bool any = false;
        while (p < str.size() && std::isdigit(static_cast<unsigned char>(str[p]))) {
            ++p;
            any = true;
        }
        if (!any) return false;

        T value{};
        auto res = std::from_chars(str.data() + start, str.data() + p, value);
        return res.ec == std::errc();
    }


    template<typename T>
    T getNextInteger() {
        size_t p = pos;
        while (p < str.size() && std::isspace(static_cast<unsigned char>(str[p]))) ++p;
        if (p >= str.size()) throw std::runtime_error("no next integer");

        size_t start = p;

        if constexpr (std::is_signed<T>::value) {
            if (str[p] == '+' || str[p] == '-') ++p;
        } else {
            if (str[p] == '+') ++p;
            if (str[p] == '-') throw std::runtime_error("negative for unsigned");
        }

        bool any = false;
        while (p < str.size() && std::isdigit(static_cast<unsigned char>(str[p]))) {
            ++p;
            any = true;
        }
        if (!any) throw std::runtime_error("invalid integer");

        T value{};
        auto res = std::from_chars(str.data() + start, str.data() + p, value);
        if (res.ec != std::errc()) throw std::runtime_error("integer out of range");

        pos = p;
        return value;
    }


    static std::string toHexStr(uint64_t v) {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << v;
        return oss.str();
    }

};