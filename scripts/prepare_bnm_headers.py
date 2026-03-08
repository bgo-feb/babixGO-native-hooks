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

    if not src_include.is_dir():
        raise FileNotFoundError(f"BNM include directory not found: {src_include}")
    if not custom_global_settings.is_file():
        raise FileNotFoundError(
            f"Custom GlobalSettings.hpp not found: {custom_global_settings}"
        )

    if dst_include.exists():
        shutil.rmtree(dst_include)

    shutil.copytree(src_include, dst_include)
    shutil.copy2(
        custom_global_settings,
        dst_include / "BNM" / "UserSettings" / "GlobalSettings.hpp",
    )

    print(f"[bnm] prepared headers: {dst_include}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

