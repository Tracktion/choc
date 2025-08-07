//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_JSON_HEADER_INCLUDED
#define CHOC_JSON_HEADER_INCLUDED

#include <limits>
#include <sstream>
#include <string_view>
#include <stdexcept>

#include "choc_UTF8.h"
#include "choc_FloatToString.h"

// Include choc_Value.h and/or choc_JSONValue.h before this header
// to enable parsing and serialisation for those types.

#undef max   // It's never a smart idea to include any C headers before your C++ ones, as it
#undef min   // risks polluting your namespace with all kinds of dangerous macros like these ones.

namespace choc::json
{

//==============================================================================
/// A parse exception, thrown by choc::json::parse() as needed.
struct ParseError  : public std::runtime_error
{
    ParseError (const char* message, choc::text::LineAndColumn lc)
        : std::runtime_error (message), lineAndColumn (lc) {}

    choc::text::LineAndColumn lineAndColumn;
};

/// Template function to parse JSON into any supported Value type.
/// Requires a ParserTraits specialization for the target type.
template <typename ValueType>
[[nodiscard]] ValueType parseAs (text::UTF8Pointer);

/// Template function to parse JSON into any supported Value type.
template <typename ValueType>
[[nodiscard]] ValueType parseAs (std::string_view);

/// Template function to parse a bare JSON value into any supported Value type.
template <typename ValueType>
[[nodiscard]] ValueType parseValueAs (std::string_view);

/// Formats a value as a JSON string.
/// If useLineBreaks is true, it'll be formatted as multi-line JSON, if false it'll
/// just be returned as a single line. Works with any type that provides the
/// appropriate value-like interface (isVoid, isString, isBool, etc.)
template <typename ValueType>
[[nodiscard]] std::string toString (const ValueType&, bool useLineBreaks = false);

/// Writes a version of a string to an output stream, with any illegal or non-ascii
/// written as their equivalent JSON escape sequences.
template <typename OutputStreamType>
void writeWithEscapeCharacters (OutputStreamType&, text::UTF8Pointer sourceString);

/// Returns a version of a string with illegal or non-ascii converted into the
/// equivalent JSON escape sequences.
[[nodiscard]] std::string addEscapeCharacters (text::UTF8Pointer sourceString);

/// Returns a version of a string with illegal or non-ascii converted into the
/// equivalent JSON escape sequences.
[[nodiscard]] std::string addEscapeCharacters (std::string_view sourceString);

/// Returns a version of a string with illegal or non-ascii converted into the
/// equivalent JSON escape sequences.
[[nodiscard]] std::string getEscapedQuotedString (std::string_view sourceString);

/// Converts a double to a JSON-format string representation.
std::string doubleToString (double value);

/// Writes a value as JSON to an output stream.
template <typename Stream, typename ValueType>
void writeAsJSON (Stream& output, const ValueType& value, bool useMultipleLines);


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

template <typename OutputStreamType>
void writeWithEscapeCharacters (OutputStreamType& out, text::UTF8Pointer source)
{
    auto writeUnicode = [] (OutputStreamType& o, auto digit)
    {
        auto hexDigit = [] (auto value) -> char { return "0123456789abcdef"[value & 15]; };

        o << "\\u" << hexDigit (digit >> 12) << hexDigit (digit >> 8) << hexDigit (digit >> 4) << hexDigit (digit);
    };

    for (;;)
    {
        auto c = *source;

        switch (c)
        {
            case 0:  return;

            case '\"':   out << "\\\""; break;
            case '\\':   out << "\\\\"; break;
            case '\n':   out << "\\n";  break;
            case '\r':   out << "\\r";  break;
            case '\t':   out << "\\t";  break;
            case '\a':   out << "\\a";  break;
            case '\b':   out << "\\b";  break;
            case '\f':   out << "\\f";  break;

            default:
                if (c > 31 && c < 127)
                {
                    out << (char) c;
                    break;
                }

                if (c >= 0x10000)
                {
                    auto pair = choc::text::splitCodePointIntoSurrogatePair (c);
                    writeUnicode (out, pair.high);
                    writeUnicode (out, pair.low);
                    break;
                }

                writeUnicode (out, c);
                break;
        }

        ++source;
    }
}

inline std::string addEscapeCharacters (text::UTF8Pointer source)
{
    std::ostringstream result (std::ios::binary);
    writeWithEscapeCharacters (result, source);
    return result.str();
}

inline std::string addEscapeCharacters (std::string_view source)
{
    return addEscapeCharacters (text::UTF8Pointer (std::string (source).c_str()));
}

inline std::string getEscapedQuotedString (std::string_view s)
{
    std::ostringstream result (std::ios::binary);
    result << '"';
    writeWithEscapeCharacters (result, text::UTF8Pointer (std::string (s).c_str()));
    result << '"';
    return result.str();
}

inline std::string doubleToString (double value)
{
    if (std::isfinite (value))  return choc::text::floatToString (value, -1, true);
    if (std::isnan (value))     return "\"NaN\"";

    return value >= 0 ?  "\"Infinity\""
                      : "\"-Infinity\"";
}

//==============================================================================
template <typename Stream, typename ValueType>
struct Writer
{
    Stream& out;
    uint32_t indentSize, currentIndent = 0;
    static constexpr const char newLine = '\n';

