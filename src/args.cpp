// -----------------------------------------------------------------------------
// Args++: an argument-parsing library in portable C++11.
// -----------------------------------------------------------------------------

#include "args.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <deque>

using namespace std;
using namespace args;


// -----------------------------------------------------------------------------
// Flags and Options.
// -----------------------------------------------------------------------------


struct args::Flag {
    int count = 0;
};


struct args::Option {
    vector<string> values;
    string fallback;
};


// -----------------------------------------------------------------------------
// ArgStream.
// -----------------------------------------------------------------------------


struct args::ArgStream {
    deque<string> args;
    void append(string const& arg);
    string next();
    bool hasNext();
};


void ArgStream::append(string const& arg) {
    args.push_back(arg);
}


string ArgStream::next() {
    string arg = args.front();
    args.pop_front();
    return arg;
}


bool ArgStream::hasNext() {
    return !args.empty();
}


// -----------------------------------------------------------------------------
// ArgParser: setup.
// -----------------------------------------------------------------------------


void ArgParser::flag(string const& name) {
    auto flag = make_shared<Flag>();
    stringstream stream(name);
    string alias;
    while (stream >> alias) {
        flags[alias] = flag;
    }
}


void ArgParser::option(string const& name, string const& fallback) {
    auto option = make_shared<Option>();
    option->fallback = fallback;
    stringstream stream(name);
    string alias;
    while (stream >> alias) {
        options[alias] = option;
    }
}


// -----------------------------------------------------------------------------
// ArgParser: retrieve values.
// -----------------------------------------------------------------------------


bool ArgParser::found(string const& name) const {
    return count(name) > 0;
}


int ArgParser::count(string const& name) const {
    auto flag = lookup(flags, name);
    if (flag) {
        return flag->count;
    }
    auto option = lookup(options, name);
    if (option) {
        return option->values.size();
    }
    return 0;
}


string ArgParser::value(string const& name) const {
    auto option = lookup(options, name);
    if (option) {
        if (!option->values.empty()) {
            return option->values.back();
        }
        return option->fallback;
    }
    return string();
}


vector<string> ArgParser::values(string const& name) const {
    auto option = lookup(options, name);
    return option ? option->values : vector<string>();
}


// -----------------------------------------------------------------------------
// ArgParser: commands.
// -----------------------------------------------------------------------------


ArgParser& ArgParser::command(
    string const& name,
    string const& helptext,
    void (*callback)(string cmd_name, ArgParser& cmd_parser)) {

    auto parser = make_shared<ArgParser>();
    parser->helptext = helptext;
    parser->callback = callback;

    stringstream stream(name);
    string alias;

    while (stream >> alias) {
        commands[alias] = parser;
    }

    return *parser;
}


bool ArgParser::commandFound() const {
    return !command_name.empty();
}


string ArgParser::commandName() const {
    return command_name;
}


ArgParser& ArgParser::commandParser() const {
    auto parser = lookup(commands, command_name);
    return parser ? *parser : const_cast<ArgParser &>(*this);
}


// -----------------------------------------------------------------------------
// ArgParser: parse arguments.
// -----------------------------------------------------------------------------


// Parse an option of the form --name=value or -n=value.
void ArgParser::parseEqualsOption(string prefix, string name, string value) {
    auto option = lookup(options, name);
    if (option) {
        if (!value.empty()) {
            option->values.push_back(value);
        } else {
            cerr << "Error: missing value for " << prefix << name << ".\n";
            exit(1);
        }
    } else {
        cerr << "Error: " << prefix << name << " is not a recognised option.\n";
        exit(1);
    }
}


// Parse a long-form option, i.e. an option beginning with a double dash.
void ArgParser::parseLongOption(string arg, ArgStream& stream) {
    size_t pos = arg.find("=");
    if (pos != string::npos) {
        parseEqualsOption("--", arg.substr(0, pos), arg.substr(pos + 1));
        return;
    }

    auto flag = lookup(flags, arg);
    if (flag) {
        flag->count++;
        return;
    }

    auto option = lookup(options, arg);
    if (option) {
        if (stream.hasNext()) {
            option->values.push_back(stream.next());
            return;
        } else {
            cerr << "Error: missing argument for --" << arg << ".\n";
            exit(1);
        }
    }

    if (arg == "help" && !helptext.empty()) {
        exitHelp();
    }

    if (arg == "version" && !version.empty()) {
        exitVersion();
    }

    cerr << "Error: --" << arg << " is not a recognised flag or option.\n";
    exit(1);
}


