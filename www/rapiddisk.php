<?php

class RapidDisk {
	public $RDISK_CMD;
	
	function __construct() {
		 $this->RDISK_CMD = 'sudo -u root /sbin/rapiddisk';
	}

	/* @GET: List all RapidDisk and RapidCache volumes with sizes (in MBs) and mappings. */
	public function listAllRapidDiskVolumes() {
                $command = implode(' ', array($this->RDISK_CMD, '--short-list'));
                exec($command, $output, $retval);
		$list = $dsk = $cache = $tmp = [];
		foreach ($output as &$volume) {
			$var = explode(':', $volume);
			if (strstr($var[0], 'rxd') != False) {
				$tmp = array('rapidDisk' => $var[0], 'size' => intval($var[1]));
				array_push($list, $tmp);
			} elseif (strstr($var[0], 'rxc') != False) {
				$tmp = array('rapidCache' => $var[0], 'cache' => explode(',', $var[1])[0], 'source' => explode(',', $var[1])[1]);
				array_push($list, $tmp);
			}
			unset($tmp);
		}
                $rtn = array('volumes' => $list);
		http_response_code(200); /* OK */
		print json_encode($rtn);
	}

        /* @GET: Show cache statistics of an existing RapidCache Volume. */
        public function showRapidCacheStatistics($cache) {
                $command = implode(' ', array($this->RDISK_CMD, '--stat-cache', $cache, '|grep "("|tr -s \'[[:space:]]\' \' \'|sed -e \'s/,//\'g -e \'s/^ //\' -e \'s/$/\n/\''));
                exec($command, $output, $retval);
                $rtn = array('errorCode' => $retval, 'message' => implode(' ', $output));
                http_response_code(200); /* OK */
                print json_encode($rtn);
        }

	/* @POST: Create a (volatile) RapidDisk Volume (size in MBs). */
	public function createRapidDisk($JSONRequest) {
                $nameObject = json_decode($JSONRequest, true);
		$size = $nameObject['size'];
                $command = implode(' ', array($this->RDISK_CMD, '--attach', $size));
                exec($command, $output, $retval);
                $rtn = array('errorCode' => $retval, 'message' => implode(' ', $output));
		http_response_code(200); /* OK */
		print json_encode($rtn);
	}

        /* @PUT: Resize an existing (volatile) RapidDisk Volume (size in MBs). */
        public function resizeRapidDisk($JSONRequest) {
                $nameObject = json_decode($JSONRequest, true);
                $name = $nameObject['rapidDisk'];
                $size = $nameObject['size'];
                $command = implode(' ', array($this->RDISK_CMD, '--resize', $name, $size));
                exec($command, $output, $retval);
                $rtn = array('errorCode' => $retval, 'message' => implode(' ', $output));
                http_response_code(200); /* OK */
                print json_encode($rtn);
        }

	/* @DELETE: Delete an existing RapidDisk Volume. */
	public function removeRapidDisk($disk) {
                $command = implode(' ', array($this->RDISK_CMD, '--detach', $disk));
                exec($command, $output, $retval);
                $rtn = array('errorCode' => $retval, 'message' => implode(' ', $output));
                http_response_code(200); /* OK */
                print json_encode($rtn);
	}

        /* @POST: Create a RapidCache Mapping. */
        public function createRapidCacheMapping($JSONRequest) {
                $nameObject = json_decode($JSONRequest, true);
                $rdsk = $nameObject['rapidDisk'];
                $source = $nameObject['sourceDrive'];
                $command = implode(' ', array($this->RDISK_CMD, '--rxc-map', $rdsk, $source, "8"));
                exec($command, $output, $retval);
                $rtn = array('errorCode' => $retval, 'message' => implode(' ', $output));
                http_response_code(200); /* OK */
                print json_encode($rtn);
        }

        /* @DELETE: Remove an existing RapidCache Mapping. */
        public function removeRapidCacheMapping($disk) {
		$command = implode(' ', array($this->RDISK_CMD, '--rxc-unmap', $disk));
		exec($command, $output, $retval);
                $rtn = array('errorCode' => $retval, 'message' => implode(' ', $output));
                http_response_code(200); /* OK */
                print json_encode($rtn);
        }
}
?>
