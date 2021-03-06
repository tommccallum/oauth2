// ----------------------------------------------------------
// Tiny JSON Parser
// Written in a C-style rather than OOP.
// I use a macro INTERNAL to indicate 'static'
// I use a macro ENTRYPOINT that indicates that this is the main entry to use the API
// ----------------------------------------------------------

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>

#ifdef TEST_JSON
#include "char_tests.cpp"
#endif
#include "macros.h"


struct JsonItem;
struct JsonParseContext;
using JsonArray = std::vector<JsonItem>;            
using JsonObject = std::map<std::string, JsonItem>; 

enum class JsonItemType {
    NULL_VALUE = 0,
    TRUE_VALUE,
    FALSE_VALUE,
    TEXT,
    INTEGER,
    FLOAT,
    ARRAY,
    OBJECT,
    EMPTY,
    ERROR,
    END_OF_JSON_VALUES
};

struct JsonItem {
    JsonItemType type;
    std::string text;
    long integer;
    double real;
    JsonArray array;
    JsonObject object;
};


struct JsonParseContext 
{
    std::string buffer;
    size_t pos;
    size_t selection_start_pos;
    size_t selection_end_pos;
    bool error;
    size_t error_pos_start;
    size_t error_pos_end;
    std::string error_message;
};

// forward declare functions due to C/C++ ordering requirements
INTERNAL JsonItem json_create_from_string_buffer(JsonParseContext& context);
JsonItem json_create_from_string(std::string const & buffer);

INTERNAL
JsonItem json_create_new() {
    auto item = JsonItem {};
    item.type = JsonItemType::END_OF_JSON_VALUES;
    return item;
}

INTERNAL
std::string json_get_selected_text(JsonParseContext& context) {
    return context.buffer.substr(context.selection_start_pos, context.selection_end_pos - context.selection_start_pos+1);
}

INTERNAL
void json_parse_text_value(JsonParseContext& context, JsonItem& item) 
{
    // assume index it currently pointing to the first quote indicating a string
    context.selection_start_pos = context.pos + 1;
    size_t index = context.pos+1;
    char open_quote = context.buffer[context.pos];
    while( true ) {
        index++;
        if ( index >= context.buffer.size() ) {
            context.error = true;
            context.error_pos_start = context.selection_start_pos;
            context.error_pos_end = index-1;
            context.error_message = "String without final quotes was detected.";
            return;
        }
        if ( context.buffer[index] == '\\') {
            // escape character
            index++;
        } else if ( context.buffer[index] == open_quote) {
            break;
        }
    }
    context.selection_end_pos = index - 1;
    context.pos = index+1; // move on passed the last quote
    item.type = JsonItemType::TEXT;
    item.text = json_get_selected_text(context);
}


INTERNAL
void json_parse_key(JsonParseContext& context, JsonItem& item) 
{
    // assume index it currently pointing to the first quote indicating a string
    context.selection_start_pos = context.pos;
    size_t index = context.pos;
    while( index++ ) {
        if ( index >= context.buffer.size() ) {
            context.error = true;
            context.error_pos_start = context.selection_start_pos;
            context.error_pos_end = index-1;
            context.error_message = "String without end was detected.";
            return;
        }
        if ( !is_key(context.buffer[index]) ) {
            break;
        }
    }
    context.selection_end_pos = index - 1;
    context.pos = index;
    item.type = JsonItemType::TEXT;
    item.text = json_get_selected_text(context);
}


INTERNAL
void json_parse_number_value(JsonParseContext& context, JsonItem& item) 
{
    auto index = context.pos;
    context.selection_start_pos = context.pos;
    while( true ) {
        index++;
        if ( index >= context.buffer.size() ) {
            break;
        }
        if ( !is_digit(context.buffer[index]) ) {
            break;
        }
    }
    if ( index < context.buffer.size() && is_decimal_point(context.buffer[index]) ) {
        // float
        while( true ) {
            ++index;
            if ( index >= context.buffer.size() ) {
                break;
            }
            if ( !is_digit(context.buffer[index]) ) {
                break;
            }
        }
        context.selection_end_pos = index-1;
        context.pos = index;
        item.type = JsonItemType::FLOAT;
        auto val = json_get_selected_text(context);
        item.real = std::stof(val);
    } else {
        context.selection_end_pos = index-1;
        context.pos = index;
        item.type = JsonItemType::INTEGER;
        auto val = json_get_selected_text(context);
        item.integer = std::stol(val);
    }
}


INTERNAL inline
void json_increment_context(JsonParseContext& context) {
    context.pos++;
}

INTERNAL inline
void json_eat_whitespace(JsonParseContext& context) {
    while ( is_whitespace(context.buffer[context.pos]) ) {
        json_increment_context(context);
    }
}

INTERNAL inline
void json_eat_commas(JsonParseContext& context) {
    while ( is_comma(context.buffer[context.pos]) ) {
        json_increment_context(context);
    }
}

