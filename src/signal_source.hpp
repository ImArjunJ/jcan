#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace jcan {

enum class source_mode : uint8_t { constant, waveform, table, expression };
enum class waveform_type : uint8_t { sine, ramp, square, triangle };

struct waveform_params {
  waveform_type type{waveform_type::sine};
  double min_val{0.0};
  double max_val{1.0};
  double period_sec{1.0};

  [[nodiscard]] double evaluate(double t) const {
    double p = std::max(period_sec, 0.001);
    double frac = std::fmod(t / p, 1.0);
    if (frac < 0.0) frac += 1.0;
    double range = max_val - min_val;

    switch (type) {
      case waveform_type::sine:
        return min_val +
               range * 0.5 * (1.0 + std::sin(2.0 * std::numbers::pi * frac));
      case waveform_type::ramp:
        return min_val + range * frac;
      case waveform_type::square:
        return frac < 0.5 ? max_val : min_val;
      case waveform_type::triangle:
        return frac < 0.5 ? min_val + range * 2.0 * frac
                          : max_val - range * 2.0 * (frac - 0.5);
    }
    return min_val;
  }
};

struct table_point {
  double time_sec{0.0};
  double value{0.0};
  bool hold{true};
};

struct table_params {
  std::vector<table_point> points;

  [[nodiscard]] double evaluate(double t) const {
    if (points.empty()) return 0.0;
    if (points.size() == 1) return points[0].value;
    if (t <= points.front().time_sec) return points.front().value;
    if (t >= points.back().time_sec) return points.back().value;

    auto it = std::lower_bound(
        points.begin(), points.end(), t,
        [](const table_point& p, double tv) { return p.time_sec < tv; });

    if (it == points.begin()) return points.front().value;
    auto hi = it;
    auto lo = std::prev(it);

    if (lo->hold) return lo->value;
    double dt = hi->time_sec - lo->time_sec;
    if (dt <= 0.0) return lo->value;
    double alpha = (t - lo->time_sec) / dt;
    return lo->value + alpha * (hi->value - lo->value);
  }
};

namespace detail {

enum class expr_op : uint8_t {
  literal,
  var_t,
  add,
  sub,
  mul,
  div_op,
  pow_op,
  neg,
  fn_sin,
  fn_cos,
  fn_abs,
  fn_sqrt,
  fn_min,
  fn_max,
  fn_clamp,
  fn_pow,
};

struct expr_node {
  expr_op op{expr_op::literal};
  double value{0.0};
  std::vector<std::unique_ptr<expr_node>> children;

  [[nodiscard]] double eval(double t) const {
    switch (op) {
      case expr_op::literal:
        return value;
      case expr_op::var_t:
        return t;
      case expr_op::add:
        return child(0, t) + child(1, t);
      case expr_op::sub:
        return child(0, t) - child(1, t);
      case expr_op::mul:
        return child(0, t) * child(1, t);
      case expr_op::div_op: {
        double d = child(1, t);
        return d == 0.0 ? 0.0 : child(0, t) / d;
      }
      case expr_op::pow_op:
        return std::pow(child(0, t), child(1, t));
      case expr_op::neg:
        return -child(0, t);
      case expr_op::fn_sin:
        return std::sin(child(0, t));
      case expr_op::fn_cos:
        return std::cos(child(0, t));
      case expr_op::fn_abs:
        return std::abs(child(0, t));
      case expr_op::fn_sqrt: {
        double v = child(0, t);
        return v >= 0.0 ? std::sqrt(v) : 0.0;
      }
      case expr_op::fn_min:
        return std::min(child(0, t), child(1, t));
      case expr_op::fn_max:
        return std::max(child(0, t), child(1, t));
      case expr_op::fn_pow:
        return std::pow(child(0, t), child(1, t));
      case expr_op::fn_clamp:
        return std::clamp(child(0, t), child(1, t), child(2, t));
    }
    return 0.0;
  }

