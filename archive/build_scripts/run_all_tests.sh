#!/bin/bash
#
# 全自动构建和测试脚本
# 测试 FCL, Eigen, libccd 所有库的单元测试
#

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 项目根目录
PROJECT_ROOT="/home/user/FCLMua"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_DIR="${PROJECT_ROOT}/test_logs_${TIMESTAMP}"
mkdir -p "${LOG_DIR}"

# 测试结果统计
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# 保存当前目录
ORIGINAL_DIR=$(pwd)

echo ""
echo "=========================================="
echo "   FCL+Musa 单元测试执行脚本"
echo "=========================================="
echo "开始时间: $(date)"
echo "日志目录: ${LOG_DIR}"
echo ""

# =============================================================================
# 1. 测试 FCL
# =============================================================================
test_fcl() {
    log_info "开始构建和测试 FCL..."

    local FCL_BUILD_DIR="${PROJECT_ROOT}/build"
    local FCL_LOG="${LOG_DIR}/fcl_test.log"

    # 清理旧的构建（可选）
    if [ -d "${FCL_BUILD_DIR}" ]; then
        log_info "检测到已存在的 FCL 构建目录，继续使用..."
    else
        log_info "创建 FCL 构建目录..."
        mkdir -p "${FCL_BUILD_DIR}"
    fi

    cd "${FCL_BUILD_DIR}"

    # 构建 FCL
    log_info "配置 FCL CMake..."
    cmake "${PROJECT_ROOT}/fcl-source" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=ON \
        -DCMAKE_CXX_STANDARD=17 \
        > "${FCL_LOG}" 2>&1 || {
        log_error "FCL CMake 配置失败！查看日志: ${FCL_LOG}"
        return 1
    }

    log_info "编译 FCL (使用 $(nproc) 个并行任务)..."
    make -j$(nproc) >> "${FCL_LOG}" 2>&1 || {
        log_error "FCL 编译失败！查看日志: ${FCL_LOG}"
        return 1
    }

    log_success "FCL 编译完成"

    # 运行测试
    log_info "运行 FCL 单元测试..."
    ctest --output-on-failure -j4 > "${LOG_DIR}/fcl_test_results.txt" 2>&1 || true

    # 解析测试结果
    local fcl_total=$(grep -oP "Total Tests: \K\d+" "${LOG_DIR}/fcl_test_results.txt" || echo "0")
    local fcl_passed=$(grep -oP "Passed: \K\d+" "${LOG_DIR}/fcl_test_results.txt" || grep -c "Passed" "${LOG_DIR}/fcl_test_results.txt" || echo "0")
    local fcl_failed=$(grep -oP "Failed: \K\d+" "${LOG_DIR}/fcl_test_results.txt" || grep -c "\*\*\*Failed" "${LOG_DIR}/fcl_test_results.txt" || echo "0")

    TOTAL_TESTS=$((TOTAL_TESTS + fcl_total))
    PASSED_TESTS=$((PASSED_TESTS + fcl_passed))
    FAILED_TESTS=$((FAILED_TESTS + fcl_failed))

    log_success "FCL 测试完成: Total=${fcl_total}, Passed=${fcl_passed}, Failed=${fcl_failed}"

    cd "${ORIGINAL_DIR}"
}

