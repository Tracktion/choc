//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
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

#ifndef CHOC_JSON_VALUE_HEADER_INCLUDED
#define CHOC_JSON_VALUE_HEADER_INCLUDED

#include <string>
#include <string_view>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>

// Forward declaration to avoid circular dependency
namespace choc::value { class ValueView; }

namespace choc::json
{

//==============================================================================
/**
    A high-performance, dynamically-typed value class optimized for JSON-like data.

    Unlike choc::value::Value which is designed for specific constraints (non-allocating,
    single data chunk, separate type representation), choc::json::Value prioritizes speed
    and ease of use by leveraging dynamic memory allocation and a streamlined internal
    structure.

    Key features:
    - Fixed 24-byte size with Small String Optimization (strings ≤22 chars stored inline)
    - Manually-managed contiguous storage for arrays and objects (no std::vector overhead)
    - API compatibility with choc::value::Value for easy migration
    - Efficient linear search for object members (optimal for small objects)
    - Full error compatibility with choc::value::Value exceptions

    This class can be converted to/from choc::value::Value and serves as a drop-in
    replacement in performance-critical scenarios where the choc::value::Value constraints
    are not required.

    @see choc::value::Value, choc::json::parse()
*/
class Value final
{
public:
    //==============================================================================
    /// Creates an undefined value.
    Value() = default;

    /// Copy constructor - performs deep copy of any heap-allocated data.
    Value (const Value&);

    /// Move constructor - steals pointers and resets source to undefined.
    Value (Value&&) noexcept;

    /// Copy assignment - performs deep copy of any heap-allocated data.
    Value& operator= (const Value&);

    /// Move assignment - steals pointers and resets source to undefined.
    Value& operator= (Value&&) noexcept;

    /// Destructor - frees any heap-allocated data.
    ~Value();

    //==============================================================================
    /// Constructors for primitive types
    explicit Value (bool value);
    explicit Value (int32_t value);
    explicit Value (int64_t value);
    explicit Value (float value);
    explicit Value (double value);
    explicit Value (const char* value);
    explicit Value (std::string_view value);
    explicit Value (const std::string& value);

    /// Converting constructor from choc::value::Value
    Value (const choc::value::ValueView&);

    /// Assignment from choc::value::ValueView
    Value& operator= (const choc::value::ValueView&);

    //==============================================================================
    /// Comparison operators
    bool operator== (const Value& other) const;
    bool operator!= (const Value& other) const    { return ! (*this == other); }

    //==============================================================================
    /// Type checking methods
    bool isUndefined() const    { return type == Type::undefined; }
    bool isNull() const         { return type == Type::null; }
    bool isVoid() const         { return isUndefined() || isNull(); }
    bool isBool() const         { return type == Type::bool_; }
    bool isInt32() const        { return type == Type::int32; }
    bool isInt64() const        { return type == Type::int64; }
    bool isInt() const          { return isInt32() || isInt64(); }
    bool isFloat() const        { return type == Type::double_; }
    bool isString() const       { return type == Type::shortString || type == Type::longString; }
    bool isArray() const        { return type == Type::array; }
    bool isObject() const       { return type == Type::object; }

    //==============================================================================
    /// Getter methods - throw std::runtime_error if type doesn't match
    bool getBool() const;
    int32_t getInt32() const;
    int64_t getInt64() const;
    int64_t getInt() const      { return isInt32() ? getInt32() : getInt64(); }
    float getFloat32() const;
    double getFloat64() const;
    double getFloat() const     { return getFloat64(); }
    std::string_view getString() const;

    /// Generic get method for compatibility with choc::value::Value
    template <typename Type>
    Type get() const;

    /// Attempts to get value as target type, returns default if not possible
    template <typename Type>
    Type getWithDefault (Type defaultValue) const;

    /// Converts value to string representation, empty string if not convertible
    std::string toString() const;

    //==============================================================================
    /// Size methods
    uint32_t size() const;
    bool empty() const;

    /// Clear methods for containers
    void clear();

    /// Reserve capacity for arrays and objects to avoid reallocations
    void reserveArray (uint32_t capacity);
    void reserveObject (uint32_t capacity);

