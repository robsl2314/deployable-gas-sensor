<?php
$debug = false;
$logfile = "request";
$urlPurpleTiger = 'https://<URL-purpleTiger>/v1/encode/position?longitude=[lon]&latitude=[lat]';
$urlRedCat = 'https://<URL-redCat>/v1/compatibility/updateVehicleInformation/[id]';

if($debug) file_put_contents($logfile, "---------------\n", FILE_APPEND);

// retrieve data from body
$requestBody = file_get_contents('php://input');
$data = json_decode($requestBody, true);

if($debug) file_put_contents($logfile, var_export($data, true) . "\n", FILE_APPEND);

// prepare purple tiger url for spacial entity query
foreach ($data["actuators"] as $key => $actuator)
{
  $urlPurpleTiger = str_replace("[lon]", $actuator['longitude'], $urlPurpleTiger);
  $urlPurpleTiger = str_replace("[lat]", $actuator['latitude'], $urlPurpleTiger);
  foreach ($actuator as $property => $value)
   {
        if($debug) file_put_contents($logfile, $property." has the value ". $value . "\n", FILE_APPEND);
   }
}

// prepare RedCat URL for transmission
$json = file_get_contents($urlPurpleTiger);
$spacialEntityID = json_decode($json, true)["spatialEntities"][0]["_id"];
$urlRedCat = str_replace("[id]", $spacialEntityID, $urlRedCat);

if($debug) file_put_contents($logfile, var_export(json_decode($json, true), true) . "\n", FILE_APPEND);
if($debug) file_put_contents($logfile, "SpacialEntityID: " . $spacialEntityID . "\n", FILE_APPEND);
if($debug) file_put_contents($logfile, "RedCat: " . $urlRedCat . "\n", FILE_APPEND);

$ch = curl_init( $urlRedCat );
# Setup request to send json via POST.
curl_setopt( $ch, CURLOPT_POSTFIELDS, $requestBody );
curl_setopt( $ch, CURLOPT_HTTPHEADER, array('Content-Type:application/json'));
# Return response instead of printing.
curl_setopt( $ch, CURLOPT_RETURNTRANSFER, true );
# Send request.
$Cresult = curl_exec($ch);
$Cinfo = curl_getinfo($ch);
curl_close($ch);

# Send Data to WebSocket
exec("python3 py-ws-bridge.py '" . $requestBody . "'");

# Print response.
if($debug) file_put_contents($logfile, "CURL:" . $Cresult . "\n", FILE_APPEND);
if($debug) file_put_contents($logfile, "CURL:" . var_export($Cinfo, true) . "\n", FILE_APPEND);

file_put_contents($logfile, implode("\t", [
        time(),
        $requestBody,
        $spacialEntityID,
        $Cinfo["http_code"]
]). "\n", FILE_APPEND);

?>