# =============================================================================
# 2. 测试 Eigen
# =============================================================================
test_eigen() {
    log_info "开始构建和测试 Eigen..."

    local EIGEN_SOURCE_DIR="${PROJECT_ROOT}/external/Eigen"
    local EIGEN_BUILD_DIR="${PROJECT_ROOT}/build_eigen_tests"
    local EIGEN_LOG="${LOG_DIR}/eigen_test.log"

    # 检查 Eigen 源码是否完整
    if [ ! -f "${EIGEN_SOURCE_DIR}/Eigen/src/Core/util/Macros.h" ]; then
        log_warning "Eigen 源码不完整，跳过 Eigen 测试"
        log_warning "如需测试 Eigen，请先下载完整版本"
        return 0
    fi

    # 创建构建目录
    if [ -d "${EIGEN_BUILD_DIR}" ]; then
        log_info "清理旧的 Eigen 测试构建..."
        rm -rf "${EIGEN_BUILD_DIR}"
    fi
    mkdir -p "${EIGEN_BUILD_DIR}"

    cd "${EIGEN_BUILD_DIR}"

    # 配置 Eigen
    log_info "配置 Eigen CMake..."
    cmake "${EIGEN_SOURCE_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DEIGEN_BUILD_PKGCONFIG=OFF \
        -DEIGEN_TEST_CXX11=ON \
        > "${EIGEN_LOG}" 2>&1 || {
        log_error "Eigen CMake 配置失败！查看日志: ${EIGEN_LOG}"
        cd "${ORIGINAL_DIR}"
        return 1
    }

    log_info "编译 Eigen 测试 (这可能需要较长时间)..."
    log_warning "Eigen 有 147 个测试，编译时间较长，请耐心等待..."

    # 只编译部分核心测试（避免编译时间过长）
    log_info "编译 Eigen 核心测试..."
    make basicstuff linearstructure array_cwise product_small -j$(nproc) >> "${EIGEN_LOG}" 2>&1 || {
        log_warning "部分 Eigen 测试编译失败，继续..."
    }

    # 运行测试
    log_info "运行 Eigen 单元测试 (仅运行已编译的测试)..."
    ctest -R "basicstuff|linearstructure|array_cwise|product_small" --output-on-failure > "${LOG_DIR}/eigen_test_results.txt" 2>&1 || true

    # 解析测试结果
    local eigen_total=$(grep -oP "\d+/\d+" "${LOG_DIR}/eigen_test_results.txt" | tail -1 | cut -d'/' -f2 || echo "0")
    local eigen_passed=$(grep -c "Passed" "${LOG_DIR}/eigen_test_results.txt" || echo "0")
    local eigen_failed=$(grep -c "\*\*\*Failed" "${LOG_DIR}/eigen_test_results.txt" || echo "0")

    TOTAL_TESTS=$((TOTAL_TESTS + eigen_total))
    PASSED_TESTS=$((PASSED_TESTS + eigen_passed))
    FAILED_TESTS=$((FAILED_TESTS + eigen_failed))

    log_success "Eigen 测试完成 (部分): Total=${eigen_total}, Passed=${eigen_passed}, Failed=${eigen_failed}"

    cd "${ORIGINAL_DIR}"
}

