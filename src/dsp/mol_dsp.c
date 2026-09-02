/* SPDX-License-Identifier: Apache-2.0 */
#include "mol_dsp.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define MOL_DSP_PI 3.14159265358979323846f
#define MOL_DSP_TWO_PI (2.0f * MOL_DSP_PI)

static const float mol_dsp_quarter_sine[17] = {
    0.0f,          0.0980171403f, 0.1950903220f, 0.2902846773f, 0.3826834324f, 0.4713967368f,
    0.5555702330f, 0.6343932842f, 0.7071067812f, 0.7730104534f, 0.8314696123f, 0.8819212643f,
    0.9238795325f, 0.9569403357f, 0.9807852804f, 0.9951847267f, 1.0f};

float mol_dsp_clamp(float value, float minimum, float maximum) {
  return value < minimum ? minimum : (value > maximum ? maximum : value);
}

float mol_dsp_db_to_linear(float decibels) { return powf(10.0f, decibels / 20.0f); }

float mol_dsp_linear_to_db(float linear) {
  return linear > FLT_MIN ? 20.0f * log10f(linear) : -160.0f;
}

static float mol_dsp_wrap_phase(float phase) {
  phase -= floorf(phase);
  return phase < 0.0f ? phase + 1.0f : phase;
}

float mol_dsp_phase_advance(float* phase, float increment) {
  float current;
  if (phase == NULL) {
    return 0.0f;
  }
  current = mol_dsp_wrap_phase(*phase);
  *phase = mol_dsp_wrap_phase(current + increment);
  return current;
}

float mol_dsp_sine(float phase) {
  float scaled;
  float local;
  float fraction;
  float first;
  float second;
  uint32_t quadrant;
  uint32_t index;
  phase = mol_dsp_wrap_phase(phase);
  scaled = phase * 64.0f;
  quadrant = (uint32_t)scaled / 16u;
  if (quadrant > 3u) {
    quadrant = 3u;
  }
  local = scaled - (float)(quadrant * 16u);
  if (quadrant == 1u || quadrant == 3u) {
    local = 16.0f - local;
  }
  index = (uint32_t)local;
  if (index >= 16u) {
    first = mol_dsp_quarter_sine[16];
    second = first;
    fraction = 0.0f;
  } else {
    first = mol_dsp_quarter_sine[index];
    second = mol_dsp_quarter_sine[index + 1u];
    fraction = local - (float)index;
  }
  first += (second - first) * fraction;
  return quadrant >= 2u ? -first : first;
}

static float mol_dsp_polyblep(float phase, float increment) {
  if (increment <= 0.0f) {
    return 0.0f;
  }
  if (phase < increment) {
    float value = phase / increment;
    return value + value - value * value - 1.0f;
  }
  if (phase > 1.0f - increment) {
    float value = (phase - 1.0f) / increment;
    return value * value + value + value + 1.0f;
  }
  return 0.0f;
}

float mol_dsp_polyblep_saw(float phase, float increment) {
  phase = mol_dsp_wrap_phase(phase);
  return 2.0f * phase - 1.0f - mol_dsp_polyblep(phase, increment);
}

float mol_dsp_polyblep_pulse(float phase, float increment, float width) {
  float shifted;
  float sample;
  phase = mol_dsp_wrap_phase(phase);
  width = mol_dsp_clamp(width, 0.05f, 0.95f);
  sample = phase < width ? 1.0f : -1.0f;
  sample += mol_dsp_polyblep(phase, increment);
  shifted = mol_dsp_wrap_phase(phase - width);
  sample -= mol_dsp_polyblep(shifted, increment);
  return sample;
}

float mol_dsp_polyblep_square(float phase, float increment) {
  return mol_dsp_polyblep_pulse(phase, increment, 0.5f);
}

float mol_dsp_triangle(float phase) {
  phase = mol_dsp_wrap_phase(phase);
  return 1.0f - 4.0f * fabsf(phase - 0.5f);
}

