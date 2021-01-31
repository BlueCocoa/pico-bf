#include <stdio.h>
#include "pico/stdlib.h"
#include <cstring>
#include <string>
#include <sstream>
#include <array>
#include <map>
#include <optional>
#include <stack>
#include <variant>
#include <vector>
#include "peko.h"

/// brainfuck virtual machine tape length
#define BRAINFUCK_VM_TAPE_LEN 30000

// https://schneide.blog/2018/01/11/c17-the-two-line-visitor-explained/
template<class... Ts> struct brainfuck_vm : Ts... { using Ts::operator()...; };
template<class... Ts> brainfuck_vm(Ts...) -> brainfuck_vm<Ts...>;

#pragma mark - brainfuck ops

struct increment_value_op {}; // +
struct decrement_value_op {}; // -

struct increment_ptr_op {};   // >
struct decrement_ptr_op {};   // <

struct print_op {};           // ,
struct read_op {};            // .

struct loop_start_op {};      // [
struct loop_end_op {};        // ]

/// brainfuck_op allowed ops in C++17 std::variant
using brainfuck_op = std::variant<
    increment_value_op,
    decrement_value_op,
    increment_ptr_op,
    decrement_ptr_op,
    print_op,
    read_op,
    loop_start_op,
    loop_end_op,
    std::monostate
>;

/// a map from char to brainfuck_op
const std::map<char, brainfuck_op> bf_op_map {
    {'+', increment_value_op{}},
    {'-', decrement_value_op{}},
    {'>', increment_ptr_op{}},
    {'<', decrement_ptr_op{}},
    {'.', print_op{}},
    {',', read_op{}},
    {'[', loop_start_op{}},
    {']', loop_end_op{}}
};

#pragma mark - brainfuck vm

/// brainfuck virtual machine status
struct brainfuck_vm_status {
    /// virtual infinity length tape
    std::map<int, char> tape;
    /// current cell of the tape
    int tape_ptr = 0;

    /// used for keeping track of all valid brainfuck_op
    std::vector<char> instruction;
    /// current brainfuck_op index
    int instruction_ptr_current = -1;
    /// keeping track of loops
    std::stack<int> instruction_loop_ptr;

    /// flag of skipping loop, e.g
    /// +-[[[------------++++++++++-.>>[>]>>>--<<<<<<--]]]++++
    ///   ^skipping from, but we need all                ^end of skipping
    ///      instructions inside.
    int jump_loop = 0;
};

#pragma mark - helper function

/**
 next brainfuck op

 @param status   the brainfuck vm status
 @param char_op  character form op
 @param via_loop due to the way I wrote, a flag is needed to avoid re-adding ops
 @return brainfuck_op
*/
brainfuck_op next_op(brainfuck_vm_status & status, char char_op, bool via_loop = false) {
    // find the brainfuck_op from bf_op_map
    if (auto op = bf_op_map.find(char_op); op != bf_op_map.end()) {
        // do not append the char_op if we're retriving the next op inside a loop_op
        if (!via_loop) {
            // save char_op to instruction
            status.instruction.emplace_back(char_op);
            // increse the ptr of current instruction
            status.instruction_ptr_current++;
        }

        // return next op
        return op->second;
    } else {
        // invaild char for brainfuck
        // monostate is returned
        return std::monostate();
    }
}

#pragma mark - brainfuck vm interpreter

