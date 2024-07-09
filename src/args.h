// -----------------------------------------------------------------------------
// Args++: an argument-parsing library in portable C++11.
//
// Author: Darren Mulholland <dmulholl@tcd.ie>
// License: Public Domain
// Version: 2.1.0
// -----------------------------------------------------------------------------

#ifndef args_h
#define args_h

#include <map>
#include <list>
#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace args {

    class ArgParser {
        struct Flag;
        struct Option;
        struct ArgStream;

        struct SizeCheck {
            size_t size;
            enum { no_check, check_eq, check_ge } mode;
            bool isValid(size_t number) const;
        };

        struct OutputContext {
            std::ostringstream buf;
            virtual ~OutputContext() {}
            virtual int flush(bool is_error);
        };

        using Callback = void (*)(std::string const& cmd_name,
                                  ArgParser const& cmd_parser);

        public:
            ArgParser(
                std::string const& helpmsg = std::string(),
                std::string const& version = std::string()
            ) : helptext(helpmsg), version(version), callback(nullptr), argcount() {}

            ArgParser(ArgParser const&) = delete;
            ArgParser& operator = (ArgParser const&) = delete;

            // Redirect help and error messages.
            template<typename ClosureT>
            void setOutput(ClosureT const& c) {
                struct OutputContextEx : OutputContext, ClosureT {
                    OutputContextEx(ClosureT const& c) : ClosureT(c) {}
                    virtual int flush(bool is_error) {
                        return this->operator()(buf.str(), is_error);
                    }
                };

                if (!octx)
                    octx.reset(new OutputContextEx(c));
            }

            // Set required number of positional arguments.
            void setArgsRequired(size_t count, bool accept_more = false) {
                argcount.size = count;
                argcount.mode = accept_more ? SizeCheck::check_ge :
                                                SizeCheck::check_eq;
            }

            // Register flags and options.
            void flag(std::string const& name,
                      std::string const& hint = std::string());
            void option(std::string const& name,
                        std::string const& fallback = std::string(),
                        std::string const& hint = std::string());

            // Parse the application's command line arguments.
            void parse(int argc, char **argv);
            void parse(std::vector<std::string> const& argv);

            // Retrieve flag and option values.
            bool found(std::string const& name) const {
                return count(name) > 0;
            }
            size_t count(std::string const& name) const;
            std::string const& value(std::string const& name) const;
            std::vector<std::string> const& values(std::string const& name) const;
            std::string const& operator[](std::string const& name) const {
                return value(name);
            }

            // Retrieve positional arguments.
            std::string const& arg(size_t index) const;
            std::vector<std::string> const& args() const {
                return pos_args;
            }
            std::string const& operator[](size_t index) const {
                return arg(index);
            }

            // Register a command. Returns the command's ArgParser instance.
            ArgParser& command(
                std::string const& name,
                std::string const& helpmsg = std::string(),
                Callback callbackFn = nullptr,
                std::string const& hint = std::string()
            );

            // Utilities for handling commands manually.
            bool commandFound() const {
                return !command_name.empty();
            }
            std::string const& commandName() const {
                return command_name;
            }
            ArgParser const& commandParser() const {
                auto parser = lookup(commands, command_name);
                return parser ? *parser : *this;
            }

            // Print a parser instance to stdout.
            void print() const;

        private:
            std::map<std::string, std::shared_ptr<Option> > options;
            std::map<std::string, std::shared_ptr<Flag> > flags;
            std::map<std::string, std::shared_ptr<ArgParser> > commands;

            template<typename SmartPtrT>
            static typename SmartPtrT::element_type *lookup(
                    std::map<std::string, SmartPtrT> const& cnr, std::string const& key) {
                auto it = cnr.find( key );
                return it != cnr.end() ? it->second.get() : nullptr;
            }

            using HintItem = std::pair<std::string const*, std::string>;

            template<typename SmartPtrT>
            static void collectHints(std::map<std::string, SmartPtrT> const& cnr,
                                     size_t& width, std::list<HintItem>& hints);
            static void printHints(std::ostream& os, char const* tag,
                                   size_t width, std::list<HintItem> const& hints);

            void parse(ArgStream& stream);
            void parseLongOption(std::string const& arg, ArgStream& stream);
            void parseShortOption(std::string const& arg, ArgStream& stream);
            void parseEqualsOption(char const* prefix,
                                   std::string const& name, std::string const& value);

            [[noreturn]] void exitHelp();
            [[noreturn]] void exitVersion();
            [[noreturn]] void exitError();

            std::string command_name;

            // Application/command help text and version strings.
            std::string helptext;
            std::string version;
            std::string hinttext;

            // Callback function for command parsers.
            Callback    callback;

            // Message buffer.
            std::shared_ptr<OutputContext> octx;

            // Positional argument checking.
            SizeCheck   argcount;

            // Stores positional arguments.
            std::vector<std::string> pos_args;
    };
}

#endif