float mol_dsp_white_noise(uint32_t* state) {
  uint32_t value;
  if (state == NULL) {
    return 0.0f;
  }
  value = *state != 0u ? *state : UINT32_C(0x6D2B79F5);
  value ^= value << 13u;
  value ^= value >> 17u;
  value ^= value << 5u;
  *state = value;
  return (float)((int32_t)(value >> 8u) - INT32_C(0x00800000)) / (float)INT32_C(0x00800000);
}

float mol_dsp_pink_noise(uint32_t* state, float* memory) {
  float white;
  if (memory == NULL) {
    return 0.0f;
  }
  white = mol_dsp_white_noise(state);
  *memory = 0.985f * *memory + 0.15f * white;
  return mol_dsp_clamp(*memory, -1.0f, 1.0f);
}

static uint32_t mol_dsp_seconds_to_frames(uint32_t sample_rate, float seconds) {
  double frames = (double)sample_rate * (double)seconds;
  if (frames < 1.0) {
    return 1u;
  }
  if (frames > (double)UINT32_MAX) {
    return UINT32_MAX;
  }
  return (uint32_t)(frames + 0.5);
}

void mol_dsp_adsr_configure(mol_dsp_adsr_t* envelope, uint32_t sample_rate, float attack_seconds,
                            float decay_seconds, float sustain, float release_seconds) {
  uint32_t attack_frames;
  uint32_t decay_frames;
  if (envelope == NULL || sample_rate == 0u) {
    return;
  }
  memset(envelope, 0, sizeof(*envelope));
  attack_frames = mol_dsp_seconds_to_frames(sample_rate, attack_seconds);
  decay_frames = mol_dsp_seconds_to_frames(sample_rate, decay_seconds);
  envelope->sustain = mol_dsp_clamp(sustain, 0.0f, 1.0f);
  envelope->attack_step = 1.0f / (float)attack_frames;
  envelope->decay_step = (1.0f - envelope->sustain) / (float)decay_frames;
  envelope->release_frames = mol_dsp_seconds_to_frames(sample_rate, release_seconds);
}

void mol_dsp_adsr_note_on(mol_dsp_adsr_t* envelope) {
  if (envelope != NULL) {
    envelope->stage = MOL_DSP_ENVELOPE_ATTACK;
  }
}

void mol_dsp_adsr_note_off(mol_dsp_adsr_t* envelope) {
  if (envelope != NULL && envelope->stage != MOL_DSP_ENVELOPE_IDLE &&
      envelope->stage != MOL_DSP_ENVELOPE_RELEASE) {
    envelope->release_step = envelope->value / (float)envelope->release_frames;
    envelope->stage = MOL_DSP_ENVELOPE_RELEASE;
  }
}

float mol_dsp_adsr_process(mol_dsp_adsr_t* envelope) {
  if (envelope == NULL) {
    return 0.0f;
  }
  switch (envelope->stage) {
    case MOL_DSP_ENVELOPE_ATTACK:
      envelope->value += envelope->attack_step;
      if (envelope->value >= 0.999999f) {
        envelope->value = 1.0f;
        envelope->stage = MOL_DSP_ENVELOPE_DECAY;
      }
      break;
    case MOL_DSP_ENVELOPE_DECAY:
      envelope->value -= envelope->decay_step;
      if (envelope->value <= envelope->sustain + 0.000001f) {
        envelope->value = envelope->sustain;
        envelope->stage = MOL_DSP_ENVELOPE_SUSTAIN;
      }
      break;
    case MOL_DSP_ENVELOPE_RELEASE:
      envelope->value -= envelope->release_step;
      if (envelope->value <= 0.000001f || !isfinite(envelope->value)) {
        envelope->value = 0.0f;
        envelope->stage = MOL_DSP_ENVELOPE_IDLE;
      }
      break;
    case MOL_DSP_ENVELOPE_SUSTAIN:
    case MOL_DSP_ENVELOPE_IDLE:
    default:
      break;
  }
  return envelope->value;
}

