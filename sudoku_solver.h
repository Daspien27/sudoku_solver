// sudoku_solver.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <array>
#include <vector>

class sudoku;

class sudoku_render {

    sf::Font font;

public:

    sudoku_render();

    void render(sf::RenderWindow& window, sudoku& s);
};

class sudoku {

    friend class sudoku_render;

    std::array<int, 9 * 9> grid_{};
    std::array<int, 9 * 9> annotations_{};

private: 

    void load_annotate();

    auto annotation(auto fn) const;

    int column_annotation(int col) const;
    int row_annotation(int row) const;
    int box_annotation(int box) const;

    void solve_naked_singles();
    void solve_hidden_singles();
    void annotate_subsets();

public:

    sudoku(std::array<int, 9 * 9> grid);
    sudoku(const sudoku& o) = default;

    static sudoku advance(const sudoku& s);
    static int distance(const sudoku& s1, const sudoku& s2);
    bool is_solved() const;
};

class application {

    sf::RenderWindow window_{ sf::VideoMode(800, 600), "sudoku_solver" };
    sudoku_render renderer;
    std::vector<sudoku> sudoku_states_;
    int sudoku_puzzle_idx_{ -1 };
    int sudoku_state_display_idx_{ -1 };


    static void draw_gridlines(sf::RenderWindow& window);
    static void draw_thicklines(sf::RenderWindow& window);
public:
    
    application();

    void handle_events(sf::Event event);

    void render();

    void run();
};
