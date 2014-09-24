#!/usr/bin/perl

if (!defined($ARGV[0])) {
	printf( "USAGE: strip_function.pl FUNCTION_NAME\n" );
	exit(0);
}

my $functionName = $ARGV[0];
my $inFunction = 0;

while (my $line = <STDIN>) {
	if ($line =~ /$functionName\(/) {
		$inFunction = 1;
	}

	if (!$inFunction) {
		printf( "%s", $line);
	} else {
		# this won't work if someone ends a function with an indented }
		if ($line =~ /^}\s*$/) {
			$inFunction = 0;
		}
	}
}
