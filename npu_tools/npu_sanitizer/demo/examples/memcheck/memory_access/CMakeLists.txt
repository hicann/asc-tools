cmake_minimum_required(VERSION 3.16)
set(CMAKE_ASC_ARCHITECTURES "dav-3510" CACHE STRING "NPU architecture")
find_package(ASC REQUIRED)
project(memory_access LANGUAGES ASC CXX)

find_library(ACL_RT_LIBRARY
  NAMES acl_rt
  HINTS $ENV{ASCEND_HOME_PATH}/lib64
  REQUIRED
)

function(add_aclsan_memory_access_example name source)
  set(target "aclsan_demo_memory_access_${name}")
  add_executable("${target}" "${source}")
  target_link_libraries("${target}" PRIVATE ${ACL_RT_LIBRARY})
  target_compile_options("${target}" PRIVATE
    $<$<COMPILE_LANGUAGE:ASC>:--npu-arch=${CMAKE_ASC_ARCHITECTURES}>
    -g
  )
  if(ARGN)
    target_compile_definitions("${target}" PRIVATE ${ARGN})
  endif()
endfunction()

add_aclsan_memory_access_example(gm_to_ub_read_valid vector_memory_case.asc
  DEVICE_CASE_KIND=1 DEVICE_CASE_FIRST_BYTES=4096 DEVICE_CASE_SECOND_BYTES=32 DEVICE_CASE_ACCESS_BYTES=4096)
add_aclsan_memory_access_example(gm_to_ub_read_oob vector_memory_case.asc
  DEVICE_CASE_KIND=1 DEVICE_CASE_FIRST_BYTES=4096 DEVICE_CASE_SECOND_BYTES=32 DEVICE_CASE_ACCESS_BYTES=4128)
add_aclsan_memory_access_example(gm_to_ub_b16_valid vector_memory_case.asc
  DEVICE_CASE_KIND=1 DEVICE_CASE_DATA_BITS=16 DEVICE_CASE_FIRST_BYTES=4096
  DEVICE_CASE_SECOND_BYTES=32 DEVICE_CASE_ACCESS_BYTES=4096)
add_aclsan_memory_access_example(gm_to_ub_b16_oob vector_memory_case.asc
  DEVICE_CASE_KIND=1 DEVICE_CASE_DATA_BITS=16 DEVICE_CASE_FIRST_BYTES=4096
  DEVICE_CASE_SECOND_BYTES=32 DEVICE_CASE_ACCESS_BYTES=4128)
add_aclsan_memory_access_example(gm_to_ub_b32_valid vector_memory_case.asc
  DEVICE_CASE_KIND=1 DEVICE_CASE_DATA_BITS=32 DEVICE_CASE_FIRST_BYTES=4096
  DEVICE_CASE_SECOND_BYTES=32 DEVICE_CASE_ACCESS_BYTES=4096)
add_aclsan_memory_access_example(gm_to_ub_b32_oob vector_memory_case.asc
  DEVICE_CASE_KIND=1 DEVICE_CASE_DATA_BITS=32 DEVICE_CASE_FIRST_BYTES=4096
  DEVICE_CASE_SECOND_BYTES=32 DEVICE_CASE_ACCESS_BYTES=4128)
add_aclsan_memory_access_example(gm_to_ub_block_repeat_valid vector_memory_case.asc
  DEVICE_CASE_KIND=3 DEVICE_CASE_FIRST_BYTES=160 DEVICE_CASE_SECOND_BYTES=32 DEVICE_CASE_ACCESS_BYTES=160)
add_aclsan_memory_access_example(gm_to_ub_block_repeat_oob vector_memory_case.asc
  DEVICE_CASE_KIND=3 DEVICE_CASE_FIRST_BYTES=128 DEVICE_CASE_SECOND_BYTES=32 DEVICE_CASE_ACCESS_BYTES=160)
add_aclsan_memory_access_example(ub_to_gm_write_valid vector_memory_case.asc
  DEVICE_CASE_KIND=2 DEVICE_CASE_FIRST_BYTES=4096 DEVICE_CASE_SECOND_BYTES=4096 DEVICE_CASE_ACCESS_BYTES=4096)
