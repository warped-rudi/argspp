#include <iostream>
#include "args.h"

using namespace args;
using namespace std;

void callback(string const& cmd_name, ArgParser const& cmd_parser) {
    cout << "---------- boo! ---------- (" << cmd_name << ")\n";
    cmd_parser.print();
    cout << "--------------------------\n\n";
}

int main(int argc, char **argv) {
    ArgParser parser("Usage: example...", "1.0");

    ArgParser& cmd_parser = parser.command("boo", "Usage: example boo...", callback);
    cmd_parser.flag("foo f");
    cmd_parser.option("bar b", "default");

    parser.parse(argc, argv);
    parser.print();
}
