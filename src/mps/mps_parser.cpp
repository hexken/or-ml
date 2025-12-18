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

[[noreturn]] void throw_error(std::string_view message, size_t lineno) {
  throw std::runtime_error(std::format("Line {}: {}", lineno, message));
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

  if (std::optional<SectionType> section = detect_section(tokens[0])) {
    state.current_section = *section;
    if (state.current_section == SectionType::Name) {
      if (tokens.size() > 1) {
        state.problem.name = std::string(tokens[1]);
      } else {
        throw_error("NAME section requires a name", state);
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

  int row_index{prob.num_rows++};

  mps::RowType row_type;
  char type_char{tokens[0][0]};
  switch (type_char) {
  case 'N':
    row_type = mps::RowType::n;
    if (prob.objective_row == -1) {
      prob.objective_row = row_index;
      prob.objective_name = tokens[1];
    } else if (prob.row_types[prob.obj_row] != mps::RowType::n) {

      throw std::runtime_error(
          std::format("Line {} : objective row not declared as N", lineno));
    }
    break;
  case 'G':
    row_type = mps::RowType::g;
    break;
  case 'L':
    row_type = mps::RowType::l;
    break;
  case 'E':
    row_type = mps::RowType::e;
    break;
  default:
    throw std::runtime_error(
        std::format("Line {} : unknown row type '{}'", lineno, type_char));
  }

  // TODO: make sure we don't put the objective row in the final constraint
  // matrix
  // Register row
  prob.row_names.emplace_back(tokens[1]);
  prob.row_indices[prob.row_names.back()] = row_index;
  prob.row_types.push_back(row_type);
  prob.rhs_values.push_back(0.0);
}

RowType parse_row_type(std::string_view type_str) {
  if (type_str.empty()) {
    throw_error("Empty row type");
  }

  switch (type_str[0]) {
  case 'N':
    return RowType::N;
  case 'G':
    return RowType::G;
  case 'L':
    return RowType::L;
  case 'E':
    return RowType::E;
  default:
    throw_error(std::format("Unknown row type: '{}'", type_str));
  }
}
} // namespace detail
} // namespace mps
