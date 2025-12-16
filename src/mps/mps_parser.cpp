// Copyright 2025 Kenneth Tjhia
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mps_parser.h"
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace std::literals::string_view_literals;

constexpr double Inf = std::numeric_limits<double>::infinity();
constexpr std::string_view kName = "NAME"sv;
constexpr std::string_view kObjName = "OBJNAME"sv;
constexpr std::string_view kObjSense = "OBJSENSE"sv;
constexpr std::string_view kRows = "ROWS"sv;
constexpr std::string_view kColumns = "COLUMNS"sv;
constexpr std::string_view kRhs = "RHS"sv;
constexpr std::string_view kBounds = "BOUNDS"sv;
constexpr std::string_view kEndData = "ENDATA"sv;
constexpr std::string_view kMin = "MIN"sv;
constexpr std::string_view kMinimize = "MINIMIZE"sv;
constexpr std::string_view kMax = "MAX"sv;
constexpr std::string_view kMaximize = "MAXIMIZE"sv;

constexpr std::string_view kUp = "UP"sv;
constexpr std::string_view kLo = "LO"sv;
constexpr std::string_view kFx = "FX"sv;
constexpr std::string_view kFr = "FR"sv;
constexpr std::string_view kMi = "MI"sv;
constexpr std::string_view kPl = "PL"sv;

constexpr bool IsSpace(char c) {
  return std::isspace(static_cast<unsigned char>(c));
}

// Split a line into tokens, ignoring comments and whitespace
std::vector<std::string_view> SplitTokens(std::string_view line) {

  std::vector<std::string_view> tokens;
  std::size_t start = 0;

  // Skip leading whitespace
  while (start < line.size() && IsSpace(line[start])) {
    ++start;
  }

  // skip blank lines or comment lines
  if (start == line.size() || line[start] == '*' || line[start] == '$') {
    return tokens;
  }

  std::size_t i{start};
  while (i < line.size()) {
    // Skip whitespace
    while (i < line.size() && IsSpace(line[i])) {
      ++i;
    }
    if (i == line.size())
      break;

    // Check for inline comment
    if (line[i] == '$')
      break;

    start = i;
    // Read token
    while (i < line.size() && !IsSpace(line[i])) {
      ++i;
    }
    tokens.emplace_back(line.substr(start, i - start));
  }
  return tokens;
}

void ParseRowLine(std::vector<std::string_view> tokens, ParsedMPS &prob,
                  int lineno);
void ParseColumnLine(std::vector<std::string_view> tokens, ParsedMPS &prob,
                     int lineno);
void ParseRhsLine(std::vector<std::string_view> tokens, ParsedMPS &prob,
                  int lineno);
void ParseBoundLine(std::vector<std::string_view> tokens, ParsedMPS &prob,
                    int lineno);

ParsedMPS parse_mps(const std::string &filename) {
  std::ifstream file(filename);
  if (!file)
    throw std::runtime_error("Cannot open file: " + filename);

  ParsedMPS prob{};
  std::string current_section;
  std::string active_rhs_name;
  std::string active_bound_name;

  std::string line;
  int lineno{};

  while (std::getline(file, line)) {
    ++lineno;
    auto tokens = SplitTokens(line);
    if (tokens.empty())
      continue;

    // Check for section headers
    if (tokens[0] == kName) {
      prob.name = tokens[1];
      continue;
    }
    if (tokens[0] == kObjName) {
      prob.name = tokens[1];
      continue;
    }
    if (tokens[0] == kObjSense) {
      current_section = kObjSense;
      continue;
    }
    if (tokens[0] == kRows) {
      current_section = kRows;
      continue;
    }
    if (tokens[0] == kColumns) {
      current_section = kColumns;
      continue;
    }
    if (tokens[0] == kRhs) {
      current_section = kRhs;
      continue;
    }
    if (tokens[0] == kBounds) {
      current_section = kBounds;
      continue;
    }
    if (tokens[0] == kEndData)
      break;

    if (current_section == kObjSense) {
      if (tokens[0] == kMin || tokens[0] == kMinimize)
        prob.obj_sense = kMin;
      else if (tokens[0] == kMax || tokens[0] == kMaximize)
        prob.obj_sense = kMax;
      else
        std::runtime_error("Line " + std::to_string(lineno) +
                           ": unknown objective sense '" +
                           std::string(tokens[0]) + "'");
      continue;
    }

    // process non-header line
    if (current_section == kRows) {
      ParseRowLine(tokens, prob, lineno);
    } else if (current_section == kColumns) {
      ParseColumnLine(tokens, prob, lineno);
    } else if (current_section == kRhs) {
      ParseRhsLine(tokens, prob, lineno);
    } else if (current_section == kBounds) {
      ParseBoundLine(tokens, prob, lineno);
    }
  } // end while

  return prob;
}