static void mol_dsp_segment_begin(mol_dsp_segment_envelope_t* envelope) {
  if (envelope->segment >= envelope->count) {
    envelope->remaining = 0u;
    envelope->step = 0.0f;
    return;
  }
  envelope->remaining = envelope->frames[envelope->segment];
  if (envelope->remaining == 0u) {
    envelope->remaining = 1u;
  }
  envelope->step =
      (envelope->targets[envelope->segment] - envelope->value) / (float)envelope->remaining;
}

void mol_dsp_segment_envelope_configure(mol_dsp_segment_envelope_t* envelope, const float* targets,
                                        const uint32_t* frames, uint32_t count,
                                        float initial_value) {
  if (envelope == NULL) {
    return;
  }
  memset(envelope, 0, sizeof(*envelope));
  envelope->count = count < MOL_DSP_MAX_ENVELOPE_SEGMENTS ? count : MOL_DSP_MAX_ENVELOPE_SEGMENTS;
  envelope->value = initial_value;
  for (uint32_t index = 0u; index < envelope->count; ++index) {
    envelope->targets[index] = targets != NULL ? targets[index] : initial_value;
    envelope->frames[index] = frames != NULL ? frames[index] : 1u;
  }
  mol_dsp_segment_begin(envelope);
}

float mol_dsp_segment_envelope_process(mol_dsp_segment_envelope_t* envelope) {
  if (envelope == NULL || envelope->segment >= envelope->count) {
    return envelope != NULL ? envelope->value : 0.0f;
  }
  envelope->value += envelope->step;
  --envelope->remaining;
  if (envelope->remaining == 0u) {
    envelope->value = envelope->targets[envelope->segment];
    ++envelope->segment;
    mol_dsp_segment_begin(envelope);
  }
  return envelope->value;
}

void mol_dsp_smoother_configure(mol_dsp_smoother_t* smoother, uint32_t sample_rate, float seconds,
                                float initial_value) {
  if (smoother == NULL || sample_rate == 0u) {
    return;
  }
  smoother->value = initial_value;
  smoother->target = initial_value;
  smoother->coefficient =
      seconds <= 0.0f ? 1.0f : 1.0f - expf(-1.0f / (seconds * (float)sample_rate));
}

void mol_dsp_smoother_set_target(mol_dsp_smoother_t* smoother, float target) {
  if (smoother != NULL) {
    smoother->target = target;
  }
}

float mol_dsp_smoother_process(mol_dsp_smoother_t* smoother) {
  if (smoother == NULL) {
    return 0.0f;
  }
  smoother->value += smoother->coefficient * (smoother->target - smoother->value);
  return smoother->value;
}

void mol_dsp_state_variable_configure(mol_dsp_state_variable_filter_t* filter, uint32_t sample_rate,
                                      float cutoff_hz, float resonance) {
  float maximum;
  if (filter == NULL || sample_rate == 0u) {
    return;
  }
  maximum = 0.45f * (float)sample_rate;
  cutoff_hz = mol_dsp_clamp(cutoff_hz, 5.0f, maximum);
  resonance = mol_dsp_clamp(resonance, 0.0f, 0.99f);
  filter->g = tanf(MOL_DSP_PI * cutoff_hz / (float)sample_rate);
  filter->k = 2.0f - 1.98f * resonance;
}

float mol_dsp_state_variable_process(mol_dsp_state_variable_filter_t* filter, float input,
                                     mol_dsp_filter_output_t output) {
  float denominator;
  float high;
  float band;
  float low;
  if (filter == NULL) {
    return 0.0f;
  }
  denominator = 1.0f + filter->g * (filter->g + filter->k);
  high = (input - filter->k * filter->integrator_1 - filter->integrator_2) / denominator;
  band = filter->g * high + filter->integrator_1;
  low = filter->g * band + filter->integrator_2;
  filter->integrator_1 = filter->g * high + band;
  filter->integrator_2 = filter->g * band + low;
  if (!isfinite(low) || !isfinite(band) || !isfinite(high)) {
    filter->integrator_1 = 0.0f;
    filter->integrator_2 = 0.0f;
    return 0.0f;
  }
  return output == MOL_DSP_FILTER_HIGH_PASS ? high
                                            : (output == MOL_DSP_FILTER_BAND_PASS ? band : low);
}

