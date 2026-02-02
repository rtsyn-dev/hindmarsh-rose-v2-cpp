#include "hr.h"
#include <cmath>
#include <cstring>

static double set_pts_burst(double burst_duration, double freq, double *dt);
static double select_dt_neuron_model(const double *dts, const double *pts,
                                     size_t length, double pts_live,
                                     double *dt);
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
  size_t steps = state->s_points;
  if (steps == 0) {
    steps = 1;
  }
  if (steps > 10000) {
    steps = 10000;
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
  double freq = 1.0 / state->period_seconds;
  if (state->burst_duration > 0.0) {
    double pts_burst = set_pts_burst(state->burst_duration, freq, &state->dt);
    size_t s_points =
        (size_t)llround(pts_burst / (state->burst_duration * freq));
    if (s_points == 0) {
      s_points = 1;
    }
    state->s_points = s_points;
  } else {
    size_t steps = (size_t)llround(state->period_seconds / state->dt);
    if (steps == 0) {
      steps = 1;
    }
    state->s_points = steps;
  }
}

static double set_pts_burst(double burst_duration, double freq, double *dt) {
  const double dts[144] = {
      0.000500, 0.000600, 0.000700, 0.000800, 0.000900, 0.001000, 0.001100,
      0.001200, 0.001300, 0.001400, 0.001500, 0.001600, 0.001800, 0.002000,
      0.002200, 0.002500, 0.002800, 0.002900, 0.003000, 0.003100, 0.003200,
      0.003300, 0.003400, 0.003500, 0.003600, 0.003700, 0.003800, 0.003900,
      0.004000, 0.004100, 0.004200, 0.004300, 0.004400, 0.004500, 0.004600,
      0.004700, 0.004800, 0.004900, 0.005000, 0.005100, 0.005200, 0.005400,
      0.005600, 0.005800, 0.006000, 0.006200, 0.006400, 0.006600, 0.006800,
      0.007000, 0.007200, 0.007400, 0.007700, 0.008000, 0.008300, 0.008600,
      0.008900, 0.009200, 0.009600, 0.010000, 0.010400, 0.010900, 0.011400,
      0.011900, 0.012500, 0.013100, 0.013800, 0.014600, 0.015400, 0.016300,
      0.017300, 0.018500, 0.019900, 0.021500, 0.023300, 0.025500, 0.028100,
      0.028400, 0.028700, 0.029000, 0.029400, 0.029800, 0.030200, 0.030600,
      0.031000, 0.031400, 0.031800, 0.032200, 0.032600, 0.033000, 0.033400,
      0.033900, 0.034400, 0.034900, 0.035400, 0.035900, 0.036400, 0.036900,
      0.037400, 0.038000, 0.038600, 0.039200, 0.039800, 0.040400, 0.041000,
      0.041700, 0.042400, 0.043100, 0.043800, 0.044500, 0.045300, 0.046100,
      0.046900, 0.047700, 0.048600, 0.049500, 0.050400, 0.051400, 0.052400,
      0.053400, 0.054500, 0.055600, 0.056800, 0.058000, 0.059300, 0.060600,
      0.062000, 0.063400, 0.064900, 0.066500, 0.068200, 0.069900, 0.071700,
      0.073600, 0.075600, 0.077700, 0.079900, 0.082300, 0.084800, 0.087500,
      0.090300, 0.093300, 0.096500, 0.100000,
  };
  const double pts[144] = {
      577638.000000, 481366.000000, 412599.000000, 357615.500000, 317880.000000,
      286092.500000, 259143.333333, 237548.000000, 218869.500000, 203236.000000,
      189687.000000, 177634.000000, 157897.000000, 142001.833333, 129024.142857,
      113496.125000, 101304.555556, 97811.222222,  94527.400000,  91478.200000,
      88619.400000,  85916.636364,  83389.636364,  81007.090909,  78743.583333,
      76615.416667,  74599.250000,  72676.000000,  70859.076923,  69130.846154,
      67476.642857,  65907.357143,  64402.666667,  62971.466667,  61602.533333,
      60286.187500,  59030.250000,  57825.562500,  56664.411765,  55553.294118,
      54485.000000,  52463.222222,  50586.263158,  48841.842105,  47211.050000,
      45685.666667,  44255.818182,  42914.772727,  41650.739130,  40459.083333,
      39335.208333,  38270.680000,  36778.346154,  35398.000000,  34117.571429,
      32926.517241,  31815.833333,  30777.612903,  29493.939394,  28313.588235,
      27223.638889,  25974.405405,  24834.410256,  23790.268293,  22647.767442,
      21609.977778,  20513.166667,  19388.627451,  18381.132075,  17365.719298,
      16361.600000,  15299.937500,  14223.202899,  13164.400000,  12147.123457,
      11098.876404,  10071.693878,  9965.282828,   9861.100000,   9759.059406,
      9626.242718,   9497.009615,   9371.179245,   9248.672897,   9129.293578,
      9012.981818,   8899.594595,   8789.000000,   8681.149123,   8575.896552,
      8473.170940,   8348.176471,   8226.809917,   8108.934426,   7994.379032,
      7883.015873,   7774.710938,   7669.348837,   7566.801527,   7447.308271,
      7331.525926,   7219.291971,   7110.435714,   7004.816901,   6902.298611,
      6786.417808,   6674.355705,   6565.940397,   6460.987013,   6359.339744,
      6247.012579,   6138.592593,   6033.866667,   5932.660714,   5822.777778,
      5716.896552,   5614.796610,   5505.541436,   5400.467391,   5299.324468,
      5192.348958,   5089.615385,   4982.075000,   4878.985294,   4772.014354,
      4669.633803,   4564.178899,   4463.381166,   4360.214912,   4255.294872,
      4149.216667,   4048.296748,   3946.654762,   3844.764479,   3743.041353,
      3641.872263,   3541.583630,   3438.300000,   3336.926421,   3233.951299,
      3133.666667,   3032.899696,   2932.320588,   2829.684659,
  };
  return select_dt_neuron_model(dts, pts, 144, burst_duration * freq, dt);
}

static double select_dt_neuron_model(const double *dts, const double *pts,
                                     size_t length, double pts_live,
                                     double *dt) {
  double aux = pts_live;
  double factor = 1.0;
  double pts_burst = -1.0;
  double selected_dt = -1.0;
  int flag = 0;

  while (aux < pts[0]) {
    aux = pts_live * factor;
    factor += 1.0;
    for (size_t i = length; i-- > 0;) {
      if (pts[i] > aux) {
        selected_dt = dts[i];
        pts_burst = pts[i];
        double ratio = pts_burst / pts_live;
        double fract = ratio - floor(ratio);
        double intp = floor(ratio);
        if (fract <= 0.1 * intp) {
          flag = 1;
        }
        break;
      }
    }
    if (flag) {
      break;
    }
  }

  if (!flag) {
    for (size_t i = length; i-- > 0;) {
      if (pts[i] > aux) {
        selected_dt = dts[i];
        pts_burst = pts[i];
        break;
      }
    }
  }

  if (selected_dt > 0.0) {
    *dt = selected_dt;
  }
  return pts_burst;
}
