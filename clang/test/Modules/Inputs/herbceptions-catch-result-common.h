#pragma once

struct module_result_value {
  int *destructions;

  explicit module_result_value(int *count) : destructions(count) {}
  module_result_value(const module_result_value &) = delete;
  module_result_value(module_result_value &&) = delete;
  ~module_result_value() { ++*destructions; }
};

inline module_result_value module_result_source(int *destructions,
                                                bool fail) return_failure{int} {
  if (fail)
    return_failure 29;
  return module_result_value(destructions);
}