add_aclsan_memory_access_example(ub_to_gm_write_oob vector_memory_case.asc
  DEVICE_CASE_KIND=2 DEVICE_CASE_FIRST_BYTES=4096 DEVICE_CASE_SECOND_BYTES=4096 DEVICE_CASE_ACCESS_BYTES=4128)
add_aclsan_memory_access_example(local_ub_to_l1_valid local_memory_case.asc)

add_aclsan_memory_access_example(gm_to_ub_outer_loop_valid vector_loop_memory_case.asc
  VECTOR_LOOP_DIRECTION=1 VECTOR_LOOP_GM_ALLOCATION_BYTES=480)
add_aclsan_memory_access_example(gm_to_ub_outer_loop_oob vector_loop_memory_case.asc
  VECTOR_LOOP_DIRECTION=1 VECTOR_LOOP_GM_ALLOCATION_BYTES=448)
add_aclsan_memory_access_example(ub_to_gm_outer_loop_valid vector_loop_memory_case.asc
  VECTOR_LOOP_DIRECTION=2 VECTOR_LOOP_GM_ALLOCATION_BYTES=480)
add_aclsan_memory_access_example(ub_to_gm_outer_loop_oob vector_loop_memory_case.asc
  VECTOR_LOOP_DIRECTION=2 VECTOR_LOOP_GM_ALLOCATION_BYTES=448)
add_aclsan_memory_access_example(cube_gm_to_l1_outer_loop_valid cube_loop_memory_case.asc
  CUBE_LOOP_INSTRUCTION_ID=74 CUBE_LOOP_GM_ALLOCATION_BYTES=480)
add_aclsan_memory_access_example(cube_gm_to_l1_outer_loop_oob cube_loop_memory_case.asc
  CUBE_LOOP_INSTRUCTION_ID=74 CUBE_LOOP_GM_ALLOCATION_BYTES=448)
foreach(instruction_id 73 75 76)
  add_aclsan_memory_access_example(cube_gm_to_l1_id${instruction_id}_outer_loop_valid cube_loop_memory_case.asc
    CUBE_LOOP_INSTRUCTION_ID=${instruction_id} CUBE_LOOP_GM_ALLOCATION_BYTES=480)
  add_aclsan_memory_access_example(cube_gm_to_l1_id${instruction_id}_outer_loop_oob cube_loop_memory_case.asc
    CUBE_LOOP_INSTRUCTION_ID=${instruction_id} CUBE_LOOP_GM_ALLOCATION_BYTES=448)
endforeach()

set(id73_source_bytes 32 1 2 4 8 16 32 32 32)
foreach(padding_mode RANGE 0 8)
  list(GET id73_source_bytes "${padding_mode}" source_bytes)
  math(EXPR valid_bytes "3 * ${source_bytes}")
  math(EXPR oob_bytes "2 * ${source_bytes}")
  add_aclsan_memory_access_example(cube_gm_to_l1_id73_mode${padding_mode}_valid cube_dma_memory_case.asc
    CUBE_DMA_INSTRUCTION_ID=73 CUBE_DMA_PADDING_MODE=${padding_mode} CUBE_DMA_SOURCE_BYTES=${valid_bytes})
  add_aclsan_memory_access_example(cube_gm_to_l1_id73_mode${padding_mode}_oob cube_dma_memory_case.asc
    CUBE_DMA_INSTRUCTION_ID=73 CUBE_DMA_PADDING_MODE=${padding_mode} CUBE_DMA_SOURCE_BYTES=${oob_bytes})
endforeach()

foreach(instruction_id RANGE 74 76)
  set(valid_bytes 160)
  set(oob_bytes 128)
  add_aclsan_memory_access_example(cube_gm_to_l1_id${instruction_id}_valid cube_dma_memory_case.asc
    CUBE_DMA_INSTRUCTION_ID=${instruction_id} CUBE_DMA_SOURCE_BYTES=${valid_bytes})
  add_aclsan_memory_access_example(cube_gm_to_l1_id${instruction_id}_oob cube_dma_memory_case.asc
    CUBE_DMA_INSTRUCTION_ID=${instruction_id} CUBE_DMA_SOURCE_BYTES=${oob_bytes})
