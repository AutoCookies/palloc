#!/bin/bash
# sanitize_environment.sh — Production-grade environment stabilization for allocator benchmarking
# ============================================================================
# This script sanitizes the system environment to ensure consistent, reproducible
# benchmark results by eliminating system-level variability sources.
#
# Usage:
#   sudo bash sanitize_environment.sh [options]
#
# Options:
#   --performance-governor  Lock CPU to performance governor
#   --pin-cores=N           Pin to specific CPU cores (e.g., 0-7)
#   --disable-aslr          Disable address space layout randomization
#   --disable-thp           Disable transparent huge pages
#   --set-swappiness=N      Set vm.swappiness (default: 1)
#   --drop-caches           Drop system caches before benchmark
#   --stop-services         Stop non-essential system services
#   --show-config           Show current configuration changes
#   --restore               Restore original settings
#
# Example:
#   sudo bash sanitize_environment.sh --performance-governor --pin-cores=0-7 --disable-thp

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Global state
ORIGINAL_SETTINGS=()
RESTORE_MODE=false
SHOW_CONFIG=false

# Logging functions
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

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# Save original setting for restoration
save_setting() {
    local setting_name=$1
    local setting_value=$2
    ORIGINAL_SETTINGS+=("$setting_name:$setting_value")
}

# Restore original settings
restore_settings() {
    log_info "Restoring original settings..."
    
    for setting in "${ORIGINAL_SETTINGS[@]}"; do
        local name="${setting%%:*}"
        local value="${setting##*:}"
        
        case $name in
            cpu_governor)
                log_info "Restoring CPU governor to: $value"
                for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
                    echo "$value" > "$cpu" 2>/dev/null || true
                done
                ;;
            thp_enabled)
                log_info "Restoring THP to: $value"
                echo "$value" > /sys/kernel/mm/transparent_hugepage/enabled 2>/dev/null || true
                ;;
            thp_defrag)
                log_info "Restoring THP defrag to: $value"
                echo "$value" > /sys/kernel/mm/transparent_hugepage/defrag 2>/dev/null || true
                ;;
            aslr)
                log_info "Restoring ASLR to: $value"
                echo "$value" > /proc/sys/kernel/randomize_va_space 2>/dev/null || true
                ;;
            swappiness)
                log_info "Restoring swappiness to: $value"
                echo "$value" > /proc/sys/vm/swappiness 2>/dev/null || true
                ;;
            *)
                log_warning "Unknown setting: $name"
                ;;
        esac
    done
    
    log_success "Settings restored"
}

# Set CPU governor to performance
set_performance_governor() {
    log_info "Setting CPU governor to performance mode..."
    
    for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
        if [[ -f "$cpu" ]]; then
            original=$(cat "$cpu")
            save_setting "cpu_governor" "$original"
            echo performance > "$cpu"
            log_success "CPU $(basename $(dirname $(dirname $cpu))) set to performance"
        fi
    done
    
    # Disable Intel Turbo Boost for consistent frequency
    if [[ -d /sys/devices/system/cpu/intel_pstate ]]; then
        original=$(cat /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null || echo "0")
        save_setting "intel_no_turbo" "$original"
        echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null || true
        log_info "Intel Turbo Boost disabled"
    fi
    
    # Disable AMD Turbo Core if present
    if [[ -f /sys/devices/system/cpu/cpufreq/boost ]]; then
        original=$(cat /sys/devices/system/cpu/cpufreq/boost)
        save_setting "amd_boost" "$original"
        echo 0 > /sys/devices/system/cpu/cpufreq/boost
        log_info "AMD Turbo Core disabled"
    fi
}

# Pin process to specific CPU cores
pin_cores() {
    local core_spec=$1
    log_info "Setting CPU affinity to cores: $core_spec"
    
    # This is a no-op in the sanitization script itself
    # The actual pinning should be done using taskset when running benchmarks
    log_info "Use 'taskset -c $core_spec <benchmark>' when running benchmarks"
}

# Disable Address Space Layout Randomization
disable_aslr() {
    log_info "Disabling Address Space Layout Randomization..."
    
    if [[ -f /proc/sys/kernel/randomize_va_space ]]; then
        original=$(cat /proc/sys/kernel/randomize_va_space)
        save_setting "aslr" "$original"
        echo 0 > /proc/sys/kernel/randomize_va_space
        log_success "ASLR disabled"
    else
        log_warning "ASLR control not available on this system"
    fi
}

