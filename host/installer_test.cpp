// SPDX-License-Identifier: GPL-3.0-or-later
#include "hub/installer.hpp"
#include <iostream>
int main(int argc, char **argv)
{
    hub::InstallerResult r;
    if (argc == 2 && std::string(argv[1]) == "probe")
        r = hub::probe_installer();
    else if (argc == 4)
    {
        hub::Entry e{"local-test",
                     "Synthetic test",
                     "01.000.000",
                     "pkg",
                     "https://example.org/test.pkg",
                     argv[3],
                     "Test"};
        e.platform = "PS4";
        r = hub::install_package(e, argv[2], [] { return false; }, {});
    }
    else
        return 2;
    std::cout << r.available << ' ' << r.accepted << ' ' << r.uncertain << '\n' << r.message;
    return 0;
}
