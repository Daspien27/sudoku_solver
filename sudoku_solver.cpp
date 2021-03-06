// sudoku_solver.cpp : Defines the entry point for the application.
//

#include "sudoku_solver.h"

#include <string_view>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <cassert>
#include <bit>
#include <fstream>
#include <chrono>
#include <vector>
#include <variant>

const float GRID_SIZE = 50.0f;
const float MARGIN = 20.0f;
const float LINE_THICKNESS = 3.0f;

sudoku_render::sudoku_render() {
    if (!font.loadFromFile(R"(data\arial.ttf)")) {
        throw std::runtime_error("could not locate arial");
    }
}

void sudoku_render::render(sf::RenderWindow& window, sudoku& s) {
    sf::Text text;
    text.setFont(font);
    text.setFillColor(sf::Color::Black);

    constexpr std::array<const char*, 10> digits = { " ", "1", "2","3","4","5","6","7","8", "9" };

    const auto draw_annotation = [&](int i, int j) {
        auto annotations = s.annotations_[9 * j + i];
        
        for (int a = 0; a < 9; ++a) {
            if (annotations & 1 << a) {
                text.setCharacterSize(GRID_SIZE / 4);
                text.setString(digits[a+1]);
                text.setPosition(sf::Vector2f{ i * GRID_SIZE + MARGIN + GRID_SIZE / 4.0f + (a % 3) * 12.0f, j * GRID_SIZE + MARGIN + 6.0f + (a / 3) * 12.0f });
                window.draw(text);
            }
        }
    };

    const auto draw_cell = [&](int i, int j) {
        text.setCharacterSize(GRID_SIZE / 2);
        text.setString(digits[s.grid_[9 * j + i]]);
        text.setPosition(sf::Vector2f{ i * GRID_SIZE + MARGIN + GRID_SIZE / 2.6f, j * GRID_SIZE + MARGIN + 6.0f });
        window.draw(text);
    };


    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {
            auto cell = s.grid_[9 * j + i];

            if (cell == 0) {
                draw_annotation(i, j);
            }
            else {
                draw_cell(i, j);
            }
        }
    }
}

sudoku::sudoku(std::array<int, 9 * 9> grid) : grid_(grid){
    load_annotate();
}

template <typename T>
auto memoize(auto fn) {
//Credits code_report: https://www.youtube.com/watch?v=aIOMRqiwziM&t=711s
    return[done = std::map <T, decltype(fn(T{}))> {}, fn] (T arg) mutable -> auto& {
        if (auto it = done.find(arg); it != done.end()) {
            return it->second;
        }
        return done[arg] = fn(arg);
    };
}

auto row(int n) {
    const auto row_list = [](int n) {
        std::array<int, 9> row_l;
        for (int i = 0; i < 9; i++) {
            row_l[i] = i + n * 9;
        }
        return row_l;
    };

    static auto memoize_row = memoize<int>(row_list);

    return memoize_row(n);
}

auto column(int n) {
    const auto col_list = [](int n) {
        std::array<int, 9> col_l;
        for (int i = 0; i < 9; i++) {
            col_l[i] = n + i * 9;
        }
        return col_l;
    };

    static auto memoize_col = memoize<int>(col_list);

    return memoize_col(n);
}

auto box(int n) {
    const auto box_list = [](int n) {
        std::array<int, 9> box_l;
        
        for (int c = 0; c < 9; ++c) {
            int i = (n / 3) * 3 + c / 3;
            int j = (n % 3) * 3 + c % 3;

            box_l[c] = 9 * i + j;
        }

        return box_l;
    };

    static auto memoize_box = memoize<int>(box_list);

    return memoize_box(n);
}

auto sudoku::annotation (auto fn) const {
    return [this, fn](int n) {
        int a = 0b111111111;
        auto idxs = fn(n);
        for (int idx : idxs) {
            if (grid_[idx]) {
                a ^= 1 << (grid_[idx] - 1);
            }
        }
        return a;
    };
}

