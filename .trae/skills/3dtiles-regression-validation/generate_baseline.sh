#!/bin/bash
# generate_baseline.sh - 生成回归测试基准数据（完整版）
# 支持 test_config.json v2.0 中定义的所有测试套件

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BASELINE_DIR="${PROJECT_ROOT}/test_data/baseline"
CONFIG_FILE="${SCRIPT_DIR}/test_config.json"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印函数
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检测可执行文件
find_executable() {
    if [ -f "${PROJECT_ROOT}/target/release/_3dtile" ]; then
        echo "${PROJECT_ROOT}/target/release/_3dtile"
    elif [ -f "${PROJECT_ROOT}/target/debug/_3dtile" ]; then
        echo "${PROJECT_ROOT}/target/debug/_3dtile"
    else
        echo ""
    fi
}

# 显示帮助
show_help() {
    cat << EOF
生成回归测试基准数据

用法: $0 [选项] [测试套件]

选项:
    -h, --help          显示帮助信息
    -l, --list          列出所有可用的测试用例
    -c, --clean         清理现有基准数据
    -f, --force         强制覆盖现有基准数据
    -v, --verbose       详细输出
    -s, --suite SUITE   指定测试套件 (smoke|core|optimization|combination|export|performance|edge_cases|all)
    -t, --test TEST     指定单个测试用例

示例:
    $0 --list                           # 列出所有测试用例
    $0 --suite smoke                    # 只生成smoke测试的基准数据
    $0 --suite core                     # 生成core测试套件的基准数据
    $0 --suite all                      # 生成所有测试套件的基准数据
    $0 --test osgb_basic                # 只生成指定测试的基准数据
    $0 --suite optimization --force     # 强制重新生成优化测试的基准数据

EOF
}

# 列出所有测试用例
list_tests() {
    if [ ! -f "$CONFIG_FILE" ]; then
        print_error "找不到配置文件: $CONFIG_FILE"
        exit 1
    fi

    echo "可用的测试套件和用例:"
    echo "===================="

    # 使用 Python 解析 JSON
    python3 << EOF
import json
import sys

try:
    with open('$CONFIG_FILE', 'r', encoding='utf-8') as f:
        config = json.load(f)

    for suite_name, suite in config.get('test_suites', {}).items():
        print(f"\n📦 {suite_name}")
        print(f"   描述: {suite.get('description', 'N/A')}")
        print(f"   测试数量: {len(suite.get('tests', []))}")
        print(f"   CI必需: {'是' if suite.get('ci_required') else '否'}")
        print(f"   超时: {suite.get('timeout', 'N/A')}秒")
        print("   测试用例:")

        for test in suite.get('tests', []):
            priority = test.get('priority', 'N/A')
            priority_icon = {'P0': '🔴', 'P1': '🟡', 'P2': '🟢'}.get(priority, '⚪')
            print(f"     {priority_icon} {test['name']} [{priority}]")
            print(f"        描述: {test.get('description', 'N/A')}")
            print(f"        输入: {test.get('input', 'N/A')}")
            if test.get('args'):
                print(f"        参数: {' '.join(test['args'])}")
            if test.get('condition'):
                print(f"        条件: {test['condition']}")
            print()
except Exception as e:
    print(f"错误: {e}", file=sys.stderr)
    sys.exit(1)
EOF
}

# 检查输入文件是否存在
check_input() {
    local input_path="$1"
    local condition="$2"

    # 处理条件
    if [ "$condition" == "manual" ]; then
        return 1  # 跳过手动测试
    fi

    if [ "$condition" == "file_exists" ]; then
        if [ ! -f "${PROJECT_ROOT}/${input_path}" ]; then
            return 1
        fi
        return 0
    fi

    # 默认检查
    if [ ! -e "${PROJECT_ROOT}/${input_path}" ]; then
        return 1
    fi

    return 0
}