void ParseRowLine(std::vector<std::string_view> &tokens, ParsedMPS &prob,
                  int lineno) {
  if (tokens.size() != 2)
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": invalid ROWS entry");
  char type_char{tokens[0][0]};

  int i{prob.num_rows++};
  RowType row_type;
  switch (type_char) {
  case 'N':
    row_type = RowType::N;
    if (prob.obj_row == -1) {
      prob.obj_row = i;
      prob.obj_name = tokens[1];
    }
    break;
  case 'G':
    row_type = RowType::G;
    break;
  case 'L':
    row_type = RowType::L;
    break;
  case 'E':
    row_type = RowType::E;
    break;
  default:
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": unknown row type '" + std::string{type_char} +
                             "'");
  }

  // TODO: should we store the objective row name here?
  // Assign row index
  prob.row_names.emplace_back(tokens[1]);
  prob.row_indices[prob.row_names.back()] = i;
  prob.row_type.push_back(row_type);
  prob.rhs.push_back(0.0);
}

void ParseColumnLine(std::vector<std::string_view> &tokens, ParsedMPS &prob,
                     int lineno) {
  if (tokens.size() != 3 && tokens.size() == 5) {
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": invalid COLUMNS entry");
  }

  // Register column if new
  if (!prob.col_indices.contains(tokens[0])) {
    prob.col_names.emplace_back(tokens[0]);
    prob.col_indices[prob.col_names.back()] = prob.num_cols++;
    // Initialize bounds to free
    prob.lower_bounds.push_back(-Inf);
    prob.upper_bounds.push_back(Inf);
    // prob.is_integer.push_back(false);
    // prob.is_binary.push_back(false);
  }

  // find should never fail
  auto search{prob.col_indices.find(tokens[0])};
  int col_index{search->second};

  // Parse (row, value) pairs
  for (size_t k = 1; k < tokens.size(); k += 2) {
    double val = std::stod(std::string{tokens[k + 1]});

    if (!prob.row_indices.contains(tokens[k])) {
      throw std::runtime_error("Line " + std::to_string(lineno) + ": row '" +
                               std::string{tokens[k]} +
                               "' not declared in ROWS");
    }
    search = prob.row_indices.find(tokens[k]);
    int row_index{search->second};

    prob.A_row.push_back(row_index);
    prob.A_col.push_back(col_index);
    prob.A_val.push_back(val);
  }
}

void ParseRhsLine(std::vector<std::string> &tokens, ParsedMPS &prob,
                  int lineno) {
  if (tokens.size() != 3 && tokens.size() != 5) {
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": invalid RHS entry");
  }
  // check if this is the first RHS vector
  if (prob.rhs_name.empty())
    prob.rhs_name = tokens[0];
  else if (tokens[0] != prob.rhs_name)
    return; // skip other RHS vectors

  for (size_t k = 1; k < tokens.size(); k += 2) {
    if (!prob.row_indices.contains(tokens[k])) {
      throw std::runtime_error("Line " + std::to_string(lineno) +
                               ": RHS row '" + std::string{tokens[k]} +
                               "' not declared");
    }

    auto search{prob.row_indices.find(tokens[k])};
    int row_index{search->second};
    prob.rhs[row_index] = std::stod(tokens[k + 1]);
  }
}

void ParseBoundsLine(std::vector<std::string> &tokens, ParsedMPS &prob,
                     int lineno) {
  if (tokens.size() != 4) {
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": invalid BOUNDS entry");
  }
  // check if this is the first BOUNDS vector
  if (prob.bound_name.empty())
    prob.bound_name = tokens[1];
  else if (prob.bound_name != tokens[1])
    return;

  double val = std::stod(tokens[3]);

  if (prob.col_indices.find(tokens[2]) == prob.col_indices.end()) {
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": BOUNDS variable '" + std::string{tokens[2]} +
                             "' not declared in COLUMNS");
  }

  auto search{prob.col_indices.find(tokens[2])};
  int col_index{search->second};
  std::string_view bound_type = tokens[0];

  if (bound_type == kUp) {
    prob.upper_bounds[col_index] = val;
  } else if (bound_type == kLo) {
    prob.lower_bounds[col_index] = val;
  } else if (bound_type == kFx) {
    prob.lower_bounds[col_index] = prob.upper_bounds[col_index] = val;
  } else if (bound_type == kFr) {
    prob.lower_bounds[col_index] = -Inf;
    prob.upper_bounds[col_index] = Inf;
  } else if (bound_type == kMi) {
    prob.lower_bounds[col_index] = -Inf;
  } else if (bound_type == kPl) {
    prob.upper_bounds[col_index] = Inf;
  } else {
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": unknown bound type '" +
                             std::string{bound_type} + "'");
  }
  // } else if (bound_type == "BV") {
  //   prob.is_binary[j] = true;
  //   prob.lower_bounds[j] = 0.0;
  //   prob.upper_bounds[j] = 1.0;
  // } else if (bound_type == "LI") {
  //   prob.is_integer[j] = true;
  //   prob.lower_bounds[j] = val;
  // } else if (bound_type == "UI") {
  //   prob.is_integer[j] = true;
  //   prob.upper_bounds[j] = val;
  // } else if (bound_type == "SC") {
  // prob.lower_bounds[i] = std::max(prob.lower_bounds[i], val);
}
