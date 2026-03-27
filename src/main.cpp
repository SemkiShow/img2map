// SPDX-FileCopyrightText: 2026 SemkiShow
//
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>

int main(int argc, char* argv[])
{
    std::cout << argc << " arguments provided\n";
    for (int i = 0; i < argc; i++)
    {
        std::cout << argv[i] << '\n';
    }
    std::cout << "Hello, World!\n";

    return 0;
}