endforeach()

add_aclsan_memory_access_example(multi_nd2nz_valid multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=1 DEVICE_CASE_SOURCE_BYTES=736)
add_aclsan_memory_access_example(multi_nd2nz_oob multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=1 DEVICE_CASE_SOURCE_BYTES=704)
add_aclsan_memory_access_example(multi_nd2nz_b8_valid multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=1 DEVICE_CASE_DATA_BITS=8 DEVICE_CASE_SOURCE_BYTES=368)
add_aclsan_memory_access_example(multi_nd2nz_b8_oob multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=1 DEVICE_CASE_DATA_BITS=8 DEVICE_CASE_SOURCE_BYTES=336)
add_aclsan_memory_access_example(multi_nd2nz_b32_valid multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=1 DEVICE_CASE_DATA_BITS=32 DEVICE_CASE_SOURCE_BYTES=1472)
add_aclsan_memory_access_example(multi_nd2nz_b32_oob multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=1 DEVICE_CASE_DATA_BITS=32 DEVICE_CASE_SOURCE_BYTES=1440)
add_aclsan_memory_access_example(multi_dn2nz_valid multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_SOURCE_BYTES=488)
add_aclsan_memory_access_example(multi_dn2nz_oob multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_SOURCE_BYTES=456)
add_aclsan_memory_access_example(multi_dn2nz_b8_valid multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_DATA_BITS=8 DEVICE_CASE_SOURCE_BYTES=244)
add_aclsan_memory_access_example(multi_dn2nz_b8_oob multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_DATA_BITS=8 DEVICE_CASE_SOURCE_BYTES=212)
add_aclsan_memory_access_example(multi_dn2nz_b32_valid multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_DATA_BITS=32 DEVICE_CASE_SOURCE_BYTES=976)
add_aclsan_memory_access_example(multi_dn2nz_b32_oob multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_DATA_BITS=32 DEVICE_CASE_SOURCE_BYTES=944)
add_aclsan_memory_access_example(multi_dn2nz_matrix2_valid multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_MATRIX_NUM=2 DEVICE_CASE_SOURCE_MATRIX_STRIDE_ELEMENTS=256
  DEVICE_CASE_DESTINATION_MATRIX_STRIDE_ELEMENTS=256 DEVICE_CASE_SOURCE_BYTES=1000)
add_aclsan_memory_access_example(multi_dn2nz_matrix2_oob multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_MATRIX_NUM=2 DEVICE_CASE_SOURCE_MATRIX_STRIDE_ELEMENTS=256
  DEVICE_CASE_DESTINATION_MATRIX_STRIDE_ELEMENTS=256 DEVICE_CASE_SOURCE_BYTES=968)
add_aclsan_memory_access_example(multi_dn2nz_b8_matrix2_valid multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_DATA_BITS=8 DEVICE_CASE_MATRIX_NUM=2
  DEVICE_CASE_SOURCE_MATRIX_STRIDE_ELEMENTS=256 DEVICE_CASE_DESTINATION_MATRIX_STRIDE_ELEMENTS=256
  DEVICE_CASE_SOURCE_BYTES=500)
add_aclsan_memory_access_example(multi_dn2nz_b8_matrix2_oob multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_DATA_BITS=8 DEVICE_CASE_MATRIX_NUM=2
  DEVICE_CASE_SOURCE_MATRIX_STRIDE_ELEMENTS=256 DEVICE_CASE_DESTINATION_MATRIX_STRIDE_ELEMENTS=256
  DEVICE_CASE_SOURCE_BYTES=468)
add_aclsan_memory_access_example(multi_dn2nz_b32_matrix2_valid multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_DATA_BITS=32 DEVICE_CASE_MATRIX_NUM=2
  DEVICE_CASE_SOURCE_MATRIX_STRIDE_ELEMENTS=256 DEVICE_CASE_DESTINATION_MATRIX_STRIDE_ELEMENTS=256
  DEVICE_CASE_SOURCE_BYTES=2000)
