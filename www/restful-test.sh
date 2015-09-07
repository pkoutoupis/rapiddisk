#!/bin/bash

if [ ! "$BASH_VERSION" ] ; then
        exec /bin/bash "$0" "$@"
fi

bold=`tput bold`
normal=`tput sgr0`

function helpMenu()
{
	echo -ne "\n${bold}Usage:${normal} sh restful-test.sh [command] <ip address>\n\n"
	echo -ne "${bold}Commands:${normal}\n"
	echo -ne "\tlistAllRapidDiskVolumes\tList all RapidDisk and RapidCache volumes.\n"
	echo -ne "\n\n"	
}

function validateip()
{
	ip=$1
	stat=1

	if [[ $ip =~ ^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$ ]]; then
		OIFS=$IFS
		IFS='.'
		ip=($ip)
		IFS=$OIFS
		[[ ${ip[0]} -le 255 && ${ip[1]} -le 255 \
			&& ${ip[2]} -le 255 && ${ip[3]} -le 255 ]]
		stat=$?
	fi
	return $stat
}

function listAllRapidDiskVolumes()
{
	volumes=`curl --silent -H "Content-Type: application/json" -X GET \
		http://$1/v1/listAllRapidDiskVolumes|python -m json.tool`
	echo -ne "VOLUMES:\n$volumes"
}

function createRapidDisk()
{
	command="curl --silent -H \"Content-Type: application/json\" -X POST \
		http://$1/v1/createRapidDisk -d '{ \"size\": $2 }'"
	eval "$command"|python -m json.tool
}

function resizeRapidDisk()
{
	command="curl --silent -H \"Content-Type: application/json\" -X PUT \
		http://$1/v1/resizeRapidDisk -d '{ \"rapidDisk\": \"$2\", \"size\": $3 }'"
	eval "$command"|python -m json.tool
}

function removeRapidDisk()
{
	curl --silent -H "Content-Type: application/json" -X DELETE \
		http://$1/v1/removeRapidDisk/$2|python -m json.tool
}

function createRapidCacheMapping()
{
	command="curl --silent -H \"Content-Type: application/json\" -X POST \
		http://$1/v1/createRapidCacheMapping \
		-d '{ \"rapidDisk\": \"$2\", \"sourceDrive\": \"$3\" }'"
	eval "$command"|python -m json.tool
}

function removeRapidCacheMapping()
{
        curl --silent -H "Content-Type: application/json" -X DELETE \
                http://$1/v1/removeRapidDisk/$2|python -m json.tool
}

function showRapidCacheStatistics()
{
        volumes=`curl --silent -H "Content-Type: application/json" -X GET \
                http://$1/v1/showRapidCacheStatistics/$2|python -m json.tool`
        echo -ne "STATISTICS:\n$volumes"
}



#
#
#

if [ $# -lt 2 ]; then
	helpMenu
	exit 1
fi

validateip $2
if [ $? -eq 1 ]; then
	echo "Invalid Virtual IP address format defined."
	exit 1
else
	IP=$2
fi

case "$1" in
	createRapidDisk)
		createRapidDisk $IP $3			# arg 3: size in MBytes #
		;;
	resizeRapidDisk)
		resizeRapidDisk $IP $3 $4		# arg 3: rapiddisk, arg 4: size in MBytes #
		;;
	removeRapidDisk)
		removeRapidDisk $IP $3			# arg 3: rapiddisk
		;;
	createRapidCacheMapping)
		createRapidCacheMapping $IP $3 $4	# arg 3: rapiddisk, arg 4: source volume #
		;;
	removeRapidCacheMapping)
		removeRapidCacheMapping $IP $3		# arg 3: rapidcache
		;;
	showRapidCacheStatistics)
		showRapidCacheStatistics $IP $3		# arg 3: rapidcache
		;;
	listAllRapidDiskVolumes)
		listAllRapidDiskVolumes $IP
		;;
	*)
		helpMenu
		exit 1
esac

exit $?
