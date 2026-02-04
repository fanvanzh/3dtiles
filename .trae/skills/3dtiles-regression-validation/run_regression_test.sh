#!/bin/bash
# run_regression_test.sh - 3D Tiles重构回归测试工作流

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BASELINE_DIR="${PROJECT_ROOT}/test_data/baseline"
TEST_OUTPUT_DIR="${PROJECT_ROOT}/test_data/test_output"
VALIDATOR="${SCRIPT_DIR}/regression_validator_v2.py"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的信息
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

# 显示帮助信息
show_help() {
    cat << EOF
3D Tiles 重构回归测试工具

用法: $0 [选项]

选项:
    -h, --help          显示帮助信息
    -g, --generate      生成基准数据
    -t, --test          运行回归测试
    -a, --all           生成基准数据并运行测试
    -m, --mode MODE     验证模式: strict|relaxed|fast (默认: strict)
    -c, --clean         清理测试输出
    -v, --verbose       详细输出

示例:
    $0 --generate       # 仅生成基准数据
    $0 --test           # 仅运行测试（需要已有基准数据）
    $0 --all            # 完整流程：生成基准 + 运行测试
    $0 --test --mode relaxed  # 使用宽松模式测试

EOF
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

# 生成基准数据
generate_baseline() {
    print_info "生成基准数据..."
    
    EXECUTABLE=$(find_executable)
    if [ -z "$EXECUTABLE" ]; then
        print_error "找不到可执行文件，请先构建项目: cargo build --release"
        exit 1
    fi
    
    print_info "使用可执行文件: $EXECUTABLE"
    
    # 创建基准目录
    mkdir -p "$BASELINE_DIR"
    
    # 清理旧的基准数据
    if [ -d "$BASELINE_DIR" ] && [ "$(ls -A "$BASELINE_DIR")" ]; then
        read -p "基准数据目录已存在，是否覆盖? (y/N): " confirm
        if [[ $confirm == [yY] || $confirm == [yY][eE][sS] ]]; then
            print_info "清理旧的基准数据..."
            rm -rf "${BASELINE_DIR:?}/"*
        else
            print_warning "跳过基准数据生成"
            return
        fi
    fi
    
    print_info "开始生成基准数据..."
    
    # 1. OSGB基本测试
    print_info "[1/5] OSGB -> 3D Tiles..."
    if [ -f "${PROJECT_ROOT}/data/OSGB/bench.osgb" ]; then
        mkdir -p "${BASELINE_DIR}/osgb_basic"
        $EXECUTABLE -f osgb -i "${PROJECT_ROOT}/data/OSGB/bench.osgb" -o "${BASELINE_DIR}/osgb_basic/" || print_warning "OSGB测试失败"
    else
        print_warning "跳过: 未找到 bench.osgb"
    fi
    
    # 2. 倾斜摄影测试
    print_info "[2/5] 倾斜摄影 -> 3D Tiles..."
    if [ -d "${PROJECT_ROOT}/data/Oblique/OSGBny" ] && [ -f "${PROJECT_ROOT}/data/Oblique/OSGBny/metadata.xml" ]; then
        mkdir -p "${BASELINE_DIR}/oblique_basic"
        $EXECUTABLE -f osgb -i "${PROJECT_ROOT}/data/Oblique/OSGBny" -o "${BASELINE_DIR}/oblique_basic/" || print_warning "倾斜摄影测试失败"
    else
        print_warning "跳过: 未找到倾斜摄影数据"
    fi
    
    # 3. Shapefile测试
    print_info "[3/5] Shapefile -> 3D Tiles..."
    SHP_FILE="${PROJECT_ROOT}/data/SHP/nanjing_buildings/南京市_百度建筑.shp"
    if [ -f "$SHP_FILE" ]; then
        mkdir -p "${BASELINE_DIR}/shp_basic"
        $EXECUTABLE -f shape -i "$SHP_FILE" -o "${BASELINE_DIR}/shp_basic/" --height height || print_warning "Shapefile测试失败"
    else
        print_warning "跳过: 未找到 Shapefile 数据"
    fi
    
    # 4. FBX测试
    print_info "[4/5] FBX -> 3D Tiles..."
    FBX_FILE="${PROJECT_ROOT}/data/FBX/TZCS_0829.FBX"
    if [ -f "$FBX_FILE" ]; then
        mkdir -p "${BASELINE_DIR}/fbx_basic"
        $EXECUTABLE -f fbx -i "$FBX_FILE" -o "${BASELINE_DIR}/fbx_basic/" --lon 118 --lat 32 --alt 20 || print_warning "FBX测试失败"
    else
        print_warning "跳过: 未找到 FBX 数据"
    fi
    
    # 5. OSGB -> GLTF
    print_info "[5/5] OSGB -> GLTF..."
    if [ -f "${PROJECT_ROOT}/data/OSGB/bench.osgb" ]; then
        mkdir -p "${BASELINE_DIR}/osgb_to_gltf"
        $EXECUTABLE -f gltf -i "${PROJECT_ROOT}/data/OSGB/bench.osgb" -o "${BASELINE_DIR}/osgb_to_gltf/bench.glb" || print_warning "GLTF导出失败"
    else
        print_warning "跳过: 未找到 OSGB 数据"
    fi
    
    print_success "基准数据生成完成!"
    echo ""
    echo "生成的测试用例:"
    find "$BASELINE_DIR" -maxdepth 1 -type d | tail -n +2 | while read dir; do
        echo "  - $(basename "$dir")"
    done
}

# 运行单个测试用例
run_single_test() {
    local test_name=$1
    local test_dir=$2
    local mode=$3
    
    print_info "测试: $test_name"
    
    # 检查基准数据是否存在
    if [ ! -d "$test_dir" ]; then
        print_warning "跳过: 基准数据不存在 $test_dir"
        return 1
    fi
    
    # 创建测试输出目录
    local output_dir="${TEST_OUTPUT_DIR}/${test_name}"
    rm -rf "$output_dir"
    mkdir -p "$output_dir"
    
    # 运行转换
    EXECUTABLE=$(find_executable)
    local input_file=""
    local format=""
    
    case "$test_name" in
        osgb_basic)
            format="osgb"
            input_file="${PROJECT_ROOT}/data/OSGB/bench.osgb"
            ;;
        oblique_basic)
            format="osgb"
            input_file="${PROJECT_ROOT}/data/Oblique/OSGBny"
            ;;
        shp_basic)
            format="shape"
            input_file="${PROJECT_ROOT}/data/SHP/nanjing_buildings/南京市_百度建筑.shp"
            ;;
        fbx_basic)
            format="fbx"
            input_file="${PROJECT_ROOT}/data/FBX/TZCS_0829.FBX"
            ;;
        osgb_to_gltf)
            format="gltf"
            input_file="${PROJECT_ROOT}/data/OSGB/bench.osgb"
            ;;
        *)
            print_warning "未知测试用例: $test_name"
            return 1
            ;;
    esac
    
    if [ -z "$input_file" ] || [ ! -e "$input_file" ]; then
        print_warning "跳过: 输入文件不存在 $input_file"
        return 1
    fi
    
    # 执行转换
    print_info "  执行转换: $format -> 3D Tiles"
    if [ "$format" == "fbx" ]; then
        $EXECUTABLE -f "$format" -i "$input_file" -o "$output_dir/" --lon 118 --lat 32 --alt 20 2>&1 | tee "${output_dir}/conversion.log" || true
    elif [ "$format" == "shape" ]; then
        $EXECUTABLE -f "$format" -i "$input_file" -o "$output_dir/" --height height 2>&1 | tee "${output_dir}/conversion.log" || true
    else
        $EXECUTABLE -f "$format" -i "$input_file" -o "$output_dir/" 2>&1 | tee "${output_dir}/conversion.log" || true
    fi
    
    # 检查转换是否成功
    if [ ! -f "${output_dir}/tileset.json" ] && [ ! -f "${output_dir}/bench.glb" ]; then
        print_error "  转换失败: 未找到输出文件"
        return 1
    fi
    
    # 运行验证
    print_info "  运行回归验证..."
    local report_file="${output_dir}/validation_report.json"
    
    if python3 "$VALIDATOR" "$test_dir" "$output_dir" --mode "$mode" --report "$report_file"; then
        print_success "  ✓ 测试通过: $test_name"
        return 0
    else
        print_error "  ✗ 测试失败: $test_name"
        return 1
    fi
}