add_aclsan_memory_access_example(multi_dn2nz_b32_matrix2_oob multi_memory_case.asc
  DEVICE_CASE_MULTI_KIND=2 DEVICE_CASE_DATA_BITS=32 DEVICE_CASE_MATRIX_NUM=2
  DEVICE_CASE_SOURCE_MATRIX_STRIDE_ELEMENTS=256 DEVICE_CASE_DESTINATION_MATRIX_STRIDE_ELEMENTS=256
  DEVICE_CASE_SOURCE_BYTES=1968)

add_aclsan_memory_access_example(fixpipe_valid fixpipe_memory_case.asc DEVICE_CASE_OUTPUT_BYTES=512)
add_aclsan_memory_access_example(fixpipe_oob fixpipe_memory_case.asc DEVICE_CASE_OUTPUT_BYTES=480)
add_aclsan_memory_access_example(fixpipe_s32_valid fixpipe_memory_case.asc
  DEVICE_CASE_FIXPIPE_INSTRUCTION_ID=92 DEVICE_CASE_OUTPUT_BYTES=1024)
add_aclsan_memory_access_example(fixpipe_s32_oob fixpipe_memory_case.asc
  DEVICE_CASE_FIXPIPE_INSTRUCTION_ID=92 DEVICE_CASE_OUTPUT_BYTES=992)
add_aclsan_memory_access_example(fixpipe_nz2nd_valid fixpipe_memory_case.asc
  DEVICE_CASE_FIXPIPE_LAYOUT=1 DEVICE_CASE_OUTPUT_BYTES=2016)
add_aclsan_memory_access_example(fixpipe_nz2nd_oob fixpipe_memory_case.asc
  DEVICE_CASE_FIXPIPE_LAYOUT=1 DEVICE_CASE_OUTPUT_BYTES=1984)
add_aclsan_memory_access_example(fixpipe_nz2dn_valid fixpipe_memory_case.asc
  DEVICE_CASE_FIXPIPE_LAYOUT=2 DEVICE_CASE_OUTPUT_BYTES=2016)
add_aclsan_memory_access_example(fixpipe_nz2dn_oob fixpipe_memory_case.asc
  DEVICE_CASE_FIXPIPE_LAYOUT=2 DEVICE_CASE_OUTPUT_BYTES=1984)
add_aclsan_memory_access_example(fixpipe_s32_nz2nd_valid fixpipe_memory_case.asc
  DEVICE_CASE_FIXPIPE_INSTRUCTION_ID=92 DEVICE_CASE_FIXPIPE_LAYOUT=1 DEVICE_CASE_OUTPUT_BYTES=4032)
add_aclsan_memory_access_example(fixpipe_s32_nz2nd_oob fixpipe_memory_case.asc
  DEVICE_CASE_FIXPIPE_INSTRUCTION_ID=92 DEVICE_CASE_FIXPIPE_LAYOUT=1 DEVICE_CASE_OUTPUT_BYTES=4000)
add_aclsan_memory_access_example(fixpipe_s32_nz2dn_valid fixpipe_memory_case.asc
  DEVICE_CASE_FIXPIPE_INSTRUCTION_ID=92 DEVICE_CASE_FIXPIPE_LAYOUT=2 DEVICE_CASE_OUTPUT_BYTES=4032)
add_aclsan_memory_access_example(fixpipe_s32_nz2dn_oob fixpipe_memory_case.asc
  DEVICE_CASE_FIXPIPE_INSTRUCTION_ID=92 DEVICE_CASE_FIXPIPE_LAYOUT=2 DEVICE_CASE_OUTPUT_BYTES=4000)

add_aclsan_memory_access_example(fixpipe_channel_split_n8_valid fixpipe_channel_split_memory_case.asc
  FIXPIPE_CHANNEL_SPLIT_N_SIZE=8 FIXPIPE_CHANNEL_SPLIT_ALLOCATION_BYTES=512)
add_aclsan_memory_access_example(fixpipe_channel_split_n8_oob fixpipe_channel_split_memory_case.asc
  FIXPIPE_CHANNEL_SPLIT_N_SIZE=8 FIXPIPE_CHANNEL_SPLIT_ALLOCATION_BYTES=480)