static void mol_dsp_biquad_set(mol_dsp_biquad_t* filter, float b0, float b1, float b2, float a0,
                               float a1, float a2) {
  filter->b0 = b0 / a0;
  filter->b1 = b1 / a0;
  filter->b2 = b2 / a0;
  filter->a1 = a1 / a0;
  filter->a2 = a2 / a0;
}

void mol_dsp_biquad_low_pass(mol_dsp_biquad_t* filter, uint32_t sample_rate, float cutoff_hz,
                             float q) {
  float cosine;
  float alpha;
  float omega;
  if (filter == NULL || sample_rate == 0u) {
    return;
  }
  cutoff_hz = mol_dsp_clamp(cutoff_hz, 5.0f, 0.45f * (float)sample_rate);
  q = mol_dsp_clamp(q, 0.1f, 20.0f);
  omega = MOL_DSP_TWO_PI * cutoff_hz / (float)sample_rate;
  cosine = cosf(omega);
  alpha = sinf(omega) / (2.0f * q);
  mol_dsp_biquad_set(filter, (1.0f - cosine) * 0.5f, 1.0f - cosine, (1.0f - cosine) * 0.5f,
                     1.0f + alpha, -2.0f * cosine, 1.0f - alpha);
}

void mol_dsp_biquad_high_pass(mol_dsp_biquad_t* filter, uint32_t sample_rate, float cutoff_hz,
                              float q) {
  float cosine;
  float alpha;
  float omega;
  if (filter == NULL || sample_rate == 0u) {
    return;
  }
  cutoff_hz = mol_dsp_clamp(cutoff_hz, 5.0f, 0.45f * (float)sample_rate);
  q = mol_dsp_clamp(q, 0.1f, 20.0f);
  omega = MOL_DSP_TWO_PI * cutoff_hz / (float)sample_rate;
  cosine = cosf(omega);
  alpha = sinf(omega) / (2.0f * q);
  mol_dsp_biquad_set(filter, (1.0f + cosine) * 0.5f, -(1.0f + cosine), (1.0f + cosine) * 0.5f,
                     1.0f + alpha, -2.0f * cosine, 1.0f - alpha);
}

float mol_dsp_biquad_process(mol_dsp_biquad_t* filter, float input) {
  float output;
  if (filter == NULL) {
    return 0.0f;
  }
  output = input * filter->b0 + filter->z1;
  filter->z1 = input * filter->b1 - filter->a1 * output + filter->z2;
  filter->z2 = input * filter->b2 - filter->a2 * output;
  if (!isfinite(output) || !isfinite(filter->z1) || !isfinite(filter->z2)) {
    filter->z1 = 0.0f;
    filter->z2 = 0.0f;
    return 0.0f;
  }
  return output;
}

void mol_dsp_lfo_configure(mol_dsp_lfo_t* lfo, uint32_t sample_rate, float rate_hz,
                           mol_dsp_lfo_shape_t shape, float phase) {
  if (lfo == NULL || sample_rate == 0u) {
    return;
  }
  lfo->phase = mol_dsp_wrap_phase(phase);
  lfo->increment = mol_dsp_clamp(rate_hz, 0.0f, 100.0f) / (float)sample_rate;
  lfo->shape = shape;
}

float mol_dsp_lfo_process(mol_dsp_lfo_t* lfo) {
  float phase;
  if (lfo == NULL) {
    return 0.0f;
  }
  phase = mol_dsp_phase_advance(&lfo->phase, lfo->increment);
  if (lfo->shape == MOL_DSP_LFO_TRIANGLE) {
    return mol_dsp_triangle(phase);
  }
  if (lfo->shape == MOL_DSP_LFO_SQUARE) {
    return phase < 0.5f ? 1.0f : -1.0f;
  }
  return mol_dsp_sine(phase);
}

