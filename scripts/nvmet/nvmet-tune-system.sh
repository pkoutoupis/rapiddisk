#!/bin/bash
  
# Copyright © 2021 Petros Koutoupis
# All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-2.0-or-later

if [ ! "$BASH_VERSION" ] ; then
        exec /bin/bash "$0" "$@"
fi

SYS_NVMET="/sys/kernel/config/nvmet"
SYS_NVMET_PORTS="$SYS_NVMET/ports"
SYS_CLASS_NET="/sys/class/net"
NVME_PORT="4420"

## Usage ##
function help_menu()
{
        echo -e "$1 Tune parts of the system for NVMe Target support."
        echo -e ""
        echo -e "usage: sh $1 <function>" 
        echo -e ""
        echo -e "Functions:"
        echo -e "\t--cpu\tDisable CPU idling and adjust frequency scaling."
        echo -e "\t--net\tEnable NVMeT ports on all ETH interfaces."
        echo -e ""
        exit 0
}

function validate_ip()
{
	ip=$1
	if [[ $ip =~ ^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$ ]]; then
		OIFS=$IFS
		IFS='.'
		ip=($ip)
		IFS=$OIFS
		[[ ${ip[0]} -le 255 && ${ip[1]} -le 255 \
			&& ${ip[2]} -le 255 && ${ip[3]} -le 255 ]]
		return $?
	fi
	return 1
}

## Enable NVMeT ports on all ETH interfaces ##
function nvmet_enable_ports()
{
	port=1

	# Check that the nvmet/nvmet_tcp modules are loaded
	lsmod|grep -q nvmet
	if [ $? -ne 0 ]; then modprobe nvmet; fi
	lsmod|grep -q nvmet_tcp
	if [ $? -ne 0 ]; then modprobe nvmet_tcp; fi

	# Mount configfs (for nvmet)
	mount -t configfs none /sys/kernel/config 2>&1

	ETH=`ls -1 /sys/class/net/|grep -v "^lo"|sort -u`
	for i in $ETH; do
		echo ""
		echo "Enabling NVMeT port for interface $ETH"
		if [ ! -e $SYS_NVMET_PORTS/$port ]; then
			mkdir -pv $SYS_NVMET_PORTS/$port 2>&1
			echo "$NVME_PORT" > $SYS_NVMET_PORTS/$port/addr_trsvcid 2>&1
			echo "Set Port number to 4420"
			echo "tcp" > $SYS_NVMET_PORTS/$port/addr_trtype 2>&1
			echo "Set Port to TCP"
			echo "ipv4" > $SYS_NVMET_PORTS/$port/addr_adrfam 2>&1
			echo "Set Port to IPv4"
		fi
		#IP=`/sbin/ifconfig $i|grep 'inet addr:'|cut -d: -f2|awk '{ print $1}'`
		IP=`/usr/sbin/ip addr show dev eno1|grep 'inet '|sed 's/^ *//g'|cut -d" " -f2|sed -e 's/\/.*//g'`
		validate_ip $IP
		if [ $? -eq 0 ]; then
			ADDR=`cat $SYS_NVMET_PORTS/$port/addr_traddr`
			if [ "x$IP" != "x" ] && [ "x$ADDR" == "x" ]; then
				echo "$IP" > $SYS_NVMET_PORTS/$port/addr_traddr 2>&1
				echo "Set IP address to $IP"
			fi
		fi
		echo ""
		port=$(($port+1))
	done
	return 0
}

## Disable CPU idling and adjust frequency scaling ##
function nvmet_disable_cpu_idle()
{
	# Set scaling_min_freq to scaling_max_freq for each CPU
	for i in `ls /sys/devices/system/cpu/cpufreq/`; do
		if [ -e /sys/devices/system/cpu/cpufreq/$i/scaling_max_freq ]; then
			freq=`cat /sys/devices/system/cpu/cpufreq/$i/scaling_max_freq` && \
			echo $freq > /sys/devices/system/cpu/cpufreq/$i/scaling_min_freq 2>&1
			echo "CPU $i: Set scaling_min_freq to: $freq"
		fi
	done
	
	# Set min_per_pct for CPU pstate to 100 percent
	if [ -e /sys/devices/system/cpu/intel_pstate/min_perf_pct ]; then
		echo 100 > /sys/devices/system/cpu/intel_pstate/min_perf_pct 2>&1
		echo "CPU pstate min_perf_pct set to 100."
	fi

	# Set cpuidle for each CPU state to disable
	for i in `ls /sys/devices/system/cpu|grep ^cpu|grep -v freq|grep -v idle`; do
		if [ -d /sys/devices/system/cpu/$i/cpuidle ]; then
			for j in `ls /sys/devices/system/cpu/$i/cpuidle|grep state`; do
				echo 1 > /sys/devices/system/cpu/$i/cpuidle/$j/disable 2>&1
				echo "CPU $i State $j: Set cpuidle to disable"
			done
		fi
	done
}

#
#
#

echo -e "$0 1.0.0"
echo -e ""

[ $# -lt "1" ] && help_menu $0

arr=($@)
case "${arr[0]}" in
--cpu)
	nvmet_disable_cpu_idle
	;;
--net)
	nvmet_enable_ports
	;;
--help)
	help_menu $0
	;;
*)
	echo -ne "Option ${arr[0]} does not exist.\n\n"
	exit 1
esac

exit 0