INTERNAL inline
void json_eat_colons(JsonParseContext& context) {
    while ( is_colon(context.buffer[context.pos]) ) {
        json_increment_context(context);
    }
}

void json_parse_object_value(JsonParseContext& context, JsonItem& objectItem ) {
    // comma separated list of items
    json_eat_whitespace(context);
    objectItem.type = JsonItemType::OBJECT;
    context.selection_start_pos = context.pos;
    context.pos++;
    while(true) {
        auto new_key = json_create_from_string_buffer(context);
        if ( new_key.type != JsonItemType::TEXT && new_key.type != JsonItemType::INTEGER ) {
            context.error_message = "Key must be a string.";
            return;
        }
        if ( new_key.type == JsonItemType::INTEGER ) {
            std::ostringstream ss;
            ss << new_key.integer;
            new_key.text = ss.str();
        }
        json_eat_whitespace(context);
        if ( is_colon(context.buffer[context.pos]) ) {
            json_eat_colons(context);
        }
        json_eat_whitespace(context);
        auto new_value = json_create_from_string_buffer(context);
        if ( new_value.type == JsonItemType::END_OF_JSON_VALUES ) {
            context.error = true;
            context.error_pos_start = context.selection_start_pos;
            context.error_pos_end = context.pos;
            context.error_message = "No value found for object key-value pair.";
            return;
        }
        objectItem.object.insert(std::make_pair(new_key.text, new_value));
        json_eat_whitespace(context);
        if ( is_comma(context.buffer[context.pos]) ) {
            json_eat_commas(context);
        } else {
            if ( context.buffer[context.pos] == '}' ) {
                return;
            } else {
                context.error_message = "Expected '}' for end of object";
                return;
            }
        }
        json_eat_whitespace(context);
    }
}

INTERNAL
void json_parse_array_value(JsonParseContext& context, JsonItem& arrayItem ) {
    // comma separated list of items
    json_eat_whitespace(context);
    arrayItem.type = JsonItemType::ARRAY;
    while(true) {
        context.pos++;
        auto new_item = json_create_from_string_buffer(context);
        if ( new_item.type == JsonItemType::END_OF_JSON_VALUES ) {
            return;
        }
        arrayItem.array.push_back(new_item);
    }
}

INTERNAL
void parse_json_text(JsonItem& textItem, std::string const & buffer, size_t& index )
{
    auto start = index;
    while( index < buffer.size() ) {
        index++;
        if ( buffer[index] == '\\') {
            // escape character so we will ignore the next
            // character
        } else {
            if ( buffer[index] == buffer[start] ) {
                break;
            }
        }
    }
    textItem.text = buffer.substr(start, index - start);
}

INTERNAL
JsonItem json_create_from_string_buffer(JsonParseContext& context) {
    auto in = std::stringstream {};
    auto item = json_create_new();
    json_eat_whitespace(context);
    context.selection_start_pos = context.pos;
    while( context.pos < context.buffer.size() ) {
        if ( context.buffer[context.pos] == '{' ) {
            json_parse_object_value(context, item);
            return item;
        } else if ( context.buffer[context.pos] == '}' ) {
            return item;
        } else if ( context.buffer[context.pos] == '[' ) {
            json_parse_array_value(context, item);
            return item;
        } else if ( context.buffer[context.pos] == ']' ) {
            return item;
        } else if ( context.buffer[context.pos] == ',' ) {
            return item;
        } else if ( context.pos < context.buffer.size() - 3 && 
                    context.buffer[context.pos] == 'n' && 
                    context.buffer[context.pos+1] == 'u' && 
                    context.buffer[context.pos+2] == 'l' && 
                    context.buffer[context.pos+3] == 'l') {
            item.type = JsonItemType::NULL_VALUE;
            context.pos += 4;
            return item;} else if ( context.pos < context.buffer.size() - 3 && 
                    context.buffer[context.pos] == 't' && 
                    context.buffer[context.pos+1] == 'r' && 
                    context.buffer[context.pos+2] == 'u' && 
                    context.buffer[context.pos+3] == 'e') {
            item.type = JsonItemType::TRUE_VALUE;
            context.pos += 4;
            return item;
         } else if ( context.pos < context.buffer.size() - 4 && 
                    context.buffer[context.pos] == 'f' && 
                    context.buffer[context.pos+1] == 'a' && 
                    context.buffer[context.pos+2] == 'l' && 
                    context.buffer[context.pos+3] == 's' && 
                    context.buffer[context.pos+4] == 'e') {
            item.type = JsonItemType::FALSE_VALUE;
            context.pos += 5;
            return item;
        } else if ( is_quote(context.buffer[context.pos]) ) {
            json_parse_text_value(context, item);
            return item;
        } else if ( is_alpha(context.buffer[context.pos]) ) {
            // are we in an object? 
            // if not then error
            json_parse_key(context, item);
            return item;
        } else if ( is_digit(context.buffer[context.pos]) ) {
            json_parse_number_value(context, item);
            if ( item.type == JsonItemType::INTEGER || item.type == JsonItemType::FLOAT ) {
                return item;
            }
        } else if ( is_whitespace(context.buffer[context.pos]) ) {
            // eat whitespace 
        } else {
            context.error = true;
            context.error_pos_start = context.pos;
            context.error_pos_end = context.pos;
            context.error_message = "invalid character found";
            return item;
        }
        context.pos++;
    }
    return item;
}