add_aclsan_memory_access_example(fixpipe_channel_split_n16_valid fixpipe_channel_split_memory_case.asc
  FIXPIPE_CHANNEL_SPLIT_N_SIZE=16 FIXPIPE_CHANNEL_SPLIT_ALLOCATION_BYTES=1024)
add_aclsan_memory_access_example(fixpipe_channel_split_n16_oob fixpipe_channel_split_memory_case.asc
  FIXPIPE_CHANNEL_SPLIT_N_SIZE=16 FIXPIPE_CHANNEL_SPLIT_ALLOCATION_BYTES=992)

add_aclsan_memory_access_example(fixpipe_c0pad_exact_valid fixpipe_c0pad_memory_case.asc
  FIXPIPE_C0PAD_N_SIZE=16 FIXPIPE_C0PAD_OUTPUT_BYTES=1024)
add_aclsan_memory_access_example(fixpipe_c0pad_exact_oob fixpipe_c0pad_memory_case.asc
  FIXPIPE_C0PAD_N_SIZE=16 FIXPIPE_C0PAD_OUTPUT_BYTES=992)
add_aclsan_memory_access_example(fixpipe_c0pad_roundup_valid fixpipe_c0pad_memory_case.asc
  FIXPIPE_C0PAD_N_SIZE=32 FIXPIPE_C0PAD_OUTPUT_BYTES=2048)
add_aclsan_memory_access_example(fixpipe_c0pad_roundup_oob fixpipe_c0pad_memory_case.asc
  FIXPIPE_C0PAD_N_SIZE=32 FIXPIPE_C0PAD_OUTPUT_BYTES=2016)

function(add_fixpipe_quant_example name source_kind destination_kind quant_pre output_bytes instruction_id)
  add_aclsan_memory_access_example("fixpipe_quant_${name}" fixpipe_quant_memory_case.asc
    FIXPIPE_SOURCE_KIND=${source_kind}
    FIXPIPE_DEST_KIND=${destination_kind}
    FIXPIPE_QUANT_PRE=${quant_pre}
    FIXPIPE_EXPECTED_OUTPUT_BYTES=${output_bytes}
    FIXPIPE_EXPECTED_INSTRUCTION_ID=${instruction_id}
    ${ARGN})
endfunction()

add_fixpipe_quant_example(q0_f32 1 6 0 1024 91)
add_fixpipe_quant_example(q0_s32 2 7 0 1024 92)
add_fixpipe_quant_example(q1 1 2 1 512 91)
foreach(quant_pre RANGE 2 5)
  add_fixpipe_quant_example("q${quant_pre}" 1 5 ${quant_pre} 256 91)
endforeach()
foreach(quant_pre RANGE 8 9)
  add_fixpipe_quant_example("q${quant_pre}" 2 1 ${quant_pre} 256 92)
endforeach()
foreach(quant_pre RANGE 10 11)
  add_fixpipe_quant_example("q${quant_pre}" 2 2 ${quant_pre} 512 92)
endforeach()
foreach(quant_pre RANGE 12 13)
  add_fixpipe_quant_example("q${quant_pre}" 1 4 ${quant_pre} 256 91)
endforeach()
foreach(quant_pre RANGE 14 15)
  add_fixpipe_quant_example("q${quant_pre}" 1 6 ${quant_pre} 1024 91)
endforeach()
add_fixpipe_quant_example(q16 1 3 16 512 91)
foreach(quant_pre RANGE 21 22)
  add_fixpipe_quant_example("q${quant_pre}" 2 8 ${quant_pre} 512 92
    FIXPIPE_N_SIZE=64 FIXPIPE_DST_STRIDE=2048)
endforeach()
foreach(quant_pre RANGE 23 24)
  add_fixpipe_quant_example("q${quant_pre}" 1 1 ${quant_pre} 256 91)
endforeach()
foreach(quant_pre RANGE 25 26)
  add_fixpipe_quant_example("q${quant_pre}" 1 8 ${quant_pre} 512 91
    FIXPIPE_N_SIZE=64 FIXPIPE_DST_STRIDE=2048)
