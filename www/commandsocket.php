<?

function CleanupSocket($path, $socket = '')
{
	@unlink($path);

	if ($socket != '')
		@socket_close($socket);

    @unlink($path);
}

$socketError = "";

function SendCommand($command)
{
	$socketDebug = 0;
	$socketError = "";
    $bytes_received = 0;
	$cpath = "/run/fppd/FPP." . getmypid() . "-" . rand(0, 9999999);
	$spath = "/run/fppd/FPPD";

	CleanupSocket($cpath);

	$socket = socket_create(AF_UNIX, SOCK_DGRAM, 0);

    socket_set_option($socket,SOL_SOCKET,SO_RCVTIMEO,array("sec"=>1,"usec"=>0));
	if ( !@socket_bind($socket, $cpath) ) {
		$socketError = 'socket_bind() failed for ' . $cpath . ' socket: ' . socket_strerror(socket_last_error());

		if ($socketDebug)
            error_log("SE: '$socketError'\n", 3, "/tmp/socketerr.txt");

		CleanupSocket($cpath, $socket);
		return false;
	}

	chmod($cpath, 01775);

	if ( @socket_connect($socket, $spath) === false)
	{
		$socketError = 'socket_connect() failed for ' . $spath . ' socket: ' . socket_strerror(socket_last_error());

		if ($socketDebug)
            error_log("SE: '$socketError'\n", 3, "/tmp/socketerr.txt");

		CleanupSocket($cpath, $socket);
		return false;
	}

    //if ($socketDebug)
    //    printf("SE: '$command'\n");
	if ( @socket_send($socket, $command, strLen($command), 0) == FALSE )
	{
		$socketError = 'socket_send() failed for ' . $spath . ' socket: ' . socket_strerror(socket_last_error());

		if ($socketDebug)
			error_log("SE: '$socketError'\n", 3, "/tmp/socketerr.txt");

		CleanupSocket($cpath, $socket);
		return false;
	}

    $buf = "";
    $bytes_received = @socket_recv($socket, $buf, 1500, 0);
    
    //error_log(sprintf("%s - %s", $command, $buf), 3, "/tmp/output.txt");
    if ($bytes_received === false) {
        $socketError = 'An error occured while receiving from the socket';
        
        if ($socketDebug)
            error_log("SE: '$socketError'\n", 3, "/tmp/socketerr.txt");

        CleanupSocket($cpath, $socket);
        return false;
    }
    if ($bytes_received === 0) {
        $socketError = 'Received 0 bytes from socket';
        
        if ($socketDebug)
            error_log("SE: '$socketError'\n", 3, "/tmp/socketerr.txt");

        CleanupSocket($cpath, $socket);
        return false;
    }

	if ( $buf == "" )
	{
        if ($socketDebug)
            error_log("SE: '$socketError'\n", 3, "/tmp/socketerr.txt");
		CleanupSocket($cpath, $socket);
		return false;
	}

	CleanupSocket($cpath, $socket);

	//if ($socketDebug)
	//	printf( "buf: '$buf'<br>  %d\n", $i);

	return $buf;
}

?>
