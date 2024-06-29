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
#include <string>
#include <vector>
#include <memory>

namespace args {

    struct ArgStream;
    struct Option;
    struct Flag;

    class ArgParser {
        using Callback = void (*)(std::string cmd_name, ArgParser& cmd_parser);

        public:
            ArgParser(
                std::string const& helptext = std::string(),
                std::string const& version = std::string()
            ) : helptext(helptext), version(version), callback(nullptr) {}

            // Stores positional arguments.
            std::vector<std::string> args;

            // Register flags and options.
            void flag(std::string const& name);
            void option(std::string const& name,
                        std::string const& fallback = std::string());

            // Parse the application's command line arguments.
            void parse(int argc, char **argv);
            void parse(std::vector<std::string> args);

            // Retrieve flag and option values.
            bool found(std::string const& name) const;
            int count(std::string const& name) const;
            std::string value(std::string const& name) const;
            std::vector<std::string> values(std::string const& name) const;

            // Register a command. Returns the command's ArgParser instance.
            ArgParser& command(
                std::string const& name,
                std::string const& helptext = std::string(),
                Callback callback = nullptr
            );

            // Utilities for handling commands manually.
            bool commandFound() const;
            std::string commandName() const;
            ArgParser& commandParser() const;

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

            void parse(ArgStream& args);
            void registerOption(std::string const& name, Option* option);
            void parseLongOption(std::string arg, ArgStream& stream);
            void parseShortOption(std::string arg, ArgStream& stream);
            void parseEqualsOption(std::string prefix, std::string name, std::string value);
            void exitHelp();
            void exitVersion();
            void exitError();

            std::string command_name;

            // Application/command help text and version strings.
            std::string helptext;
            std::string version;

            // Callback function for command parsers.
            Callback    callback;
    };
}

#endif