/**
 run brainfuck vm

 @param status   run brainfuck vm from the given state
 @param char_op  character form op
 @param via_loop due to the way I wrote, a flag is needed to avoid re-adding ops
*/
void run_vm(brainfuck_vm_status & status, char char_op, bool via_loop = false) {
    // get the op from char_op
    brainfuck_op op = next_op(status, char_op, via_loop);

    // parttern matching
    std::visit(brainfuck_vm {
        [&](increment_value_op) {
            // printf("increment_value_op\n");
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                status.tape[status.tape_ptr]++;
            }
        },
        [&](decrement_value_op) {
            // printf("decrement_value_op\n");
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                status.tape[status.tape_ptr]--;
            }
        },
        [&](increment_ptr_op) {
            // printf("increment_ptr_op\n");
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                status.tape_ptr++;
            }
        },
        [&](decrement_ptr_op) {
            // printf("decrement_ptr_op\n");
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                status.tape_ptr--;
            }
        },
        [&](print_op) {
            // printf("print_op\n");
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                putchar(status.tape[status.tape_ptr]);
                // printf("%c - %d\n", status.tape[status.tape_ptr], status.tape[status.tape_ptr]);
            }
        },
        [&](read_op) {
            // skip actual action if we're skipping loop
            if (status.jump_loop == 0) {
                status.tape[status.tape_ptr] = getchar();
            }
        },
        [&](loop_start_op) {
            // printf("loop from ins ptr: %d cond[%d]\n", status.instruction_ptr_current, status.tape[status.tape_ptr]);
            // if and only if 1) `current_cell_value != 0`
            //                2) and we're not do the skipping
            // we can record the starting index of the if instruction
            // besides, if we're in condition 1)
            // the if statement should be also skipped
            if (status.tape[status.tape_ptr] != 0 && status.jump_loop == 0) {
                // push the starting instruction index of loop
                status.instruction_loop_ptr.emplace(status.instruction_ptr_current);
            } else {
                status.jump_loop++;
            }
        },
        [&](loop_end_op) {
            // printf("loop end ins ptr: %d cond[%d]\n", status.instruction_ptr_current, status.tape[status.tape_ptr]);
            // decrease the jump_loop value if we encounter the `]`
            // and we were previously doing the skip
            if (status.jump_loop != 0) {
                status.jump_loop--;
            } else {
                // if we were not in skipping
                // then we need to check the loop condition, `current_cell_value != 0`
                if (status.tape[status.tape_ptr] != 0) {
                    // the instruction range of current loop
                    // printf("loop [%d, %d]:\n", status.instruction_loop_ptr.top(), status.instruction_ptr_current);
#ifdef DEBUG
                    // dump all instructions inside the loop
                    // for (int debug = status.instruction_loop_ptr.top(); debug <= status.instruction_ptr_current; debug++) {
                        // putchar(status.instruction[debug]);
                    //}
                    //putchar('\n');
#endif

                    // loop the instruction until condition satisfies no more
                    while (status.tape[status.tape_ptr] != 0) {
                        // save current instruction pointer
                        int current = status.instruction_ptr_current;
                        // start the loop right after the index of `[`
                        status.instruction_ptr_current = status.instruction_loop_ptr.top() + 1;
                        // run one op at a time
                        // until the next op is the corresponding `]`
                        while (status.instruction_ptr_current < current) {
                            run_vm(status, status.instruction[status.instruction_ptr_current], true);
                            status.instruction_ptr_current++;
                        }
                        // restore the current instruction pointer
                        status.instruction_ptr_current = current;
                    }

                    // pop current loop starting index
                    status.instruction_loop_ptr.pop();
                }
            }
        },
        [&](std::monostate) {
        }
    }, op);
}

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
            input.str("");
            input << s;
            continue;
        } else {
            input << c;
        }
        printf("\r%s %s", prompt, input.str().c_str());
    }
}

int run_bf(const char * run, bool print_run) {
    const char * prompt = ">>>";
    // the brainfuck vm
    brainfuck_vm_status status;
    if (run != nullptr) {
        if (print_run) {
            printf("%s\n\n", run);
        }
        for (size_t i = 0; i < strlen(run); i++) {
            // interpret
            run_vm(status, run[i]);
        }
        printf("\n");
        return 1;
    }
    while (true) {
        std::string input = getline(prompt);
        printf("\n");
        if (input == "reset") {
            return 1;
        } else if (input == "example") {
            return 2;
        } else if (input == "peko") {
            return 3;
        }
        for (size_t i = 0; i < input.length(); i++) {
            // interpret
            run_vm(status, input[i]);
        }
        printf("\n");
    }
    return 0;
}

int main() {
    stdio_init_all();
    while (true) {
        int ret = run_bf(nullptr, false);
        if (ret == 1) {
            printf("\nPicoBf by Cocoa v0.0.1\n  type reset to clear vm states\n  type example to see an example\n  type peko to peko!\n\n");
        } else if (ret == 2) {
            run_bf("+++++ +++[- >++++ ++++< ]>+++ +++++ +++++ +++.< +++++ [->++ +++<] >.---"\
"---.< +++[- >+++< ]>+++ .<+++ +++[- >---- --<]> ----- ----- --.<+ +++[-"\
">++++ <]>+. <++++ [->++ ++<]> +++++ .++++ ++.++ ++.<+ +++++ ++[-> -----"\
"---<] >---- ----- ----- .<+++ +++++ +++++ [->++ +++++ +++++ +<]>+ +++++"\
"+++++ +++++ +++++ ++++. <++++ +++++ [->-- ----- --<]> ----- ----- -----"\
"--.<+ +++++ +[->+ +++++ +<]>+ +++++ ++.<+ +++++ [->++ ++++< ]>+++ ++.<+"\
"+++++ +++[- >---- ----- <]>-- ----- ----- ----- .<+++ +[->+ +++<] >++.<"\
"+++++ +++[- >++++ ++++< ]>+++ +++++ +++++ +++.< +++++ ++++[ ->--- -----"\
"-<]>- ----- ----- ----- -.<++ +++++ [->++ +++++ <]>++ +++++ +.<++ ++++["\
"->+++ +++<] >++++ +.<++ +++++ ++[-> ----- ----< ]>--- ----- ----- ----."\
"<++++ [->++ ++<]> ++.<+ +++++ ++[-> +++++ +++<] >++++ +++++ +++++ ++.<+"\
"+++++ +++[- >---- ----- <]>-- ----- ----- ----- .<+++ ++++[ ->+++ ++++<"\
"]>+++ +++++ .<+++ +++[- >++++ ++<]> +++++ .<+++ +++++ +[->- ----- ---<]"\
">---- ----- ----- ---.< ++++[ ->+++ +<]>+ +.<++ +++++ ++[-> +++++ ++++<"\
"]>+++ +++++ +++.< +++++ ++[-> ----- --<]> --.<+ +++++ +[->- ----- -<]>-"\
"----- ----. <", true);
        } else if (ret == 3) {
            run_bf(peko, false);
        }
    }
}