endforeach()
foreach(quant_pre RANGE 31 32)
  add_fixpipe_quant_example("q${quant_pre}" 1 2 ${quant_pre} 512 91)
endforeach()
foreach(quant_pre RANGE 33 34)
  add_fixpipe_quant_example("q${quant_pre}" 1 3 ${quant_pre} 512 91)
endforeach()
foreach(quant_pre RANGE 35 36)
  add_fixpipe_quant_example("q${quant_pre}" 2 3 ${quant_pre} 512 92)
endforeach()

add_fixpipe_quant_example(channel_merge_tail 1 1 24 1280 91
  FIXPIPE_N_SIZE=48 FIXPIPE_DST_STRIDE=1024)
add_fixpipe_quant_example(b4_channel_merge_two_group 1 8 26 1112 91
  FIXPIPE_N_SIZE=128 FIXPIPE_M_SIZE=16 FIXPIPE_DST_STRIDE=1200)
add_fixpipe_quant_example(b8_f32_nz2nd 1 1 24 1008 91
  FIXPIPE_N_SIZE=16 FIXPIPE_M_SIZE=16 FIXPIPE_DST_STRIDE=32 FIXPIPE_LAYOUT=1)
add_fixpipe_quant_example(b8_s32_nz2dn 2 1 9 1008 92
  FIXPIPE_N_SIZE=16 FIXPIPE_M_SIZE=16 FIXPIPE_DST_STRIDE=32 FIXPIPE_LAYOUT=2)
add_fixpipe_quant_example(b4_f32_nz2nd 1 8 26 504 91
  FIXPIPE_N_SIZE=16 FIXPIPE_M_SIZE=16 FIXPIPE_DST_STRIDE=32 FIXPIPE_LAYOUT=1)
add_fixpipe_quant_example(b4_s32_nz2dn 2 8 22 504 92
  FIXPIPE_N_SIZE=16 FIXPIPE_M_SIZE=16 FIXPIPE_DST_STRIDE=32 FIXPIPE_LAYOUT=2)
add_fixpipe_quant_example(relu_pre 1 2 1 512 91 FIXPIPE_RELU_PRE=1)
add_fixpipe_quant_example(relu_scalar 1 2 1 512 91 FIXPIPE_RELU_PRE=2)
add_fixpipe_quant_example(relu_vector 1 2 1 512 91 FIXPIPE_RELU_PRE=3)
add_fixpipe_quant_example(unit_keep 1 2 1 512 91 FIXPIPE_UNIT_FLAG=2)
add_fixpipe_quant_example(unit_update 1 2 1 512 91 FIXPIPE_UNIT_FLAG=3)
add_fixpipe_quant_example(clip_relu_pre 1 2 1 512 91 FIXPIPE_RELU_PRE=1 FIXPIPE_CLIP_RELU_PRE=1)

add_aclsan_memory_access_example(nddma_b8_valid nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=8 DEVICE_CASE_SOURCE_BYTES=342)
add_aclsan_memory_access_example(nddma_b8_oob nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=8 DEVICE_CASE_SOURCE_BYTES=341)
add_aclsan_memory_access_example(nddma_b16_valid nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=16 DEVICE_CASE_SOURCE_BYTES=684)
add_aclsan_memory_access_example(nddma_b16_oob nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=16 DEVICE_CASE_SOURCE_BYTES=682)
add_aclsan_memory_access_example(nddma_b32_valid nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=32 DEVICE_CASE_SOURCE_BYTES=1368)
add_aclsan_memory_access_example(nddma_b32_oob nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=32 DEVICE_CASE_SOURCE_BYTES=1364)