void mol_dsp_fm2_configure(mol_dsp_fm2_t* fm, uint32_t sample_rate, float frequency_hz, float ratio,
                           float index) {
  if (fm == NULL || sample_rate == 0u) {
    return;
  }
  memset(fm, 0, sizeof(*fm));
  fm->carrier_increment = frequency_hz / (float)sample_rate;
  fm->modulator_increment = frequency_hz * mol_dsp_clamp(ratio, 0.125f, 16.0f) / (float)sample_rate;
  fm->index = mol_dsp_clamp(index, 0.0f, 20.0f);
}

float mol_dsp_fm2_process(mol_dsp_fm2_t* fm) {
  float carrier;
  float modulator;
  if (fm == NULL) {
    return 0.0f;
  }
  carrier = mol_dsp_phase_advance(&fm->carrier_phase, fm->carrier_increment);
  modulator = mol_dsp_sine(mol_dsp_phase_advance(&fm->modulator_phase, fm->modulator_increment));
  return mol_dsp_sine(carrier + modulator * fm->index / MOL_DSP_TWO_PI);
}

void mol_dsp_additive_configure(mol_dsp_additive_t* additive, uint32_t sample_rate,
                                float frequency_hz, const float* ratios, const float* gains,
                                uint32_t count) {
  if (additive == NULL || sample_rate == 0u) {
    return;
  }
  memset(additive, 0, sizeof(*additive));
  additive->count = count < MOL_DSP_MAX_PARTIALS ? count : MOL_DSP_MAX_PARTIALS;
  additive->base_increment = frequency_hz / (float)sample_rate;
  for (uint32_t index = 0u; index < additive->count; ++index) {
    additive->ratios[index] = ratios != NULL ? ratios[index] : (float)(index + 1u);
    additive->gains[index] = gains != NULL ? gains[index] : 1.0f / (float)(index + 1u);
  }
}

float mol_dsp_additive_process(mol_dsp_additive_t* additive) {
  float output = 0.0f;
  if (additive == NULL) {
    return 0.0f;
  }
  for (uint32_t index = 0u; index < additive->count; ++index) {
    float phase = mol_dsp_phase_advance(&additive->phases[index],
                                        additive->base_increment * additive->ratios[index]);
    output += mol_dsp_sine(phase) * additive->gains[index];
  }
  return output;
}

void mol_dsp_karplus_configure(mol_dsp_karplus_strong_t* pluck, float* buffer, uint32_t capacity,
                               uint32_t sample_rate, float frequency_hz, float damping,
                               uint32_t seed) {
  uint32_t length;
  if (pluck == NULL || buffer == NULL || capacity < 2u || sample_rate == 0u ||
      frequency_hz <= 0.0f) {
    return;
  }
  memset(pluck, 0, sizeof(*pluck));
  length = (uint32_t)((float)sample_rate / frequency_hz + 0.5f);
  length = length < 2u ? 2u : (length > capacity ? capacity : length);
  pluck->buffer = buffer;
  pluck->capacity = capacity;
  pluck->length = length;
  pluck->random_state = seed;
  pluck->damping = mol_dsp_clamp(damping, 0.0f, 0.9999f);
  for (uint32_t index = 0u; index < length; ++index) {
    buffer[index] = mol_dsp_white_noise(&pluck->random_state);
  }
}

float mol_dsp_karplus_process(mol_dsp_karplus_strong_t* pluck) {
  uint32_t next;
  float output;
  if (pluck == NULL || pluck->buffer == NULL || pluck->length < 2u) {
    return 0.0f;
  }
  next = pluck->index + 1u;
  if (next >= pluck->length) {
    next = 0u;
  }
  output = pluck->buffer[pluck->index];
  pluck->buffer[pluck->index] =
      0.5f * (pluck->buffer[pluck->index] + pluck->buffer[next]) * pluck->damping;
  pluck->index = next;
  return output;
}

