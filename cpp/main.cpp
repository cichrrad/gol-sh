#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>

const std::string ANSI_RESET = "\033[0m";

// Single color table: base codes for both fg (30–37) and bg (40–47)
const std::unordered_map<std::string, int> COLORS = {
    {"black", 0},   {"red", 1},  {"green", 2}, {"yellow", 3}, {"blue", 4},
    {"magenta", 5}, {"cyan", 6}, {"white", 7}, {"default", 9}};

using Coord = std::pair<int, int>;

struct CoordHash {
  std::size_t operator()(const Coord &c) const {
    return std::hash<int>()(c.first) ^ (std::hash<int>()(c.second) << 1);
  }
};

using Grid = std::unordered_set<Coord, CoordHash>;

struct Config {
  int window_width = -1;
  int window_height = -1;
  int init_coverage = 10;
  int stable_treshold = 70;
  int heat_treshold = 50;
  int speed_ms = 250;
  bool repopulate = true;
  std::string color_fg = "";
  std::string color_bg = "";
  std::string cell = "█";
  std::string wall_horizontal = "─";
  std::string wall_vertical = "│";
  std::string corner_ul = "┌";
  std::string corner_ur = "┐";
  std::string corner_ll = "└";
  std::string corner_lr = "┘";
};

std::string ansi_color_code(const std::string &color, bool background = false) {
  auto it = COLORS.find(color);
  if (it == COLORS.end())
    return "";
  int base = background ? 40 : 30;
  return "\033[" + std::to_string(base + it->second) + "m";
}

void get_terminal_size(int &window_width, int &window_height) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
    window_width = ws.ws_col;
    window_height = ws.ws_row;
  } else {
    window_width = 80;
    window_height = 24;
  }
}

void parse_args(int argc, char *argv[], Config &cfg) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 < argc)
        return argv[++i];
      return "";
    };

    auto value = [&](const std::string &arg) -> std::string {
      auto eq = arg.find('=');
      return eq != std::string::npos ? arg.substr(eq + 1) : next();
    };

    if (arg == "--help") {
      std::cout
          << "Usage: ./gof [OPTIONS]\n\n"
             "OPTIONS:\n"
             " [--window-width N]\n"
             " [--window-height N]\n"
             " [--init-coverage PERCENT] (0 <= PERCENT <= 100)\n"
             " [--speed MS] (100 <= MS <= 10000\n"
             " [--color-fg COLOR]\n"
             " [--color-bg COLOR]\n"
             " [--one-universe] (disables repopulation)\n"
             " [--tile-cell GLYPH]\n\n"
             "COLOR:\n"
             " [BLACK,RED,GREEN,CYAN,MAGENTA,YELLOW,BLUE,WHITE,DEFAULT]\n";
      std::exit(0);
    } else if (arg.rfind("--window-width", 0) == 0) {
      cfg.window_width = std::stoi(value(arg));
    } else if (arg.rfind("--window-height", 0) == 0) {
      cfg.window_height = std::stoi(value(arg));
    } else if (arg.rfind("--init-coverage", 0) == 0) {
      cfg.init_coverage = std::stoi(value(arg));
    } else if (arg.rfind("--speed", 0) == 0) {
      cfg.speed_ms = std::clamp(std::stoi(value(arg)), 100, 10000);
    } else if (arg.rfind("--color-fg", 0) == 0) {
      cfg.color_fg = value(arg);
    } else if (arg.rfind("--color-bg", 0) == 0) {
      cfg.color_bg = value(arg);
    } else if (arg.rfind("--tile-cell", 0) == 0) {
      cfg.cell = value(arg);
    } else if (arg.rfind("--one-universe", 0) == 0) {
      cfg.repopulate = false;
    };
  }
};

void seed_initial(Grid &grid,
                  std::unordered_map<Coord, unsigned int, CoordHash> &heatmap,
                  int window_width, int window_height, int spawn_rate) {

  heatmap.clear();

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> chance(0, 99);
  for (int y = 0; y < window_height; ++y) {
    for (int x = 0; x < window_width; ++x) {
      if (chance(gen) < spawn_rate) {
        grid.emplace(x, y);
        Coord loc = {x, y};
        heatmap[loc] = 0;
      }
    }
  }
}