    //==============================================================================
    /// Array access methods
    Value& operator[] (uint32_t index);
    const Value& operator[] (uint32_t index) const;
    Value& operator[] (int index)                   { return operator[] (static_cast<uint32_t> (index)); }
    const Value& operator[] (int index) const       { return operator[] (static_cast<uint32_t> (index)); }

    /// Adds an element to an array (converts to array if not already one)
    void addArrayElement (Value value);

    /// Move-optimized array element addition methods
    template <typename T>
    void push_back (T&& value)          { addArrayElement (Value (std::forward<T> (value))); }

    template <typename T>
    void append (T&& value)             { addArrayElement (Value (std::forward<T> (value))); }

    /// JavaScript-style splice method for arrays
    /// Removes count elements starting at index and optionally inserts new elements
    /// Returns an array containing the removed elements
    Value splice (uint32_t index, uint32_t deleteCount = 1);

    /// JavaScript-style splice method with replacement elements
    template <typename... Elements>
    Value splice (uint32_t index, uint32_t deleteCount, Elements&&... elementsToInsert);

    //==============================================================================
    /// Object access methods
    Value& operator[] (std::string_view key);
    const Value& operator[] (std::string_view key) const;
    Value& operator[] (const char* key);
    const Value& operator[] (const char* key) const;

    /// Returns true if object contains the given member
    bool hasObjectMember (std::string_view key) const;

    /// Returns name and value of member at given index
    struct MemberNameAndValue
    {
        std::string_view name;
        const Value& value;
    };

    MemberNameAndValue getObjectMemberAt (uint32_t index) const;

    /// Appends one or more members to an object, with the given names and values.
    /// The value can be a supported primitive type, a string, or a Value.
    /// The function can take any number of name/value pairs.
    /// This will throw an Error if this isn't possible for some reason (e.g. if the value isn't an object)
    template <typename MemberType, typename... Others>
    void addMember (std::string_view name, MemberType value, Others&&...);

    template <typename MemberType>
    void setMember (std::string_view name, MemberType newValue);

    /// Removes a member from an object
    /// Returns true if the member was found and removed, false otherwise
    bool removeMember (std::string_view key);

    //==============================================================================
    /// Iterator support for range-based for loops
    struct Iterator
    {
        Iterator (const Value*, uint32_t);
        Iterator& operator++();
        Iterator operator++ (int);
        bool operator== (const Iterator&) const;
        bool operator!= (const Iterator&) const;

        /// For arrays, returns the element. For objects, returns the value part.
        const Value& operator*() const;
        const Value* operator->() const;

        /// For objects, returns name and value
        MemberNameAndValue getObjectMember() const;

        const Value* container;
        uint32_t index;
    };

    Iterator begin() const;
    Iterator end() const;

private:
    //==============================================================================
    enum class Type : uint8_t
    {
        undefined,
        null,
        bool_,
        int32,
        int64,
        double_,        // unified floating point type
        shortString,    // stored in union, first byte is size
        longString,     // heap-allocated
        array,          // heap-allocated
        object          // heap-allocated
    };

    struct MemberKey;  // Forward declaration

    // Friend functions for factory methods
    friend Value createNull();
    friend Value createEmptyArray();
    friend Value createObject();

    struct Member;

    #pragma pack(push, 1)
    struct ArrayData
    {
        Value* elements;
        uint32_t numElements;
        uint32_t capacity;
    };

    struct ObjectData
    {
        Member* members;
        uint32_t numMembers;
        uint32_t capacity;
    };

    struct LongStringData
    {
        char* chars;
        size_t length;
    };

    static constexpr size_t shortStringMaxSize = 22;

    union Data
    {
        bool boolValue;
        int32_t int32Value;
        int64_t int64Value;
        double doubleValue;  // Use double for all floating point

        struct
        {
            uint8_t length;
            char chars[shortStringMaxSize];
        } shortString;

        LongStringData longString;
        ArrayData arrayData;
        ObjectData objectData;

        Data() : int64Value (0) {}
    };
    #pragma pack(pop)

    Type type = Type::undefined;
    Data data;