# 生成单个测试的基准数据
generate_test_baseline() {
    local test_name="$1"
    local input_path="$2"
    local format="$3"
    local args="$4"
    local priority="$5"
    local condition="$6"

    # 检查是否应该跳过
    if ! check_input "$input_path" "$condition"; then
        print_warning "跳过 $test_name: 输入不存在或条件不满足"
        return 1
    fi

    local output_dir="${BASELINE_DIR}/${test_name}"

    # 检查是否已存在
    if [ -d "$output_dir" ] && [ "$FORCE" != "true" ]; then
        print_info "跳过 $test_name: 基准数据已存在 (使用 --force 覆盖)"
        return 0
    fi

    # 清理旧数据
    if [ -d "$output_dir" ]; then
        rm -rf "$output_dir"
    fi
    mkdir -p "$output_dir"

    print_info "生成 $test_name [${priority}]..."

    # 构建命令
    local full_input="${PROJECT_ROOT}/${input_path}"
    local output_path="$output_dir/"
    
    # gltf格式需要指定具体的glb文件名
    if [ "$format" == "gltf" ]; then
        output_path="$output_dir/bench.glb"
    fi
    
    local cmd="$EXECUTABLE -f $format -i \"$full_input\" -o \"$output_path\""

    # 添加参数
    if [ -n "$args" ] && [ "$args" != "null" ]; then
        # 解析JSON数组格式的参数
        local parsed_args=$(echo "$args" | python3 -c "import sys,json; print(' '.join(json.load(sys.stdin)))" 2>/dev/null || echo "")
        if [ -n "$parsed_args" ]; then
            cmd="$cmd $parsed_args"
        fi
    fi

    # 执行转换
    if [ "$VERBOSE" == "true" ]; then
        echo "执行: $cmd"
        eval $cmd 2>&1 | tee "${output_dir}/generation.log"
    else
        eval $cmd > "${output_dir}/generation.log" 2>&1
    fi

    # 检查结果
    if [ $? -eq 0 ] && [ -f "${output_dir}/tileset.json" -o -f "${output_dir}/bench.glb" ]; then
        print_success "✓ $test_name 生成成功"
        return 0
    else
        print_error "✗ $test_name 生成失败"
        return 1
    fi
}

# 生成指定套件的所有测试
generate_suite() {
    local suite_name="$1"

    print_info "生成测试套件: $suite_name"

    python3 << EOF
import json
import sys
import subprocess

try:
    with open('$CONFIG_FILE', 'r', encoding='utf-8') as f:
        config = json.load(f)

    suite = config.get('test_suites', {}).get('$suite_name')
    if not suite:
        print(f"错误: 找不到测试套件 '$suite_name'", file=sys.stderr)
        sys.exit(1)

    tests = suite.get('tests', [])
    total = len(tests)
    success = 0
    skipped = 0
    failed = 0

    print(f"\\n套件 '$suite_name' 包含 {total} 个测试用例\\n")

    for i, test in enumerate(tests, 1):
        test_name = test['name']
        input_path = test['input']
        format_type = test['format']
        args = test.get('args', [])
        priority = test.get('priority', 'P2')
        condition = test.get('condition', '')

        print(f"[{i}/{total}] {test_name} [{priority}]")

        # 构建参数JSON
        args_json = json.dumps(args)

        # 调用生成函数
        result = subprocess.run([
            'bash', '-c',
            f'source "$0"; generate_test_baseline "{test_name}" "{input_path}" "{format_type}" \'{args_json}\' "{priority}" "{condition}"',
            '${BASH_SOURCE[0]}'
        ], capture_output=True, text=True)

        if result.returncode == 0:
            success += 1
        elif "跳过" in result.stdout:
            skipped += 1
        else:
            failed += 1

        print(result.stdout)
        if result.stderr:
            print(result.stderr, file=sys.stderr)

    print(f"\\n套件 '$suite_name' 完成:")
    print(f"  成功: {success}")
    print(f"  跳过: {skipped}")
    print(f"  失败: {failed}")

    sys.exit(0 if failed == 0 else 1)

except Exception as e:
    print(f"错误: {e}", file=sys.stderr)
    sys.exit(1)
EOF
}