int sudoku::column_annotation(int col) const {
    return annotation(column)(col);
}

int sudoku::row_annotation(int r) const {
    return annotation(row)(r);
}

int sudoku::box_annotation(int b) const {
    return annotation(box)(b);
}

void sudoku::load_annotate() {
    std::fill(annotations_.begin(), annotations_.end(), 0b111111111);

    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {
            auto& annotation = annotations_[9 * i + j];
            
            if (grid_[9 * i + j] == 0) {
                annotation &= column_annotation(j);
                annotation &= row_annotation(i);
                int box = 3 * (i / 3) + j / 3;
                annotation &= box_annotation(box);
            }
            else {
                annotation = 1 << (grid_[9 * i + j] - 1);
            }
        }
    }
}

void sudoku::solve_naked_singles() {
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {
            const auto& annotation = annotations_[9 * i + j];
            auto& cell = grid_[9 * i + j];

            if (cell == 0 && annotation && (annotation & (annotation - 1)) == 0) {
                //single power of 2
                int a = annotation;
                int i = 1;
                while (a != 1) { a >>= 1; ++i; }
                cell = i;
            }
        }
    }
}

void sudoku::solve_hidden_singles() {
    auto is_candidate = [this](int idx, int n) {
        return (grid_[idx] == 0 && (annotations_[idx] & 1 << n)) || grid_[idx] == n;
    };

    auto hidden_singles = [&](auto unit) {
        for (int i = 0; i < 9; ++i) {
            auto idxs = unit(i);

            std::array<int, 9> counts{};
            std::array<int, 9> location{};
            
            for (int idx : idxs) {
                for (int n = 0; n < 9; ++n) {
                    bool is_set_already = (grid_[idx] == n + 1);
                    bool has_annotation = (annotations_[idx] & 1 << n);
                    
                    counts[n] += 2 * is_set_already;
                    counts[n] += 1 * has_annotation;
                    location[n] += idx * has_annotation;
                }
            }
            
            for (int n = 0; n < 9; ++n) {
                if (counts[n] == 1) {
                    grid_[location[n]] = n + 1;
                }
            }

            //for (int n = 0; n < 9; ++n) {
            //    int hidden_idx = -1;
            //    for (int idx : idxs) {
            //        if (hidden_idx != -2) {
            //            if (grid_[idx] == n + 1) {
            //                hidden_idx = -2;
            //            }
            //            else if (grid_[idx] == 0 && annotations_[idx] & 1 << n) {
            //                if (hidden_idx == -1) {
            //                    hidden_idx = idx;
            //                }
            //                else {
            //                    hidden_idx = -2;
            //                }
            //            }
            //        }
            //    }
            //
            //    if (hidden_idx >= 0) grid_[hidden_idx] = n + 1;
            //}
        }
    };

    hidden_singles(column);
    hidden_singles(row);
    hidden_singles(box);
}

const auto& unit_subset_permutations(int k, int n) {
    const auto permutations = [](std::pair<int, int> p) {
        if (p.second > 9) throw std::runtime_error("n is larger than expected.");
        std::array<bool, 9> subset{};
        std::vector<std::array<bool, 9>> perms;
        std::fill_n(subset.begin(), p.first, true);
        do {
            perms.push_back(subset);
        } while (std::prev_permutation(subset.begin(), subset.begin() + p.second));
        return perms;
    };

    static auto memoize_unit_subset_permutations = memoize<std::pair<int, int>>(permutations);

    return memoize_unit_subset_permutations(std::pair{ k, n });
}