    std::string getIndent() const         { return std::string (currentIndent, ' '); }
    void startIndent()                    { currentIndent += indentSize; out << newLine << getIndent(); }
    void endIndent()                      { currentIndent -= indentSize; out << newLine << getIndent(); }

    template <typename V>
    void dump (const V& v)
    {
        if (v.isVoid())                   { out << "null"; return; }
        if (v.isString())                 { out << getEscapedQuotedString (v.getString()); return; }
        if (v.isBool())                   { out << (v.getBool() ? "true" : "false"); return; }
        if (v.isFloat())                  { out << doubleToString (v.template get<double>()); return; }
        if (v.isInt())                    { out << (v.isInt32() ? static_cast<int64_t> (v.getInt32()) : v.getInt64()); return; }
        if (v.isObject())                 return dumpObject (v);
        if (v.isArray() || v.isVector())  return dumpArrayOrVector (v);
    }

    template <typename V>
    void dumpArrayOrVector (const V& v)
    {
        out << '[';
        auto numElements = v.size();

        if (indentSize != 0 && numElements != 0)
        {
            startIndent();

            for (uint32_t i = 0; i < numElements; ++i)
            {
                dump (v[i]);

                if (i != numElements - 1)
                    out << "," << newLine << getIndent();
            }

            endIndent();
        }
        else
        {
            for (uint32_t i = 0; i < numElements; ++i)
            {
                if (i != 0) out << ", ";
                dump (v[i]);
            }
        }

        out << ']';
    }

    template <typename V>
    void dumpObject (const V& object)
    {
        out << '{';
        auto numMembers = object.size();

        if (indentSize != 0 && numMembers != 0)
        {
            startIndent();

            for (uint32_t i = 0; i < numMembers; ++i)
            {
                auto member = object.getObjectMemberAt (i);
                out << getEscapedQuotedString (member.name) << ": ";
                dump (member.value);

                if (i != numMembers - 1)
                    out << "," << newLine << getIndent();
            }

            endIndent();
        }
        else
        {
            for (uint32_t i = 0; i < numMembers; ++i)
            {
                if (i != 0) out << ", ";

                auto member = object.getObjectMemberAt (i);
                out << getEscapedQuotedString (member.name) << ": ";
                dump (member.value);
            }
        }

        out << '}';
    }
};

template <typename Stream, typename ValueType>
void writeAsJSON (Stream& output, const ValueType& value, bool useMultipleLines)
{
    Writer<Stream, ValueType> { output, useMultipleLines ? 2u : 0u }.dump (value);
}

template <typename ValueType>
std::string toString (const ValueType& v, bool useLineBreaks)
{
    std::ostringstream out (std::ios::binary);
    writeAsJSON (out, v, useLineBreaks);
    return out.str();
}

//==============================================================================
[[noreturn]] static inline void throwParseError (const char* error, text::UTF8Pointer source, text::UTF8Pointer errorPos)
{
    throw ParseError (error, text::findLineAndColumn (source, errorPos));
}

//==============================================================================
// Generic parser implementation for different Value types
namespace ParserImpl
{

//==============================================================================
/// Generic parser traits for different Value types.
/// Specializations are provided conditionally when value headers are included.
template <typename ValueType>
struct ParserTraits;

//==============================================================================
/// Generic parser implementation
template <typename ValueType>
struct Parser
{
    using Traits = ParserTraits<ValueType>;
    text::UTF8Pointer source, current;

    bool isEOF() const            { return current.empty(); }
    uint32_t peek() const         { return *current; }
    uint32_t pop()                { return current.popFirstChar(); }
    bool popIf (char c)           { return current.skipIfStartsWith (c); }
    bool popIf (const char* c)    { return current.skipIfStartsWith (c); }

