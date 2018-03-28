<?

function CleanupSocket($path, $socket = '')
{
	@unlink($path);

	if ($socket != '')
		@socket_close($socket);
}

$socketError = "";

function SendCommand($command)
{
	$socketDebug = 0;
	$socketError = "";
	$cpath = "/tmp/FPP." . getmypid();
	$spath = "/tmp/FPPD";

	CleanupSocket($cpath);

	$socket = socket_create(AF_UNIX, SOCK_DGRAM, 0);
	if ( !@socket_set_nonblock($socket) ) {
		$socketError = 'Unable to set nonblocking mode for ' . $spath . ' socket: ' . socket_strerror(socket_last_error());

		if ($socketDebug)
			printf("SE: '$socketError'\n");

		CleanupSocket($cpath, $socket);
		return false;
	}

	if ( !@socket_bind($socket, $cpath) ) {
		$socketError = 'socket_bind() failed for ' . $cpath . ' socket: ' . socket_strerror(socket_last_error());

		if ($socketDebug)
			printf("SE: '$socketError'\n");

		CleanupSocket($cpath, $socket);
		return false;
	}

	if ( @socket_connect($socket, $spath) === false)
	{
		$socketError = 'socket_connect() failed for ' . $spath . ' socket: ' . socket_strerror(socket_last_error());

		if ($socketDebug)
			printf("SE: '$socketError'\n");

		CleanupSocket($cpath, $socket);
		return false;
	}

	if ( @socket_send($socket, $command, strLen($command), 0) == FALSE )
	{
		$socketError = 'socket_send() failed for ' . $spath . ' socket: ' . socket_strerror(socket_last_error());

		if ($socketDebug)
			printf("SE: '$socketError'\n");

		CleanupSocket($cpath, $socket);
		return false;
	}

	$i = 0;
	$max_timeout = 1000;
	$buf = "";
	while ($i < $max_timeout)
	{
		$i++;
		$bytes_received = @socket_recv($socket, $buf, 1024, MSG_DONTWAIT);
		if ($bytes_received == -1)
		{
			$socketError = 'An error occured while receiving from the socket';

			if ($socketDebug)
				printf("SE: '$socketError'\n");

			CleanupSocket($cpath, $socket);
			return false;
		}

		if ($bytes_received > 0)
		{
			break;
		}
		usleep(500);
	}

	if ( $buf == "" )
	{
		CleanupSocket($cpath, $socket);
		return false;
	}

	CleanupSocket($cpath, $socket);
	return $buf;
}

?>