void sudoku::annotate_subsets() {

    auto subsets = [&](auto unit) {

        auto unit_annotations = annotations_;

        for (int i = 0; i < 9; ++i) {
            std::array<int, 9> counts{};
            auto idxs = unit(i);
        
            std::array<int, 9> frontier{}; //enough space for the entire unit
            int to_insert = 0;
            for (int idx : idxs) {
                if (grid_[idx] == 0) {
                    frontier[to_insert++] = idx;
                }
            }

            for (int ss = 2; ss < 8 && ss < to_insert; ++ss) {
                const auto& candidate_subsets = unit_subset_permutations(ss, to_insert);

                for (const auto& css : candidate_subsets) {
                    unsigned subset_space = std::transform_reduce(frontier.begin(), frontier.begin() + to_insert, css.begin(), 0u, [](int u, int a) {
                        return u | a;
                        }, [&](int f, bool s) {
                            return annotations_[f] * s;
                        });

                    if (std::popcount(subset_space) == ss) {
                        auto rem = 0b111111111 ^ subset_space;
                        for (int r = 0; r < to_insert; ++r) {
                            if (css[r] == 0) {
                                unit_annotations[frontier[r]] &= rem;
                            }
                        }
                    }
                }
            }
        }

        return unit_annotations;
    };

    auto box_annotations = subsets(box);
    auto column_annotations = subsets(column);
    auto row_annotations = subsets(row);

    for (int i = 0; i < 81; ++i) {
        annotations_[i] &= (box_annotations[i] & column_annotations[i] & row_annotations[i]);
    }
}

std::variant<sudoku, contradiction> sudoku::advance(const sudoku& s) {
    sudoku new_s (s);

    // solving operations
    new_s.solve_naked_singles();
    new_s.solve_hidden_singles();

    // validation
    if (!validate(new_s)) {
        return contradiction{};
    }

    //reannotate
    new_s.load_annotate();
    new_s.annotate_subsets();
    return new_s;
}

int sudoku::distance(const sudoku& s1, const sudoku& s2) {
    return std::transform_reduce(s1.grid_.begin(), s1.grid_.end(), s2.grid_.begin(), 0, std::plus<>{}, [](int s1c, int s2c) {
        return (s1c != s2c) * 1;
    }) + std::transform_reduce(s1.annotations_.begin(), s1.annotations_.end(), s2.annotations_.begin(), 0, std::plus<>{}, [](int s1c, int s2c) {
        return (s1c != s2c) * 1;
        });
}

bool sudoku::is_solved() const {
    return (std::count(grid_.begin(), grid_.end(), 0) == 0) && validate(*this);
}

bool sudoku::validate(const sudoku& s) {

    auto validate_unit = [&](auto unit) {
        for (int i = 0; i < 9; ++i) {
            auto idxs = unit(i);

            std::array<int, 10> count{};
            for (int idx : idxs) {
                ++count[s.grid_[idx]];
            }

            if (std::find_if(count.begin() + 1, count.end(), [](int c) {
                return c > 1;
                }) != count.end()) return false;

        }

        return true;
    };

    return validate_unit(row) && validate_unit(column) && validate_unit(box);
}

std::vector<cell_action> sudoku::get_minimal_cell_actions(const sudoku& s, int branch_factor) {

    std::vector<cell_action> list;

    for (int i = 0; i < 81; ++i) {
        if (std::popcount(static_cast<unsigned> (s.annotations_[i])) == branch_factor && s.grid_[i] == 0) {
            list.emplace_back(i);
        }
    }

    return list;
}

std::vector<unit_action> sudoku::get_minimal_unit_actions(const sudoku& s, int branch_factor) {
    std::vector<unit_action> list;

    auto unit_action_branches = [&](auto u, unit type) {
        for (int i = 0; i < 9; ++i) {
            auto idxs = u(i);
            for (int n = 0; n < 9; ++n) {
                int count = std::count_if(idxs.begin(), idxs.end(), [&](int idx) {
                    return (s.annotations_[idx] & 1 << n) && s.grid_[idx] == 0;
                    });

                if (count == branch_factor) {
                    list.emplace_back(type, i, n+1);
                }
            }
        }
    };

    unit_action_branches(column, unit::column);
    unit_action_branches(row, unit::row);
    unit_action_branches(box, unit::box);

    return list;
}

