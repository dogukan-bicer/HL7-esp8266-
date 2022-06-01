<?php
$servername = "localhost";
$username = "root";
$password = "";
try {
    $conn = new PDO("mysql:host=$servername;dbname=hl7_server", $username, $password);
    $conn->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    echo "Baglantı basarılı"; 
    }
catch(PDOException $e)
    {
    echo "Baglanmadi: " . $e->getMessage();
    }
?>
