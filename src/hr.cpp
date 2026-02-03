#include "hr.h"
#include <cmath>
#include <cstring>

static void update_burst_settings(hr_state_cpp_t *state);

extern "C" void hr_init(hr_state_cpp_t *state) {
  state->x = -0.9013747551021072;
  state->y = -3.15948829665501;
  state->z = 3.247826955037619;
  state->input_syn = 0.0;
  state->e = 3.25;
  state->mu = 0.006;
  state->s = 4.0;
  state->vh = 1.0;
  state->dt = 0.0015;
  state->burst_duration = 1.0;
  state->period_seconds = 0.001;
  state->s_points = 1;
  state->cfg_x = state->x;
  state->cfg_y = state->y;
  state->cfg_z = state->z;
}

extern "C" void hr_set_config(hr_state_cpp_t *state, const char *key,
                              size_t len, double value) {
  if (len == 1 && strncmp(key, "x", len) == 0) {
    state->cfg_x = value;
    state->x = value;
  } else if (len == 1 && strncmp(key, "y", len) == 0) {
    state->cfg_y = value;
    state->y = value;
  } else if (len == 1 && strncmp(key, "z", len) == 0) {
    state->cfg_z = value;
    state->z = value;
  } else if (len == 1 && strncmp(key, "e", len) == 0) {
    state->e = value;
  } else if (len == 2 && strncmp(key, "mu", len) == 0) {
    state->mu = value;
  } else if (len == 1 && strncmp(key, "s", len) == 0) {
    state->s = value;
  } else if (len == 2 && strncmp(key, "vh", len) == 0) {
    state->vh = value;
  } else if (len == 14 && strncmp(key, "time_increment", len) == 0) {
    state->dt = value < 0.0 ? 0.0 : value;
  } else if (len == 14 && strncmp(key, "burst_duration", len) == 0) {
    state->burst_duration = value;
  } else if (len == 14 && strncmp(key, "period_seconds", len) == 0) {
    state->period_seconds = value;
  }
  update_burst_settings(state);
}

extern "C" void hr_set_input(hr_state_cpp_t *state, const char *name,
                             size_t len, double value) {
  if (len == 5 && strncmp(name, "i_syn", len) == 0) {
    state->input_syn = value;
  }
}

extern "C" void hr_process(hr_state_cpp_t *state) {
  // Limit steps for real-time performance
  size_t steps = state->s_points;
  if (steps == 0) {
    steps = 1;
  }
  if (steps > 50) { // Hard limit for real-time performance
    steps = 50;
  }
  for (size_t step = 0; step < steps; ++step) {
    double vars[3] = {state->x, state->y, state->z};
    double k[6][3];
    double aux[3];

    auto eval = [&](const double in[3], double out[3]) {
      double v = in[1] + 3.0 * in[0] * in[0] - in[0] * in[0] * in[0] -
                 state->vh * in[2] + state->e - state->input_syn;
      double ydot = 1.0 - 5.0 * in[0] * in[0] - in[1];
      double zdot = state->mu * (-state->vh * in[2] + state->s * (in[0] + 1.6));
      out[0] = v;
      out[1] = ydot;
      out[2] = zdot;
    };

    double r[3];
    eval(vars, r);
    for (int j = 0; j < 3; j++) {
      k[0][j] = state->dt * r[j];
      aux[j] = vars[j] + k[0][j] * 0.2;
    }

    eval(aux, r);
    for (int j = 0; j < 3; j++) {
      k[1][j] = state->dt * r[j];
      aux[j] = vars[j] + k[0][j] * 0.075 + k[1][j] * 0.225;
    }

    eval(aux, r);
    for (int j = 0; j < 3; j++) {
      k[2][j] = state->dt * r[j];
      aux[j] = vars[j] + k[0][j] * 0.3 - k[1][j] * 0.9 + k[2][j] * 1.2;
    }

    eval(aux, r);
    for (int j = 0; j < 3; j++) {
      k[3][j] = state->dt * r[j];
      aux[j] = vars[j] + k[0][j] * 0.075 + k[1][j] * 0.675 - k[2][j] * 0.6 +
               k[3][j] * 0.75;
    }

    eval(aux, r);
    for (int j = 0; j < 3; j++) {
      k[4][j] = state->dt * r[j];
      aux[j] = vars[j] + k[0][j] * 0.660493827160493 + k[1][j] * 2.5 -
               k[2][j] * 5.185185185185185 + k[3][j] * 3.888888888888889 -
               k[4][j] * 0.864197530864197;
    }

    eval(aux, r);
    for (int j = 0; j < 3; j++) {
      k[5][j] = state->dt * r[j];
    }

    for (int j = 0; j < 3; j++) {
      vars[j] += k[0][j] * 0.098765432098765 + k[2][j] * 0.396825396825396 +
                 k[3][j] * 0.231481481481481 + k[4][j] * 0.308641975308641 -
                 k[5][j] * 0.035714285714285;
    }

    state->x = vars[0];
    state->y = vars[1];
    state->z = vars[2];
  }
}

static void update_burst_settings(hr_state_cpp_t *state) {
  if (state->period_seconds <= 0.0) {
    state->s_points = 1;
    return;
  }

  // For real-time performance, limit integration steps regardless of frequency
  if (state->burst_duration > 0.0) {
    // Use fixed steps for burst mode to ensure consistent performance
    state->s_points = 1;
  } else {
    // Limit steps to maintain real-time performance
    const size_t max_steps =
        10; // Maximum steps per tick for real-time performance
    size_t desired_steps = (size_t)llround(state->period_seconds / state->dt);
    if (desired_steps == 0) {
      desired_steps = 1;
    }

    if (desired_steps > max_steps) {
      // Adapt dt to maintain step count instead of increasing steps
      state->dt = state->period_seconds / max_steps;
      state->s_points = max_steps;
    } else {
      state->s_points = desired_steps;
    }
  }
}
