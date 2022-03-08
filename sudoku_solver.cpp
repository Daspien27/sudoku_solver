// sudoku_solver.cpp : Defines the entry point for the application.
//

#include "sudoku_solver.h"

#include <string_view>
#include <algorithm>
#include <iostream>
#include <cassert>

const float GRID_SIZE = 50.0f;
const float MARGIN = 20.0f;
const float LINE_THICKNESS = 3.0f;

sudoku_render::sudoku_render() {
    if (!font.loadFromFile(R"(C:\Users\Amund\source\repos\sudoku_solver\arial.ttf)")) {
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


void load_sudoku(std::array<int, 9 * 9>& grid) {
    //std::fill(grid_.begin(), grid_.end(), 1);

    using namespace std::string_view_literals;
    //easy from sudoku.com
    constexpr static auto puzzle_1 = " 94   6  "
                                     " 53986 41"
                                     " 82 13975"
                                     "   16 3 7"
                                     "9    2   "
                                     " 3     12"
                                     "56  41   "
                                     " 1    7  "
                                     "3  29  5 "sv;

    constexpr static auto puzzle_2 = "   7  218"
                                     "751  249 "
                                     "    96753"
                                     " 1 3 8  2"
                                     " 6     85"
                                     "8295   7 "
                                     "1   5  49"
                                     " 76  45  "
                                     "   6 38  "sv;

    constexpr static auto puzzle_3 = " 2 5 6 1 "
                                     "6 3179   "
                                     " 1 3     "
                                     "  1  234 "
                                     "349 1  26"
                                     "2 64 78  "
                                     "   658   "
                                     "5 8743 6 "
                                     "76   1   "sv;

    // medium from sudoku.com
    constexpr static auto puzzle_4 = "  2  7 96"
                                     "7 5 9  18"
                                     "1    47  "
                                     "  97  1 5"
                                     "    28   "
                                     "     5 62"
                                     "   672  1"
                                     "   8   4 "
                                     "  3 4  2 "sv;


    // hard from sudoku.com
    constexpr static auto puzzle_5 = "9 4   3 1"
                                     "  78314  "
                                     "     928 "
                                     "3        "
                                     "4  7  8  "
                                     " 6 92    "
                                     "  2 579  "
                                     "  5    2 "
                                     "   28  7 "sv;


    constexpr static auto& puzzle = puzzle_4;

    std::transform(puzzle.begin(), puzzle.end(), grid.begin(), [](const char c) {
            switch (c) {
            case '1': return 1;
            case '2': return 2;
            case '3': return 3;
            case '4': return 4;
            case '5': return 5;
            case '6': return 6;
            case '7': return 7;
            case '8': return 8;
            case '9': return 9;
            default: return 0;
            }
        });
}


sudoku::sudoku() {
    load_sudoku(grid_);
    load_annotate();
}

int sudoku::column_annotation(int col) const {
    int annotation = 0b111111111;
    for (int i = 0; i < 9; i++) {
        auto cell = grid_[col + i * 9];
        if (cell) {
            annotation ^= 1 << (cell - 1);
        }
    }

    return annotation;
}

int sudoku::row_annotation(int row) const {
    int annotation = 0b111111111;
    for (int i = 0; i < 9; i++) {
        auto cell = grid_[i + row * 9];
        if (cell) {
            annotation ^= 1 << (cell-1);
        }
    }

    return annotation;
}

int sudoku::box_annotation(int box) const {
    int annotation = 0b111111111;
    for (int i = (box / 3) * 3; i < (box / 3) * 3 + 3; i++) {
        for (int j = (box % 3) * 3; j < (box % 3) * 3 + 3; j++) {
            auto cell = grid_[9 * i + j];
            if (cell) {
                annotation ^= 1 << (cell - 1);
            }
        }
    }

    return annotation;
}

void sudoku::load_annotate() {
    std::fill(annotations_.begin(), annotations_.end(), 0b111111111);

    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {
            auto& annotation = annotations_[9 * i + j];
            
            annotation &= column_annotation(j);
            annotation &= row_annotation(i);
            int box = 3 * (i / 3) + j / 3;
            annotation &= box_annotation(box);
        }
    }
}


void sudoku::render(sf::RenderWindow& window) {
    renderer.render(window, *this);
}


void sudoku::draw_gridlines(sf::RenderWindow& window)
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

void sudoku::draw_thicklines(sf::RenderWindow& window)
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

sudoku sudoku::advance(const sudoku& s) {
    sudoku new_s (s);

    //  look for single annotations
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 9; ++j) {
            const auto& annotation = new_s.annotations_[9 * i + j];
            auto& cell = new_s.grid_[9 * i + j];

            if (cell == 0 && annotation && (annotation & (annotation - 1)) == 0) {
                //single power of 2
                int a = annotation;
                int i = 1;
                while (a != 1) { a >>= 1; ++i; }
                cell = i;
            }
        }
    }


    // look for single occurences
    
    // rows
    for (int i = 0; i < 9; ++i) {
         int row_annotation = s.row_annotation(i);

        for (int n = 0; n < 9; ++n) {
            if (row_annotation & 1 << n) {
                int potential_cells = 0;
                for (int k = 0; k < 9; ++k) {
                    int cell_annotation = s.annotations_[9 * i + k];
                    if (cell_annotation & (1 << n) && s.grid_[9*i+k]==0) {
                        potential_cells |= 1 << k;
                    }
                }

                if (potential_cells && (potential_cells & (potential_cells - 1)) == 0) {
                    int cell = 0;
                    int pot = potential_cells;
                    while (pot != 1) { pot >>= 1; ++cell; }

                    auto& n_cell = new_s.grid_[i * 9 + cell];
                    assert(n_cell == 0 || n_cell == n + 1);
                    n_cell = n + 1;
                }
            }
        }
    }
    // columms
    for (int i = 0; i < 9; ++i) {
        int col_annotation = s.column_annotation(i);

        for (int n = 0; n < 9; ++n) {
            if (col_annotation & 1 << n) {
                int potential_cells = 0;
                for (int k = 0; k < 9; ++k) {
                    int cell_annotation = s.annotations_[9 * k + i];
                    if (cell_annotation & (1 << n) && s.grid_[9 * k + i] == 0) {
                        potential_cells |= 1 << k;
                    }
                }

                if (potential_cells && (potential_cells & (potential_cells - 1)) == 0) {
                    int cell = 0;
                    int pot = potential_cells;
                    while (pot != 1) { pot >>= 1; ++cell; }

                    auto& n_cell = new_s.grid_[cell * 9 + i];
                    assert(n_cell == 0 || n_cell == n + 1);
                    n_cell = n + 1;
                }
            }
        }
    }
    // boxes



    //reannotate
    new_s.load_annotate();
    return new_s;
}

application::application() {
    sudoku_states_.emplace_back();
}


void application::handle_events(sf::Event event) {
    // "close requested" event: we close the window
    if (event.type == sf::Event::Closed) {
        window_.close();
    }
    else if (event.type == sf::Event::KeyPressed 
        && event.key.code == sf::Keyboard::Space) {
        sudoku_states_.emplace_back(sudoku::advance(sudoku_states_.back()));
    }
        
}

void application::render() {

    window_.clear(sf::Color::White);

    sudoku::draw_gridlines(window_);
    sudoku::draw_thicklines(window_);
    sudoku_states_.back().render(window_);

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
