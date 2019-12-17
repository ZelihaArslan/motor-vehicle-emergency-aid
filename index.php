<?php

if ($_SERVER['REQUEST_METHOD'] == "GET") {
    echo "Yetkiniz bulunmuyor.";
    die();
}

require 'vendor/autoload.php';

use Kreait\Firebase\Configuration;
use Kreait\Firebase\Firebase;

$config = new Configuration();
$config->setAuthConfigFile(__DIR__ . 'google-service-account-kisi-takip.json');

$firebase = new Firebase('https://kisitakip-101e5.firebaseio.com/', $config);

$reference = $firebase->getReference('Kullanicilar/kpXp1EWpsUXqklYD5J90cL0fANf2/Arkadaslar');
$data = $reference->getData();

foreach ($data as $row){
    echo $row["telefon"];
}
?>
