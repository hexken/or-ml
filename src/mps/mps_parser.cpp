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

// TODO: validate sections as they are parsed?
#include "mps_parser.h"
#include "string_utils.h"
#include <cassert>
#include <cctype>
#include <format>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mps {

namespace keys {
using namespace std::literals::string_view_literals;
constexpr std::string_view name = "NAME"sv;
constexpr std::string_view object_name = "OBJNAME"sv;
constexpr std::string_view objective_sense = "OBJSENSE"sv;
constexpr std::string_view rows = "ROWS"sv;
constexpr std::string_view columns = "COLUMNS"sv;
constexpr std::string_view rhs = "RHS"sv;
constexpr std::string_view bounds = "BOUNDS"sv;
constexpr std::string_view end_data = "ENDATA"sv;

constexpr std::string_view min = "MIN"sv;
constexpr std::string_view minimize = "MINIMIZE"sv;
constexpr std::string_view max = "MAX"sv;
constexpr std::string_view maximize = "MAXIMIZE"sv;
constexpr std::string_view empty = ""sv;

constexpr std::string_view upper = "UP"sv;
constexpr std::string_view lower = "LO"sv;
constexpr std::string_view fixed = "FX"sv;
constexpr std::string_view free = "FR"sv;
constexpr std::string_view minus_infinity = "MI"sv;
constexpr std::string_view plus_infinity = "PL"sv;
} // namespace keys