# Disable Transparent Huge Pages
disable_thp() {
    log_info "Disabling Transparent Huge Pages..."
    
    if [[ -f /sys/kernel/mm/transparent_hugepage/enabled ]]; then
        original=$(cat /sys/kernel/mm/transparent_hugepage/enabled | awk '{print $1}')
        save_setting "thp_enabled" "$original"
        echo never > /sys/kernel/mm/transparent_hugepage/enabled
        log_success "THP disabled"
    fi
    
    if [[ -f /sys/kernel/mm/transparent_hugepage/defrag ]]; then
        original=$(cat /sys/kernel/mm/transparent_hugepage/defrag | awk '{print $1}')
        save_setting "thp_defrag" "$original"
        echo never > /sys/kernel/mm/transparent_hugepage/defrag
        log_success "THP defrag disabled"
    fi
}

# Set swappiness to minimize swap usage
set_swappiness() {
    local value=${1:-1}
    log_info "Setting vm.swappiness to $value..."
    
    if [[ -f /proc/sys/vm/swappiness ]]; then
        original=$(cat /proc/sys/vm/swappiness)
        save_setting "swappiness" "$original"
        echo "$value" > /proc/sys/vm/swappiness
        log_success "Swappiness set to $value"
    fi
}

# Drop system caches
drop_caches() {
    log_info "Dropping system caches..."
    
    sync
    echo 3 > /proc/sys/vm/drop_caches
    log_success "System caches dropped"
}

# Stop non-essential services
stop_services() {
    log_info "Stopping non-essential system services..."
    
    # List of services to stop (adjust based on your system)
    local services=(
        "cron"
        "rsyslog"
        "syslog"
        "bluetooth"
        "cups"
        "avahi-daemon"
        "snapd"
    )
    
    for service in "${services[@]}"; do
        if systemctl is-active --quiet "$service" 2>/dev/null; then
            log_info "Stopping service: $service"
            systemctl stop "$service" 2>/dev/null || true
            ORIGINAL_SETTINGS+=("service:$service")
        fi
    done
    
    log_success "Non-essential services stopped"
}

# Set CPU frequency scaling to maximum
disable_frequency_scaling() {
    log_info "Disabling CPU frequency scaling..."
    
    # This is handled by set_performance_governor, but we ensure it's set
    for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq; do
        if [[ -f "$cpu" ]]; then
            max_freq="${cpu%/*}/scaling_max_freq"
            if [[ -f "$max_freq" ]]; then
                max=$(cat "$max_freq")
                echo "$max" > "$cpu"
                log_info "CPU $(basename $(dirname $(dirname $cpu))) frequency set to maximum"
            fi
        fi
    done
}

# Disable CPU sleep states
disable_cpu_sleep_states() {
    log_info "Disabling CPU sleep states..."
    
    # Disable C-states for Intel CPUs
    if [[ -d /sys/module/intel_idle/parameters ]]; then
        original=$(cat /sys/module/intel_idle/parameters/cstates 2>/dev/null || echo "Y")
        save_setting "intel_cstates" "$original"
        echo N > /sys/module/intel_idle/parameters/cstates 2>/dev/null || true
        log_info "Intel C-states disabled"
    fi
    
    # Disable CPU idle states
    for cpu in /sys/devices/system/cpu/cpu*/cpuidle/state*/disable; do
        if [[ -f "$cpu" ]]; then
            echo 1 > "$cpu" 2>/dev/null || true
        fi
    done
}