std::vector<std::variant<cell_action, unit_action>> sudoku::get_minimal_actions(const sudoku& s, int branch_factor) {
    
    // find all cell actions
    auto cell_actions = get_minimal_cell_actions(s, branch_factor);
    // find all unit actions
    auto unit_actions = get_minimal_unit_actions(s, branch_factor);
    //merge lists
    std::vector<std::variant<cell_action, unit_action>> list;
    list.reserve(cell_actions.size() + unit_actions.size());
    std::transform(cell_actions.begin(), cell_actions.end(), std::back_inserter(list), [](const auto action) {
        return std::variant<cell_action, unit_action> { action };
        });
    std::transform(unit_actions.begin(), unit_actions.end(), std::back_inserter(list), [](const auto action) {
        return std::variant<cell_action, unit_action> { action };
        });
    return list;
}

std::vector<sudoku> sudoku::branch(const sudoku& s, cell_action ca) {

    std::vector<sudoku> branches;
    for (int n = 0; n < 9; ++n) {
        if (s.annotations_[ca.cell_idx] & 1 << n && s.grid_[ca.cell_idx] == 0) {
            auto& new_s = branches.emplace_back(s);
            new_s.grid_[ca.cell_idx] = n + 1;
            new_s.load_annotate();
            new_s.annotate_subsets();
        }
    }

    return branches;
}

std::vector<sudoku> sudoku::branch(const sudoku& s, unit_action ua) {

    auto idxs = [&]() {
        switch (ua.type) {
            case unit::column: return column(ua.unit_idx);
            case unit::row: return row(ua.unit_idx);
            case unit::box: return box(ua.unit_idx);
            default: throw std::runtime_error("Unexpected unit type.");
        }
    }();

    std::vector<sudoku> branches;

    std::array<int, 9> a{};
    std::transform(idxs.begin(), idxs.end(), a.begin(), [&](int idx) {
        return s.annotations_[idx];
        });
    std::array<int, 9> g{};
    std::transform(idxs.begin(), idxs.end(), g.begin(), [&](int idx) {
        return s.grid_[idx];
        });

    for (int idx : idxs) {
        if ((s.annotations_[idx] & 1 << (ua.action - 1)) && s.grid_[idx] == 0) {
            auto& new_s = branches.emplace_back(s);
            new_s.grid_[idx] = ua.action;
            new_s.load_annotate();
            new_s.annotate_subsets();
        }
    }

    return branches;
}


