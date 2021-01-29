#include <stdio.h>
#include "pico/stdlib.h"
#include <string>
#include <sstream>
#include <array>
#include <map>
#include <optional>
#include <stack>
#include <variant>
#include <vector>

std::string getline(const char * prompt) {
    std::stringstream input;
    printf("%s ", prompt);
    while (true) {
        char c = getchar();
        if (c == 13) {
            return input.str();
        } else if (c == 127) {
             std::string s = input.str();
             if (!s.empty()) {
                 s[s.length() - 1] = ' ';
                 printf("\r%s %s", prompt, s.c_str());
                 s.erase(std::prev(s.end()));
                 printf("\r%s %s", prompt, s.c_str());
            }
            input.clear();
            input.str(s);
            continue;
        } else {
            input << c;
        }
        printf("\r%s %s", prompt, input.str().c_str());
    }
}

int main() {
    stdio_init_all();
    const char * prompt = ">>>";
    while (true) {
    std::string input = getline(prompt);
    printf("\n: %s\n", input.c_str());
    }
    return 0;
}

