# SPDX-License-Identifier: Apache-2.0

set(MOL_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../..")

idf_component_register(
  SRCS
    "${MOL_REPOSITORY_ROOT}/src/c_api/mol_engine.c"
    "${MOL_REPOSITORY_ROOT}/src/dsp/mol_dsp.c"
    "${MOL_REPOSITORY_ROOT}/src/effects/mol_effects.c"
    "${MOL_REPOSITORY_ROOT}/src/music/mol_music.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/mol_patch.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/mol_builtin_patches.c"
    "${MOL_REPOSITORY_ROOT}/src/sequence/mol_sequence.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_grand_piano.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_electric_piano.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_harpsichord.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_church_organ.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_jazz_organ.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_nylon_guitar.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_steel_guitar.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_violin.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_cello.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_flute.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_clarinet.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_synth_lead.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_synth_pad.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_synth_bass.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_choir.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_vibraphone.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_harp.c"
    "${MOL_REPOSITORY_ROOT}/src/patch/generated/mol_patch_music_box.c"
    "${MOL_REPOSITORY_ROOT}/src/transport/mol_transport.c"
  INCLUDE_DIRS "${MOL_REPOSITORY_ROOT}/include"
  PRIV_INCLUDE_DIRS "${MOL_REPOSITORY_ROOT}/src/dsp" "${MOL_REPOSITORY_ROOT}/src/effects")

target_compile_definitions(
  ${COMPONENT_LIB}
  PUBLIC MOL_BUILD_PROFILE=1
         MOL_PROFILE_MAX_VOICES=12
         MOL_PROFILE_SEQUENCE_EVENTS=256
         MOL_PROFILE_PLUCK_FRAMES=384
         MOL_PROFILE_CHORUS_FRAMES=1024
         MOL_PROFILE_DELAY_FRAMES=8192
         MOL_PROFILE_REVERB_FRAMES=8192
         MOL_PROFILE_REVERB_SCALE_MILLI=650
         MOL_ENABLE_CHORUS=1
         MOL_ENABLE_DELAY=1
         MOL_ENABLE_REVERB=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wpedantic -Werror)
