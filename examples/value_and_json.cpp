#include <iostream>

#include "../choc/containers/choc_Value.h"
#include "../choc/containers/choc_JSONValue.h"
#include "../choc/text/choc_JSON.h"

int main()
{
    // Create a choc::json::Value object
    auto address = choc::json::create ("street", "123 Main St",
                                       "city", "Anytown");

    auto person = choc::json::create ("name", "John Doe",
                                      "age", 30,
                                      "isStudent", false,
                                      "address", address);

    // Convert the choc::json::Value object to a JSON string
    auto jsonString = choc::json::toString (person);
    std::cout << "Generated JSON:\n" << jsonString << std::endl;

    // Parse the JSON string back into a choc::json::Value object
    auto parsedValue = choc::json::parse (jsonString);

    // Access the data from the parsed object
    std::cout << "\nParsed JSON data:\n";
    std::cout << "Name: " << parsedValue["name"].get<std::string>() << std::endl;
    std::cout << "Age: " << parsedValue["age"].getInt() << std::endl;
    std::cout << "Is Student: " << (parsedValue["isStudent"].get<bool>() ? "true" : "false") << std::endl;
    std::cout << "Address: " << parsedValue["address"]["street"].get<std::string>() << ", " << parsedValue["address"]["city"].get<std::string>() << std::endl;

    return 0;
}
