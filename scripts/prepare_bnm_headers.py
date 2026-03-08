from __future__ import annotations

import shutil
from pathlib import Path


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    src_include = repo_root / "jni" / "external" / "BNM" / "include"
    dst_include = repo_root / "jni" / "generated" / "BNM" / "include"
    custom_global_settings = (
        repo_root / "jni" / "config" / "BNM" / "UserSettings" / "GlobalSettings.hpp"
    )
    src_dobby = repo_root / "jni" / "external" / "Dobby"
    dst_dobby = repo_root / "jni" / "generated" / "Dobby"

    if not src_include.is_dir():
        raise FileNotFoundError(f"BNM include directory not found: {src_include}")
    if not custom_global_settings.is_file():
        raise FileNotFoundError(
            f"Custom GlobalSettings.hpp not found: {custom_global_settings}"
        )
    if not src_dobby.is_dir():
        raise FileNotFoundError(f"Dobby directory not found: {src_dobby}")

    if dst_include.exists():
        shutil.rmtree(dst_include)
    if dst_dobby.exists():
        shutil.rmtree(dst_dobby)

    shutil.copytree(src_include, dst_include)
    shutil.copytree(
        src_dobby,
        dst_dobby,
        ignore=shutil.ignore_patterns(".git", "prebuilt", "cmake-build-*"),
    )
    shutil.copy2(
        custom_global_settings,
        dst_include / "BNM" / "UserSettings" / "GlobalSettings.hpp",
    )

    os_arch_features = dst_dobby / "common" / "os_arch_features.h"
    process_runtime = (
        dst_dobby / "source" / "Backend" / "UserMode" / "PlatformUtil" / "Linux" / "ProcessRuntime.cc"
    )
    code_patch_tool = (
        dst_dobby / "source" / "Backend" / "UserMode" / "ExecMemory" / "code-patch-tool-posix.cc"
    )
    closure_trampoline = (
        dst_dobby
        / "source"
        / "TrampolineBridge"
        / "ClosureTrampolineBridge"
        / "arm64"
        / "ClosureTrampolineARM64.cc"
    )
    near_trampoline = (
        dst_dobby
        / "source"
        / "InterceptRouting"
        / "NearBranchTrampoline"
        / "near_trampoline_arm64.cc"
    )

    os_arch_features_text = os_arch_features.read_text(encoding="utf-8")
    os_arch_features_text = os_arch_features_text.replace(
        '#include "PlatformUnifiedInterface/platform.h"\n',
        '#include "PlatformUnifiedInterface/platform.h"\n#include <sys/mman.h>\n#include <unistd.h>\n',
    )
    os_arch_features_text = os_arch_features_text.replace(
        "inline void make_memory_readable(void *address, size_t size) {\n"
        "#if defined(ANDROID)\n"
        "  auto page = (void *)ALIGN_FLOOR(address, OSMemory::PageSize());\n"
        "  if (!OSMemory::SetPermission(page, OSMemory::PageSize(), kReadExecute)) {\n"
        "    return;\n"
        "  }\n"
        "#endif\n"
        "}\n",
        "inline void make_memory_readable(void *address, size_t size) {\n"
        "#if defined(ANDROID)\n"
        "  const long page_size = sysconf(_SC_PAGESIZE);\n"
        "  if (page_size <= 0) {\n"
        "    return;\n"
        "  }\n"
        "  auto page = (void *)ALIGN_FLOOR(address, page_size);\n"
        "  const size_t protect_size = ((size + (size_t)page_size - 1) / (size_t)page_size) * (size_t)page_size;\n"
        "  mprotect(page, protect_size == 0 ? (size_t)page_size : protect_size, PROT_READ | PROT_EXEC);\n"
        "#endif\n"
        "}\n",
    )
    os_arch_features.write_text(os_arch_features_text, encoding="utf-8")

    process_runtime_text = process_runtime.read_text(encoding="utf-8")
    process_runtime_text = process_runtime_text.replace(
        "  return (a.start < b.start);\n",
        "  return (a.start() < b.start());\n",
    )
    process_runtime_text = process_runtime_text.replace("module.load_address", "module.base")
    process_runtime.write_text(process_runtime_text, encoding="utf-8")

    code_patch_tool.write_text(
        code_patch_tool.read_text(encoding="utf-8").replace(
            '#include "core/arch/Cpu.h"\n',
            "",
        ),
        encoding="utf-8",
    )

    near_trampoline.write_text(
        near_trampoline.read_text(encoding="utf-8").replace(
            "#define assert(x)",
            "#undef assert\n#define assert(x)",
        ),
        encoding="utf-8",
    )

    closure_trampoline_text = closure_trampoline.read_text(encoding="utf-8")
    closure_trampoline_text = closure_trampoline_text.replace(
        "  auto closure_bridge_addr = (addr_t)get_closure_bridge_addr();\n"
        "  auto closure_bridge_data_label = _ createDataLabel(closure_bridge_addr);\n",
        "  auto closure_bridge_data_label = _ createDataLabel((addr_t)closure_bridge_addr);\n",
    )
    closure_trampoline_text = closure_trampoline_text.replace(
        "  _ EmitInt64((uint64_t)tramp_entry);\n",
        "  _ EmitInt64((uint64_t)closure_tramp);\n",
    )
    closure_trampoline_text = closure_trampoline_text.replace(
        "  auto tramp_code_block =\n"
        "      AssemblerCodeBuilder::FinalizeFromTurboAssembler(static_cast<AssemblerBase *>(&turbo_assembler_));\n"
        "#endif\n"
        "\n"
        "  closure_tramp->buffer = tramp_block;\n",
        "  auto tramp_block =\n"
        "      AssemblerCodeBuilder::FinalizeFromTurboAssembler(static_cast<AssemblerBase *>(&turbo_assembler_));\n"
        "#endif\n"
        "\n"
        "  closure_tramp->buffer = tramp_block;\n",
    )
    closure_trampoline.write_text(closure_trampoline_text, encoding="utf-8")

    print(f"[bnm] prepared headers: {dst_include}")
    print(f"[dobby] prepared source tree: {dst_dobby}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
