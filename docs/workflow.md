# Cross-Platform Workflow

How this repo is developed across two machines:

- **Outside (Ubuntu)** — code editing, documentation, git operations, protocol design
- **Home (Windows + STM32CubeIDE)** — `.ioc` configuration, build, flash, debug

Both machines pull from and push to the same GitHub remote.

---

## Golden Rules

1. **One machine at a time.** Never edit on both simultaneously. `git pull` before any work, `git push` after.
2. **`.ioc` belongs to CubeIDE only.** Do not edit `.ioc` files in a text editor. Open them in CubeMX/CubeIDE on the home machine.
3. **User code goes in `USER CODE` blocks or separate files.** CubeIDE regenerates `main.c` and HAL init files on every `.ioc` save. Code outside `/* USER CODE BEGIN */ ... /* USER CODE END */` blocks will be erased.
4. **Prefer separate `app_*.c/h` files** over USER CODE blocks for non-trivial logic. They survive regeneration without ceremony.

---

## What to Edit Where

### Safe to edit on Ubuntu (outside)

- `Core/Src/main.c` — only inside `USER CODE` blocks
- `Core/Inc/main.h` — only inside `USER CODE` blocks
- `Core/Src/app_*.c`, `Core/Inc/app_*.h` — application code in separate files
- `Core/Src/freertos.c` — only inside `USER CODE` blocks
- All Python code under `rpi/`
- All documentation under `docs/`, `README.md`
- `.gitignore`, `.gitattributes`, this file

### Edit on Windows (home, in CubeIDE)

- `*.ioc` files — pin assignments, clock config, peripheral setup
- Any code change that requires running the debugger
- Final flash to hardware (drag & drop `.bin` to NODE_F446RE drive)

---

## Standard Session Flow

### Outside (Ubuntu) session

```bash
cd ~/projects/multi-mcu-can
git pull origin main          # always pull first
# ... edit code, write docs ...
git status                    # review changes
git add -A
git commit -m "phase2: add CAN message dispatcher skeleton"
git push origin main
```

### Home (Windows + CubeIDE) session

```
1. Open Git Bash or CubeIDE's git plugin
2. git pull origin main
3. Open project in CubeIDE
4. If .ioc changed on the other machine, CubeIDE will prompt to regenerate
5. Build → Flash → Debug
6. If you modified code or .ioc:
   git add -A
   git commit -m "..."
   git push origin main
```

---

## Avoiding the Two Most Common Mistakes

### Mistake 1: Editing `.ioc` in a text editor

The `.ioc` file is INI-format text, so it's tempting. **Don't.** CubeMX maintains internal cross-references and a checksum-like consistency that breaks on hand edits. Symptoms range from "MX_*_Init() not regenerated" to full project corruption.

**Fix:** If a peripheral change is needed and CubeIDE isn't available, write a TODO note in `docs/todo.md` and handle it on the home machine.

### Mistake 2: Code outside USER CODE blocks

```c
/* USER CODE BEGIN Includes */
#include "app_can.h"      // ✅ survives regeneration
/* USER CODE END Includes */

#include "stm32f4xx_hal.h"
#include "my_helper.h"    // ❌ outside USER CODE blocks — will be deleted next regen
```

**Fix:** Always check that user additions are between `BEGIN` / `END` markers. Better: put non-trivial code in `app_*.c/h` files and only call them from inside USER CODE blocks.

---

## Optional: Building on Ubuntu

If you want to verify compilation without the home machine:

```bash
sudo apt install gcc-arm-none-eabi
# Then either:
#   - Use CubeIDE's exported Makefile (Project Properties → C/C++ Build → ...),
#   - Or write a CMakeLists.txt targeting arm-none-eabi-gcc.
```

This gives you compilation errors and `.bin` output on Ubuntu. You still need the home machine for flashing and debugging unless you also set up `openocd` + ST-Link drivers on Ubuntu.

**Recommendation:** Skip this until the workflow above feels comfortable. Adding a second toolchain before the basic git flow is solid is exactly the "two new layers at once" anti-pattern this repo was built to avoid.

---

## When Things Go Wrong

### "CubeIDE shows merge conflicts in regenerated files"

The regenerated files (`main.c`, etc.) probably differ in non-USER-CODE sections because both machines regenerated them slightly differently. **Resolve by:** keeping the home machine's version (since it just ran CubeMX), then re-applying your USER CODE additions if any were lost. Tedious but recoverable. Prevention: don't regenerate on the Ubuntu side at all (you shouldn't be opening `.ioc` there anyway).

### "Line ending hell"

If you see entire files marked as changed with no apparent diff, it's CRLF/LF. The `.gitattributes` file should prevent this. If it still happens once:

```bash
git rm --cached -r .
git reset --hard
```

This re-checks-out everything with the new normalization rules applied.

### "I committed a `.elf` / `Debug/` folder"

```bash
git rm -r --cached Debug/ *.elf
git commit -m "remove build artifacts"
```

The `.gitignore` will prevent it next time.