    static bool isWhitespace (uint32_t c)   { return c == ' ' || (c <= 13 && c >= 9); }
    void skipWhitespace()                   { auto p = current; while (isWhitespace (p.popFirstChar())) current = p; }

    [[noreturn]] void throwError (const char* error, text::UTF8Pointer errorPos)    { throwParseError (error, source, errorPos); }
    [[noreturn]] void throwError (const char* error)                                { throwError (error, current); }

    ValueType parseTopLevel()
    {
        skipWhitespace();

        if (popIf ('[')) return parseArray();
        if (popIf ('{')) return parseObject();
        if (! isEOF()) throwError ("Expected an object or array");
        return Traits::createNull();
    }

    ValueType parseArray()
    {
        auto result = Traits::createArrayBuilder();
        auto arrayStart = current;

        skipWhitespace();
        if (popIf (']')) return Traits::finaliseArray (result);

        for (;;)
        {
            skipWhitespace();
            if (isEOF())  throwError ("Unexpected EOF in array declaration", arrayStart);

            Traits::addToArray (result, parseValue());
            skipWhitespace();

            if (popIf (',')) continue;
            if (popIf (']')) break;
            throwError ("Expected ',' or ']'");
        }

        return Traits::finaliseArray (result);
    }

    ValueType parseObject()
    {
        auto result = Traits::createObjectBuilder();
        auto objectStart = current;

        skipWhitespace();
        if (popIf ('}')) return Traits::finaliseObject (result);

        for (;;)
        {
            skipWhitespace();
            if (isEOF())  throwError ("Unexpected EOF in object declaration", objectStart);

            if (! popIf ('"')) throwError ("Expected a name");
            auto errorPos = current;
            auto name = parseString();

            if (name.empty())
                throwError ("Property names cannot be empty", errorPos);

            skipWhitespace();
            errorPos = current;
            if (! popIf (':')) throwError ("Expected ':'");
            Traits::addToObject (result, std::move (name), parseValue());
            skipWhitespace();

            if (popIf (',')) continue;
            if (popIf ('}')) break;
            throwError ("Expected ',' or '}'");
        }

        return Traits::finaliseObject (result);
    }

    ValueType parseValue()
    {
        skipWhitespace();
        auto startPos = current;

        switch (pop())
        {
            case '[':    return parseArray();
            case '{':    return parseObject();
            case '"':    return Traits::createString (parseString());
            case '-':    skipWhitespace(); return parseNumber (true);
            case 'n':    if (popIf ("ull")) return Traits::createNull(); break;
            case 't':    if (popIf ("rue"))  return Traits::createBool (true); break;
            case 'f':    if (popIf ("alse")) return Traits::createBool (false); break;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                current = startPos;
                return parseNumber (false);

            default: break;
        }

        throwError ("Syntax error", startPos);
    }

    ValueType parseNumber (bool negate)
    {
        auto startPos = current;
        bool hadDot = false, hadExponent = false;

        for (;;)
        {
            auto lastPos = current;
            auto c = pop();

            if (c >= '0' && c <= '9')  continue;
            if (c == '.' && ! hadDot)  { hadDot = true; continue; }

            if (! hadExponent && (c == 'e' || c == 'E'))
            {
                hadDot = true;
                hadExponent = true;
                popIf ('-');
                continue;
            }

            if (isWhitespace (c) || c == ',' || c == '}' || c == ']' || c == 0)
            {
                current = lastPos;
                char* endOfParsedNumber = nullptr;

                if (! (hadDot || hadExponent))
                {
                    auto v = std::strtoll (startPos.data(), &endOfParsedNumber, 10);

                    if (endOfParsedNumber == lastPos.data()
                         && v != std::numeric_limits<long long>::max()
                         && v != std::numeric_limits<long long>::min())
                        return Traits::createInt64 (static_cast<int64_t> (negate ? -v : v));
                }

                auto v = std::strtod (startPos.data(), &endOfParsedNumber);

                if (endOfParsedNumber == lastPos.data())
                    return Traits::createFloat64 (negate ? -v : v);
            }

            throwError ("Syntax error in number", lastPos);
        }
    }

