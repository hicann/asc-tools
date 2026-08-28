if(NOT DEFINED DBI_ROOT)
  message(FATAL_ERROR "DBI_ROOT is required")
endif()

set(probe_sources
  "${DBI_ROOT}/src/probes/fixpipe.cpp"
  "${DBI_ROOT}/src/probes/mte1.cpp"
  "${DBI_ROOT}/src/probes/mte2.cpp"
  "${DBI_ROOT}/src/probes/mte3.cpp"
  "${DBI_ROOT}/src/probes/scalar.cpp"
  "${DBI_ROOT}/src/probes/sync.cpp"
)

set(probe_content "")
foreach(probe_source IN LISTS probe_sources)
  if(NOT EXISTS "${probe_source}")
    message(FATAL_ERROR "missing probe source: ${probe_source}")
  endif()
  file(READ "${probe_source}" source_content)
  string(APPEND probe_content "\n${source_content}")
endforeach()
file(READ "${DBI_ROOT}/include/trace_record.h" trace_record_content)
file(READ "${DBI_ROOT}/include/trace_buffer_abi.h" trace_buffer_abi_content)

foreach(trace_type IN ITEMS AclsanTraceBufferHeader AclsanTraceSliceHeader AclsanRawTraceRecord)
  string(FIND "${trace_buffer_abi_content}" "struct ${trace_type}" type_offset)
  if(type_offset EQUAL -1)
    message(FATAL_ERROR "trace_buffer_abi.h must define ${trace_type}")
  endif()
endforeach()
string(CONCAT legacy_trace_prefix "Asc" "san")
if(trace_buffer_abi_content MATCHES "${legacy_trace_prefix}[A-Za-z0-9_]*" OR
    trace_record_content MATCHES "${legacy_trace_prefix}[A-Za-z0-9_]*")
  message(FATAL_ERROR "DBI trace ABI must not expose legacy-prefixed type names")
endif()

foreach(required_abi_token IN ITEMS
    "ASCSAN_TRACE_BUFFER_MAGIC"
    "ASCSAN_PHYSICAL_CORE_PART_COUNT"
    "ASCSAN_AIC_CORE_RATIO_DENOMINATOR"
    "ASCSAN_PHYSICAL_CORE_TOPOLOGY_UNIT"
    "physicalCoreCount")
  string(FIND "${trace_buffer_abi_content}" "${required_abi_token}" token_offset)
  if(token_offset EQUAL -1)
    message(FATAL_ERROR "trace_buffer_abi.h must contain ${required_abi_token}")
  endif()
endforeach()

foreach(required_record_token IN ITEMS
    "AscendC::GetBlockIdx()"
    "get_coreid()"
    "record->category = category"
    "record->blockId = blockId"
    "static_cast<uint64_t>(phyCoreId) * sliceBytes")
  string(FIND "${trace_record_content}" "${required_record_token}" token_offset)
  if(token_offset EQUAL -1)
    message(FATAL_ERROR "trace_record.h must contain ${required_record_token}")
  endif()
endforeach()
string(REGEX MATCHALL "DeviceInstructionCategory::MemoryAccess" memory_access_categories "${probe_content}")
list(LENGTH memory_access_categories memory_access_category_count)
if(NOT memory_access_category_count EQUAL 49)
  message(FATAL_ERROR "expected 49 memory-access probe categories, found ${memory_access_category_count}")
endif()
string(REGEX MATCHALL "DeviceInstructionCategory::Synchronization" synchronization_categories "${probe_content}")
list(LENGTH synchronization_categories synchronization_category_count)
if(NOT synchronization_category_count EQUAL 24)
  message(FATAL_ERROR "expected 24 synchronization probe categories, found ${synchronization_category_count}")
endif()
string(REGEX MATCHALL "DeviceInstructionCategory::RegisterState" register_state_categories "${probe_content}")
list(LENGTH register_state_categories register_state_category_count)
if(NOT register_state_category_count EQUAL 10)
  message(FATAL_ERROR "expected 10 register-state probe categories, found ${register_state_category_count}")
endif()
foreach(expected_pipeline IN ITEMS PIPE_S PIPE_MTE1 PIPE_MTE2 PIPE_MTE3 PIPE_FIX)
  if(NOT probe_content MATCHES "static_cast<uint16_t>\\(${expected_pipeline}\\)")
    message(FATAL_ERROR "probe sources must record ${expected_pipeline}")
  endif()
endforeach()
if(trace_record_content MATCHES "PIPELINE_SET_WAIT_FLAG|PIPELINE_GET_RLS_BUF|PIPELINE_MTE[123]|PIPELINE_FIXPIPE")
  message(FATAL_ERROR "trace_record.h must not mix instruction categories with execution pipelines")
endif()
if(trace_record_content MATCHES "get_subblockdim\\(\\)|get_subblockid\\(\\)")
  message(FATAL_ERROR "trace_record.h must derive blockId directly from AscendC::GetBlockIdx()")
endif()
if(trace_record_content MATCHES "ASCSAN_TRACE_BUFFER_MAGIC_V[0-9]+")
  message(FATAL_ERROR "trace_record.h must use the single unversioned trace buffer magic")
endif()
if(trace_buffer_abi_content MATCHES "ASCSAN_TRACE_SLICES_PER_BLOCK|TraceSliceCount|recordsPerBlock" OR
    trace_record_content MATCHES "ASCSAN_TRACE_SLICES_PER_BLOCK|TraceSliceCount|recordsPerBlock")
  message(FATAL_ERROR "DBI trace ABI must use fixed physical-core slices")
endif()

if(probe_content MATCHES "ASCSAN_PROBE" OR trace_record_content MATCHES "ASCSAN_PROBE")
  message(FATAL_ERROR "ASCSAN_PROBE must not appear in the DBI probe header or sources")
endif()

string(REGEX MATCHALL
  "__sanitizer_report_[A-Za-z0-9_]+[ \t\r\n]*\\("
  probe_definitions "${probe_content}")
list(LENGTH probe_definitions probe_count)
if(NOT probe_count EQUAL 83)
  message(FATAL_ERROR "expected 83 probe definitions, found ${probe_count}")
endif()

string(CONCAT explicit_declaration
  "extern[ \t\r\n]+__attribute__\\(\\(noinline\\)\\)[ \t\r\n]+"
  "__attribute__\\(\\(weak\\)\\)[ \t\r\n]+__aicore__[ \t\r\n]+void[ \t\r\n]+"
  "__sanitizer_report_[A-Za-z0-9_]+[ \t\r\n]*\\(")
string(REGEX MATCHALL "${explicit_declaration}" explicit_probes "${probe_content}")
list(LENGTH explicit_probes explicit_probe_count)
if(NOT explicit_probe_count EQUAL 83)
  message(FATAL_ERROR "expected 83 explicit probe declarations, found ${explicit_probe_count}")
endif()
