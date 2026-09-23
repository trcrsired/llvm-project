unset(LLVM_LINK_COMPONENTS)
set(CG_TEST_PLUGIN_SOURCES CodeGenTestPass.cpp Plugin.cpp)
set(CG_TEST_PLUGIN_LIBS)
set(CG_TEST_PLUGIN_PARTIAL_SOURCES)
if (WIN32 OR CYGWIN)
  # Symbols are not interposed on DLL platforms: import the LLVM symbols from
  # the component libraries and the pass itself from CGPluginTestPass, so that
  # the plugin shares state (CodeGenTest::ID, CodeGenTest::RunCallback) with
  # the test binary.
  set(LLVM_LINK_COMPONENTS
    CodeGen
    Target
    Core
    Support
    )
  set(CG_TEST_PLUGIN_SOURCES Plugin.cpp)
  set(CG_TEST_PLUGIN_LIBS CGPluginTestPass)
  # CodeGenTestPass.cpp is compiled into CGPluginTestPass instead.
  set(CG_TEST_PLUGIN_PARTIAL_SOURCES PARTIAL_SOURCES_INTENDED)
endif()

add_llvm_library(CGTestPlugin MODULE BUILDTREE_ONLY
  ${CG_TEST_PLUGIN_SOURCES}
  ${CG_TEST_PLUGIN_PARTIAL_SOURCES}

  LINK_LIBS
  ${CG_TEST_PLUGIN_LIBS}

  DEPENDS
  intrinsics_gen
  vt_gen
  )

set_target_properties(CGTestPlugin PROPERTIES FOLDER "Tests")
add_dependencies(CGPluginTest CGTestPlugin)
