#!/bin/bash
# scripts/check-style.sh — nxstyle 编码规范检查
# 检查板级所有 C/H 文件是否符合 NuttX 编码规范
set -e

WORKSPACE="/home/takumi/mi/open-velao-contest/ctrl_future"
CONTEST_DIR="contest2026_137_CtrlFuture"

cd "$WORKSPACE"

# 确保 nxstyle 已编译
if [ ! -x nuttx/tools/nxstyle ]; then
    echo "Building nxstyle..."
    make -C nuttx/tools -f Makefile.host nxstyle 2>/dev/null
fi

FAIL=0
FILES=$(find "$CONTEST_DIR/board" "$CONTEST_DIR/app" "$CONTEST_DIR/arch" \
    -name "*.c" -o -name "*.h" 2>/dev/null)

for f in $FILES; do
    # 运行 nxstyle（过滤 vendor 路径误报）
    OUTPUT=$(nuttx/tools/nxstyle "$f" 2>&1 \
        | grep -v "Path relative to repository" \
        | grep -v "Relative file path does not match actual file" || true)
    if [ -n "$OUTPUT" ]; then
        echo "--- $f ---"
        echo "$OUTPUT"
        FAIL=1
    fi
done

# 额外检查：行宽 78 列（nxstyle 的核心规则）
for f in $FILES; do
    LONG_LINES=$(awk 'length > 78 {print NR": "$0}' "$f" || true)
    if [ -n "$LONG_LINES" ]; then
        echo "--- $f: lines > 78 columns ---"
        echo "$LONG_LINES"
        FAIL=1
    fi
done

# 检查执行权限
for f in $FILES; do
    if [ -x "$f" ]; then
        echo "$f: error: execute permission on source file"
        FAIL=1
    fi
done

if [ $FAIL -eq 0 ]; then
    echo "=== STYLE CHECK PASSED ==="
else
    echo ""
    echo "=== STYLE CHECK FAILED ==="
    echo "Auto-fix with: uncrustify -c nuttx/tools/uncrustify.cfg --replace --no-backup <file>"
    exit 1
fi