sudoku load_sudoku(int puzzle_choice = -1) {
    //std::fill(grid_.begin(), grid_.end(), 1);

    using namespace std::string_view_literals;

    constexpr std::array puzzles = {
        //easy from sudoku.com
                                         " 94   6  "
                                         " 53986 41"
                                         " 82 13975"
                                         "   16 3 7"
                                         "9    2   "
                                         " 3     12"
                                         "56  41   "
                                         " 1    7  "
                                         "3  29  5 "sv,

                                         "   7  218"
                                         "751  249 "
                                         "    96753"
                                         " 1 3 8  2"
                                         " 6     85"
                                         "8295   7 "
                                         "1   5  49"
                                         " 76  45  "
                                         "   6 38  "sv,

                                         " 2 5 6 1 "
                                         "6 3179   "
                                         " 1 3     "
                                         "  1  234 "
                                         "349 1  26"
                                         "2 64 78  "
                                         "   658   "
                                         "5 8743 6 "
                                         "76   1   "sv,


        // march 11 from sudoku.com (seemed easy)
                                        " 8 25  9 "
                                        " 5 613872"
                                        "   9 4 1 "
                                        "5 7    6 "
                                        "9     2 1"
                                        "  4      "
                                        "1  37 9  "
                                        "  8   34 "
                                        "67       "sv,

        // medium from sudoku.com
                                         "  2  7 96"
                                         "7 5 9  18"
                                         "1    47  "
                                         "  97  1 5"
                                         "    28   "
                                         "     5 62"
                                         "   672  1"
                                         "   8   4 "
                                         "  3 4  2 "sv,


        // hard from sudoku.com*
                                         "9 4   3 1"
                                         "  78314  "
                                         "     928 "
                                         "3        "
                                         "4  7  8  "
                                         " 6 92    "
                                         "  2 579  "
                                         "  5    2 "
                                         "   28  7 "sv,

        // expert from sudoku.com* 
                                         "    5   9"
                                         "4    6  1"
                                         "  1  3 5 "
                                         "     84  "
                                         "  7      "
                                         " 2 19  8 "
                                         "  9    3 "
                                         "6   34   "
                                         "3     7  "sv,


                                         "  52 6   "
                                         "  8   1  "
                                         "4      6 "
                                         "    7    "
                                         " 1  9  8 "
                                         "79   4   "
                                         "   45   8"
                                         "      719"
                                         "   3    4"sv,

        // evil from sudoku.com*
                                         " 9       "
                                         "   7   8 "
                                         " 54 3 7  "
                                         "6        "
                                         "     1  2"
                                         " 73 5 8  "
                                         "9     4  "
                                         "8   6    "
                                         " 46  5 1 "sv,

        // evil from sudoku.com*
                                         "      9  "
                                         " 7   843 "
                                         "8  6     "
                                         "  2 1    "
                                         " 4   687 "
                                         "        5"
                                         "  42  35 "
                                         " 5      6"
                                         "     3  9"sv,

        // evil from sudoku.com*
                                         "    5    "
                                         "1  92   6"
                                         " 6     7 "
                                         "  4   8  "
                                         "     3   "
                                         "2  16   7"
                                         "  239  4 "
                                         "     5  9"
                                         "3    7   "sv,

        // evil from sudoku.com*
                                          "  3      "
                                          "64  1 7  "
                                          "   5    8"
                                          "  2 9    "
                                          "  1   3  "
                                          "93   8  7"
                                          "79  6 4  "
                                          "     1 6 "
                                          "2        "sv,

        // evil from sudoku.com*
                                          "    1    "
                                          "  256 4  "
                                          " 3      2"
                                          "7      9 "
                                          "     8   "
                                          "  342 6  "
                                          " 9 85  6 "
                                          "  5  1   "
                                          "     38  "sv,

        // evil from sudoku.com*
                                          " 1     2 "
                                          "     9   "
                                          "4  75 6  "
                                          "  293  6 "
                                          "     49  "
                                          "3    8   "
                                          "  4     5"
                                          "5  36 7  "
                                          "    8    "sv,

        // evil from sudoku.com*
                                           "7 2  5 8 "
                                           "  1      "
                                           "    8 6  "
                                           " 4       "
                                           "   3    9"
                                           "5 8  2 6 "
                                           " 1     7 "
                                           "4 72  3  "
                                           " 6   4   "sv,

        // evil from sudoku.com
                                           "8 47  1  "
                                           " 6       "
                                           "    2   9"
                                           "     8 1 "
                                           "7 54  8  "
                                           "3        "
                                           " 1 6     "
                                           "5 6 7  2 "
                                           " 3    5  "sv,

        // evil from sudoku.com*
                                           "    6    "
                                           "  8   3  "
                                           "5  1 7  9"
                                           "   4     "
                                           "1  9 2  7"
                                           " 5     1 "
                                           " 3 2 69  "
                                           "    5   6"
                                           "2   4    "sv,

        // partial puzzle
                                             " 752 6  3"
                                             "  894 17 "
                                             "4  7   6 "
                                             "    7    "
                                             " 1  9  87"
                                             "79   4   "
                                             "   45   8"
                                             "      719"
                                             "   3    4"sv,

        // evil from sudoku.com*
                                             "4 3 2 9  "
                                             "  6      "
                                             "   1   2 "
                                             " 6  4    "
                                             " 1    5  "
                                             "5 48    3"
                                             " 5       "
                                             "     7  8"
                                             "9 2 1 3  "sv,

        // expert sudoku.com*
                                             " 7    6 8"
                                             "1 2      "
                                             " 3 7     "
                                             "   42   6"
                                             "     5 2 "
                                             "      17 "
                                             "3 5      "
                                             "   2564  "
                                             "7    9 1 "sv,

        // expert sudoku.com
                                             "6      4 "
                                             "2  35    "
                                             "  1   5  "
                                             "   9    1"
                                             "      478"
                                             "   1 2  6"
                                             "     7   "
                                             " 4   86  "
                                             " 87 1    "sv,

        // expert sudoku.com
                                             "1      49"
                                             "       7 "
                                             "396 5    "
                                             "6  9     "
                                             "    7    "
                                             " 49  182 "
                                             "4   87   "
                                             "  3  2  5"
                                             "         "sv,

        // evil sudoku.com
                                             " 2 49   6"
                                             "     3   "
                                             "7     5  "
                                             " 9 16   4"
                                             "  2    9 "
                                             "    8    "
                                             "     2  3"
                                             " 1   8   "
                                             "  531 6  "sv,

        // evil sudoku.com
                                             "  5    2 "
                                             "9  4  1 5"
                                             "    1  7 "
                                             "       1 "
                                             " 8 9     "
                                             "  7 4 6 3"
                                             "  3 6 5 4"
                                             "        2"
                                             "7    3   "sv
    };

    std::array<int, 9 * 9> grid;
    const auto& puzzle = puzzles[(puzzle_choice + puzzles.size()) % puzzles.size()];
    std::transform(puzzle.begin(), puzzle.end(), grid.begin(), [](const char c) {
        switch (c) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': return c - '0';
        default: return 0;
        }
        });

    return sudoku{ grid };
}