    //==============================================================================
    void reset();
    void copyFrom (const Value& other);
    void ensureArray();
    void ensureObject();
    uint32_t findObjectMember (std::string_view key) const;
    void setObjectMember (std::string_view, Value&&);
    [[noreturn]] static void throwError (const char* message);
};

//==============================================================================
// Factory functions
Value createNull();
Value createBool (bool value);
Value createInt (int64_t value);
Value createFloat (double value);
Value createString (std::string_view value);
Value createEmptyArray();
Value createObject();

/// A helper function to create a JSON-friendly Value object with a set of properties.
/// The argument list must be contain pairs of names and values, e.g.
///
///  auto myObject = choc::json::create ("property1", 1234,
///                                      "property2", "hello",
///                                      "property3", 100.0f);
template <typename... Properties>
[[nodiscard]] Value create (Properties&&... propertyNamesAndValues);

/// Creates an array from an iterable container such as a std::vector. The container
/// must contain either Values, or primitive elements which can be turned into Values.
template <typename ContainerType>
static Value createArray (const ContainerType&);

/// Creates an array with a given size, and uses the supplied functor to populate it.
/// The functor must be a class or lambda which can take a uint32_t index parameter and return
/// either Value objects or primitive types to store at that index.
template <typename GetElementValue>
static Value createArray (uint32_t numArrayElements, const GetElementValue& getValueAt);









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

inline Value::Value (const Value& other)
{
    copyFrom (other);
}

inline Value::Value (Value&& other) noexcept
    : type (other.type), data (other.data)
{
    other.type = Type::undefined;
    other.data.int64Value = 0;
}

inline Value& Value::operator= (const Value& other)
{
    if (this != &other)
    {
        reset();
        copyFrom (other);
    }

    return *this;
}

inline Value& Value::operator= (Value&& other) noexcept
{
    if (this != &other)
    {
        reset();
        type = other.type;
        data = other.data;
        other.type = Type::undefined;
        other.data.int64Value = 0;
    }

    return *this;
}

inline Value& Value::operator= (const choc::value::ValueView& other)
{
    reset();
    *this = Value (other);  // Use the converting constructor
    return *this;
}

inline Value::~Value()
{
    reset();
}

//==============================================================================
inline Value::Value (bool value)     : type (Type::bool_)    { data.boolValue = value; }
inline Value::Value (int32_t value)  : type (Type::int32)    { data.int32Value = value; }
inline Value::Value (int64_t value)  : type (Type::int64)    { data.int64Value = value; }
inline Value::Value (float value)    : type (Type::double_)  { data.doubleValue = value; }
inline Value::Value (double value)   : type (Type::double_)  { data.doubleValue = value; }

inline Value::Value (const char* value) : Value (value != nullptr ? std::string_view (value) : std::string_view()) {}
inline Value::Value (const std::string& value) : Value (std::string_view (value)) {}

inline Value::Value (const choc::value::ValueView& other)
{
    if (other.isVoid())          { type = Type::null; data.int64Value = 0; }
    else if (other.isBool())     *this = Value (other.getBool());
    else if (other.isInt32())    *this = Value (other.getInt32());
    else if (other.isInt64())    *this = Value (other.getInt64());
    else if (other.isFloat32())  *this = Value (other.getFloat32());
    else if (other.isFloat64())  *this = Value (other.getFloat64());
    else if (other.isString())   *this = Value (other.getString());
    else if (other.isArray() || other.isVector())
    {
        *this = createEmptyArray();
        reserveArray (other.size());  // Pre-allocate for known size
        for (uint32_t i = 0; i < other.size(); ++i)
            addArrayElement (Value (other[i]));
    }
    else if (other.isObject())
    {
        *this = createObject();
        reserveObject (other.size());  // Pre-allocate for known size
        for (uint32_t i = 0; i < other.size(); ++i)
        {
            auto member = other.getObjectMemberAt (i);
            addMember (member.name, Value (member.value));
        }
    }
    else
    {
        type = Type::null;
        data.int64Value = 0;
    }
}

inline Value::Value (std::string_view value)
{
    if (value.length() <= shortStringMaxSize)
    {
        type = Type::shortString;
        data.shortString.length = static_cast<uint8_t> (value.length());
        std::memcpy (data.shortString.chars, value.data(), value.length());
    }
    else
    {
        type = Type::longString;
        data.longString.length = value.length();
        data.longString.chars = static_cast<char*> (std::malloc (value.length()));
        std::memcpy (data.longString.chars, value.data(), value.length());
    }
}

//==============================================================================
// Trivially-relocatable string class for Member keys - allows safe use with realloc
struct Value::MemberKey
{
    MemberKey() = default;
    MemberKey (const MemberKey&) = delete;
    MemberKey (MemberKey&&) = delete;
    ~MemberKey() noexcept   { freeIfLongString(); }
    MemberKey& operator= (const MemberKey&) = delete;

