#include <iostream>
#include "args.h"

using namespace args;
using namespace std;

int main(int argc, char **argv) {
    ArgParser parser("Usage: example command [Options]", "1.0");

    parser.setOutput([](string const& msg, bool is_error) -> int {
        char const* hilite = is_error ? "\x1b[1;31m" : "\x1b[1;32m";
        cout << hilite << msg << "\x1b[0m";
        return is_error ? 42 : 0;
    });

    ArgParser& cmd_boo = parser.command("boo", "Usage: example boo [Options]", nullptr, "This is command boo");
    cmd_boo.flag("foo f", "Enable feature foo");
    cmd_boo.option("bar b", "default", "Specify the value of bar");

    ArgParser& cmd_zoo = parser.command("zoo", "Usage: example zoo [Options] arg1 ...", nullptr, "This is command zoo");
    cmd_zoo.flag("foz f", "Enable feature foz\nwhich is really, really nice");
    cmd_zoo.option("baz b", "default", "Specify the value of baz");
    cmd_zoo.setArgsRequired(1, true);

    parser.parse(argc, argv);
    parser.commandParser().print();
}