    std::string parseString()
    {
        std::ostringstream s (std::ios::binary);

        for (;;)
        {
            auto c = pop();

            if (c == '"')
                break;

            if (c == '\\')
            {
                auto errorPos = current;
                c = pop();

                switch (c)
                {
                    case 'a':  c = '\a'; break;
                    case 'b':  c = '\b'; break;
                    case 'f':  c = '\f'; break;
                    case 'n':  c = '\n'; break;
                    case 'r':  c = '\r'; break;
                    case 't':  c = '\t'; break;
                    case 'u':  c = parseUnicodeCharacterNumber (false); break;
                    case 0:    throwError ("Unexpected EOF in string constant", errorPos);
                    default:   break;
                }
            }

            char utf8Bytes[8];
            auto numBytes = text::convertUnicodeCodepointToUTF8 (utf8Bytes, c);

            for (uint32_t i = 0; i < numBytes; ++i)
                s << utf8Bytes[i];
        }

        return s.str();
    }

    uint32_t parseUnicodeCharacterNumber (bool isLowSurrogate)
    {
        uint32_t result = 0;

        for (int i = 4; --i >= 0;)
        {
            auto errorPos = current;
            auto digit = pop();

            if (digit >= '0' && digit <= '9')         digit -= '0';
            else if (digit >= 'a' && digit <= 'f')    digit = 10 + (digit - 'a');
            else if (digit >= 'A' && digit <= 'F')    digit = 10 + (digit - 'A');
            else throwError ("Syntax error in unicode character", errorPos);

            result = (result << 4) + digit;
        }

        if (isLowSurrogate && ! text::isUnicodeLowSurrogate (result))
            throwError ("Expected a unicode low surrogate codepoint");

        if (text::isUnicodeHighSurrogate (result))
        {
            if (! isLowSurrogate && popIf ("\\u"))
                return text::createUnicodeFromHighAndLowSurrogates ({ result, parseUnicodeCharacterNumber (true) });

            throwError ("Expected a unicode low surrogate codepoint");
        }

        return result;
    }
};

//==============================================================================
/// Parse functions that use the generic parser
template <typename ValueType>
ValueType parseGeneric (text::UTF8Pointer text, bool parseBareValue)
{
    Parser<ValueType> p { text, text };
    return parseBareValue ? p.parseValue() : p.parseTopLevel();
}

template <typename ValueType>
ValueType parseGeneric (const char* text, size_t numbytes, bool parseBareValue)
{
    if (text == nullptr)
    {
        text = "";
        numbytes = 0;
    }

    if (auto error = text::findInvalidUTF8Data (text, numbytes))
        throwParseError ("Illegal UTF8 data", text::UTF8Pointer (text), text::UTF8Pointer (error));

    return parseGeneric<ValueType> (text::UTF8Pointer (text), parseBareValue);
}

} // namespace ParserImpl

//==============================================================================
// Template implementations using the generic parser
template <typename ValueType>
inline ValueType parseAs (text::UTF8Pointer text)
{
    return ParserImpl::parseGeneric<ValueType> (text, false);
}

template <typename ValueType>
inline ValueType parseAs (std::string_view text)
{
    return ParserImpl::parseGeneric<ValueType> (text.data(), text.length(), false);
}

template <typename ValueType>
inline ValueType parseValueAs (std::string_view text)
{
    return ParserImpl::parseGeneric<ValueType> (text.data(), text.length(), true);
}

} // namespace choc::json

//==============================================================================
// Conditional support for choc::value::Value — available when choc_Value.h is
// included before this header.
//==============================================================================
#ifdef CHOC_VALUE_POOL_HEADER_INCLUDED

namespace choc::json
{

namespace ParserImpl
{
    template <>
    struct ParserTraits<choc::value::Value>
    {
        using ValueType = choc::value::Value;
        using ArrayBuilder = ValueType;
        using ObjectBuilder = ValueType;

        static ValueType createNull()                   { return {}; }
        static ValueType createBool (bool v)            { return choc::value::createBool (v); }
        static ValueType createInt64 (int64_t v)        { return choc::value::createInt64 (v); }
        static ValueType createFloat64 (double v)       { return choc::value::createFloat64 (v); }
        static ValueType createString (std::string s)   { return choc::value::createString (std::move (s)); }

        static ArrayBuilder createArrayBuilder()                                     { return choc::value::createEmptyArray(); }
        static void addToArray (ArrayBuilder& b, ValueType&& value)                  { b.addArrayElement (std::move (value)); }
        static ValueType finaliseArray (ArrayBuilder& b)                             { return std::move (b); }
        static ObjectBuilder createObjectBuilder()                                   { return choc::value::createObject ({}); }
        static void addToObject (ObjectBuilder& b, std::string&& key, ValueType&& v) { b.addMember (std::move (key), std::move (v)); }
        static ValueType finaliseObject (ObjectBuilder& b)                           { return std::move (b); }
    };
}

} // namespace choc::json