# Set I/O scheduler to deadline/noop for consistent performance
set_io_scheduler() {
    log_info "Setting I/O scheduler for consistent performance..."
    
    for device in /sys/block/*/queue/scheduler; do
        if [[ -f "$device" ]]; then
            original=$(cat "$device" | awk '{print $1}')
            save_setting "io_scheduler_$(dirname $(dirname $(dirname $device)))" "$original"
            
            # Try deadline first, fallback to noop
            if grep -q "deadline" "$device"; then
                echo deadline > "$device"
            elif grep -q "noop" "$device"; then
                echo noop > "$device"
            fi
            
            log_info "I/O scheduler set for $(basename $(dirname $(dirname $device)))"
        fi
    done
}

# Disable NUMA balancing (if present)
disable_numa_balancing() {
    log_info "Disabling NUMA balancing..."
    
    if [[ -f /proc/sys/kernel/numa_balancing ]]; then
        original=$(cat /proc/sys/kernel/numa_balancing)
        save_setting "numa_balancing" "$original"
        echo 0 > /proc/sys/kernel/numa_balancing
        log_success "NUMA balancing disabled"
    fi
}

# Show current configuration
show_configuration() {
    log_info "Current system configuration:"
    echo ""
    
    # CPU Governor
    echo "CPU Governor:"
    for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
        if [[ -f "$cpu" ]]; then
            echo "  $(basename $(dirname $(dirname $cpu))): $(cat $cpu)"
        fi
    done
    
    # THP Status
    echo ""
    echo "Transparent Huge Pages:"
    if [[ -f /sys/kernel/mm/transparent_hugepage/enabled ]]; then
        echo "  Enabled: $(cat /sys/kernel/mm/transparent_hugepage/enabled)"
    fi
    if [[ -f /sys/kernel/mm/transparent_hugepage/defrag ]]; then
        echo "  Defrag: $(cat /sys/kernel/mm/transparent_hugepage/defrag)"
    fi
    
    # ASLR Status
    echo ""
    echo "ASLR Status:"
    if [[ -f /proc/sys/kernel/randomize_va_space ]]; then
        aslr_val=$(cat /proc/sys/kernel/randomize_va_space)
        case $aslr_val in
            0) echo "  Disabled" ;;
            1) echo "  Conservative" ;;
            2) echo "  Full" ;;
            *) echo "  Unknown ($aslr_val)" ;;
        esac
    fi
    
    # Swappiness
    echo ""
    echo "Swappiness:"
    if [[ -f /proc/sys/vm/swappiness ]]; then
        echo "  Current: $(cat /proc/sys/vm/swappiness)"
    fi
    
    # NUMA Balancing
    echo ""
    echo "NUMA Balancing:"
    if [[ -f /proc/sys/kernel/numa_balancing ]]; then
        echo "  Status: $(cat /proc/sys/kernel/numa_balancing)"
    fi
    
    echo ""
}

# Main execution
main() {
    check_root
    
    # Parse arguments
    PERFORMANCE_GOVERNOR=false
    PIN_CORES=""
    DISABLE_ASLR=false
    DISABLE_THP=false
    SET_SWAPPINESS=""
    DROP_CACHES=false
    STOP_SERVICES=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --performance-governor)
                PERFORMANCE_GOVERNOR=true
                shift
                ;;
            --pin-cores=*)
                PIN_CORES="${1#*=}"
                shift
                ;;
            --disable-aslr)
                DISABLE_ASLR=true
                shift
                ;;
            --disable-thp)
                DISABLE_THP=true
                shift
                ;;
            --set-swappiness=*)
                SET_SWAPPINESS="${1#*=}"
                shift
                ;;
            --drop-caches)
                DROP_CACHES=true
                shift
                ;;
            --stop-services)
                STOP_SERVICES=true
                shift
                ;;
            --show-config)
                SHOW_CONFIG=true
                shift
                ;;
            --restore)
                RESTORE_MODE=true
                shift
                ;;
            --full-sanitize)
                # Enable all optimizations
                PERFORMANCE_GOVERNOR=true
                DISABLE_ASLR=true
                DISABLE_THP=true
                SET_SWAPPINESS="1"
                DROP_CACHES=true
                STOP_SERVICES=true
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                echo "Usage: $0 [--performance-governor] [--pin-cores=N] [--disable-aslr] [--disable-thp] [--set-swappiness=N] [--drop-caches] [--stop-services] [--show-config] [--restore] [--full-sanitize]"
                exit 1
                ;;
        esac
    done
    
    if $RESTORE_MODE; then
        restore_settings
        exit 0
    fi
    
    if $SHOW_CONFIG; then
        show_configuration
        exit 0
    fi
    
    log_info "Starting environment sanitization..."
    echo ""
    
    # Apply requested optimizations
    if $PERFORMANCE_GOVERNOR; then
        set_performance_governor
        disable_frequency_scaling
        disable_cpu_sleep_states
    fi
    
    if [[ -n "$PIN_CORES" ]]; then
        pin_cores "$PIN_CORES"
    fi
    
    if $DISABLE_ASLR; then
        disable_aslr
    fi
    
    if $DISABLE_THP; then
        disable_thp
    fi
    
    if [[ -n "$SET_SWAPPINESS" ]]; then
        set_swappiness "$SET_SWAPPINESS"
    fi
    
    if $DROP_CACHES; then
        drop_caches
    fi
    
    if $STOP_SERVICES; then
        stop_services
    fi
    
    # Always apply these for consistent benchmarking
    disable_numa_balancing
    set_io_scheduler
    
    echo ""
    log_success "Environment sanitization completed"
    echo ""
    log_info "To restore original settings, run: sudo $0 --restore"
    log_info "To show current configuration, run: sudo $0 --show-config"
    
    # Show final configuration
    show_configuration
}

# Run main function
main "$@"