 private:
  [[nodiscard]] double child(int i, double t) const {
    if (i < static_cast<int>(children.size()) && children[i])
      return children[i]->eval(t);
    return 0.0;
  }
};

using expr_ptr = std::unique_ptr<expr_node>;

inline expr_ptr make_literal(double v) {
  auto n = std::make_unique<expr_node>();
  n->op = expr_op::literal;
  n->value = v;
  return n;
}

inline expr_ptr make_var_t() {
  auto n = std::make_unique<expr_node>();
  n->op = expr_op::var_t;
  return n;
}

inline expr_ptr make_unary(expr_op op, expr_ptr a) {
  auto n = std::make_unique<expr_node>();
  n->op = op;
  n->children.push_back(std::move(a));
  return n;
}

inline expr_ptr make_binary(expr_op op, expr_ptr a, expr_ptr b) {
  auto n = std::make_unique<expr_node>();
  n->op = op;
  n->children.push_back(std::move(a));
  n->children.push_back(std::move(b));
  return n;
}

inline expr_ptr make_ternary(expr_op op, expr_ptr a, expr_ptr b, expr_ptr c) {
  auto n = std::make_unique<expr_node>();
  n->op = op;
  n->children.push_back(std::move(a));
  n->children.push_back(std::move(b));
  n->children.push_back(std::move(c));
  return n;
}

struct expr_parser {
  std::string_view input;
  std::size_t pos{0};
  std::string error;

  void skip_ws() {
    while (pos < input.size() && (input[pos] == ' ' || input[pos] == '\t'))
      ++pos;
  }

  [[nodiscard]] bool at_end() const { return pos >= input.size(); }

  [[nodiscard]] char peek() const { return at_end() ? '\0' : input[pos]; }

  bool match(char c) {
    skip_ws();
    if (peek() == c) {
      ++pos;
      return true;
    }
    return false;
  }

  expr_ptr parse_expr() {
    auto left = parse_term();
    if (!left) return nullptr;
    skip_ws();
    while (peek() == '+' || peek() == '-') {
      char op = input[pos++];
      auto right = parse_term();
      if (!right) return nullptr;
      left = make_binary(op == '+' ? expr_op::add : expr_op::sub,
                         std::move(left), std::move(right));
      skip_ws();
    }
    return left;
  }

  expr_ptr parse_term() {
    auto left = parse_unary();
    if (!left) return nullptr;
    skip_ws();
    while (peek() == '*' || peek() == '/') {
      char op = input[pos++];
      auto right = parse_unary();
      if (!right) return nullptr;
      left = make_binary(op == '*' ? expr_op::mul : expr_op::div_op,
                         std::move(left), std::move(right));
      skip_ws();
    }
    return left;
  }

  expr_ptr parse_unary() {
    skip_ws();
    if (match('-')) {
      auto operand = parse_unary();
      if (!operand) return nullptr;
      return make_unary(expr_op::neg, std::move(operand));
    }
    return parse_power();
  }

  expr_ptr parse_power() {
    auto base = parse_primary();
    if (!base) return nullptr;
    skip_ws();
    if (match('^')) {
      auto exp = parse_unary();
      if (!exp) return nullptr;
      return make_binary(expr_op::pow_op, std::move(base), std::move(exp));
    }
    return base;
  }

  expr_ptr parse_primary() {
    skip_ws();
    char c = peek();

    if ((c >= '0' && c <= '9') || c == '.') {
      return parse_number();
    }

    if (c == '(') {
      ++pos;
      auto inner = parse_expr();
      if (!inner) return nullptr;
      if (!match(')')) {
        error = "expected ')'";
        return nullptr;
      }
      return inner;
    }

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
      auto ident = parse_ident();

      if (ident == "t") return make_var_t();
      if (ident == "pi") return make_literal(std::numbers::pi);
      if (ident == "e") return make_literal(std::numbers::e);

      skip_ws();
      if (!match('(')) {
        error = "expected '(' after '" + ident + "'";
        return nullptr;
      }
      auto args = parse_args();
      if (!error.empty()) return nullptr;
      if (!match(')')) {
        error = "expected ')'";
        return nullptr;
      }

      return dispatch_func(ident, std::move(args));
    }

    error = std::string("unexpected character: '") + c + "'";
    return nullptr;
  }

  expr_ptr parse_number() {
    std::size_t start = pos;
    while (pos < input.size() &&
           ((input[pos] >= '0' && input[pos] <= '9') || input[pos] == '.'))
      ++pos;
    if (pos < input.size() && (input[pos] == 'e' || input[pos] == 'E')) {
      ++pos;
      if (pos < input.size() && (input[pos] == '+' || input[pos] == '-')) ++pos;
      while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9')
        ++pos;
    }
    std::string num_str(input.substr(start, pos - start));
    double val = 0.0;
    try {
      val = std::stod(num_str);
    } catch (...) {
      error = "invalid number: " + num_str;
      return nullptr;
    }
    return make_literal(val);
  }

