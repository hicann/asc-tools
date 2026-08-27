if(NOT DEFINED DBI_ROOT)
  message(FATAL_ERROR "DBI_ROOT is required")
endif()

set(probe_sources
  "${DBI_ROOT}/src/probes/fixpipe.cpp"
  "${DBI_ROOT}/src/probes/mte1.cpp"
  "${DBI_ROOT}/src/probes/mte2.cpp"
  "${DBI_ROOT}/src/probes/mte3.cpp"
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

if(probe_content MATCHES "ASCSAN_PROBE" OR trace_record_content MATCHES "ASCSAN_PROBE")
  message(FATAL_ERROR "ASCSAN_PROBE must not appear in the DBI probe header or sources")
endif()

string(REGEX MATCHALL
  "__sanitizer_report_[A-Za-z0-9_]+[ \t\r\n]*\\("
  probe_definitions "${probe_content}")
list(LENGTH probe_definitions probe_count)
if(NOT probe_count EQUAL 76)
  message(FATAL_ERROR "expected 76 probe definitions, found ${probe_count}")
endif()

set(explicit_declaration
  "extern[ \t\r\n]+__attribute__\\(\\(noinline\\)\\)[ \t\r\n]+__attribute__\\(\\(weak\\)\\)[ \t\r\n]+__aicore__[ \t\r\n]+void[ \t\r\n]+__sanitizer_report_[A-Za-z0-9_]+[ \t\r\n]*\\(")
string(REGEX MATCHALL "${explicit_declaration}" explicit_probes "${probe_content}")
list(LENGTH explicit_probes explicit_probe_count)
if(NOT explicit_probe_count EQUAL 76)
  message(FATAL_ERROR "expected 76 explicit probe declarations, found ${explicit_probe_count}")
endif()