void solve_sudoku17() {
    // Inspired by https://abhinavsarkar.net/posts/fast-sudoku-solver-in-haskell-2/
    std::vector<std::string> sudoku_strings;
    std::ifstream is(R"(data\sudoku17.txt)");
    while (std::getline(is, sudoku_strings.emplace_back()));
    sudoku_strings.pop_back(); // the false call for getline appends an empty string

    std::vector<sudoku> sudokus;
    sudokus.reserve(sudoku_strings.size());
    std::transform(sudoku_strings.begin(), sudoku_strings.end(), std::back_inserter(sudokus), [](const std::string& s) {
        std::array<int, 81> grid{};
        std::transform(s.begin(), s.end(), grid.begin(), [](const char c) {
            switch (c) {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': return c - '0';
            default: return 0;
            }
            });
        return sudoku{ grid };
    });

    int i = 0;

    using double_ms = std::chrono::duration<double, std::milli>;
    using double_s = std::chrono::duration<double>;

    std::vector<sudoku> sudoku_states;
    std::vector<sudoku> sudoku_search_stack;
    std::vector<sudoku> solved_sudokus;
    solved_sudokus.reserve(sudokus.size());
    sudoku_states.reserve(16);       //preallocate memory for hot path
    sudoku_search_stack.reserve(16);

    auto solver_start = std::chrono::system_clock::now();
    for (auto& s : sudokus) {
        auto start = std::chrono::system_clock::now();
        sudoku_states.clear();
        sudoku_search_stack.clear();
        sudoku_states.push_back(s);

        while (!sudoku_states.back().is_solved()) {
            if (auto new_s = sudoku::advance(sudoku_states.back());
                std::holds_alternative<sudoku>(new_s) && sudoku::distance(std::get<sudoku>(new_s), sudoku_states.back()) > 0) {
                sudoku_states.push_back(std::get<sudoku>(new_s));
            }
            else if (std::holds_alternative<sudoku>(new_s))
            {
                int branch_factor = 2;
                std::vector<std::variant<cell_action, unit_action>> actions;

                do {
                    actions = sudoku::get_minimal_actions(std::get<sudoku>(new_s), branch_factor++);
                } while (actions.empty() && branch_factor < 9);

                if (!actions.empty()) {
                    //choose the action to use (priotize first for now since we have no heuristics)
                    auto& action_choice = *actions.begin();

                    auto branches = std::visit([&](const auto& action) {
                        return sudoku::branch(std::get<sudoku>(new_s), action);
                        }, action_choice);

                    if (branches.empty()) {
                        std::cout << "poor action\n";
                    }
                    else {
                        sudoku_search_stack.insert(sudoku_search_stack.end(), branches.begin(), branches.end());
                        sudoku_states.push_back(sudoku_search_stack.back());
                        sudoku_search_stack.pop_back();
                    }
                }
                else {

                    sudoku_states.push_back(sudoku_search_stack.back());
                    sudoku_search_stack.pop_back();
                }
            }
            else {
                sudoku_states.push_back(sudoku_search_stack.back());
                sudoku_search_stack.pop_back();
            }
        }

        auto end = std::chrono::system_clock::now();
        solved_sudokus.push_back(sudoku_states.back());
        //std::cout << "#" << i++ << ": " << std::chrono::duration_cast<double_ms> (end - start) << "\n";
    }

    int num_solved = std::count_if(solved_sudokus.begin(), solved_sudokus.end(), [](const auto& s) {
        return s.is_solved();
        });

    auto solver_end = std::chrono::system_clock::now();

    std::cout << num_solved << "/" << sudokus.size() << " sudokus in sudoku17.txt were solved completely in " 
              << std::chrono::duration_cast<double_s> (solver_end - solver_start) << "\n";
}
 