// entry point
ENTRYPOINT
JsonItem json_create_from_string(std::string const & buffer) {
    auto context = JsonParseContext {};
    context.buffer = buffer;
    context.error_pos_start = 0;
    context.error_pos_end = 0;
    context.selection_start_pos = 0;
    context.selection_end_pos = 0;
    context.error = false;
    auto item = json_create_from_string_buffer(context);
    if ( context.error ) {
        item.type = JsonItemType::ERROR;
        std::ostringstream oss;
        oss << "Error at " << context.error_pos_start << "," << context.error_pos_end << ": " << context.error_message;
        item.text = oss.str();;
    }
    return item;
}

INTERNAL
std::string json_json_pretty_print_item(JsonItem const & json, size_t indent) {
    switch( json.type ) {
    case JsonItemType::EMPTY:
        return {};
    case JsonItemType::NULL_VALUE:
        return "null";
    case JsonItemType::TRUE_VALUE:
        return "true";
    case JsonItemType::FALSE_VALUE:
        return "false";
    case JsonItemType::TEXT:{
        std::ostringstream in;
        in << '"' << json.text << '"';
        return in.str();
    }
    case JsonItemType::INTEGER:{
        std::ostringstream in;
        in << json.integer;
        return in.str();
    }
    case JsonItemType::FLOAT:{
        std::ostringstream in;
        in << json.real;
        return in.str();
    }
    case JsonItemType::ARRAY:{
        std::ostringstream in;
        in << "[";
        if ( json.array.size() == 0 ) {
            in << "]";
            return in.str();
        }
        auto indentation = std::string( (indent+1) * 2, ' ');
        in << '\n';
        size_t counter = 0;
        for_each(begin(json.array), end(json.array), [&](JsonItem const & item) {
            in << indentation << json_json_pretty_print_item(item, indent+1);
            if ( counter < json.array.size()-1 ) {
                in << ",\n";
            } else {
                in << '\n';
            }
            counter++;
        });
        auto last_indentation = std::string( indent * 2, ' ');
        in << last_indentation << "]";
        return in.str();
    }
    case JsonItemType::OBJECT:{
        std::ostringstream in;
        in << "{";
        if ( json.object.size() == 0 ) {
            in << "}";
            return in.str();
        }
        auto indentation = std::string( (indent+1) * 2, ' ');
        in << '\n' << indentation;
        size_t counter = 0;
        for( auto const & [key,val] : json.object ) {
            in << key << " : " << json_json_pretty_print_item(val, indent+1);
            if ( counter < json.object.size()-1 ) {
                in << ",\n" << indentation;
            } else {
                in << "\n";
            }
            counter++;
        };
        auto last_indentation = std::string( indent * 2, ' ');
        in << last_indentation << "}";
        return in.str();
    }
    case JsonItemType::END_OF_JSON_VALUES:{
        std::ostringstream in;
        in << "<EMPTY>";
        return in.str();
    }
    case JsonItemType::ERROR: {
        std::ostringstream in;
        in << json.text;
        return in.str();
    }
    default:{
        std::ostringstream in;
        in << "invalid type found '" << (int) json.type ;
        return in.str();
    }
    }
}

INTERNAL
void json_pretty_print(JsonItem const & json ) {
    std::ostringstream in;
    in << json_json_pretty_print_item(json, 0);
    in << '\n';
    std::cout << in.str();
}

#ifdef TEST_JSON
int main() {
#if 1
    {
        std::string test = "";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "null";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "true";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "false";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "[]";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "{}";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "1234567890";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "123.4567890";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "\"a string\"";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "\"a multiline\nstring\"";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "\"a string with a \\\"quote\\\" in it\"";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "[0,1,2,3,4,5,6,7,8,9]";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "[0,1,2,3,4,5,6,7,8,[[[5]]]]";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "{ keyonly }";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "{ key: value }";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "{ key: 9 }";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "{ key: 9, 1: 14 }";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "{ key: 9, 1: [14,2,3] }";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }

    {
        std::string test = "{ key: 9, 1: [14,2,3], \"a\":\"b\", g: h }";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
#endif
    {
        std::string test = "\"''\"";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
    {
        std::string test = "'\"'";
        auto json = json_create_from_string(test);
        json_pretty_print(json);
    }
}
#endif