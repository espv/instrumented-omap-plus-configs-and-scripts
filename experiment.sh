#! /bin/bash

SESSION_ROOT=$PWD/master/experiment
SESSION_DIRECTORY_NAME=`date +%FT%H:%M:%S`
SESSION_PATH=$SESSION_ROOT/$SESSION_DIRECTORY_NAME

SEPARATOR_DELAY="3s"
INTERMISSION="10s"

KERNEL_DIR="$SESSION_ROOT/kernels"

if ! mkdir -p $SESSION_PATH; then
    echo "ERROR: Failed to create experiment session directory"
    exit -1
fi

LOG_FILE=$SESSION_PATH/output.log

BOOT_IMAGE=/home/maestro/android/boot.img
KERNEL_BUILD_HOSTNAME="maestro@toasty"

SOURCE_IP="10.46.47.30"
PHONE_MAC="a0:0b:ba:cc:d4:37"
PHONE_IP="10.46.47.45"
SINK_IP="10.46.47.60"
SINK_MAC="00:16:ea:ba:7e:e8"

# Monitoring interface
# CAP_MAC="60:e3:27:0e:e3:b8"

# Logs both to stdout and the specified log file
function log() {
    echo "`LANG=en_NO date` - $1" | tee -a $LOG_FILE
}

WIRELESS_DEVICES="wlan0 wlp3s0"
WIRELESS_DEVICE=""
for device in $WIRELESS_DEVICES; do
    if iwconfig $device > /dev/null; then
        WIRELESS_DEVICE=$device
    fi
done

if [ "$WIRELESS_DEVICE" = "" ]; then
    echo "ERROR: Failed to find wireless device"
    exit -1
fi

log "Starting experiment"
log "Found wireless device $WIRELESS_DEVICE"

# Make sure this script is running with privileges, required for pktgen and fastboot
if [ `whoami` != "root" ]; then
    echo "Please run as root"
    exit
fi

# Make sure pktgen is loaded
modprobe pktgen
has_pktgen=`lsmod | fgrep "pktgen"`
if [ "$has_pktgen" = "" ]; then
    echo "ERROR: pktgen was not loaded"
    exit
fi

# Utility function for pktgen
function pgset() {
    local result

    # log "Issuing pktgen command: $1"
    echo $1 > $PGDEV

    result=`cat $PGDEV | fgrep "Result: OK:"`
    # log "Result: $result"
    if [ "$result" = "" ]; then
         echo "ERROR: "
         cat $PGDEV | fgrep Result:
         exit
    fi
}

function pgset_nofail() {
    echo $1 > $PGDEV
}

# Utility function for pktgen
function pg() {
    echo inject > $PGDEV
    cat $PGDEV
}

#   - rate (pps)
#   - count
#   - packet size
#   - burst size (default is 1)
#   - delay (nanoseconds, default is 0)
function pktgen_setup {
    log "pktgen_setup pps=$1 count=$2 pkt_size=$3 delay=$5"

    PGDEV=/proc/net/pktgen/kpktgend_0
    pgset "rem_device_all"
    pgset "add_device $WIRELESS_DEVICE"
    pgset "max_before_softirq 10000"

    PGDEV=/proc/net/pktgen/$WIRELESS_DEVICE
    pgset "count $2"
    pgset "pkt_size $3"
    pgset "src_min $SOURCE_IP"
    pgset "src_max $SOURCE_IP"
    pgset "dst $SINK_IP"
    pgset "dst_mac $PHONE_MAC"
    pgset "burst 0"
    pgset "delay 0"
    pgset "ratep $1" # Must be set after size
    pgset "udp_src_min 10"
    pgset "udp_src_max 10"
}

experiment_counter=0

function write_header() {
    log "HEADER: exp_no=$experiment_counter runtime=$TIME kernel=$kernel rate=$pps size=$PACKET_SIZE count=$count phone_rate=$phone_rate phone_count=$phone_count phone_delay=$phone_delay"
}

function experiment_start() {
    let "experiment_counter++"
    log "START of experiment $experiment_counter"

    # TODO: Write a grep-able header into the log for generating titles
    write_header

    check_kernel

    # TODO: Write experiment parameters to a file

    # Separator packet
    echo "START" | netcat -u $SINK_IP 4123
    sleep 1s
}

