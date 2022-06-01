<?php
include('connect.php');
$etco2= $_GET['etco2'];
$fico2= $_GET['fico2'];
$etco2rr= $_GET['etco2rr'];
$sql = "INSERT INTO `hl7data`(`etco2`, `fico2`, `etco2rr`) VALUES ($etco2,$fico2,$etco2rr)";
$conn->query($sql);
?>