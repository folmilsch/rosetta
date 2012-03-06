<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Frameset//EN"
   "http://www.w3.org/TR/html4/frameset.dtd">

<HTML>

<HEAD>
	<META content="text/html; charset=utf-8" http-equiv="content-type">
	<TITLE>
		Feature Analysis: Plot Viewer
	</TITLE>
	<LINK rel="Stylesheet" href="main.css" type="text/css" media="screen">

	<SCRIPT type="text/javascript">
// handle 'j' 'k' key navigation between plots
var current_plot=1;

// TODO: figure out how to set this on the server side

<?php
try{
$con = new PDO("sqlite:analysis_manager.db3");
$sql = "SELECT count(*) AS nplots FROM features_analysis_plots WHERE format_id = 'output_web_raster';";
$res = $con->query($sql);
foreach($res as $row){
	echo "var nplots = ", $row['nplots'], ";\n";
}
$con = NULL;
} catch (PDOException $e) {
	print 'Exception : '.$e->getMessage();
}
?>

function navigate_plots(e){
	var keycode=e.keyCode? e.keyCode : e.charCode;
	if(keycode==106 && current_plot < nplots){
		current_plot += 1;
		top.frames['main'].location.href = "plot_viewer_main_frame.php#plot_" + current_plot;
	} else if(keycode==107 && current_plot > 1){
		current_plot -= 1;
		top.frames['main'].location.href = "plot_viewer_main_frame.php#plot_" + current_plot;
	}
}
document.onkeypress=navigate_plots
	</SCRIPT>
</HEAD>

<FRAMESET cols="15%,*">
	<FRAME name="navigator" src="plot_viewer_navigator_frame.php" scrolling="auto" frameborder=0/>
	<FRAME name="main" src="plot_viewer_main_frame.php" scrolling="auto"/>

	<NOFRAMES>
		this page contains frames...
	</NOFRAMES>

</FRAMESET>

</HTML>
