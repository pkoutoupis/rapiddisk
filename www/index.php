<?php

/* insantiate f3 object */
$f3 = require('./lib/base.php');

/* instantiate rapiddisk and set on f3 to be visable to the callback funtions */
$f3->set('rapidDisk', new \rapiddisk);

$f3->route('GET /v1/listAllRapidDiskVolumes','rapidDisk->listAllRapidDiskVolumes');
$f3->route('GET /v1/showRapidDiskCacheStatistics/@cache',
        function($f3)  {
                $f3->get('rapidDisk')->showRapidDiskCacheStatistics($f3->get('PARAMS.cache'));
        }
);

$f3->route('POST /v1/createRapidDisk',
	function($f3) {
		$inputData = file_get_contents('php://input');
		$f3->get('rapidDisk')->createRapidDisk($inputData);
	}
);

$f3->route('PUT /v1/resizeRapidDisk',
	function($f3) {
		$inputData = $f3->get('BODY');
		$f3->get('rapidDisk')->resizeRapidDisk($inputData);
	}
);

$f3->route('POST /v1/createRapidDiskCacheMapping',
        function($f3) {
                $inputData = file_get_contents('php://input');
                $f3->get('rapidDisk')->createRapidDiskCacheMapping($inputData);
        }
);

$f3->route('DELETE /v1/removeRapidDisk/@disk',
	function($f3)  {
		$f3->get('rapidDisk')->removeRapidDisk($f3->get('PARAMS.disk'));
	}
);

$f3->route('DELETE /v1/removeRapidDiskCacheMapping/@cache',
        function($f3)  {
                $f3->get('rapidDisk')->removeRapidDiskCacheMapping($f3->get('PARAMS.cache'));
        }
);

$f3->run();
?>
