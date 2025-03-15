#!/bin/bash
  
# Copyright © 2021 - 2025 Petros Koutoupis
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

## Usage ##
function help_menu()
{
        echo -e "$1 Tune parts of the system for NVMe Target support."
        echo -e ""
        echo -e "usage: sh $1 <function>" 
        echo -e ""
        echo -e "Functions:"
        echo -e "\t--cpu\tDisable CPU idling and adjust frequency scaling."
        echo -e "\t--mod\tLoad NVMeT modules for tcp and rdma."
        echo -e ""
        echo -e "Examples:"
        echo -e "\tsh $1 --cpu"
        echo -e "\tsh $1 --mod"
        exit 0
}

## Enable NVMeT load kernel modules ##
function nvmet_load_modules()
{
	# Check that the nvmet/nvmet_tcp modules are loaded
	lsmod|grep -q nvmet
	if [ $? -ne 0 ]; then modprobe nvmet; fi
	lsmod|grep -q nvmet_tcp
	if [ $? -ne 0 ]; then modprobe nvmet_tcp; fi
	lsmod|grep -q nvmet_rdmad
	if [ $? -ne 0 ]; then modprobe nvmet_rdma; fi

	# Mount configfs (for nvmet)
	mount -t configfs none /sys/kernel/config 2>&1

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
--mod)
	nvmet_load_modules
	;;
--help)
	help_menu $0
	;;
*)
	echo -ne "Option ${arr[0]} does not exist.\n\n"
	exit 1
esac

exit 0