    MemberKey& operator= (MemberKey&& other) noexcept
    {
        std::memcpy (data, other.data, sizeof (data));
        other.data[0] = 0;
        return *this;
    }

    MemberKey (std::string_view str)
    {
        auto len = str.length();

        if (len <= maxShortLength)
        {
            // Short string: store length in first byte
            freeIfLongString();
            data[0] = static_cast<char> (len);
            std::memcpy (data + 1, str.data(), len);
        }
        else
        {
            // Long string: first byte top bit set, then aligned size_t length, then char* pointer
            auto& longData = getLongData();

            if (isShortString())
                longData.chars = static_cast<char*> (std::malloc (len));
            else
                longData.chars = static_cast<char*> (std::realloc (longData.chars, len));

            data[0] = longStringMarker;
            longData.length = len;
            std::memcpy (longData.chars, str.data(), len);
        }
    }

    std::string_view get() const noexcept
    {
        if (isShortString())
            return std::string_view (data + 1, static_cast<uint8_t> (data[0]));

        auto& longData = getLongData();
        return std::string_view (longData.chars, longData.length);
    }

    bool operator== (std::string_view str) const noexcept    { return get() == str; }

    void freeIfLongString() noexcept
    {
        if (! isShortString())
            std::free (getLongData().chars);
    }

    struct LongStringData
    {
        size_t length;
        char* chars;
    };

    char data[24] {};

    static constexpr size_t maxShortLength = sizeof (data) - 1;
    static constexpr char longStringMarker = 64;