  std::string parse_ident() {
    std::size_t start = pos;
    while (pos < input.size() &&
           ((input[pos] >= 'a' && input[pos] <= 'z') ||
            (input[pos] >= 'A' && input[pos] <= 'Z') ||
            (input[pos] >= '0' && input[pos] <= '9') || input[pos] == '_'))
      ++pos;
    return std::string(input.substr(start, pos - start));
  }

  std::vector<expr_ptr> parse_args() {
    std::vector<expr_ptr> args;
    skip_ws();
    if (peek() == ')') return args;
    auto first = parse_expr();
    if (!first) return args;
    args.push_back(std::move(first));
    while (match(',')) {
      auto next = parse_expr();
      if (!next) return args;
      args.push_back(std::move(next));
    }
    return args;
  }

  expr_ptr dispatch_func(const std::string& name, std::vector<expr_ptr> args) {
    if (name == "sin" || name == "cos" || name == "abs" || name == "sqrt") {
      if (args.size() != 1) {
        error = name + "() expects 1 argument";
        return nullptr;
      }
      expr_op op = expr_op::fn_sin;
      if (name == "cos")
        op = expr_op::fn_cos;
      else if (name == "abs")
        op = expr_op::fn_abs;
      else if (name == "sqrt")
        op = expr_op::fn_sqrt;
      return make_unary(op, std::move(args[0]));
    }
    if (name == "min" || name == "max" || name == "pow") {
      if (args.size() != 2) {
        error = name + "() expects 2 arguments";
        return nullptr;
      }
      expr_op op = expr_op::fn_min;
      if (name == "max")
        op = expr_op::fn_max;
      else if (name == "pow")
        op = expr_op::fn_pow;
      return make_binary(op, std::move(args[0]), std::move(args[1]));
    }
    if (name == "clamp") {
      if (args.size() != 3) {
        error = "clamp() expects 3 arguments";
        return nullptr;
      }
      return make_ternary(expr_op::fn_clamp, std::move(args[0]),
                          std::move(args[1]), std::move(args[2]));
    }
    error = "unknown function: " + name;
    return nullptr;
  }
};

}  // namespace detail

struct expression_params {
  std::string text;
  std::shared_ptr<detail::expr_node> ast_;
  std::string error;

  void compile() {
    error.clear();
    ast_.reset();
    if (text.empty()) return;
    detail::expr_parser parser{text, 0, {}};
    auto node = parser.parse_expr();
    if (!node || !parser.error.empty()) {
      error = parser.error.empty() ? "parse error" : parser.error;
      return;
    }
    parser.skip_ws();
    if (!parser.at_end()) {
      error = "unexpected characters after expression";
      return;
    }
    ast_ = std::move(node);
  }

  [[nodiscard]] double evaluate(double t) const {
    if (!ast_) return 0.0;
    double v = ast_->eval(t);
    if (std::isnan(v) || std::isinf(v)) return 0.0;
    return v;
  }
};

struct signal_source {
  source_mode mode{source_mode::constant};
  double constant_value{0.0};
  waveform_params waveform;
  table_params table;
  expression_params expression;
  bool repeat{false};

  [[nodiscard]] double evaluate(double t_sec) const {
    switch (mode) {
      case source_mode::constant:
        return constant_value;

      case source_mode::waveform: {
        double t = t_sec;
        if (!repeat && waveform.period_sec > 0.0)
          t = std::min(t, waveform.period_sec);
        return waveform.evaluate(t);
      }

      case source_mode::table: {
        if (table.points.empty()) return 0.0;
        double t = t_sec;
        if (repeat && table.points.size() >= 2) {
          double dur = table.points.back().time_sec;
          if (dur > 0.0) t = std::fmod(t, dur);
        }
        return table.evaluate(t);
      }

      case source_mode::expression:
        return expression.evaluate(t_sec);
    }
    return 0.0;
  }

  void preview(float* out, int count, double duration_sec) const {
    if (count <= 0) return;
    for (int i = 0; i < count; ++i) {
      double t = (count > 1) ? duration_sec * static_cast<double>(i) /
                                   static_cast<double>(count - 1)
                             : 0.0;
      out[i] = static_cast<float>(evaluate(t));
    }
  }

  [[nodiscard]] double preview_duration() const {
    switch (mode) {
      case source_mode::constant:
        return 1.0;
      case source_mode::waveform:
        return repeat ? waveform.period_sec * 2.0 : waveform.period_sec;
      case source_mode::table:
        if (table.points.size() >= 2) {
          double dur = table.points.back().time_sec;
          return repeat ? dur * 2.0 : dur;
        }
        return 1.0;
      case source_mode::expression:
        return 5.0;
    }
    return 1.0;
  }
};

}  // namespace jcan