void application::draw_gridlines(sf::RenderWindow& window)
{
    std::vector<sf::Vertex> grid;
    grid.reserve(40);
    for (float i = 0; i < 10; ++i) {
        grid.emplace_back(sf::Vector2f{ i * GRID_SIZE + MARGIN, MARGIN }, sf::Color::Black);
        grid.emplace_back(sf::Vector2f{ i * GRID_SIZE + MARGIN, 9 * GRID_SIZE + MARGIN }, sf::Color::Black);

        grid.emplace_back(sf::Vector2f{ MARGIN, i * GRID_SIZE + MARGIN }, sf::Color::Black);
        grid.emplace_back(sf::Vector2f{ 9 * GRID_SIZE + MARGIN, i * GRID_SIZE + MARGIN }, sf::Color::Black);
    }

    window.draw(grid.data(), grid.size(), sf::Lines);
}

void application::draw_thicklines(sf::RenderWindow& window)
{
    for (float i = 0; i < 2; ++i) {
        sf::RectangleShape vertical(sf::Vector2f{ LINE_THICKNESS, 9 * GRID_SIZE });
        vertical.setPosition(sf::Vector2f{ 3 * (i + 1) * GRID_SIZE + MARGIN - LINE_THICKNESS / 2.0f, MARGIN });
        vertical.setFillColor(sf::Color::Black);
        window.draw(vertical);

        sf::RectangleShape horizontal(sf::Vector2f{ 9 * GRID_SIZE, LINE_THICKNESS });
        horizontal.setPosition(sf::Vector2f{ MARGIN, 3 * (i + 1) * GRID_SIZE + MARGIN - LINE_THICKNESS / 2.0f });
        horizontal.setFillColor(sf::Color::Black);
        window.draw(horizontal);
    }
}

application::application() {
    sudoku_states_.emplace_back(load_sudoku());
}