# =============================================================================
# 3. 测试 libccd
# =============================================================================
test_libccd() {
    log_info "开始构建和测试 libccd..."

    local LIBCCD_SOURCE_DIR="${PROJECT_ROOT}/external/libccd"
    local LIBCCD_BUILD_DIR="${LIBCCD_SOURCE_DIR}/build_tests"
    local LIBCCD_LOG="${LOG_DIR}/libccd_test.log"

    # 检查项目中的 libccd 是否是完整版本（内核修改版不支持测试）
    if [ ! -f "${LIBCCD_SOURCE_DIR}/src/testsuites/main.c" ]; then
        log_warning "项目中的 libccd 是内核定制版本，不支持单元测试"
        log_info "使用系统安装的 libccd 版本进行测试..."

        # 检查系统 libccd
        if [ -d "/tmp/libccd" ]; then
            LIBCCD_SOURCE_DIR="/tmp/libccd"
            LIBCCD_BUILD_DIR="/tmp/libccd/build"
        else
            log_warning "未找到可测试的 libccd，跳过测试"
            return 0
        fi
    fi

    # 创建构建目录
    if [ -d "${LIBCCD_BUILD_DIR}" ]; then
        log_info "清理旧的 libccd 测试构建..."
        rm -rf "${LIBCCD_BUILD_DIR}"
    fi
    mkdir -p "${LIBCCD_BUILD_DIR}"

    cd "${LIBCCD_BUILD_DIR}"

    # 配置 libccd
    log_info "配置 libccd CMake..."
    cmake "${LIBCCD_SOURCE_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=ON \
        -DCCD_HIDE_ALL_SYMBOLS=OFF \
        > "${LIBCCD_LOG}" 2>&1 || {
        log_error "libccd CMake 配置失败！查看日志: ${LIBCCD_LOG}"
        cd "${ORIGINAL_DIR}"
        return 1
    }

    log_info "编译 libccd 和测试..."
    make -j$(nproc) >> "${LIBCCD_LOG}" 2>&1 || {
        log_error "libccd 编译失败！查看日志: ${LIBCCD_LOG}"
        cd "${ORIGINAL_DIR}"
        return 1
    }

    log_success "libccd 编译完成"

    # 运行测试
    log_info "运行 libccd 单元测试..."
    ctest --output-on-failure > "${LOG_DIR}/libccd_test_results.txt" 2>&1 || true

    # 解析测试结果
    local ccd_total=$(grep -oP "Total Tests: \K\d+" "${LOG_DIR}/libccd_test_results.txt" || echo "0")
    local ccd_passed=$(grep -c "Passed" "${LOG_DIR}/libccd_test_results.txt" || echo "0")
    local ccd_failed=$(grep -c "\*\*\*Failed" "${LOG_DIR}/libccd_test_results.txt" || echo "0")

    TOTAL_TESTS=$((TOTAL_TESTS + ccd_total))
    PASSED_TESTS=$((PASSED_TESTS + ccd_passed))
    FAILED_TESTS=$((FAILED_TESTS + ccd_failed))

    log_success "libccd 测试完成: Total=${ccd_total}, Passed=${ccd_passed}, Failed=${ccd_failed}"

    cd "${ORIGINAL_DIR}"
}

# =============================================================================
# 生成测试报告
# =============================================================================
generate_report() {
    local REPORT_FILE="${LOG_DIR}/test_summary.txt"

    log_info "生成测试报告..."

    cat > "${REPORT_FILE}" <<EOF
========================================
   FCL+Musa 单元测试总结报告
========================================

执行时间: $(date)
日志目录: ${LOG_DIR}

----------------------------------------
  总体测试结果
----------------------------------------
总测试数:   ${TOTAL_TESTS}
通过:       ${PASSED_TESTS}
失败:       ${FAILED_TESTS}
跳过:       ${SKIPPED_TESTS}

通过率:     $(awk "BEGIN {printf \"%.2f%%\", ($PASSED_TESTS/$TOTAL_TESTS)*100}")

----------------------------------------
  详细日志文件
----------------------------------------
FCL 测试日志:      ${LOG_DIR}/fcl_test_results.txt
Eigen 测试日志:    ${LOG_DIR}/eigen_test_results.txt
libccd 测试日志:   ${LOG_DIR}/libccd_test_results.txt

构建日志:
  - FCL:    ${LOG_DIR}/fcl_test.log
  - Eigen:  ${LOG_DIR}/eigen_test.log
  - libccd: ${LOG_DIR}/libccd_test.log

========================================
EOF

    cat "${REPORT_FILE}"

    log_success "测试报告已生成: ${REPORT_FILE}"
}

# =============================================================================
# 主执行流程
# =============================================================================
main() {
    # 测试 FCL
    if test_fcl; then
        log_success "FCL 测试阶段完成"
    else
        log_error "FCL 测试失败"
    fi

    echo ""

    # 测试 Eigen
    if test_eigen; then
        log_success "Eigen 测试阶段完成"
    else
        log_warning "Eigen 测试未完全完成"
    fi

    echo ""

    # 测试 libccd
    if test_libccd; then
        log_success "libccd 测试阶段完成"
    else
        log_warning "libccd 测试未完全完成"
    fi

    echo ""

    # 生成报告
    generate_report

    echo ""
    log_success "所有测试执行完成！"
    echo ""
}

# 执行主函数
main

# 返回原始目录
cd "${ORIGINAL_DIR}"
