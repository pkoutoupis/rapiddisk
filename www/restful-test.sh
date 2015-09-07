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
		http://$1/api/listAllRapidDiskVolumes|python -m json.tool`
	echo -ne "VOLUMES:\n$volumes"
}

#	snapshots=`echo "$pools"|grep "\-snap-"|cut -d'"' -f4`
#	for snapshot in $snapshots; do
#		echo -ne "Removing Snapshot: $snapshot..."
 #               command="curl --silent -H \"Content-Type: application/json\" -X PUT \
  #                      http://$1/api/removeLogicalVolume \
   #                     -d '{ \"logicalVolume\": \"$snapshot\" }'"
    #            returnMsg=`eval "$command"|python -m json.tool`
#		echo $returnMsg|grep -q "\"errorCode\": 0," 2>&1
#		if [ $? -ne 0 ]; then
#			echo -ne "\t${bold}FAILED${normal}.\n------\n"
#			echo "$returnMsg"
#			exit 1
#		else
#			echo -ne "\t${bold}SUCCESS${normal}.\n"
#		fi
#	done
#	volumes=`echo "$pools"|grep "\-thinvol"|grep -v "iqn"|grep -v "\-snap-"|cut -d'"' -f4`
#	for volume in $volumes; do
#		echo -ne "Removing Logical Volume: $volume..."
 #               command="curl --silent -H \"Content-Type: application/json\" -X PUT \
  #                      http://$1/api/removeLogicalVolume \
  #                      -d '{ \"logicalVolume\": \"$volume\" }'"
   #             returnMsg=`eval "$command"|python -m json.tool`
    #            echo $returnMsg|grep -q "\"errorCode\": 0," 2>&1
   #             if [ $? -ne 0 ]; then
    #                    echo -ne "\t${bold}FAILED${normal}.\n------\n"
     #                   echo "$returnMsg"
  #                      exit 1
   #             else
 #                       echo -ne "\t${bold}SUCCESS${normal}.\n"
  #              fi
#	done
#	pools=`echo "$pools"|grep "\"rpool"|grep -v "-"|cut -d'"' -f4`
#	for pool in $pools; do
 #               echo -ne "Removing RapidPool: $pool..."
  #              command="curl --silent -H \"Content-Type: application/json\" -X PUT \
 #                       http://$1/api/removeRapidPool \
  #                      -d '{ \"rapidPool\": \"$pool\" }'"
   #             returnMsg=`eval "$command"|python -m json.tool`
    #            echo $returnMsg|grep -q "\"errorCode\": 0," 2>&1
     #           if [ $? -ne 0 ]; then
  #                      echo -ne "\t${bold}FAILED${normal}.\n------\n"
   #                     echo "$returnMsg"
    #                    exit 1
     #           else
  #                      echo -ne "\t${bold}SUCCESS${normal}.\n"
   #             fi
#	done

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
		;;
	resizeRapidDisk)
		;;
	removeRapidDisk)
		;;
	createRapidCacheMapping)
		;;
	removeRapidCacheMapping)
		;;
	showRapidCacheStatistics)
		;;
	listAllRapidDiskVolumes)
		listAllRapidDiskVolumes $IP
		;;
	*)
		helpMenu
		exit 1
esac

exit $?
