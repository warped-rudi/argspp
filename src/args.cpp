// -----------------------------------------------------------------------------
// Args++: an argument-parsing library in portable C++11.
// -----------------------------------------------------------------------------

#include "args.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <deque>

using namespace std;
using namespace args;


// -----------------------------------------------------------------------------
// Flags and Options.
// -----------------------------------------------------------------------------


struct ArgParser::Flag {
    size_t count = 0;
};


struct ArgParser::Option {
    vector<string> values;
    string fallback;
};


// -----------------------------------------------------------------------------
// ArgStream.
// -----------------------------------------------------------------------------


struct ArgParser::ArgStream {
    deque<string> args;

    void append(string const& arg) {
        args.push_back(arg);
    }

    string next() {
        string arg = args.front();
        args.pop_front();
        return arg;
    }

    bool hasNext() const {
        return !args.empty();
    }
};


// -----------------------------------------------------------------------------
// ArgParser: setup.
// -----------------------------------------------------------------------------


void ArgParser::flag(string const& name) {
    auto flag = make_shared<Flag>();
    istringstream stream(name);
    string alias;
    while (stream >> alias) {
        flags[alias] = flag;
    }
}


void ArgParser::option(string const& name, string const& fallback) {
    auto option = make_shared<Option>();
    option->fallback = fallback;
    istringstream stream(name);
    string alias;
    while (stream >> alias) {
        options[alias] = option;
    }
}


int ArgParser::OutputContext::flush(bool is_error) {
    std::ostream &strm = is_error ? std::cerr : std::cout;
    strm << buf.str();
    return static_cast<int>( is_error );
}


// -----------------------------------------------------------------------------
// ArgParser: retrieve values.
// -----------------------------------------------------------------------------


bool ArgParser::found(string const& name) const {
    return count(name) > 0;
}


size_t ArgParser::count(string const& name) const {
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


string const& ArgParser::value(string const& name) const {
    auto option = lookup(options, name);
    if (option) {
        if (!option->values.empty()) {
            return option->values.back();
        }
        return option->fallback;
    }
    static const string empty;
    return empty;
}


vector<string> const& ArgParser::values(string const& name) const {
    auto option = lookup(options, name);
    if (option) {
        return option->values;
    }
    static const vector<string> empty;
    return empty;
}


size_t ArgParser::count() const {
    return args.size();
}


string const& ArgParser::value(size_t index) const {
    if (index < args.size()) {
        return args[index];
    }
    static const string empty;
    return empty;
}


// -----------------------------------------------------------------------------
// ArgParser: commands.
// -----------------------------------------------------------------------------


ArgParser& ArgParser::command(
    string const& name, string const& helptext, Callback callback) {

    auto parser = make_shared<ArgParser>();
    parser->helptext = helptext;
    parser->callback = callback;

    if (!octx) {
        octx.reset(new OutputContext());
    }
    parser->octx = octx;

    istringstream stream(name);
    string alias;

    while (stream >> alias) {
        commands[alias] = parser;
    }

    return *parser;
}


bool ArgParser::commandFound() const {
    return !command_name.empty();
}


string const& ArgParser::commandName() const {
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
            octx->buf << "Error: missing value for " << prefix << name << ".\n";
            exitError();
        }
    } else {
        octx->buf << "Error: " << prefix << name << " is not a recognised option.\n";
        exitError();
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
            octx->buf << "Error: missing argument for --" << arg << ".\n";
            exitError();
        }
    }

    if (arg == "help" && !helptext.empty()) {
        exitHelp();
    }

    if (arg == "version" && !version.empty()) {
        exitVersion();
    }

    octx->buf << "Error: --" << arg << " is not a recognised flag or option.\n";
    exitError();
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
                    octx->buf << "Error: missing argument for '" << c << "' in -" << arg << ".\n";
                } else {
                    octx->buf << "Error: missing argument for -" << c << ".\n";
                }
                exitError();
            }
        }

        if (c == 'h' && !helptext.empty()) {
            exitHelp();
        }

        if (c == 'v' && !version.empty()) {
            exitVersion();
        }

        if (arg.size() > 1) {
            octx->buf << "Error: '" << c << "' in -" << arg << " is not a recognised flag or option.\n";
        } else {
            octx->buf << "Error: -" << c << " is not a recognised flag or option.\n";
        }
        exitError();
    }
}


// Parse a stream of string arguments.
void ArgParser::parse(ArgStream& stream) {
    bool is_first_arg = true;

    if (!octx) {
        octx.reset(new OutputContext());
    }

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
                    octx->buf << "Error: '" << name << "' is not a recognised command.\n";
                    exitError();
                } else {
                    command_parser->exitHelp();
                }
            } else {
                octx->buf << "Error: the help command requires an argument.\n";
                exitError();
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
void ArgParser::parse(vector<string> const& args) {
    ArgStream stream;
    for (auto& arg: args) {
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
    octx->buf << helptext << '\n';
    exit(octx->flush(false));
}


// Print the parser's version string and exit.
void ArgParser::exitVersion() {
    octx->buf << version << '\n';
    exit(octx->flush(false));
}


// Exit indicating an error.
void ArgParser::exitError() {
    exit(octx->flush(true));
}

