<?php
set_time_limit( 0);
ob_implicit_flush( 1);
$prefix = ''; if ( is_dir( "ajaxkit")) $prefix = 'ajaxkit/'; for ( $i = 0; $i < 3; $i++) { if ( ! is_dir( $prefix . 'lib')) $prefix .= '../'; else break; }
if ( ! is_file( $prefix . "env.php")) $prefix = '/web/ajaxkit/'; // hoping for another location of ajaxkit
if ( ! is_file( $prefix . "env.php")) die( "\nERROR! Cannot find env.php in [$prefix], check your environment! (maybe you need to go to ajaxkit first?)\n\n");
// global functions and env
require_once( $prefix . 'functions.php');
require_once(	 $prefix . 'env.php'); //echo "env[" . htt( $env) . "]\n";
// additional (local) functions and env (if present)
if ( is_file( "$BDIR/functions.php")) require_once( "$BDIR/functions.php");
if ( is_file( "$BDIR/env.php")) require_once( "$BDIR/env.php");
htg( hm( $_GET, $_POST));  

if ( $action == 'info') { // lastime
	if ( ! isset( $lastime)) $lastime = tsystem() - 10;
	$H = array();
	$H[ 'localtime'] = tsystem();
	$H[ 'data'] = array();
	foreach ( flget( '.', 'traffic') as $file) {
		$L = ttl( $file, '.'); if ( count( $L) != 3) { `rm -Rf $file`; continue; }
		lpop( $L); $time = round( lpop( $L));
		if ( $time < $lastime) { `rm -Rf $file`; continue; }
		$lines = file( $file); 
		$v = ltt( $lines, ' ');
		$v = str_replace( "\n", '', $v);
		$H[ 'data'][ "$time"] = $v;
		`rm -Rf $file`;
	}
	$JO = $H;
	die( jsonsend( $JO));
}


?>