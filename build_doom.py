import os
import sys
import subprocess
import shutil

ROOT_DIR = r"C:\Projet\Banana-OS"
SDK_DIR = os.path.join(ROOT_DIR, "sdk")
DOOM_DIR = os.path.join(ROOT_DIR, "apps", "doom")
BUILD_DIR = os.path.join(DOOM_DIR, "build")

CLANG = r"C:\Program Files\LLVM\bin\clang.exe"
LLD = r"C:\Program Files\LLVM\bin\ld.lld.exe"
BPKG = os.path.join(SDK_DIR, "tools", "bpkg")

SRC_DOOM = [
    "dummy.c", "am_map.c", "doomdef.c", "doomstat.c", "dstrings.c", "d_event.c", "d_items.c",
    "d_iwad.c", "d_loop.c", "d_main.c", "d_mode.c", "d_net.c", "f_finale.c", "f_wipe.c",
    "g_game.c", "hu_lib.c", "hu_stuff.c", "info.c", "i_cdmus.c", "i_endoom.c", "i_joystick.c",
    "i_scale.c", "i_sound.c", "i_system.c", "i_timer.c", "memio.c", "m_argv.c", "m_bbox.c",
    "m_cheat.c", "m_config.c", "m_controls.c", "m_fixed.c", "m_menu.c", "m_misc.c",
    "m_random.c", "p_ceilng.c", "p_doors.c", "p_enemy.c", "p_floor.c", "p_inter.c",
    "p_lights.c", "p_map.c", "p_maputl.c", "p_mobj.c", "p_plats.c", "p_pspr.c",
    "p_saveg.c", "p_setup.c", "p_sight.c", "p_spec.c", "p_switch.c", "p_telept.c",
    "p_tick.c", "p_user.c", "r_bsp.c", "r_data.c", "r_draw.c", "r_main.c", "r_plane.c",
    "r_segs.c", "r_sky.c", "r_things.c", "sha1.c", "sounds.c", "statdump.c", "st_lib.c",
    "st_stuff.c", "s_sound.c", "tables.c", "v_video.c", "wi_stuff.c", "w_checksum.c",
    "w_file.c", "w_main.c", "w_wad.c", "z_zone.c", "w_file_stdc.c", "i_input.c",
    "i_video.c", "doomgeneric.c", "doomgeneric_banana.c", "i_bananasound.c"
]

LIBC_SRCS = ["crt0.c", "libc.c", "stdio.c", "banana.c", "math.c", "posix.c", "setjmp.c"]

def compile_arch(arch, clang_target, cflags_arch, lld_emul):
    print(f"\n=== Compiling for {arch} ===")
    out_dir = os.path.join(BUILD_DIR, arch)
    os.makedirs(out_dir, exist_ok=True)

    cflags = [
        f"--target={clang_target}",
        "-ffreestanding",
        "-fno-stack-protector",
        "-fpie",
        "-fvisibility=hidden",
        "-nostdlib",
        "-nostdinc",
        "-fno-asynchronous-unwind-tables",
        "-fno-exceptions",
        "-fno-builtin-malloc",
        "-fno-math-errno",
        "-O2",
        "-Wno-everything",  # doom code has old C idioms
        "-DNORMALUNIX",
        "-DLINUX",
        "-DFEATURE_SOUND=1",
        "-D__bool_true_false_are_defined=1",
        "-DDOOMGENERIC_RESX=640",
        "-DDOOMGENERIC_RESY=400",
        f"-I{os.path.join(SDK_DIR, 'include')}",
        f"-I{DOOM_DIR}"
    ] + cflags_arch

    objs = []

    # Compile SDK libc
    for src in LIBC_SRCS:
        src_path = os.path.join(SDK_DIR, "lib", src)
        obj_path = os.path.join(out_dir, "sdk_" + src.replace(".c", ".o"))
        subprocess.check_call([CLANG] + cflags + ["-c", src_path, "-o", obj_path])
        objs.append(obj_path)

    if arch == "i686":
        # i686 needs divdi3
        div_src = os.path.join(SDK_DIR, "lib", "divdi3.c")
        div_obj = os.path.join(out_dir, "sdk_divdi3.o")
        subprocess.check_call([CLANG] + cflags + ["-c", div_src, "-o", div_obj])
        objs.append(div_obj)

    # Compile DOOM sources
    total = len(SRC_DOOM)
    for i, src in enumerate(SRC_DOOM, 1):
        src_path = os.path.join(DOOM_DIR, src)
        obj_path = os.path.join(out_dir, src.replace(".c", ".o"))
        print(f"[{i}/{total}] {src}...", end="\r", flush=True)
        subprocess.check_call([CLANG] + cflags + ["-c", src_path, "-o", obj_path])
        objs.append(obj_path)
    print(f"[{total}/{total}] Compilation completed.          ")

    # Link with lld
    app_out = os.path.join(BUILD_DIR, f"app-{arch}")
    print(f"Linking {app_out}...")
    ldflags = [
        "-m", lld_emul,
        "-pie",
        "--no-dynamic-linker",
        "-z", "noexecstack",
        "-z", "norelro",
        "-z", "max-page-size=4096",
        "-z", "noseparate-code",
        "--hash-style=sysv",
        "-e", "_banana_start",
        "-s",
        "-o", app_out
    ] + objs
    subprocess.check_call([LLD] + ldflags)
    return app_out

def main():
    os.makedirs(BUILD_DIR, exist_ok=True)

    # 1. Compile x86_64
    app_x86_64 = compile_arch(
        "x86_64",
        "x86_64-unknown-elf",
        ["-m64", "-mno-red-zone", "-mcmodel=small"],
        "elf_x86_64"
    )

    # 2. Compile i686
    app_i686 = compile_arch(
        "i686",
        "i686-unknown-elf",
        ["-m32", "-march=i686", "-mstackrealign"],
        "elf_i386"
    )

    # 3. Create manifest
    manifest_path = os.path.join(BUILD_DIR, "manifest")
    with open(manifest_path, "w", encoding="utf-8") as f:
        f.write(
            "name=doom\n"
            "title=DOOM\n"
            "version=1.0\n"
            "type=gui\n"
            "description=Classic DOOM with .wad launcher\n"
            "author=id Software / Banana OS Port\n"
            "category=Games\n"
            "depends=\n"
        )

    # 4. Package with bpkg
    bpk_path = os.path.join(DOOM_DIR, "doom.bpk")
    icon_path = os.path.join(DOOM_DIR, "icon.png")

    wad_files = []
    for w in ["doom1.wad", "doom2.wad", "doom3.wad"]:
        wp = os.path.join(DOOM_DIR, w)
        if os.path.exists(wp):
            wad_files.append(wp)

    pack_cmd = [
        sys.executable, BPKG, "pack", "-o", bpk_path,
        manifest_path, app_x86_64, app_i686
    ] + wad_files + [f"icon.png={icon_path}"]
    print(f"\nPacking {bpk_path} with {len(wad_files)} WADs...")
    subprocess.check_call(pack_cmd)

    # 5. Check package
    print("Verifying package...")
    subprocess.check_call([sys.executable, BPKG, "check", bpk_path])

    # Also copy doom.bpk to root C:\Projet\doom.bpk for easy web serving
    shutil.copyfile(bpk_path, os.path.join(r"C:\Projet", "doom.bpk"))
    print("\n[SUCCESS] doom.bpk is ready in C:\\Projet\\doom.bpk and apps\\doom\\doom.bpk !")

if __name__ == "__main__":
    main()