void render(const Grid &grid, int window_width, int window_height,
            const Config &cfg, float &stable_ratio) {
  int render_window_height = window_height;

  std::string fg = ansi_color_code(cfg.color_fg);
  std::string bg = ansi_color_code(cfg.color_bg, true);

  // Info line with fg and bg start
  std::cout << fg << bg << " Alive cells: " << grid.size() << " ("
            << (float)(stable_ratio) << " % stable)" << "\033[K\n";

  // Top border
  std::cout << cfg.corner_ul;
  for (int i = 0; i < window_width; ++i)
    std::cout << cfg.wall_horizontal;
  std::cout << cfg.corner_ur << "\n";

  // Grid area
  for (int y = 0; y < render_window_height; ++y) {
    std::cout << cfg.wall_vertical;
    for (int x = 0; x < window_width; ++x) {
      if (grid.find({x, y}) != grid.end())
        std::cout << cfg.cell;
      else
        std::cout << " ";
    }
    std::cout << cfg.wall_vertical << "\n";
  }

  // Bottom border
  std::cout << cfg.corner_ll;
  for (int i = 0; i < window_width; ++i)
    std::cout << cfg.wall_horizontal;
  std::cout << cfg.corner_lr << ANSI_RESET;
}

bool update(Grid &current,
            std::unordered_map<Coord, unsigned int, CoordHash> &heatmap,
            int &heated_cells, float &stable_ratio, int window_width,
            int window_height, const Config &cfg) {
  Grid next;
  std::unordered_map<Coord, int, CoordHash> candidate_counts;
  int visible_alive_count = 0;

  for (const Coord &cell : current) {
    int x = cell.first, y = cell.second, alive_neighbors = 0;

    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0)
          continue;
        int nx = (x + dx + window_width) % window_width;
        int ny = (y + dy + window_height) % window_height;
        Coord neighbor = {nx, ny};

        if (current.find(neighbor) != current.end())
          ++alive_neighbors;
        else
          ++candidate_counts[neighbor];
      }
    }

    Coord loc = {x, y};
    if (alive_neighbors == 2 || alive_neighbors == 3) {
      next.insert({x, y});
      if (x >= 0 && x < window_width && y >= 0 && y < window_height) {
        ++visible_alive_count;
      }
    }
  }

  for (const auto &[coord, count] : candidate_counts) {
    if (count == 3) {
      next.insert(coord);
      int x = coord.first, y = coord.second;
      Coord loc = {x, y};
      if (x >= 0 && x < window_width && y >= 0 && y < window_height) {
        ++visible_alive_count;
      }
    }
  }
  current = std::move(next);
  return visible_alive_count > 0;
}

int main(int argc, char *argv[]) {
  Config cfg;
  parse_args(argc, argv, cfg);

  int term_window_width, term_window_height;
  get_terminal_size(term_window_width, term_window_height);

  int grid_window_width =
      (cfg.window_width > 0 ? cfg.window_width : term_window_width - 2);
  int grid_window_height =
      (cfg.window_height > 0 ? cfg.window_height : term_window_height - 3);

  Grid current;
  std::unordered_map<Coord, unsigned int, CoordHash> heatmap;
  int heated_cells = 0;
  float stable_ratio = 0.0F;

  seed_initial(current, heatmap, grid_window_width, grid_window_height,
               cfg.init_coverage);

  while (true) {
    std::cout << "\033[H"; // Cursor to top-left
    render(current, grid_window_width, grid_window_height, cfg, stable_ratio);
    std::this_thread::sleep_for(std::chrono::milliseconds(cfg.speed_ms));

    if (!update(current, heatmap, heated_cells, stable_ratio, grid_window_width,
                grid_window_height, cfg)) {
      std::cout << "\033[H";
      render(current, grid_window_width, grid_window_height, cfg, stable_ratio);
      if (cfg.repopulate) {
        current.clear();
        heatmap.clear();
        heated_cells = 0;
        std::cout
            << "\033[2J\033[H"; // clear screen and move cursor to top-left
        std::cout << "This universe is doomed, but there is another...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "Creating new Big Bang...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        seed_initial(current, heatmap, grid_window_width, grid_window_height,
                     cfg.init_coverage);
      } else {

        std::cout << "Simulation ended: all visible cells dead.\n";
        break;
      }
    }
  }

  return 0;
}
