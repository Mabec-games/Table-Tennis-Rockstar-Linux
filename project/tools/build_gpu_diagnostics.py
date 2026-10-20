"""Build an isolated diagnostic Vulkan plugin against the pinned Linux SDK.

Default builds diagnostics; --production stages only the confirmed renderer fix.
All outputs remain under build/. Original plugins are backed up.
"""
from pathlib import Path
import argparse
import ctypes
import os
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / '.tools/rexglue-source'
SDK = ROOT / '.tools/sdk-linux/linux-amd64'
WORK = ROOT / 'build/gpu-diagnostics'


def main():
    global WORK
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--production', action='store_true')
    args = parser.parse_args()
    if args.production:
        WORK = ROOT / 'build/gpu-fixed'
    revision = subprocess.check_output(['git', '-C', str(SOURCE), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != 'f5337cdc947ff6d4c4196737e2c807a48f2a1fc2':
        raise ValueError('Expected pinned SDK v0.10.0 source')
    if not (SOURCE / 'thirdparty/glslang/SPIRV/SpvBuilder.h').is_file():
        raise ValueError('Missing pinned glslang headers. Run: git -C .tools/rexglue-source submodule update --init --depth 1 thirdparty/glslang')
    graphics = WORK / 'src'
    shutil.copytree(SOURCE / 'src/graphics', graphics, dirs_exist_ok=True)
    shutil.copy2(ROOT / 'project/src/gpu_worker_profile.h', graphics / 'tt_gpu_worker_profile.h')
    path = graphics / 'command_processor.cpp'
    text = path.read_text()
    text = text.replace('#include <rex/logging.h>', '#include <rex/logging.h>\n#include "tt_gpu_worker_profile.h"', 1)
    needle = '  while (worker_running_) {'
    assert text.count(needle) == 1
    text = text.replace(needle, needle + '\n    TabletennisGpuProfileEvent(0);', 1)
    needle = '      PrepareForWait();\n      uint32_t loop_count = 0;'
    assert text.count(needle) == 1
    text = text.replace(needle, needle.replace('      uint32_t loop_count = 0;',
        '      TabletennisGpuProfileScope profile_idle(5);\n      uint32_t loop_count = 0;'), 1)
    needle = '  SCOPE_profile_cpu_f("gpu");\n\n  // wait until a register or memory location is a specific value'
    assert text.count(needle) == 1
    text = text.replace(needle, needle.replace('\n\n  // wait',
        '\n  TabletennisGpuProfileScope profile_wait_reg(1);\n\n  // wait'), 1)
    path.write_text(text)
    path = graphics / 'vulkan/command_processor.cpp'
    text = path.read_text()
    text = text.replace('#include <rex/logging.h>', '#include <rex/logging.h>\n#include "../tt_gpu_worker_profile.h"', 1)
    needle = '    VkResult wait_result =\n        dfn.vkWaitForFences(device, uint32_t(await_submission - submission_completed_),\n                            submissions_in_flight_fences_.data(), VK_TRUE, UINT64_MAX);'
    assert text.count(needle) == 1
    text = text.replace(needle, '    TabletennisGpuProfileScope profile_fence_wait(3);\n' + needle, 1)
    path.write_text(text)
    if not args.production:
        shutil.copy2(ROOT / 'project/tools/renderdoc_guest_capture.h',
                     graphics / 'vulkan/tt_renderdoc_capture.h')
        path = graphics / 'vulkan/command_processor.cpp'
        text = path.read_text()
        text = text.replace('#include <rex/logging.h>',
                            '#include <rex/logging.h>\n#include "tt_renderdoc_capture.h"', 1)
        needle = '  bool is_opening_frame = is_guest_command && !frame_open_;'
        assert text.count(needle) == 1
        text = text.replace(needle, needle +
                            '\n  if (is_opening_frame) tt_diagnostics::GuestFrameBoundary();')
        path.write_text(text)
        path = graphics / 'vulkan/texture_cache.cpp'
        text = path.read_text()
        needle = '  if (command_processor_.GetVulkanDevice()->properties().nullDescriptor) {'
        assert text.count(needle) == 1
        text = text.replace(needle, '''  // Log each missing fetch/dimension/sign combination once per process.
  static std::unordered_map<uint32_t, bool> reported_missing;
  uint32_t diagnostic_key = fetch_constant_index | (uint32_t(dimension) << 8) |
                            (uint32_t(is_signed) << 16);
  if (reported_missing.emplace(diagnostic_key, true).second) {
    REXGPU_WARN("TT_DIAG missing texture: fetch={} shader_dim={} signed={} binding={} data_dim={}",
                fetch_constant_index, uint32_t(dimension), is_signed, binding != nullptr,
                binding ? uint32_t(binding->key.dimension) : 255u);
  }
''' + needle)
        path.write_text(text)
    # Texture result exp_adjust is in dword 3; dword 4 contains LOD bias.
    # Captured skin fetches have dword-3 bias 0 but dword-4 bits decode as -8.
    path = graphics / 'pipeline/shader/spirv_translator_fetch.cpp'
    text = path.read_text()
    old = '''        // Apply the exponent bias from the bits 13:18 of the fetch constant
        // word 4.
        spv::Id result_exponent_bias = builder_->createBinBuiltinCall(
            type_float_, ext_inst_glsl_std_450_, GLSLstd450Ldexp, const_float_1_,
            builder_->createTriOp(spv::OpBitFieldSExtract, type_int_, fetch_constant_word_4_signed,
                                  builder_->makeUintConstant(13), builder_->makeUintConstant(6)));'''
    new = '''        // Apply exp_adjust from bits 13:18 of fetch dword 3, matching
        // xe_gpu_texture_fetch_t and the DXBC translator. Dword 4 is LOD.
        id_vector_temp_.clear();
        id_vector_temp_.push_back(const_int_0_);
        id_vector_temp_.push_back(
            builder_->makeIntConstant(int((fetch_constant_word_0_index + 3) >> 2)));
        id_vector_temp_.push_back(
            builder_->makeIntConstant(int((fetch_constant_word_0_index + 3) & 3)));
        spv::Id result_exponent_word_3 = builder_->createLoad(
            builder_->createAccessChain(spv::StorageClassUniform, uniform_fetch_constants_,
                                        id_vector_temp_),
            spv::NoPrecision);
        spv::Id result_exponent_word_3_signed =
            builder_->createUnaryOp(spv::OpBitcast, type_int_, result_exponent_word_3);
        spv::Id result_exponent_bias = builder_->createBinBuiltinCall(
            type_float_, ext_inst_glsl_std_450_, GLSLstd450Ldexp, const_float_1_,
            builder_->createTriOp(spv::OpBitFieldSExtract, type_int_, result_exponent_word_3_signed,
                                  builder_->makeUintConstant(13), builder_->makeUintConstant(6)));'''
    assert text.count(old) == 1, 'Pinned texture exponent code changed'
    text = text.replace(old, new)
    text = text.replace('biasing, for result exponent biasing, and conditionally for stacked',
                        'biasing and conditionally for stacked')
    path.write_text(text)
    if not args.production:
        path = graphics / 'vulkan/pipeline_cache.cpp'
        text = path.read_text()
        needle = '  if (translation.GetOrCreateShaderModule() == VK_NULL_HANDLE) {'
        assert text.count(needle) == 1
        text = text.replace(needle, '''  if (!REXCVAR_GET(dump_shaders).empty()) {
    translation.Dump(REXCVAR_GET(dump_shaders), "spirv");
  }
''' + needle)
        path.write_text(text)
    original = (SOURCE / 'src/graphics/CMakeLists.txt').read_text()
    sources = []
    for group in ('REXGPU_XENOS_SOURCES', 'REXGPU_XENOS_SPIRV_SOURCES', 'REXGPU_XENOS_VULKAN_SOURCES'):
        match = re.search(r'set\(' + group + r'\s+(.*?)\n\)', original, re.S)
        assert match
        for line in match[1].splitlines():
            line = line.split('#', 1)[0].strip()
            if line:
                sources.append('src/' + line)
    cmake = '''cmake_minimum_required(VERSION 3.25)
project(tabletennis_gpu_diagnostics LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
find_package(rexglue 0.10.0 EXACT REQUIRED CONFIG)
add_library(tt_gpu_diagnostics SHARED
''' + '\n'.join(sources) + ''')
set_target_properties(tt_gpu_diagnostics PROPERTIES
  CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON
  OUTPUT_NAME rexgpu-xenos BUILD_RPATH "${SDK_PREFIX}/lib")
target_include_directories(tt_gpu_diagnostics PRIVATE
  "${SDK_SOURCE}/include" "${SDK_SOURCE}/thirdparty/glslang"
  "${SDK_SOURCE}/thirdparty/renderdoc" "${SDK_SOURCE}")
target_compile_definitions(tt_gpu_diagnostics PRIVATE
  REX_HAS_D3D12=0 SPDLOG_COMPILED_LIB SPDLOG_FMT_EXTERNAL)
target_compile_options(tt_gpu_diagnostics PRIVATE -msse4.1)
target_link_libraries(tt_gpu_diagnostics PRIVATE rex::runtime
  "${SDK_PREFIX}/lib/libxxhash.a"
  "${SDK_PREFIX}/lib/libSPIRV.a" "${SDK_PREFIX}/lib/libglslang.a"
  "${SDK_PREFIX}/lib/libMachineIndependent.a" "${SDK_PREFIX}/lib/libGenericCodeGen.a"
  "${SDK_PREFIX}/lib/libOSDependent.a" "${SDK_PREFIX}/lib/libOGLCompiler.a")
'''
    (WORK / 'CMakeLists.txt').write_text(cmake)
    subprocess.run(['cmake', '-S', str(WORK), '-B', str(WORK / 'out'),
                    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_COMPILER=clang++',
                    f'-DCMAKE_PREFIX_PATH={SDK}', f'-DSDK_PREFIX={SDK}', f'-DSDK_SOURCE={SOURCE}'], check=True)
    subprocess.run(['cmake', '--build', str(WORK / 'out'), '--parallel', '4'], check=True)
    plugin = WORK / 'out/librexgpu-xenos.so'
    library = ctypes.CDLL(str(plugin), mode=os.RTLD_NOW)
    library.rex_gpu_abi_version.restype = ctypes.c_uint32
    if library.rex_gpu_abi_version() != 1:
        raise ValueError('Unexpected diagnostic plugin ABI')
    name = 'librexgpu-xenos.so' if args.production else 'librexgpu-xenos-diagnostics.so'
    destination = ROOT / 'build/linux' / name
    backup = destination.with_name(destination.stem + '.before-exp-fix.so')
    if destination.exists() and not backup.exists():
        shutil.copy2(destination, backup)
    staged = destination.with_suffix('.so.new')
    shutil.copy2(plugin, staged)
    os.replace(staged, destination)
    print(('Fixed production' if args.production else 'Diagnostic') +
          ' plugin built, load-checked and staged: ' + str(destination))


if __name__ == '__main__':
    main()