# 运行回归测试
run_tests() {
    local mode=$1
    
    print_info "运行回归测试 (模式: $mode)..."
    
    # 检查验证器
    if [ ! -f "$VALIDATOR" ]; then
        print_error "找不到验证器: $VALIDATOR"
        exit 1
    fi
    
    # 检查基准数据
    if [ ! -d "$BASELINE_DIR" ] || [ -z "$(ls -A "$BASELINE_DIR")" ]; then
        print_error "基准数据不存在，请先运行: $0 --generate"
        exit 1
    fi
    
    # 创建测试输出目录
    mkdir -p "$TEST_OUTPUT_DIR"
    
    # 统计结果
    local total=0
    local passed=0
    local failed=0
    
    # 运行所有测试用例
    for test_dir in "$BASELINE_DIR"/*; do
        if [ -d "$test_dir" ]; then
            local test_name=$(basename "$test_dir")
            total=$((total + 1))
            
            if run_single_test "$test_name" "$test_dir" "$mode"; then
                passed=$((passed + 1))
            else
                failed=$((failed + 1))
            fi
            echo ""
        fi
    done
    
    # 输出总结
    echo "=========================================="
    echo "测试总结"
    echo "=========================================="
    echo "总计: $total"
    echo -e "通过: ${GREEN}$passed${NC}"
    echo -e "失败: ${RED}$failed${NC}"
    echo ""
    
    if [ $failed -eq 0 ]; then
        print_success "所有测试通过!"
        return 0
    else
        print_error "存在失败的测试"
        return 1
    fi
}

# 清理测试输出
clean_output() {
    print_info "清理测试输出..."
    if [ -d "$TEST_OUTPUT_DIR" ]; then
        rm -rf "${TEST_OUTPUT_DIR:?}/"*
        print_success "测试输出已清理"
    fi
}

# 主函数
main() {
    local generate=false
    local test=false
    local mode="strict"
    local clean=false
    
    # 解析参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -g|--generate)
                generate=true
                shift
                ;;
            -t|--test)
                test=true
                shift
                ;;
            -a|--all)
                generate=true
                test=true
                shift
                ;;
            -m|--mode)
                mode="$2"
                shift 2
                ;;
            -c|--clean)
                clean=true
                shift
                ;;
            -v|--verbose)
                set -x
                shift
                ;;
            *)
                print_error "未知选项: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # 如果没有指定任何操作，显示帮助
    if [ "$generate" = false ] && [ "$test" = false ] && [ "$clean" = false ]; then
        show_help
        exit 0
    fi
    
    # 执行操作
    if [ "$clean" = true ]; then
        clean_output
    fi
    
    if [ "$generate" = true ]; then
        generate_baseline
    fi
    
    if [ "$test" = true ]; then
        run_tests "$mode"
    fi
}

# 运行主函数
main "$@"
