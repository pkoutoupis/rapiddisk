#!/bin/bash

# Copyright © 2021 - 2022 Petros Koutoupis
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


## usage ##
function help_menu()
{
	echo -e "$1 Generate or set a Host NQN to the local system."
	echo -e ""
	echo -e "usage: sh $1 <function> [parameter: force]" 
	echo -e ""
	echo -e "Functions:"
	echo -e "\t--generate\tGenerate a random Host NQN but do not set it to the local system."
	echo -e "\t--get\tGet the Host NQN from the local system."
	echo -e "\t--set\tSet the randomly generated Host NQN to the local system."
        echo -e ""
        echo -e "Parameters:"
        echo -e "\t[force]\t\tOverride a pre-existing host NQN."
	echo -e ""
	exit 0
}

#
#
#

echo -e "$0 1.0.0"
echo -e ""

[ $# -lt "1" ] && help_menu $0

# Example Host NQN:
#     nqn.2021-06.org.rapiddisk:<hostname>-<randomly generate integer value>

force=0

arr=($@)
case "${arr[0]}" in
--generate)
	echo "nqn.2021-06.org.rapiddisk:`hostname`-`(echo "obase=16"; echo $((10000 + ${RANDOM} % 100000))) | bc`"
        ;;
--get)
	if [ ! -e /etc/nvme/hostnqn ]; then
		echo -e "A Host NQN has not been set"
		exit 1
	else
		cat /etc/nvme/hostnqn
	fi
        ;;
--set)
        if [ $# -gt "1" ]; then
		if [ ${arr[1]} = "force" ]; then
			force=1
		fi
	fi
	if [ -e /etc/nvme/hostnqn ] && [ ${force} -ne 1 ]; then
		echo -e "A Host NQN file already exists. If you wish to override, please set the \"force\" option."
	else
		NQN=`echo "nqn.2021-06.org.rapiddisk:\`hostname\`-\`(echo "obase=16"; echo $((10000 + ${RANDOM} % 100000))) | bc\`"`
		if [ ! -d /etc/nvme ]; then
			mkdir -p /etc/nvme
			if [ $? -ne 0 ]; then
				echo -e "Error. Unable to create the /etc/nvme directory."
				exit 1
			fi
		fi	
		echo "${NQN}" > /etc/nvme/hostnqn
		if [ $? -ne 0 ]; then
			echo "Error. Unable to set ${NQN} to /etc/nvme/hostnqn."
			exit 1
		else
			echo "${NQN} has been set to /etc/nvme/hostnqn."
		fi
	fi
        ;;
--help)
        help_menu $0
        ;;
*)
        echo -ne "Option ${arr[0]} does not exist.\n\n"
        exit 1
esac

exit 0