add_aclsan_memory_access_example(nddma_rank1_b8_valid nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=8 DEVICE_CASE_NDDMA_PROFILE=1 DEVICE_CASE_SOURCE_BYTES=4)
add_aclsan_memory_access_example(nddma_rank1_b8_oob nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=8 DEVICE_CASE_NDDMA_PROFILE=1 DEVICE_CASE_SOURCE_BYTES=3)
add_aclsan_memory_access_example(nddma_rank2_singleton_b16_valid nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=16 DEVICE_CASE_NDDMA_PROFILE=2 DEVICE_CASE_SOURCE_BYTES=26)
add_aclsan_memory_access_example(nddma_rank2_singleton_b16_oob nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=16 DEVICE_CASE_NDDMA_PROFILE=2 DEVICE_CASE_SOURCE_BYTES=24)
add_aclsan_memory_access_example(nddma_rank3_singleton_b32_valid nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=32 DEVICE_CASE_NDDMA_PROFILE=3 DEVICE_CASE_SOURCE_BYTES=136)
add_aclsan_memory_access_example(nddma_rank3_singleton_b32_oob nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=32 DEVICE_CASE_NDDMA_PROFILE=3 DEVICE_CASE_SOURCE_BYTES=132)
add_aclsan_memory_access_example(nddma_rank4_singleton_b8_valid nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=8 DEVICE_CASE_NDDMA_PROFILE=4 DEVICE_CASE_SOURCE_BYTES=98)
add_aclsan_memory_access_example(nddma_rank4_singleton_b8_oob nddma_memory_case.asc
  DEVICE_CASE_NDDMA_BITS=8 DEVICE_CASE_NDDMA_PROFILE=4 DEVICE_CASE_SOURCE_BYTES=97)
add_aclsan_memory_access_example(nddma_missing_active_stride nddma_missing_stride_memory_case.asc)

function(add_nddma_padding_example name bits valid_bytes oob_bytes)
  add_aclsan_memory_access_example("nddma_padding_${name}_valid" nddma_padding_memory_case.asc
    NDDMA_PADDING_BITS=${bits} NDDMA_PADDING_SOURCE_BYTES=${valid_bytes} ${ARGN})
  add_aclsan_memory_access_example("nddma_padding_${name}_oob" nddma_padding_memory_case.asc
    NDDMA_PADDING_BITS=${bits} NDDMA_PADDING_SOURCE_BYTES=${oob_bytes} ${ARGN})
endfunction()

add_nddma_padding_example(nearest_b8 8 20 19 NDDMA_PADDING_NEAREST=1)
add_nddma_padding_example(nearest_b16 16 40 38 NDDMA_PADDING_NEAREST=1)
add_nddma_padding_example(nearest_b32 32 80 76 NDDMA_PADDING_NEAREST=1)
add_nddma_padding_example(constant_nonzero_b8 8 20 19 NDDMA_PADDING_CONSTANT_VALUE=7)
add_nddma_padding_example(uniform_constant_b16 16 40 38 NDDMA_PADDING_UNIFORM_CONFIG=1)
add_nddma_padding_example(max_loop0_padding_b8 8 4 3 NDDMA_PADDING_MAX_LOOP0=1)

function(add_nddma_p0_example name profile valid_bytes oob_bytes)
  add_aclsan_memory_access_example("nddma_p0_${name}_valid" nddma_p0_memory_case.asc
    NDDMA_P0_PROFILE=${profile} NDDMA_P0_SOURCE_BYTES=${valid_bytes})
  add_aclsan_memory_access_example("nddma_p0_${name}_oob" nddma_p0_memory_case.asc
    NDDMA_P0_PROFILE=${profile} NDDMA_P0_SOURCE_BYTES=${oob_bytes})
endfunction()

add_nddma_p0_example(broadcast_b8 1 16 15)
add_nddma_p0_example(b64_4d_padding 2 688 687)
add_nddma_p0_example(b64_5d_padding 3 2736 2735)
add_nddma_p0_example(b64_5d_singleton 4 688 687)
add_nddma_p0_example(multi_block_b8 5 52 51)

add_aclsan_memory_access_example(load2dv2_mode0_valid load2dv2_memory_case.asc
  LOAD2DV2_SOURCE_ALLOCATION_BYTES=6144)
add_aclsan_memory_access_example(load2dv2_mode0_oob load2dv2_memory_case.asc
  LOAD2DV2_SOURCE_ALLOCATION_BYTES=6112)