void mol_dsp_modal_configure(mol_dsp_modal_bank_t* modal, uint32_t sample_rate,
                             float fundamental_hz, const float* ratios, const float* gains,
                             const float* decays, uint32_t count) {
  if (modal == NULL || sample_rate == 0u) {
    return;
  }
  memset(modal, 0, sizeof(*modal));
  modal->count = count < MOL_DSP_MAX_MODES ? count : MOL_DSP_MAX_MODES;
  for (uint32_t index = 0u; index < modal->count; ++index) {
    float ratio = ratios != NULL ? ratios[index] : (float)(index + 1u);
    float gain = gains != NULL ? gains[index] : 1.0f / (float)(index + 1u);
    float decay = decays != NULL ? decays[index] : 0.25f;
    float frequency = mol_dsp_clamp(fundamental_hz * ratio, 5.0f, 0.45f * (float)sample_rate);
    float radius = expf(-1.0f / ((float)sample_rate * mol_dsp_clamp(decay, 0.005f, 20.0f)));
    modal->coefficient[index] =
        2.0f * radius * cosf(MOL_DSP_TWO_PI * frequency / (float)sample_rate);
    modal->radius_squared[index] = radius * radius;
    modal->gain[index] = gain;
  }
}

float mol_dsp_modal_process(mol_dsp_modal_bank_t* modal, float excitation) {
  float output = 0.0f;
  if (modal == NULL) {
    return 0.0f;
  }
  for (uint32_t index = 0u; index < modal->count; ++index) {
    float value = excitation * modal->gain[index] + modal->coefficient[index] * modal->y1[index] -
                  modal->radius_squared[index] * modal->y2[index];
    modal->y2[index] = modal->y1[index];
    modal->y1[index] = isfinite(value) ? value : 0.0f;
    output += modal->y1[index];
  }
  return output;
}

float mol_dsp_soft_saturate(float input, float drive) {
  float scaled = input * (drive < 0.0f ? 0.0f : drive);
  return scaled / (1.0f + fabsf(scaled));
}

void mol_dsp_dc_blocker_configure(mol_dsp_dc_blocker_t* blocker, float coefficient) {
  if (blocker != NULL) {
    memset(blocker, 0, sizeof(*blocker));
    blocker->coefficient = mol_dsp_clamp(coefficient, 0.0f, 0.99999f);
  }
}

float mol_dsp_dc_blocker_process(mol_dsp_dc_blocker_t* blocker, float input) {
  float output;
  if (blocker == NULL) {
    return 0.0f;
  }
  output = input - blocker->previous_input + blocker->coefficient * blocker->previous_output;
  blocker->previous_input = input;
  blocker->previous_output = isfinite(output) ? output : 0.0f;
  return blocker->previous_output;
}

void mol_dsp_limiter_configure(mol_dsp_limiter_t* limiter, uint32_t sample_rate, float ceiling_db,
                               float attack_seconds, float release_seconds) {
  if (limiter == NULL || sample_rate == 0u) {
    return;
  }
  memset(limiter, 0, sizeof(*limiter));
  limiter->gain = 1.0f;
  limiter->ceiling = mol_dsp_clamp(mol_dsp_db_to_linear(ceiling_db), 0.01f, 1.0f);
  limiter->attack_coefficient =
      attack_seconds <= 0.0f ? 0.0f : expf(-1.0f / (attack_seconds * (float)sample_rate));
  limiter->release_coefficient =
      release_seconds <= 0.0f ? 0.0f : expf(-1.0f / (release_seconds * (float)sample_rate));
}

float mol_dsp_limiter_process(mol_dsp_limiter_t* limiter, float input) {
  float magnitude;
  float coefficient;
  float desired_gain;
  float output;
  if (limiter == NULL || !isfinite(input)) {
    return 0.0f;
  }
  magnitude = fabsf(input);
  coefficient =
      magnitude > limiter->envelope ? limiter->attack_coefficient : limiter->release_coefficient;
  limiter->envelope = coefficient * limiter->envelope + (1.0f - coefficient) * magnitude;
  desired_gain = limiter->envelope > limiter->ceiling ? limiter->ceiling / limiter->envelope : 1.0f;
  coefficient =
      desired_gain < limiter->gain ? limiter->attack_coefficient : limiter->release_coefficient;
  limiter->gain = coefficient * limiter->gain + (1.0f - coefficient) * desired_gain;
  output = input * limiter->gain;
  return mol_dsp_clamp(output, -limiter->ceiling, limiter->ceiling);
}