# 生成所有测试套件
generate_all() {
    print_info "生成所有测试套件的基准数据..."

    local suites=("smoke" "core" "optimization" "combination" "export" "performance" "edge_cases")
    local total_suites=${#suites[@]}
    local current=0

    for suite in "${suites[@]}"; do
        current=$((current + 1))
        print_info "[$current/$total_suites] 处理套件: $suite"
        generate_suite "$suite" || true
        echo ""
    done

    print_success "所有套件处理完成"
}

# 主函数
main() {
    local suite=""
    local test=""
    local list_only=false
    local clean=false
    FORCE=false
    VERBOSE=false

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -l|--list)
                list_only=true
                shift
                ;;
            -c|--clean)
                clean=true
                shift
                ;;
            -f|--force)
                FORCE=true
                shift
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -s|--suite)
                suite="$2"
                shift 2
                ;;
            -t|--test)
                test="$2"
                shift 2
                ;;
            *)
                print_error "未知选项: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # 检查可执行文件
    EXECUTABLE=$(find_executable)
    if [ -z "$EXECUTABLE" ]; then
        print_error "找不到可执行文件，请先构建项目"
        print_info "运行: cargo build --release"
        exit 1
    fi

    print_info "使用可执行文件: $EXECUTABLE"

    # 创建基准目录
    mkdir -p "$BASELINE_DIR"

    # 清理
    if [ "$clean" == true ]; then
        print_info "清理基准数据目录..."
        rm -rf "${BASELINE_DIR:?}/"*
        print_success "清理完成"
        exit 0
    fi

    # 列出测试
    if [ "$list_only" == true ]; then
        list_tests
        exit 0
    fi

    # 生成指定测试
    if [ -n "$test" ]; then
        print_info "生成单个测试: $test"
        # 从配置中查找测试信息
        python3 << EOF
import json
import sys
import subprocess

try:
    with open('$CONFIG_FILE', 'r', encoding='utf-8') as f:
        config = json.load(f)

    # 在所有套件中查找测试
    for suite_name, suite in config.get('test_suites', {}).items():
        for test_info in suite.get('tests', []):
            if test_info['name'] == '$test':
                test_name = test_info['name']
                input_path = test_info['input']
                format_type = test_info['format']
                args = test_info.get('args', [])
                priority = test_info.get('priority', 'P2')
                condition = test_info.get('condition', '')

                args_json = json.dumps(args)

                result = subprocess.run([
                    'bash', '-c',
                    f'source "$0"; generate_test_baseline "{test_name}" "{input_path}" "{format_type}" \'{args_json}\' "{priority}" "{condition}"',
                    '${BASH_SOURCE[0]}'
                ])
                sys.exit(result.returncode)

    print(f"错误: 找不到测试 '$test'", file=sys.stderr)
    sys.exit(1)

except Exception as e:
    print(f"错误: {e}", file=sys.stderr)
    sys.exit(1)
EOF
        exit $?
    fi

    # 生成指定套件或全部
    if [ -n "$suite" ]; then
        if [ "$suite" == "all" ]; then
            generate_all
        else
            generate_suite "$suite"
        fi
    else
        # 默认生成 core 套件
        print_info "未指定套件，默认生成 core 套件"
        generate_suite "core"
    fi

    # 显示总结
    echo ""
    print_success "基准数据生成完成!"
    print_info "基准数据位置: $BASELINE_DIR"

    # 显示生成的内容
    if [ -d "$BASELINE_DIR" ]; then
        echo ""
        echo "生成的测试用例:"
        find "$BASELINE_DIR" -maxdepth 1 -type d | sort | tail -n +2 | while read dir; do
            local test_name=$(basename "$dir")
            local file_count=$(find "$dir" -type f 2>/dev/null | wc -l)
            echo "  ✓ $test_name ($file_count 个文件)"
        done
    fi

    echo ""
    echo "使用以下命令运行回归测试:"
    echo "  python3 ${SCRIPT_DIR}/run_tests.py <suite> --mode strict"
    echo ""
    echo "示例:"
    echo "  python3 ${SCRIPT_DIR}/run_tests.py core --mode strict"
}

# 运行主函数
main "$@"
