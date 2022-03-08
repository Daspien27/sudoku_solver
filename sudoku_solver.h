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

    std::array<int, 9 * 9> grid_;
    std::array<int, 9 * 9> annotations_;
    sudoku_render renderer;

private: 

    void load_annotate();
    int column_annotation(int col) const;
    int row_annotation(int row) const;
    int box_annotation(int box) const;

public:

    sudoku();

    sudoku(const sudoku& o) = default;

    void render(sf::RenderWindow& window);

    static void draw_gridlines(sf::RenderWindow& window);

    static void draw_thicklines(sf::RenderWindow& window);

    static sudoku advance(const sudoku& s);
};

class application {

    sf::RenderWindow window_{ sf::VideoMode(800, 600), "sudoku_solver" };
    std::vector<sudoku> sudoku_states_;

public:
    
    application();

    void handle_events(sf::Event event);

    void render();

    void run();
};