# Sometimes the tracing on the phone might not stop correctly for any unknown
# reason. This function terminates adb and makes sure 
function terminate_after_timeout() {
    let t=60
    while [[ t -gt 0 ]]; do
        ps -o args | grep "^adb shell"
        match=$?
        if [ "$match" -eq 0 ]; then # 0 = still present
            let t--
            sleep 1s
        else
            log "adb has exited"
            return
        fi
    done

    log "Force-killed adb after 60s timeout"
    killall adb
    log "Restarting phone..."
    adb wait-for-device
    boot_kernel "$KERNEL_DIR/$kernel"
    wait_for_root
    check_kernel
    phone_post_boot

    sleep 5s
    # Resume the experiment
}

function experiment_end() {

    # Stop tracing, save buffer
    log "Stopping tracing on phone..."
    adb shell "su -c 'echo 1 > /proc/sepextcmd'"
    terminate_after_timeout
    wait # synchronize with any forked adb shells
    adb pull "/sdcard/trace.out" "$SESSION_PATH/exp$experiment_counter.trace"

    # Send a UDP packet which we can use in order to separate each experiment run
    log "$SEPARATOR_DELAY before emitting separator packet..."
    sleep "$SEPARATOR_DELAY"
    log "Emitting separator packet"
    echo "END" | netcat -u $SINK_IP 4567

    log "END of experiment $experiment_counter"

    # Let the devices settle before next experiment
    log "Sleeping $INTERMISSION before next experiment..."
    sleep $INTERMISSION
}

# Execute a pktgen experiment with a given set of parameters
# Parameters:
#   - rate (pps)
#   - count
#   - packet size
#   - burst size
#   - delay (unit?)
#   -
function pktgen_experiment {
    pktgen_setup $1 $2 $3 $4 $5

    # Log the pktgen settings
    T=`sudo cat /proc/net/pktgen/$WIRELESS_DEVICE`
    log "$T"

    PGDEV=/proc/net/pktgen/pgctrl
    log "Running experiment $experiment_counter..."
    pgset_nofail "start"
    log "Finished experiment $experiment_counter"

    # Log the pktgen output after the experiment
    T=`sudo cat /proc/net/pktgen/$WIRELESS_DEVICE`
    log "$T"
}

log "Setting up phone"

# TODO: Check if the phone is connected to USB

# phone_state=`adb get-state`
#
# # Wait until the phone is ready
# while [ "$phone_state" != "device" ]; do
#     echo "Connect phone or wait for it to boot..."
#     phone_state=`adb get-state`
#     sleep 1s
# done

# Check if the kernel is built on a hostname we know
function check_kernel() {
    adb shell cat /proc/version | grep "$KERNEL_BUILD_HOSTNAME" > /dev/null
    phone_kernel_match=$?
    if [ "$phone_kernel_match" -eq 1 ]; then # Wrong version
        log "ERROR: Wrong kernel, aborting"
        exit -1
    fi
    log "Kernel built by our hostname"
}

# $1 path to directory containing boot.img
function boot_kernel() {
    log "Booting $1/boot.img"

    adb reboot bootloader
    fastboot boot "$1/boot.img"

    adb wait-for-device
}

function wait_for_root() {
    log "Waiting for root access on phone..."

    # Wait for root access to be enabled
    sudo_result=`adb shell su root echo -n success`
    while [ "$sudo_result" != "success" ]; do
        sleep 1s
        sudo_result=`adb shell su root echo -n success`
    done
}

function phone_post_boot() {
    adb shell "su -c bash /data/setup.sh"
    adb shell "su -c bash /data/ipsetup.sh"
}

