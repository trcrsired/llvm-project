set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Support
  Core
  MC
  Target
  TargetParser
  CodeGen
  )

# On DLL platforms there is no symbol interposition between the test binary
# and the plugin, so the pass has to live in a shared library that both can
# link against instead.
set(CG_PLUGIN_TEST_PASS_SOURCES Plugin/CodeGenTestPass.cpp)
set(CG_PLUGIN_TEST_PASS_LIBS)
if (WIN32 OR CYGWIN)
  add_llvm_library(CGPluginTestPass SHARED BUILDTREE_ONLY
    Plugin/CodeGenTestPass.cpp
    PARTIAL_SOURCES_INTENDED
    )
  set(CG_PLUGIN_TEST_PASS_SOURCES)
  set(CG_PLUGIN_TEST_PASS_LIBS CGPluginTestPass)
endif()

add_llvm_unittest(CGPluginTest
  PluginTest.cpp
  ${CG_PLUGIN_TEST_PASS_SOURCES}

  EXPORT_SYMBOLS
  )
target_link_libraries(CGPluginTest PRIVATE LLVMTestingSupport ${CG_PLUGIN_TEST_PASS_LIBS})

unset(LLVM_LINK_COMPONENTS)
add_subdirectory(Plugin)