// Parse a short-form option, i.e. an option beginning with a single dash.
void ArgParser::parseShortOption(string arg, ArgStream& stream) {
    size_t pos = arg.find("=");
    if (pos != string::npos) {
        parseEqualsOption("-", arg.substr(0, pos), arg.substr(pos + 1));
        return;
    }

    for (char& c: arg) {
        string name = string(1, c);

        auto flag = lookup(flags, name);
        if (flag) {
            flag->count++;
            continue;
        }

        auto option = lookup(options, name);
        if (option) {
            if (stream.hasNext()) {
                option->values.push_back(stream.next());
                continue;
            } else {
                if (arg.size() > 1) {
                    cerr << "Error: missing argument for '" << c << "' in -" << arg << ".\n";
                } else {
                    cerr << "Error: missing argument for -" << c << ".\n";
                }
                exit(1);
            }
        }

        if (c == 'h' && !helptext.empty()) {
            exitHelp();
        }

        if (c == 'v' && !version.empty()) {
            exitVersion();
        }

        if (arg.size() > 1) {
            cerr << "Error: '" << c << "' in -" << arg << " is not a recognised flag or option.\n";
        } else {
            cerr << "Error: -" << c << " is not a recognised flag or option.\n";
        }
        exit(1);
    }
}


// Parse a stream of string arguments.
void ArgParser::parse(ArgStream& stream) {
    bool is_first_arg = true;

    while (stream.hasNext()) {
        string arg = stream.next();

        // If we enounter a '--', turn off option parsing.
        if (arg == "--") {
            while (stream.hasNext()) {
                args.push_back(stream.next());
            }
            continue;
        }

        // Is the argument a long-form option or flag?
        if (arg.compare(0, 2, "--") == 0) {
            parseLongOption(arg.substr(2), stream);
            continue;
        }

        // Is the argument a short-form option or flag? If the argument
        // consists of a single dash or a dash followed by a digit, we treat
        // it as a positional argument.
        if (arg[0] == '-') {
            if (arg.size() == 1 || isdigit(arg[1])) {
                args.push_back(arg);
            } else {
                parseShortOption(arg.substr(1), stream);
            }
            continue;
        }

        // Is the argument a registered command?
        ArgParser* command_parser = is_first_arg ? lookup(commands, arg) : nullptr;
        if (command_parser) {
            command_name = arg;
            command_parser->parse(stream);
            if (command_parser->callback != nullptr) {
                command_parser->callback(arg, *command_parser);
            }
            continue;
        }

        // Is the argument the automatic 'help' command?
        if (is_first_arg && arg == "help" && !commands.empty()) {
            if (stream.hasNext()) {
                string name = stream.next();
                command_parser = lookup(commands, name);
                if (!command_parser) {
                    cerr << "Error: '" << name << "' is not a recognised command.\n";
                    exit(1);
                } else {
                    command_parser->exitHelp();
                }
            } else {
                cerr << "Error: the help command requires an argument.\n";
                exit(1);
            }
        }

        // Otherwise add the argument to our list of positional arguments.
        args.push_back(arg);
        is_first_arg = false;
    }
}


// Parse an array of string arguments. We assume that [argc] and [argv] are the
// original parameters passed to main() and skip the first element. In some
// situations [argv] can be empty, i.e. [argc == 0]. This can lead to security
// vulnerabilities if not handled explicitly.
void ArgParser::parse(int argc, char **argv) {
    if (argc > 1) {
        ArgStream stream;
        for (int i = 1; i < argc; i++) {
            stream.append(argv[i]);
        }
        parse(stream);
    }
}


// Parse a vector of string arguments.
void ArgParser::parse(vector<string> args) {
    ArgStream stream;
    for (string& arg: args) {
        stream.append(arg);
    }
    parse(stream);
}


// -----------------------------------------------------------------------------
// ArgParser: utilities.
// -----------------------------------------------------------------------------


// Override the << stream insertion operator to support vectors. This will
// allow us to cout our lists of option values in the print() method.
template<typename T>
static ostream& operator<<(ostream& stream, const vector<T>& vec) {
    stream << "[";
    for(size_t i = 0; i < vec.size(); ++i) {
        if (i) stream << ", ";
        stream << vec[i];
    }
    stream << "]";
    return stream;
}


// Dump the parser's state to stdout.
void ArgParser::print() const {
    cout << "Options:\n";
    if (!options.empty()) {
        for (auto element: options) {
            cout << "  " << element.first << ": ";
            Option *option = element.second.get();
            cout << "(" << option->fallback << ") ";
            cout << option->values;
            cout << "\n";
        }
    } else {
        cout << "  [none]\n";
    }

    cout << "\nFlags:\n";
    if (!flags.empty()) {
        for (auto element: flags) {
            cout << "  " << element.first << ": " << element.second->count << "\n";
        }
    } else {
        cout << "  [none]\n";
    }

    cout << "\nArguments:\n";
    if (!args.empty()) {
        for (auto arg: args) {
            cout << "  " << arg << "\n";
        }
    } else {
        cout << "  [none]\n";
    }

    cout << "\nCommand:\n";
    if (commandFound()) {
        cout << "  " << command_name << "\n";
    } else {
        cout << "  [none]\n";
    }
}


// Print the parser's help text and exit.
void ArgParser::exitHelp() {
    cout << helptext << endl;
    exit(0);
}


// Print the parser's version string and exit.
void ArgParser::exitVersion() {
    cout << version << endl;
    exit(0);
}

