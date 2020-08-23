# If we don't need RTTI or EH, there's no reason to export anything
# from the plugin.
if( NOT MSVC ) # MSVC mangles symbols differently, and
               # BinderIface.export contains C++ symbols.
  if( NOT LLVM_REQUIRES_RTTI )
    if( NOT LLVM_REQUIRES_EH )
      set(LLVM_EXPORTED_SYMBOL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/BinderIface.exports)
    endif()
  endif()
endif()

add_llvm_loadable_module(BinderIface BinderIface.cpp PLUGIN_TOOL clang)
SET(CMAKE_INSTALL_RPATH "")
if(LLVM_ENABLE_PLUGINS AND (WIN32 OR CYGWIN))
  target_link_libraries(BinderIface PRIVATE
    clangAST
    clangBasic
    clangFrontend
    LLVMSupport
    )
endif()

INCLUDE_DIRECTORIES(/usr/include/libxml2)
#LINK_DIRECTORIES(/usr/lib/x86_64-linux-gnu)
#rosbuild_add_executable(kinectueye src/kinect_ueye.cpp)
TARGET_LINK_LIBRARIES(BinderIface xml2 clangToolingCore)