namespace detail {

struct ParserState {
  ParsedMps &problem;
  mps::SectionType current_section{mps::SectionType::None};
  size_t lineno{0};
  std::string active_rhs_name;
  std::string active_bound_name;
};

[[noreturn]] void throw_error(std::string message, size_t lineno) {
  throw std::runtime_error(std::format("Line {}: {}", lineno, message));
}

void validate_objective_name(ParserState &state) {
  if (!state.problem.objective_name.has_value()) {
    throw_error("objective name must be specified before COLUMNS section",
                state.lineno);
  }
}

void validate_problem_name(ParserState &state) {
  if (!state.problem.name.has_value()) {
    throw_error("problem name must be specified before ROWS section",
                state.lineno);
  }
}

std::vector<std::string_view>
split_tokens_ignore_comments(std::string_view line);

void parse_stream(std::istream &input_stream, ParsedMps &problem);
void parse_line(const std::string &line, ParserState &state);
constexpr std::string_view get_section_header(mps::SectionType section);
void set_section(const std::string_view &section_name);
std::optional<SectionType> detect_section(std::string_view token);
void parse_name_line(const std::vector<std::string_view> &tokens);
void parse_objective_name_line(const std::vector<std::string_view> &tokens,
                               ParserState &state);
void parse_objective_sense_line(const std::vector<std::string_view> &tokens,
                                ParserState &state);
void parse_row_line(const std::vector<std::string_view> &tokens,
                    ParserState &state);
RowType parse_row_type(std::string_view token);
void parse_column_line(const std::vector<std::string_view> &tokens,
                       ParserState &state);
void parse_rhs_line(const std::vector<std::string_view> &tokens,
                    ParserState &state);
void parse_bound_line(const std::vector<std::string_view> &tokens,
                      ParserState &state);

ParsedMps parse_file(const std::string &filename) {
  // TODO: handle compressed files
  std::ifstream input_stream{filename};

  ParsedMps problem{};
  ParserState state{problem};

  for (std::string line; std::getline(input_stream, line);) {
    parse_line(line, state);
  }
  // TODO: validate problem, convert to minimization if necessary?
  return problem;
}

void parse_line(const std::string &line, ParserState &state) {
  std::vector<std::string_view> tokens = split_tokens_ignore_comments(line);

  if (tokens.empty()) {
    return;
  }

  // Check if section header, handle NAME as special case
  if (std::optional<SectionType> section = detect_section(tokens[0])) {
    state.current_section = *section;
    if (state.current_section == SectionType::Name) {
      if (tokens.size() > 1) {
        state.problem.name = std::string(tokens[1]);
      } else {
        throw_error("NAME section requires a name", state.lineno);
      }
    }
    return;
  }

  switch (state.current_section) {
  case SectionType::ObjectiveName:
    parse_objective_name_line(tokens, state);
    break;
  case SectionType::ObjectiveSense:
    parse_objective_sense_line(tokens, state);
    break;
  case SectionType::Rows:
    parse_row_line(tokens, state);
    break;
  case SectionType::Columns:
    parse_column_line(tokens, state);
    break;
  case SectionType::Rhs:
    parse_rhs_line(tokens, state);
    break;
  case SectionType::Bounds:
    parse_bound_line(tokens, state);
    break;
  default:
    break;
  }
}

std::vector<std::string_view>
split_tokens_ignore_comments(std::string_view line) {
  std::vector<std::string_view> tokens;
  const char *ptr = line.data();
  const char *end_ptr = ptr + line.size();

  // Skip leading whitespace
  while (ptr < end_ptr && string_utils::is_space(*ptr)) {
    ++ptr;
  }

  // Skip blank or comment lines
  if (ptr == end_ptr || *ptr == '*' || *ptr == '$') {
    return tokens;
  }

  // Parse tokens
  while (ptr < end_ptr) {
    // Skip whitespace
    while (ptr < end_ptr && string_utils::is_space(*ptr)) {
      ++ptr;
    }
    if (ptr == end_ptr)
      break;

    // Check for inline comment
    if (*ptr == '$')
      break;

    const char *token_start = ptr;

    // Read token
    while (ptr < end_ptr && !string_utils::is_space(*ptr)) {
      ++ptr;
    }

    tokens.emplace_back(token_start, ptr - token_start);
  }

  return tokens;
}

constexpr std::string_view get_section_header(SectionType section) {
  switch (section) {
  case SectionType::Name:
    return keys::name;
  case SectionType::Rows:
    return keys::rows;
  case SectionType::Columns:
    return keys::columns;
  case SectionType::Rhs:
    return keys::rhs;
  case SectionType::Bounds:
    return keys::bounds;
  case SectionType::EndData:
    return keys::end_data;
  default:
    return keys::empty;
  }
}

std::optional<SectionType> detect_section(std::string_view token) {
  if (token == keys::name)
    return SectionType::Name;
  if (token == keys::rows)
    return SectionType::Rows;
  if (token == keys::columns)
    return SectionType::Columns;
  if (token == keys::rhs)
    return SectionType::Rhs;
  if (token == keys::bounds)
    return SectionType::Bounds;
  if (token == keys::end_data)
    return SectionType::EndData;
  if (token == keys::objective_sense)
    return SectionType::ObjectiveSense;
  if (token == keys::object_name)
    return SectionType::ObjectiveName;
  return std::nullopt;
}

void parse_row_line(const std::vector<std::string_view> &tokens,
                    ParserState &state) {
  if (tokens.size() != 2)
    throw_error("invalid ROWS entry", state.lineno);

  RowType row_type = parse_row_type(tokens[0]);
  if (row_type == RowType::N && !state.problem.objective_name.has_value()) {
    state.problem.objective_name = std::string(tokens[1]);
    return;
  }
  state.problem.row_names.emplace_back(tokens[1]);
  state.problem.row_indices[state.problem.row_names.back()] =
      state.problem.num_rows++;
  state.problem.row_types.push_back(row_type);
  state.problem.rhs_values.push_back(0.0);
}

RowType parse_row_type(std::string_view token, ParserState &state) {
  switch (token[0]) {
  case 'N':
    return RowType::N;
  case 'G':
    return RowType::G;
  case 'L':
    return RowType::L;
  case 'E':
    return RowType::E;
  default:
    throw_error(std::format("unknown row type: '{}'", token[0]), state.lineno);
  }
}

size_t get_or_create_column(std::string_view column_name,
                            ParsedMps &problem) noexcept {
  const auto it = problem.column_indices.find(column_name);
  if (it != problem.column_indices.end()) {
    return it->second;
  }

  const size_t column_index = problem.num_cols++;
  problem.column_names.emplace_back(column_name);
  problem.column_indices[problem.column_names.back()] = column_index;
  problem.variable_bounds.emplace_back();

  return column_index;
}

size_t get_row_index(std::string_view row_name, ParserState &state) {
  const auto it = state.problem.row_indices.find(row_name);
  if (it == state.problem.row_indices.end()) {
    throw_error(std::format("row not found: '{}'", row_name), state.lineno);
  }
  return it->second;
}

void parse_column_line(const std::vector<std::string_view> &tokens,
                       ParserState &state) {
  validate_objective_name(state);
  if (tokens.size() != 3 && tokens.size() != 5) {
    throw_error("invalid COLUMNS entry", state.lineno);
  }
  const size_t column_index = get_or_create_column(tokens[0], state.problem);
  // Parse coefficient pairs
  for (std::size_t i = 1; i < tokens.size(); i += 2) {
    const size_t row_index = get_row_index(tokens[i], state);
    const double value = string_utils::parse_double(tokens[i + 1]);
    state.problem.matrix_entries.push_back({row_index, column_index, value});
  }
}
} // namespace detail
} // namespace mps
