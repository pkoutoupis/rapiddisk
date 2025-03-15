#!/bin/bash
  
if [ ! "$BASH_VERSION" ] ; then
        exec /bin/bash "$0" "$@"
fi


## usage ##
function help_menu()
{
	echo -e "$1 9.2.0"
	echo -e "Copyright 2011 - 2025 Petros Koutoupis"
	echo -e ""
	echo -e "$1 is an administration tool to manage the RapidDisk RAM disk devices and"
	echo -e "\tRapidDisk-Cache mappings."
	echo -e ""
	echo -e "usage: sh $1 <function> [ parameters:  unit | size | start & end | src & dest |"
	echo -e "\tcache & source | mode ]"
	echo -e ""
	echo -e "Functions:"
	echo -e "\t--attach\tAttach RAM disk device."
	echo -e "\t--detach\tDetach RAM disk device."
	echo -e "\t--list\t\tList all attached RAM disk devices."
	echo -e "\t--list-json\tList all attached RAM disk devices in JSON format."
	echo -e "\t--flush\t\tErase all data to a specified RapidDisk device (dangerous)."
	echo -e "\t--resize\tDynamically grow the size of an existing RapidDisk device."
	echo -e "\t--cache-map\tMap an RapidDisk device as a caching node to another block device."
	echo -e "\t--cache-unmap\tRemove cache map of a RapidDisk device from another block device."
	echo -e "\t--stat-cache\tObtain RapidDisk-Cache Mappings statistics."
	echo -e ""
	echo -e "Parameters:"
	echo -e "\t[size]\t\tSpecify desired size of attaching RAM disk device in MBytes."
	echo -e "\t[unit]\t\tSpecify unit number of RAM disk device to detach."
	echo -e "\t[cache]\t\tSpecify RapidDisk node to use as caching volume."
	echo -e "\t[source]\tSpecify block device to map cache to."
	echo -e "\t[mode]\t\tWrite Through (wt) or Write Around (wa) for cache."
	echo -e ""
	echo -e "Example Usage:"
	echo -e "\t$1 --attach 64"
	echo -e "\t$1 --detach rd2"
	echo -e "\t$1 --resize rd2 128"
	echo -e "\t$1 --cache-map rd1 /dev/sdb"
	echo -e "\t$1 --cache-map rd1 /dev/sdb wt"
	echo -e "\t$1 --cache-unmap rc_sdb"
	echo -e "\t$1 --flush rd2"
	echo -e ""
	exit 0
}

#
#
#

[ $# -lt "1" ] && help_menu $0

arr=($@)
case "${arr[0]}" in
--attach)
	[ $# -lt "2" ] && help_menu $0
	rapiddisk -a ${arr[1]}
	;;
--detach)
	[ $# -lt "2" ] && help_menu $0
	rapiddisk -d ${arr[1]}
	;;
--list)
	rapiddisk -l
	;;
--list-json)
	rapiddisk -l -j
	;;
--flush)
	[ $# -lt "2" ] && help_menu $0
	rapiddisk -f ${arr[1]}
	;;
--resize)
	[ $# -lt "3" ] && help_menu $0
	rapiddisk -r ${arr[1]} -c ${arr[2]}
	;;
--cache-map)
	MODE="wt"
	[ $# -lt "3" ] && help_menu $0
	[ $# -eq "4" ] && MODE="${arr[3]}"
	rapiddisk -m ${arr[1]} -b ${arr[2]} -p ${MODE}
	;;
--cache-unmap)
	[ $# -lt "2" ] && help_menu $0
	rapiddisk -u ${arr[1]}
	;;
--stat-cache)
	[ $# -lt "2" ] && help_menu $0
	rapiddisk -s ${arr[1]}
	;;
--help)
	help_menu $0
	;;
*)
	echo -ne "Option ${arr[0]} does not exist.\n\n"
	exit 1
esac

exit $?