    bool isShortString() const noexcept                  { return data[0] != longStringMarker; }
    const LongStringData& getLongData() const noexcept   { return *reinterpret_cast<const LongStringData*> (data + 8); }
    LongStringData& getLongData() noexcept               { return *reinterpret_cast<LongStringData*> (data + 8); }
};

struct Value::Member
{
    MemberKey key;
    Value value;
};

//==============================================================================
inline bool Value::getBool() const
{
    if (type != Type::bool_)
        throwError ("Value is not a boolean");

    return data.boolValue;
}

inline int32_t Value::getInt32() const
{
    if (type != Type::int32)
        throwError ("Value is not a 32-bit integer");

    return data.int32Value;
}

inline int64_t Value::getInt64() const
{
    if (type != Type::int64)
        throwError ("Value is not a 64-bit integer");

    return data.int64Value;
}

inline float Value::getFloat32() const
{
    return static_cast<float> (getFloat64());
}

inline double Value::getFloat64() const
{
    if (type != Type::double_)
        throwError ("Value is not a float");

    return data.doubleValue;
}

inline std::string_view Value::getString() const
{
    if (type == Type::shortString)
        return std::string_view (data.shortString.chars, data.shortString.length);

    if (type == Type::longString)
        return std::string_view (data.longString.chars, data.longString.length);

    throwError ("Value is not a string");
}

template <typename T>
inline T Value::get() const
{
    if constexpr (std::is_same_v<T, bool>)              return getBool();
    else if constexpr (std::is_same_v<T, int32_t>)      return getInt32();
    else if constexpr (std::is_same_v<T, int64_t>)      return getInt64();
    else if constexpr (std::is_same_v<T, int>)          return static_cast<T> (getInt());
    else if constexpr (std::is_same_v<T, float>)        return getFloat32();
    else if constexpr (std::is_same_v<T, double>)       return getFloat64();
    else if constexpr (std::is_same_v<T, std::string>)  return std::string (getString());
    else if constexpr (std::is_same_v<T, std::string_view>) return getString();
    else                                                static_assert (std::is_same_v<T, void>, "Unsupported type for get<T>()");
}

template <typename T>
inline T Value::getWithDefault (T defaultValue) const
{
    if constexpr (std::is_same_v<T, bool>)              return isBool() ? getBool() : defaultValue;
    else if constexpr (std::is_same_v<T, int32_t>)      return isInt32() ? getInt32() : defaultValue;
    else if constexpr (std::is_same_v<T, int64_t>)      return isInt64() ? getInt64() : (isInt32() ? getInt32() : defaultValue);
    else if constexpr (std::is_same_v<T, float>)        return isFloat() ? getFloat32() : defaultValue;
    else if constexpr (std::is_same_v<T, double>)       return isFloat() ? getFloat64() : defaultValue;
    else if constexpr (std::is_same_v<T, std::string>)  return isString() ? std::string (getString()) : defaultValue;
    else                                                return defaultValue;
}

inline std::string Value::toString() const
{
    return getWithDefault<std::string> ({});
}

//==============================================================================
inline uint32_t Value::size() const
{
    switch (type)
    {
        case Type::shortString:     return data.shortString.length;
        case Type::longString:      return static_cast<uint32_t> (data.longString.length);
        case Type::array:           return data.arrayData.numElements;
        case Type::object:          return data.objectData.numMembers;
        default:                    throwError ("Value does not have a size");
    }
}

inline bool Value::empty() const
{
    switch (type)
    {
        case Type::shortString:     return data.shortString.length == 0;
        case Type::longString:      return data.longString.length == 0;
        case Type::array:           return data.arrayData.numElements == 0;
        case Type::object:          return data.objectData.numMembers == 0;
        case Type::undefined:
        case Type::null:            return true;
        default:                    return false;
    }
}

inline void Value::clear()
{
    switch (type)
    {
        case Type::array:
            for (uint32_t i = 0; i < data.arrayData.numElements; ++i)
                data.arrayData.elements[i].~Value();

            data.arrayData.numElements = 0;
            break;

        case Type::object:
            for (uint32_t i = 0; i < data.objectData.numMembers; ++i)
                data.objectData.members[i].~Member();

            data.objectData.numMembers = 0;
            break;

        default:
            reset();
            break;
    }
}

//==============================================================================
inline bool Value::operator== (const Value& other) const
{
    // Handle numeric equality across different number types (JSON semantics)
    if ((isInt() || isFloat()) && (other.isInt() || other.isFloat()))
    {
        // Convert both to double for comparison
        double thisVal = isFloat() ? getFloat64() : static_cast<double>(getInt());
        double otherVal = other.isFloat() ? other.getFloat64() : static_cast<double>(other.getInt());
        return thisVal == otherVal;
    }

    if (type != other.type)
        return false;

    switch (type)
    {
        case Type::undefined:
        case Type::null:
            return true;

        case Type::bool_:
            return data.boolValue == other.data.boolValue;

        case Type::int32:
        case Type::int64:
        case Type::double_:   // These are handled by numeric comparison above
            return false; // Should never reach here due to early return

        case Type::shortString:
            return data.shortString.length == other.data.shortString.length
                    && std::memcmp (data.shortString.chars, other.data.shortString.chars, data.shortString.length) == 0;

        case Type::longString:
            return data.longString.length == other.data.longString.length
                    && std::memcmp (data.longString.chars, other.data.longString.chars, data.longString.length) == 0;

        case Type::array:
            if (data.arrayData.numElements != other.data.arrayData.numElements)
                return false;

            for (uint32_t i = 0; i < data.arrayData.numElements; ++i)
                if (! (data.arrayData.elements[i] == other.data.arrayData.elements[i]))
                    return false;

            return true;

        case Type::object:
            if (data.objectData.numMembers != other.data.objectData.numMembers)
                return false;

            for (uint32_t i = 0; i < data.objectData.numMembers; ++i)
            {
                const auto& member = data.objectData.members[i];
                auto otherIndex = other.findObjectMember (member.key.get());

                if (otherIndex >= other.data.objectData.numMembers
                     || ! (member.value == other.data.objectData.members[otherIndex].value))
                    return false;
            }

            return true;
    }

    return false;
}

//==============================================================================
inline Value& Value::operator[] (uint32_t index)
{
    if (type != Type::array)
        throwError ("Value is not an array");

    if (index >= data.arrayData.numElements)
        throwError ("Array index out of bounds");

    return data.arrayData.elements[index];
}

inline const Value& Value::operator[] (uint32_t index) const
{
    if (type != Type::array)
        throwError ("Value is not an array");

    if (index >= data.arrayData.numElements)
        throwError ("Array index out of bounds");

    return data.arrayData.elements[index];
}

inline void Value::addArrayElement (Value value)
{
    ensureArray();

    if (data.arrayData.numElements >= data.arrayData.capacity)
        reserveArray ((data.arrayData.capacity + 8) & ~7u);

    new (&data.arrayData.elements[data.arrayData.numElements]) Value (std::move (value));
    ++data.arrayData.numElements;
}

inline Value Value::splice (uint32_t index, uint32_t deleteCount)
{
    if (type != Type::array)
        throwError ("Value is not an array");

    auto result = createEmptyArray();

    if (index >= data.arrayData.numElements)
        return result;  // Return empty array if index is out of bounds

    deleteCount = std::min (deleteCount, data.arrayData.numElements - index);

    if (deleteCount == 0)
        return result;  // Nothing to delete

    result.reserveArray (deleteCount);

    for (uint32_t i = 0; i < deleteCount; ++i)
        result.addArrayElement (std::move (data.arrayData.elements[index + i]));

    std::memmove (reinterpret_cast<char*> (data.arrayData.elements + index),
                  reinterpret_cast<const char*> (data.arrayData.elements + index + deleteCount),
                  sizeof (*data.arrayData.elements) * (data.arrayData.numElements - index - deleteCount));

    data.arrayData.numElements -= deleteCount;
    return result;
}

template <typename... Elements>
inline Value Value::splice (uint32_t index, uint32_t deleteCount, Elements&&... elementsToInsert)
{
    if (type != Type::array)
        throwError ("Value is not an array");

    auto deletedElements = splice (index, deleteCount);

    constexpr auto numToInsert = sizeof...(elementsToInsert);

    if constexpr (numToInsert != 0)
    {
        if (index > data.arrayData.numElements)
            index = data.arrayData.numElements;

        auto newSize = static_cast<uint32_t> (data.arrayData.numElements + numToInsert);

        if (newSize > data.arrayData.capacity)
            reserveArray (newSize);

        if (index < data.arrayData.numElements)
            std::memmove (reinterpret_cast<char*> (data.arrayData.elements + index + numToInsert),
                          reinterpret_cast<const char*> (data.arrayData.elements + index),
                          sizeof (*data.arrayData.elements) * (data.arrayData.numElements - index));

        auto insertIndex = index;

        auto insertElement = [&] (auto&& element)
        {
            new (&data.arrayData.elements[insertIndex++]) Value (Value (std::forward<decltype(element)> (element)));
        };

        (insertElement (std::forward<Elements> (elementsToInsert)), ...);
        data.arrayData.numElements = newSize;
    }

    return deletedElements;
}

//==============================================================================
inline Value& Value::operator[] (std::string_view key)
{
    if (type != Type::object)
        throwError ("Value is not an object");

    auto index = findObjectMember (key);
    if (index >= data.objectData.numMembers)
        throwError ("Object member not found");

    return data.objectData.members[index].value;
}

inline const Value& Value::operator[] (std::string_view key) const
{
    if (type != Type::object)
        throwError ("Value is not an object");

    auto index = findObjectMember (key);
    if (index >= data.objectData.numMembers)
        throwError ("Object member not found");

    return data.objectData.members[index].value;
}

inline Value& Value::operator[] (const char* key)                 { return operator[] (key != nullptr ? std::string_view (key) : std::string_view()); }
inline const Value& Value::operator[] (const char* key) const     { return operator[] (key != nullptr ? std::string_view (key) : std::string_view()); }

inline bool Value::hasObjectMember (std::string_view key) const
{
    if (type != Type::object)
        return false;

    return findObjectMember (key) < data.objectData.numMembers;
}

inline Value::MemberNameAndValue Value::getObjectMemberAt (uint32_t index) const
{
    if (type != Type::object)
        throwError ("Value is not an object");

    if (index >= data.objectData.numMembers)
        throwError ("Object member index out of bounds");

    const auto& member = data.objectData.members[index];
    return { member.key.get(), member.value };
}

template <typename MemberType, typename... Others>
void Value::addMember (std::string_view name, MemberType v, Others&&... others)
{
    static_assert ((sizeof...(others) & 1) == 0, "The arguments must be a sequence of name, value pairs");

    ensureObject();
    // Pre-allocate space for all the members we're about to add
    reserveObject (data.objectData.numMembers + 1u + sizeof...(others) / 2);

    setMember (name, std::move (v));

    if constexpr (sizeof...(others) != 0)
        addMember (std::forward<Others> (others)...);
}

inline void Value::setObjectMember (std::string_view key, Value&& value)
{
    ensureObject();

    if (auto index = findObjectMember (key); index < data.objectData.numMembers)
    {
        data.objectData.members[index].value = std::move (value);
        return;
    }

    if (data.objectData.numMembers >= data.objectData.capacity)
        reserveObject ((data.objectData.numMembers + 4) & ~3u);

    auto& member = data.objectData.members[data.objectData.numMembers++];
    new (&member) Member { MemberKey(key), std::move(value) };
}

template <typename MemberType>
void Value::setMember (std::string_view name, MemberType v)
{
    setObjectMember (name, Value (std::move (v)));
}

inline bool Value::removeMember (std::string_view key)
{
    if (type != Type::object)
        return false;  // Not an object, can't remove members

    auto index = findObjectMember (key);

    if (index >= data.objectData.numMembers)
        return false;  // Member not found

    for (uint32_t i = index + 1; i < data.objectData.numMembers; ++i)
        data.objectData.members[i - 1] = std::move (data.objectData.members[i]);

    --data.objectData.numMembers;
    return true;
}

//==============================================================================
inline Value::Iterator::Iterator (const Value* c, uint32_t i)
    : container (c), index (i)
{
}

inline Value::Iterator& Value::Iterator::operator++()
{
    ++index;
    return *this;
}

inline Value::Iterator Value::Iterator::operator++ (int)
{
    auto old = *this;
    ++index;
    return old;
}

inline bool Value::Iterator::operator== (const Iterator& other) const { return container == other.container && index == other.index; }
inline bool Value::Iterator::operator!= (const Iterator& other) const { return ! (*this == other); }

inline const Value& Value::Iterator::operator*() const
{
    if (container->isArray())
        return (*container)[index];

    if (container->isObject())
        return container->getObjectMemberAt (index).value;

    throwError ("Cannot dereference iterator on non-container type");
}

inline const Value* Value::Iterator::operator->() const
{
    return &(**this);
}

inline Value::MemberNameAndValue Value::Iterator::getObjectMember() const
{
    if (! container->isObject())
        throwError ("Iterator is not on an object");

    return container->getObjectMemberAt (index);
}

inline Value::Iterator Value::begin() const
{
    if (! (isArray() || isObject()))
        throwError ("Value is not iterable");

    return Iterator (this, 0);
}

inline Value::Iterator Value::end() const
{
    if (! (isArray() || isObject()))
        throwError ("Value is not iterable");

    return Iterator (this, size());
}


//==============================================================================
inline void Value::reset()
{
    switch (type)
    {
        case Type::longString:
            std::free (data.longString.chars);
            break;

        case Type::array:
            for (uint32_t i = 0; i < data.arrayData.numElements; ++i)
                data.arrayData.elements[i].~Value();

            std::free (data.arrayData.elements);
            break;

        case Type::object:
            for (uint32_t i = 0; i < data.objectData.numMembers; ++i)
                data.objectData.members[i].~Member();

            std::free (data.objectData.members);
            break;

        default:
            break;
    }

    type = Type::undefined;
    data.int64Value = 0;
}

inline void Value::copyFrom (const Value& other)
{
    type = other.type;

    switch (type)
    {
        case Type::undefined:
        case Type::null:
        case Type::bool_:
        case Type::int32:
        case Type::int64:
        case Type::double_:
        case Type::shortString:
            data = other.data;
            break;

        case Type::longString:
        {
            const auto& otherString = other.data.longString;
            data.longString.length = otherString.length;
            data.longString.chars = static_cast<char*> (std::malloc (otherString.length));
            std::memcpy (data.longString.chars, otherString.chars, otherString.length);
            break;
        }

        case Type::array:
        {
            const auto& otherArray = other.data.arrayData;
            data.arrayData.numElements = otherArray.numElements;
            data.arrayData.capacity = otherArray.capacity;
            data.arrayData.elements = static_cast<Value*> (std::malloc (otherArray.capacity * sizeof (Value)));

            for (uint32_t i = 0; i < otherArray.numElements; ++i)
                new (&data.arrayData.elements[i]) Value (otherArray.elements[i]);

            break;
        }

        case Type::object:
        {
            const auto& otherObject = other.data.objectData;
            data.objectData.numMembers = otherObject.numMembers;
            data.objectData.capacity = otherObject.capacity;
            data.objectData.members = static_cast<Member*> (std::malloc (otherObject.capacity * sizeof (Member)));

            for (uint32_t i = 0; i < otherObject.numMembers; ++i)
            {
                new (&data.objectData.members[i].key) MemberKey (otherObject.members[i].key.get());
                new (&data.objectData.members[i].value) Value (otherObject.members[i].value);
            }

            break;
        }
    }
}

inline void Value::ensureArray()
{
    if (type != Type::array)
    {
        reset();
        type = Type::array;
        data.arrayData.elements = nullptr;
        data.arrayData.numElements = 0;
        data.arrayData.capacity = 0;
    }
}

inline void Value::ensureObject()
{
    if (type != Type::object)
    {
        reset();
        type = Type::object;
        data.objectData.members = nullptr;
        data.objectData.numMembers = 0;
        data.objectData.capacity = 0;
    }
}

inline void Value::reserveArray (uint32_t capacity)
{
    ensureArray();

    if (capacity <= data.arrayData.capacity)
        return;  // Already have enough capacity

    auto newElements = static_cast<Value*> (std::realloc (static_cast<void*> (data.arrayData.elements), capacity * sizeof (Value)));

    if (! newElements)
        throwError ("Failed to allocate memory for array");

    data.arrayData.elements = newElements;
    data.arrayData.capacity = capacity;
}

inline void Value::reserveObject (uint32_t capacity)
{
    ensureObject();

    if (capacity <= data.objectData.capacity)
        return;  // Already have enough capacity

    // MemberKey is trivially relocatable, so we can safely use realloc
    auto newMembers = static_cast<Member*> (std::realloc (static_cast<void*> (data.objectData.members),
                                                          capacity * sizeof (Member)));

    if (! newMembers)
        throwError ("Failed to allocate memory for object");

    data.objectData.members = newMembers;
    data.objectData.capacity = capacity;
}

inline uint32_t Value::findObjectMember (std::string_view key) const
{
    if (type != Type::object)
        return 0;

    for (uint32_t i = 0; i < data.objectData.numMembers; ++i)
        if (data.objectData.members[i].key == key)
            return i;

    return data.objectData.numMembers;
}

inline void Value::throwError (const char* message)
{
    throw std::runtime_error (message);
}

//==============================================================================
inline Value createNull()                           { Value v; v.type = Value::Type::null; return v; }
inline Value createBool (bool value)                { return Value (value); }
inline Value createInt (int64_t value)              { return Value (value); }
inline Value createFloat (double value)             { return Value (value); }
inline Value createString (std::string_view value)  { return Value (value); }
inline Value createEmptyArray()                     { Value v; v.type = Value::Type::array; v.data.arrayData.elements = nullptr; v.data.arrayData.numElements = 0; v.data.arrayData.capacity = 0; return v; }
inline Value createObject()                         { Value v; v.type = Value::Type::object; v.data.objectData.members = nullptr; v.data.objectData.numMembers = 0; v.data.objectData.capacity = 0; return v; }

template <typename... Properties>
json::Value create (Properties&&... properties)
{
    static_assert ((sizeof...(properties) & 1) == 0, "The arguments must be a sequence of name, value pairs");

    auto v = createObject();
    v.reserveObject (sizeof...(properties) / 2);  // Pre-allocate for known number of members
    v.addMember (std::forward<Properties> (properties)...);
    return v;
}

template <typename GetElementValue>
Value createArray (uint32_t numArrayElements, const GetElementValue& getValueAt)
{
    auto result = createEmptyArray();
    result.reserveArray (numArrayElements);  // Pre-allocate for known number of elements

    for (uint32_t i = 0; i < numArrayElements; ++i)
        result.addArrayElement (Value (getValueAt (i)));

    return result;
}

template <typename ContainerType>
Value createArray (const ContainerType& container)
{
    auto result = createEmptyArray();
    result.reserveArray (static_cast<uint32_t> (container.size()));  // Pre-allocate for known size

    for (const auto& element : container)
        result.addArrayElement (Value (element));

    return result;
}

static_assert (sizeof (Value) <= 24, "Value should be no more than 24 bytes");

} // namespace choc::json

#endif // CHOC_JSON_VALUE_HEADER_INCLUDED