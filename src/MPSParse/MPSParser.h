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

// A simple MPS parser that does minimal validation and stores the data in a
// struct.
// Supports a subset of MPS (CPLEX extensions) free-format.
// Supports NAME, OBJNAME, OBJSENSE, ROWS, COLUMNS, RHS, BOUNDS sections.
// Follows convention on OBJNAME, OBJSENSE, RHS, and BOUNDS being optional.
// Assumes sections in that order.
// Identifers are assumed to be alphanumeric and underscores only.
// Does not currently support integer or binary variables.
// If multiple BOUNDS or RHS vectors are specified, only the first is stored.
// If no RHS is specified for a row, defaults to 0.0.
// If no BOUNDS are specified for a variable, defaults to free (-inf, inf).
// Of course, COLUMNS only specifies nonzero entries.
// TODO: add support for integer and binary variables.
#ifndef MPSParser_H
#define MPSParser_H

#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mps {

using std::map;
using std::pair;
using std::string;
using std::string_view;
using std::unordered_map;
using std::vector;

constexpr double inf = std::numeric_limits<double>::infinity();

constexpr string_view NAME = "NAME";

// Row constraint types
enum RowType {
  N, // Free row
  G, // Greater than or equal
  L, // Less than or equal
  E, // Equality
};

// Variable bound types
enum BoundType {
  UP, // Upper
  LO, // Lower
  FX, // Fixed (upper = lower)
  FR, // Free
  MI, // Minus infinity
  PL, // Plus infinity
  BV, // Binary variable
  LI, // Integer lower
  SC, // Semi-continuous (either zero or >= lower bound)
  UI  // Integer upper
};

struct ParsedMPS {
  string name;
  int num_rows{};
  int num_cols{};

  int obj_row{-1};
  string obj_name;
  // we will always minimize internally
  string obj_sense;

  string rhs_name;
  string bound_name;

  // Indices for rows and columns
  vector<string> col_names;
  vector<string> row_names;
  unordered_map<string, int> col_index;
  unordered_map<string, int> row_index;

  // Objective coefficients
  vector<double> obj;

  // Constraint types and RHS vector
  vector<RowType> row_type;
  vector<double> rhs;

  // Contraint matrix A in COO format
  vector<int> A_row;
  vector<int> A_col;
  vector<double> A_val;

  // Variable bounds
  vector<double> lower_bounds;
  vector<double> upper_bounds;
  // vector<bool> is_integer;
  // vector<bool> is_binary;
};

// Split line into tokens, discarding comments after $
static vector<string> split_tokens(const string &line) {
  std::istringstream iss{line};
  vector<string> tokens;
  string token;

  // Check for comment line
  iss >> token;
  if (token[0] == '*' || token[0] == '$')
    return tokens;

  while (iss >> token) {
    if (token[0] == '$')
      break; // stop at comment
    tokens.push_back(token);
  }
  return tokens;
}

void parse_rows_line(const vector<string> &tokens, ParsedMPS &prob, int lineno);
void parse_columns_line(const vector<string> &tokens, ParsedMPS &prob,
                        int lineno);
void parse_rhs_line(const vector<string> &tokens, ParsedMPS &prob, int lineno);
void parse_bounds_line(const vector<string> &tokens, ParsedMPS &prob,
                       int lineno);

inline ParsedMPS parse_mps(const string &filename) {
  std::ifstream file(filename);
  if (!file)
    throw std::runtime_error("Cannot open file: " + filename);

  ParsedMPS prob;
  string current_section;
  string active_rhs_name;
  string active_bound_name;

  string line;
  int lineno = 0;

  while (std::getline(file, line)) {
    ++lineno;
    auto tokens = split_tokens(line);
    if (tokens.empty())
      continue;

    // Check for section headers
    if (tokens[0] == "NAME") {
      current_section = "NAME";
      prob.name = tokens[1];
      continue;
    }
    if (tokens[0] == "OBJNAME") {
      prob.name = tokens[1];
      continue;
    }
    if (tokens[0] == "OBJSENSE") {
      current_section = "OBJSENSE";
      continue;
    }
    if (tokens[0] == "ROWS") {
      current_section = "ROWS";
      continue;
    }
    if (tokens[0] == "COLUMNS") {
      current_section = "COLUMNS";
      continue;
    }
    if (tokens[0] == "RHS") {
      current_section = "RHS";
      continue;
    }
    if (tokens[0] == "BOUNDS") {
      current_section = "BOUNDS";
      continue;
    }
    if (tokens[0] == "ENDATA")
      break;

    if (current_section == "OBJSENSE") {
      prob.obj_sense = tokens[0];
      continue;
    }

    // process non-header line
    if (current_section == "ROWS") {
      parse_rows_line(tokens, prob, lineno);
    } else if (current_section == "COLUMNS") {
      parse_columns_line(tokens, prob, lineno);
    } else if (current_section == "RHS") {
      parse_rhs_line(tokens, prob, lineno);
    } else if (current_section == "BOUNDS") {
      parse_bounds_line(tokens, prob, lineno);
    }
  } // end while

  // // Resolve active RHS/BOUNDS (use first encountered if none specified)
  // if (state.active_rhs_name.empty() && !state.rhs_vectors.empty()) {
  //     state.active_rhs_name = state.rhs_vectors.begin()->first;
  // }
  // if (state.active_bound_name.empty() && !state.bound_vectors.empty()) {
  //     state.active_bound_name = state.bound_vectors.begin()->first;
  // }
  //
  // Finalize into indexed structure
  return prob;
}
} // namespace mps

