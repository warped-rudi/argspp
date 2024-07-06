// -----------------------------------------------------------------------------
// Args++: an argument-parsing library in portable C++11.
// -----------------------------------------------------------------------------

#include "args.h"

#include <iostream>
#include <algorithm>

using namespace std;
using namespace args;


// -----------------------------------------------------------------------------
// Flags and Options.
// -----------------------------------------------------------------------------


struct ArgParser::Flag {
    size_t count;
    string hinttext;
    Flag(string const& hint) : count(0), hinttext(hint) {}
};


struct ArgParser::Option {
    vector<string> values;
    string fallback;
    string hinttext;
    Option(string const& defVal, string const& hint)
        : fallback(defVal), hinttext(hint) {}
};


// -----------------------------------------------------------------------------
// ArgStream.
// -----------------------------------------------------------------------------


struct ArgParser::ArgStream {
    list<string> args;

    template<typename IteratorT>
    ArgStream(IteratorT begin, IteratorT end) {
        for ( ; begin != end; ++begin) {
            args.push_back(*begin);
        }
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


void ArgParser::flag(string const& name, string const& hint) {
    auto flag = make_shared<Flag>(hint);
    istringstream stream(name);
    string alias;
    while (stream >> alias) {
        flags[alias] = flag;
    }
}


void ArgParser::option(string const& name, string const& fallback, string const& hint) {
    auto option = make_shared<Option>(fallback, hint);
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


bool ArgParser::SizeCheck::isValid(size_t number) const {
    return (mode == no_check) ||
           (mode == check_eq && number == size) ||
           (mode == check_ge && number >= size);
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


string const& ArgParser::arg(size_t index) const {
    if (index < pos_args.size()) {
        return pos_args[index];
    }
    static const string empty;
    return empty;
}


// -----------------------------------------------------------------------------
// ArgParser: commands.
// -----------------------------------------------------------------------------


ArgParser& ArgParser::command(string const& name,
                              string const& helpmsg,
                              Callback callbackFn, string const& hint) {
    auto parser = make_shared<ArgParser>(helpmsg);
    parser->callback = callbackFn;
    parser->hinttext = hint;

    if (!octx) {
        octx.reset(new OutputContext());
    }
    parser->octx = octx;

    istringstream stream(name);
    string alias;

    while (stream >> alias) {
        commands[alias] = parser;
    }

    argcount.size = 0;
    argcount.mode = SizeCheck::check_eq;
    return *parser;
}


bool ArgParser::commandFound() const {
    return !command_name.empty();
}


string const& ArgParser::commandName() const {
    return command_name;
}


ArgParser const& ArgParser::commandParser() const {
    auto parser = lookup(commands, command_name);
    return parser ? *parser : *this;
}


// -----------------------------------------------------------------------------
// ArgParser: parse arguments.
// -----------------------------------------------------------------------------


// Parse an option of the form --name=value or -n=value.
void ArgParser::parseEqualsOption(string const& prefix,
                                  string const& name, string const& value) {
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
void ArgParser::parseLongOption(string const& arg, ArgStream& stream) {
    size_t pos = arg.find('=');
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
void ArgParser::parseShortOption(string const& arg, ArgStream& stream) {
    size_t pos = arg.find('=');
    if (pos != string::npos) {
        parseEqualsOption("-", arg.substr(0, pos), arg.substr(pos + 1));
        return;
    }

    for (char c: arg) {
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
                pos_args.push_back(stream.next());
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
                pos_args.push_back(arg);
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
        pos_args.push_back(arg);
        is_first_arg = false;
    }

    if (!argcount.isValid(pos_args.size())) {
        octx->buf << "Error: invalid number of arguments.\n";
        exitError();
    }
}


// Parse an array of string arguments. We assume that [argc] and [argv] are the
// original parameters passed to main() and skip the first element. In some
// situations [argv] can be empty, i.e. [argc == 0]. This can lead to security
// vulnerabilities if not handled explicitly.
void ArgParser::parse(int argc, char **argv) {
    if (argc > 1) {
        ArgStream stream(&argv[1], &argv[argc]);
        parse(stream);
    }
}


// Parse a vector of string arguments.
void ArgParser::parse(vector<string> const& argv) {
    ArgStream stream(argv.begin(), argv.end());
    parse(stream);
}


// -----------------------------------------------------------------------------
// ArgParser: utilities.
// -----------------------------------------------------------------------------


// Override the << stream insertion operator to support vectors. This will
// allow us to cout our lists of option values in the print() method.
template<typename T>
static ostream& operator<<(ostream& stream, vector<T> const& vec) {
    stream << '[';
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i)
            stream << ", ";
        stream << vec[i];
    }
    stream << ']';
    return stream;
}


// Dump the parser's state to stdout.
void ArgParser::print() const {
    cout << "Options:\n";
    if (!options.empty()) {
        for (auto& element: options) {
            const Option* option = element.second.get();
            cout << "  " << element.first << ": (" << option->fallback << ") ";
            cout << option->values << '\n';
        }
    } else {
        cout << "  [none]\n";
    }

    cout << "\nFlags:\n";
    if (!flags.empty()) {
        for (auto& element: flags) {
            cout << "  " << element.first << ": " << element.second->count << "\n";
        }
    } else {
        cout << "  [none]\n";
    }

    cout << "\nArguments:\n";
    if (!pos_args.empty()) {
        for (auto& arg: pos_args) {
            cout << "  " << arg << '\n';
        }
    } else {
        cout << "  [none]\n";
    }

    cout << "\nCommand:\n";
    if (commandFound()) {
        cout << "  " << command_name << '\n';
    } else {
        cout << "  [none]\n";
    }
}


// Extract unique hint messages from a container.
template<typename SmartPtrT>
void ArgParser::collectHints(map<string, SmartPtrT> const& cnr,
                             size_t& width, list<HintItem>& hints)
{
    using ObjectType = typename SmartPtrT::element_type;

    for (auto& element : cnr ) {
        string const* msg = &element.second->hinttext;

        if (!msg->empty()) {
            string name = element.first;
            if (!is_same<ObjectType, ArgParser>::value) {
                name.insert(0, name.length() == 1 ? 1 : 2, '-');

                if (is_same<ObjectType, Option>::value)
                    name.append(name[1] == '-' ? "=<arg>" : " <arg>");
            }

            auto it = find_if(hints.begin(), hints.end(),
                              [msg](HintItem const& hi)
            {
                return hi.first == msg;
            });

            if (it == hints.end()) {
                it = find_if(hints.begin(), hints.end(),
                             [&name](HintItem const& hi)
                {
                    return hi.second > name;
                });

                it = hints.emplace(it, msg, name);
            } else {
                it->second.append(", ").append(name);
            }

            width = max(width, it->second.length());
        }
    };
}


// Helper function to print out collected help messages.
void ArgParser::printHints(ostream& os, char const* tag,
                           size_t width, list<HintItem> const& hints) {
    if(!hints.empty()) {
        os << tag;

        for (auto& item : hints ) {
            size_t pos = 0;
            size_t padLen = item.second.length() < width ?
                                width - item.second.length() : 0;
            const string& text = *item.first;

            do {
                size_t next = text.find('\n', pos);
                if (next == string::npos)
                    next = text.size();

                if (pos == 0)
                    os << "  " << item.second;

                os << string(padLen, ' ')
                   << text.substr(pos, next - pos) << '\n';

                padLen = width + 2;
                pos = next + 1;
            } while (pos < text.size());
        };
    }
}


// Print the parser's help text and exit.
void ArgParser::exitHelp() {
    size_t width = 0;
    list<HintItem> hints;

    octx->buf << helptext << '\n';

    if (!commands.empty()) {
        collectHints(commands, width, hints);
        printHints(octx->buf, "\nCommands:\n", width + 2, hints);
        hints.clear();
        width = 0;
    }

    map<string, shared_ptr<Flag> > builtIns;

    if (!version.empty()) {
        static const char *names[] = { "v", "version" };
        auto flag = make_shared<Flag>("Show program version");

        for (auto it = begin(names); it != end(names); ++it) {
            if (!lookup(flags, *it) && !lookup(options, *it)) {
                builtIns[*it] = flag;
            }
        }
    }

    if (!helptext.empty()) {
        static const char *names[] = { "h", "help" };
        auto flag = make_shared<Flag>("Show this help text");

        for (auto it = begin(names); it != end(names); ++it) {
            if (!lookup(flags, *it) && !lookup(options, *it)) {
                builtIns[*it] = flag;
            }
        }
    }

    collectHints(flags, width, hints);
    collectHints(options, width, hints);
    collectHints(builtIns, width, hints);
    printHints(octx->buf, "\nOptions:\n", width + 2, hints);

    octx->buf << '\n';
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