void application::handle_events(sf::Event event) {
    // "close requested" event: we close the window
    if (event.type == sf::Event::Closed) {
        window_.close();
    }
    else if (event.type == sf::Event::KeyPressed)
    {
        switch (event.key.code) {
        case sf::Keyboard::Space: {
            //only insert states that are different from the previous state
            if (!sudoku_states_.back().is_solved()) {
                if (auto new_s = sudoku::advance(sudoku_states_.back());
                    std::holds_alternative<sudoku>(new_s) && sudoku::distance(std::get<sudoku>(new_s), sudoku_states_.back()) > 0) {
                    sudoku_states_.push_back(std::get<sudoku>(new_s));
                    sudoku_state_display_idx_ = sudoku_states_.size() - 1;
                }
                else if (std::holds_alternative<sudoku>(new_s))
                {
                    //find actions
                    //  - prefer minimal branching
                    //  - actions are cell or unit based
                    int branch_factor = 2;
                    std::vector<std::variant<cell_action, unit_action>> actions;

                    do {
                        actions = sudoku::get_minimal_actions(std::get<sudoku>(new_s), branch_factor++);
                    } while (actions.empty() && branch_factor < 9);

                    if (!actions.empty()) {
                        //choose the action to use (priotize first for now since we have no heuristics)
                        auto& action_choice = *actions.begin();

                        auto branches = std::visit([&](const auto& action) {
                            return sudoku::branch(std::get<sudoku>(new_s), action);
                            }, action_choice);

                        //and branches (with actions to state)
                        std::vector<std::pair<std::variant<cell_action, unit_action>, sudoku>> action_and_branches;
                        action_and_branches.reserve(branches.size());
                        std::transform(branches.begin(), branches.end(), std::back_inserter(action_and_branches), [&](const sudoku& s) {
                            return std::pair{ action_choice, s };
                            });
                        sudoku_search_stack_.insert(sudoku_search_stack_.end(), action_and_branches.begin(), action_and_branches.end());
                        sudoku_states_.push_back(sudoku_search_stack_.back().second);
                        sudoku_search_stack_.pop_back();
                        sudoku_state_display_idx_ = sudoku_states_.size() - 1;
                    }
                    else{
                        
                        sudoku_states_.push_back(sudoku_search_stack_.back().second);
                        sudoku_search_stack_.pop_back();
                        sudoku_state_display_idx_ = sudoku_states_.size() - 1;
                    }
                }
                else {
                    sudoku_states_.push_back(sudoku_search_stack_.back().second);
                    sudoku_search_stack_.pop_back();
                    sudoku_state_display_idx_ = sudoku_states_.size() - 1;
                }
            }
            break;
        }
        case sf::Keyboard::PageUp: {
            sudoku_states_.clear();
            sudoku_search_stack_.clear();
            sudoku_states_.emplace_back(load_sudoku(++sudoku_puzzle_idx_));
            break;
        }
        case sf::Keyboard::PageDown: {
            sudoku_states_.clear();
            sudoku_search_stack_.clear();
            sudoku_states_.emplace_back(load_sudoku(--sudoku_puzzle_idx_));
            break;
        }
        case sf::Keyboard::Left: {
            sudoku_state_display_idx_ = std::clamp(sudoku_state_display_idx_ - 1, 0, static_cast<int> (sudoku_states_.size() - 1));
            break;
        }
        case sf::Keyboard::Right: {
            sudoku_state_display_idx_ = std::clamp(sudoku_state_display_idx_ + 1, 0, static_cast<int> (sudoku_states_.size() - 1));
            break;
        }
        case sf::Keyboard::P: {
            solve_sudoku17();
            break;
        }
        }
    }
        
}

void application::render() {

    window_.clear(sf::Color::White);

    draw_gridlines(window_);
    draw_thicklines(window_);
    renderer.render(window_, sudoku_states_[sudoku_state_display_idx_ % sudoku_states_.size()]);

    window_.display();
}

void application::run() {

    // run the program as long as the window is open
    while (window_.isOpen())
    {
        // check all the window's events that were triggered since the last iteration of the loop
        sf::Event event;
        while (window_.pollEvent(event))
        {
            handle_events(event);

            render();
        }
    }

}


int main()
{
    application app;

    app.run();

    return 0;
}