add_aclsan_memory_access_example(load2dv2_negative_stride_valid load2dv2_memory_case.asc
  LOAD2DV2_SRC_STRIDE=-4 LOAD2DV2_SOURCE_ALLOCATION_BYTES=4096)
add_aclsan_memory_access_example(load2dv2_negative_stride_oob load2dv2_memory_case.asc
  LOAD2DV2_SRC_STRIDE=-4 LOAD2DV2_SOURCE_ALLOCATION_BYTES=4064)

add_aclsan_memory_access_example(api_datacopypad_gm2ub_normal_valid datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=1 DEVICE_API_CASE_GM_BYTES=175)
add_aclsan_memory_access_example(api_datacopypad_gm2ub_normal_oob datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=1 DEVICE_API_CASE_GM_BYTES=174)
add_aclsan_memory_access_example(api_datacopypad_gm2ub_compact_valid datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=2 DEVICE_API_CASE_GM_BYTES=175)
add_aclsan_memory_access_example(api_datacopypad_gm2ub_compact_oob datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=2 DEVICE_API_CASE_GM_BYTES=174)
add_aclsan_memory_access_example(api_datacopypad_ub2gm_normal_valid datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=3 DEVICE_API_CASE_GM_BYTES=175)
add_aclsan_memory_access_example(api_datacopypad_ub2gm_normal_oob datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=3 DEVICE_API_CASE_GM_BYTES=174)
add_aclsan_memory_access_example(api_datacopypad_ub2gm_compact_valid datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=4 DEVICE_API_CASE_GM_BYTES=175)
add_aclsan_memory_access_example(api_datacopypad_ub2gm_compact_oob datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=4 DEVICE_API_CASE_GM_BYTES=174)
add_aclsan_memory_access_example(api_datacopy_nd2nz_valid datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=5 DEVICE_API_CASE_GM_BYTES=175)
add_aclsan_memory_access_example(api_datacopy_nd2nz_oob datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=5 DEVICE_API_CASE_GM_BYTES=174)
add_aclsan_memory_access_example(api_datacopy_nz2nd_valid datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=6 DEVICE_API_CASE_GM_BYTES=160)
add_aclsan_memory_access_example(api_datacopy_nz2nd_oob datacopy_api_memory_case.asc
  DEVICE_API_CASE_KIND=6 DEVICE_API_CASE_GM_BYTES=159)

add_aclsan_memory_access_example(api_load_data_2d_valid api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=1 DEVICE_CASE_SOURCE_BYTES=4608)
add_aclsan_memory_access_example(api_load_data_2d_oob api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=1 DEVICE_CASE_SOURCE_BYTES=4576)
add_aclsan_memory_access_example(api_load_data_2dv2_valid api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=2 DEVICE_CASE_SOURCE_BYTES=6144)
add_aclsan_memory_access_example(api_load_data_2dv2_oob api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=2 DEVICE_CASE_SOURCE_BYTES=6112)
add_aclsan_memory_access_example(api_datacopypad_gm2l1_normal_valid api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=3 DEVICE_CASE_SOURCE_BYTES=176)
add_aclsan_memory_access_example(api_datacopypad_gm2l1_normal_oob api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=3 DEVICE_CASE_SOURCE_BYTES=175)
add_aclsan_memory_access_example(api_datacopypad_gm2l1_compact_valid api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=4 DEVICE_CASE_SOURCE_BYTES=176)
add_aclsan_memory_access_example(api_datacopypad_gm2l1_compact_oob api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=4 DEVICE_CASE_SOURCE_BYTES=175)
add_aclsan_memory_access_example(api_nddma_b64_4d_valid api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=5 DEVICE_CASE_SOURCE_BYTES=688)
add_aclsan_memory_access_example(api_nddma_b64_4d_oob api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=5 DEVICE_CASE_SOURCE_BYTES=687)
add_aclsan_memory_access_example(api_nddma_b64_5d_valid api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=6 DEVICE_CASE_SOURCE_BYTES=2736)
add_aclsan_memory_access_example(api_nddma_b64_5d_oob api_gm_to_local_memory_case.asc
  DEVICE_CASE_API_KIND=6 DEVICE_CASE_SOURCE_BYTES=2735)