namespace choc::value
{

/// Parses a JSON string into a choc::value::Value object.
inline Value parse (text::UTF8Pointer text)        { return choc::json::parseAs<Value> (text); }

/// Parses a JSON string into a choc::value::Value object.
inline Value parse (std::string_view text)         { return choc::json::parseAs<Value> (text); }

/// Parses a bare JSON value (number, string, etc.) into a choc::value::Value object.
inline Value parseValue (std::string_view text)    { return choc::json::parseValueAs<Value> (text); }

} // namespace choc::value

#endif // CHOC_VALUE_POOL_HEADER_INCLUDED

//==============================================================================
// Conditional support for choc::json::Value — available when choc_JSONValue.h is
// included before this header.
//==============================================================================
#ifdef CHOC_JSON_VALUE_HEADER_INCLUDED

namespace choc::json
{

namespace ParserImpl
{
    template <>
    struct ParserTraits<choc::json::Value>
    {
        using ValueType = choc::json::Value;
        using ArrayBuilder = ValueType;
        using ObjectBuilder = ValueType;

        static ValueType createNull()                   { return choc::json::createNull(); }
        static ValueType createBool (bool v)            { return choc::json::createBool (v); }
        static ValueType createInt64 (int64_t v)        { return choc::json::createInt (v); }
        static ValueType createFloat64 (double v)       { return choc::json::createFloat (v); }
        static ValueType createString (std::string s)   { return choc::json::createString (std::move (s)); }

        static ArrayBuilder createArrayBuilder()                                        { return choc::json::createEmptyArray(); }
        static void addToArray (ArrayBuilder& b, ValueType&& value)                     { b.addArrayElement (std::move (value)); }
        static ValueType finaliseArray (ArrayBuilder& b)                                { return std::move (b); }
        static ObjectBuilder createObjectBuilder()                                      { return choc::json::createObject(); }
        static void addToObject (ObjectBuilder& b, std::string&& key, ValueType&& v)    { b.addMember (std::move (key), std::move (v)); }
        static ValueType finaliseObject (ObjectBuilder& b)                              { return std::move (b); }
    };
}

/// Parses a JSON string into a choc::json::Value object.
inline json::Value parse (text::UTF8Pointer text)        { return parseAs<json::Value> (text); }

/// Parses a JSON string into a choc::json::Value object.
inline json::Value parse (std::string_view text)         { return parseAs<json::Value> (text); }

/// Parses a bare JSON value (number, string, etc.) into a choc::json::Value object.
inline json::Value parseValue (std::string_view text)    { return parseValueAs<json::Value> (text); }

} // namespace choc::json

#endif // CHOC_JSON_VALUE_HEADER_INCLUDED

//==============================================================================
// Conversion between choc::value::Value and choc::json::Value — available when
// both value headers are included before this header.
//==============================================================================
#if defined(CHOC_VALUE_POOL_HEADER_INCLUDED) && defined(CHOC_JSON_VALUE_HEADER_INCLUDED)

namespace choc::json
{

/// Converts a choc::json::Value to a choc::value::Value
[[nodiscard]] inline choc::value::Value toValue (const json::Value& v)
{
    if (v.isVoid())     return {};
    if (v.isBool())     return choc::value::createBool (v.getBool());
    if (v.isInt32())    return choc::value::createInt32 (v.getInt32());
    if (v.isInt64())    return choc::value::createInt64 (v.getInt64());
    if (v.isFloat())    return choc::value::createFloat64 (v.getFloat64());
    if (v.isString())   return choc::value::createString (v.getString());

    if (v.isArray())
    {
        auto result = choc::value::createEmptyArray();

        for (uint32_t i = 0; i < v.size(); ++i)
            result.addArrayElement (toValue (v[i]));

        return result;
    }

    if (v.isObject())
    {
        auto result = choc::value::createObject ({});

        for (uint32_t i = 0; i < v.size(); ++i)
        {
            auto member = v.getObjectMemberAt (i);
            result.addMember (member.name, toValue (member.value));
        }

        return result;
    }

    return {};
}

} // namespace choc::json

#endif // CHOC_VALUE_POOL_HEADER_INCLUDED && CHOC_JSON_VALUE_HEADER_INCLUDED

#endif // CHOC_JSON_HEADER_INCLUDED