function source_network_config() {
    log "Stopping network manager"
    systemctl stop NetworkManager.service

    if ! ifconfig $WIRELESS_DEVICE up; then
        log "Unable to up $WIRELESS_DEVICE"
        exit -1
    fi

    iwconfig_string="`iwconfig $WIRELESS_DEVICE`"
    if ! echo "$iwconfig_string" | grep -Eq "Mode:Ad-Hoc\s+Frequency:2.462 GHz" || ! echo "$iwconfig_string" | grep -q "ESSID:\"dmms-lab-adhoc\""; then
        if ! iw $WIRELESS_DEVICE ibss join "dmms-lab-adhoc" 2462; then
            log "Unable to join ad-hoc network"
            exit -1
        fi
    fi

    if ! ifconfig $WIRELESS_DEVICE | grep -q "inet.*$SOURCE_IP"; then
        ip addr add $SOURCE_IP/24 broadcast 10.46.47.255 dev $WIRELESS_DEVICE
        if ! ifconfig $WIRELESS_DEVICE | grep -q "inet $SOURCE_IP"; then
            log "ERROR: Unable to set source IP address: $SOURCE_IP"
            exit -1
        fi
    fi
}

function test_network() {
    # Check reachability of the phone
    if ! ping -I $WIRELESS_DEVICE -c 1 -W 5 $PHONE_IP > /dev/null; then
        log "ERROR: $PHONE_IP is unreachable (ping)"
        exit -1
    fi

    # Check reachability of the destination machine
    if ! ping -I $WIRELESS_DEVICE -c 1 -W 5 $SINK_IP > /dev/null; then
        log "ERROR: $SINK_IP is unreachable (ping)"
        exit -1
    fi

    # Check the arp cache. Possibly not needed.
    if arp -i $WIRELESS_DEVICE $PHONE_IP | grep -q "no entry"; then
        log "ERROR: No ARP entry for $PHONE_IP"
        exit -1
    fi

    # if arp -i $WIRELESS_DEVICE $SINK_IP | grep -q "no entry"; then
    #     log "ERROR: No ARP entry for $SINK_IP"
    #     exit -1
    # fi
}

INFINITE=0
NO_BURST=1
NO_DELAY=0

# pktgen_params.sh on phone, placed here for reference
# pgset "count $1"
# pgset "pkt_size $2"
# pgset "dst $3"
# pgset "dst_mac $4"
# pgset "burst $5" (this is not available on the phone, pre-3.18 kernel)
# pgset "delay $6"
# pgset "ratep $7"

# The directory should contain other metadata about the kernel such as config and git hash
# RATES="1000 2000 3000 4000 5000"
# RATES="3100 3200 3300 3400 3500 3600 3700 3800 3900 4000"
RATES="2000"
DELAYS="0 50 250 500"

KERNELS="160324-1324-earlytx"

# PHONE_RATES="0 250 500 1000"
PHONE_RATES="0"
TIME="60"
PACKET_SIZE="100"

function first_time_setup() {
    # source_network_config
    test_network

    log "Setup completed. Press RETURN to start..."
    read
    log "Starting experiment soon..."
    sleep 5s
    log "STARTING"
}

let phone_pps=0
let phone_count=0

let n=0
for kernel in $KERNELS; do

    boot_kernel "$KERNEL_DIR/$kernel"
    wait_for_root
    check_kernel
    phone_post_boot

    if [ "$n" -eq 0 ]; then
        first_time_setup
    fi

    let "n++"

    for rate in $RATES; do
        for phone_rate in $PHONE_RATES; do
            for delay in $DELAYS; do

                let time=$TIME # seconds
                let pps=$rate
                let count=time*pps

                experiment_start

                if [[ $phone_rate -ne 0 ]]; then
                    let phone_pps=$phone_rate
                    let phone_count=time*phone_pps
                    # Convert from ms to ns
                    let phone_delay=1000000*delay

                    adb shell "su -c \"bash /data/pktgen_params.sh $phone_count 100 $SINK_IP $SINK_MAC 0 $phone_delay $phone_pps\"" | sed -e "s/^/PHONE[pktgen] - /" | tee -a $LOG_FILE &
                else
                    let phone_pps=0
                    let phone_count=0
                fi

		# if [[ $phone_rate -eq 0 ]] && [[ $phone_delay -ne 0 ]]; then
		# 	continue
		# fi

                adb shell "su -c /data/usnl" |  sed -e "s/^/PHONE[usnl] - /" | tee -a $LOG_FILE &

                pktgen_experiment $pps $count $PACKET_SIZE $NO_BURST $NO_DELAY

                experiment_end
            done
        done
    done
done

log "ALL DONE"

exit 0
