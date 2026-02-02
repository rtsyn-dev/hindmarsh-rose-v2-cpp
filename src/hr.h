#pragma once
#include <cstddef>

extern "C" {

typedef struct hr_state_cpp {
  double x;
  double y;
  double z;
  double input_syn;
  double e;
  double mu;
  double s;
  double vh;
  double dt;
  double burst_duration;
  double period_seconds;
  size_t s_points;
  double cfg_x;
  double cfg_y;
  double cfg_z;
} hr_state_cpp_t;

void hr_init(hr_state_cpp_t *state);
void hr_set_config(hr_state_cpp_t *state, const char *key, size_t len,
                   double value);
void hr_set_input(hr_state_cpp_t *state, const char *name, size_t len,
                  double value);
void hr_process(hr_state_cpp_t *state);
}