namespace mps::detail {

inline void parse_rows_line(const vector<string> &tokens, ParsedMPS &prob,
                            int lineno) {
  if (tokens.size() != 2)
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": invalid ROWS entry");
  char type_char = tokens[0][0];
  string row_name = tokens[1];

  RowType row_type;
  switch (type_char) {
  case 'N':
    row_type = N;
    break;
  case 'G':
    row_type = G;
    break;
  case 'L':
    row_type = L;
    break;
  case 'E':
    row_type = E;
    break;
  default:
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": unknown row type '" + string{type_char} + "'");
  }

  // Assign row index
  int i = prob.num_rows++;
  prob.row_names.push_back(row_name);
  prob.row_index[row_name] = i;
  prob.row_type.push_back(row_type);
  prob.rhs.push_back(0.0); // TODO: default RHS = 0?

  if (row_type == N && prob.obj_row == -1) {
    prob.obj_row = i;
  }
}

inline void parse_columns_line(const vector<string> &tokens, ParsedMPS &prob,
                               int lineno) {
  if (tokens.size() != 3 && tokens.size() == 5) {
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": invalid COLUMNS entry");
  }

  string col_name = tokens[0];

  // Register column if new
  if (prob.col_index.find(col_name) == prob.col_index.end()) {
    int j = prob.num_cols++;
    prob.col_names.push_back(col_name);
    prob.col_index[col_name] = j;
    // Initialize bounds to free
    prob.lower_bounds.push_back(-inf);
    prob.upper_bounds.push_back(inf);
    // prob.is_integer.push_back(false);
    // prob.is_binary.push_back(false);
  }

  int col_idx = prob.col_index[col_name];

  // Parse (row, value) pairs
  for (size_t k = 1; k < tokens.size(); k += 2) {
    string row_name = tokens[k];
    double val = std::stod(tokens[k + 1]);

    if (prob.row_index.find(row_name) == prob.row_index.end()) {
      throw std::runtime_error("Line " + std::to_string(lineno) + ": row '" +
                               row_name + "' not declared in ROWS");
    }
    int row_idx = prob.row_index[row_name];

    prob.A_row.push_back(row_idx);
    prob.A_col.push_back(col_idx);
    prob.A_val.push_back(val);
  }
}

inline void parse_rhs_line(const vector<string> &tokens, ParsedMPS &prob,
                           int lineno) {
  if (tokens.size() != 3 && tokens.size() != 5) {
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": invalid RHS entry");
  }
  // check if this is the first RHS vector
  string rhs_name = tokens[0];
  if (prob.rhs_name.empty())
    prob.rhs_name = rhs_name;
  else if (rhs_name != prob.rhs_name)
    return; // skip other RHS vectors

  for (size_t k = 1; k < tokens.size(); k += 2) {
    string row_name = tokens[k];
    double val = std::stod(tokens[k + 1]);
    if (prob.row_index.find(row_name) == prob.row_index.end()) {
      throw std::runtime_error("Line " + std::to_string(lineno) +
                               ": RHS row '" + row_name + "' not declared");
    }
    int i = prob.row_index[row_name];
    prob.rhs[i] = val;
  }
}

inline void parse_bounds_line(const vector<string> &tokens, ParsedMPS &prob,
                              int lineno) {
  if (tokens.size() != 4) {
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": invalid BOUNDS entry");
  }
  // check if this is the first BOUNDS vector
  string bound_name = tokens[1];
  if (prob.bound_name.empty())
    prob.bound_name = bound_name;
  else if (prob.bound_name != bound_name)
    return;

  string bound_type = tokens[0];
  string col_name = tokens[2];
  double val = std::stod(tokens[3]);

  if (prob.col_index.find(col_name) == prob.col_index.end()) {
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": BOUNDS variable '" + col_name +
                             "' not declared in COLUMNS");
  }

  int i = prob.col_index[col_name];

  if (bound_type == "UP") {
    prob.upper_bounds[i] = val;
  } else if (bound_type == "LO") {
    prob.lower_bounds[i] = val;
  } else if (bound_type == "FX") {
    prob.lower_bounds[i] = prob.upper_bounds[i] = val;
  } else if (bound_type == "FR") {
    prob.lower_bounds[i] = -inf;
    prob.upper_bounds[i] = inf;
  } else if (bound_type == "MI") {
    prob.lower_bounds[i] = -inf;
  } else if (bound_type == "PL") {
    prob.upper_bounds[i] = inf;
  } else {
    throw std::runtime_error("Line " + std::to_string(lineno) +
                             ": unknown bound type '" + bound_type + "'");
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
} // namespace mps::detail

#endif
