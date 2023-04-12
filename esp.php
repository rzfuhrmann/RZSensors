<?php
    $time = time(); 
    include '/var/www/home/ESPRoomSensors.php';

    function senddataToSensorCommunity($devicename, $sensor, $values, $version){
        $sensor2pin = [
            "SDS011" => 1,
            "BME280" => 11
        ];
        $devicename = explode("-", $devicename); 
        if (sizeof($devicename) == 3){
            $deviceid = "esp32-".hexdec($devicename[2]);

            $data = array(
                "software_version" => $version,
                "sensordatavalues" => $values
            );

            $ch = curl_init("https://api.sensor.community/v1/push-sensor-data/");
            $payload = json_encode($data);
            curl_setopt( $ch, CURLOPT_POSTFIELDS, $payload );
            curl_setopt( $ch, CURLOPT_HTTPHEADER, array(
                'Content-Type: application/json',
                'X-Pin: '.$sensor2pin[$sensor],
                'X-Sensor: '.$deviceid
            ));
            curl_setopt( $ch, CURLOPT_RETURNTRANSFER, true );

            curl_setopt( $ch, CURLOPT_PROXY, "192.168.1.166:8888");
            curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);

            # Send request.
            $result = curl_exec($ch);
            curl_close($ch);
        }
    }

    if ($_SERVER['REQUEST_METHOD'] == "POST"){
        $body = file_get_contents('php://input');
        file_put_contents(__DIR__.'/esp_postbody.json', $body);

        $body = json_decode($body, true); 
        if ($body){
            

            require_once __DIR__ . '/vendor/autoload.php';
            $client = new InfluxDB\Client("127.0.0.1", 8086);
            $influx_dbs = ["esp_test", "esp_new"];

            $points = array();
            $tags = array("device" => $body["device"]); 
            file_put_contents(__DIR__.'/esp_postbody_'.$body["device"].'.json', json_encode($body, JSON_PRETTY_PRINT));

            if (isset($_CONFIG["RZ"]["ESPSensors"]["device2sensor"][$body["device"]])){
                foreach ($_CONFIG["RZ"]["ESPSensors"]["device2sensor"][$body["device"]] as $k => $v){
                    $tags[$k] = $v; 
                }
            }

            if (isset($body["wifi"]["bssid"])) $tags["wifi_bssid"] = $body["wifi"]["bssid"];
            if (isset($body["wifi"]["gateway"])) $tags["wifi_gateway"] = $body["wifi"]["gateway"];
            if (isset($body["wifi"]["ip"])) $tags["wifi_ip"] = $body["wifi"]["ip"];
            if (isset($body["sketch"]["cs"])) $tags["sketch_checksum"] = $body["sketch"]["cs"];
            if (isset($body["sketch"]["v"])) $tags["sketch_version"] = $body["sketch"]["v"];

            $points[] = new InfluxDB\Point(
                'esp_requests', // name of the measurement
                1,
                $tags,
                [], // optional additional fields
                $time // Time precision has to be set to seconds!
            );

            // uptime
            $points[] = new InfluxDB\Point(
                'esp_uptime', // name of the measurement
                (double)$body["t"],
                $tags,
                [], // optional additional fields
                $time // Time precision has to be set to seconds!
            );

            if (isset($body["wifi"]["rssi"])){
                $points[] = new InfluxDB\Point(
                    'esp_wifi_rssi', // name of the measurement
                    (double)$body["wifi"]["rssi"],
                    $tags,
                    [], // optional additional fields
                    $time // Time precision has to be set to seconds!
                );
            }

            if (isset($body["wifi"]["tries"])){
                $points[] = new InfluxDB\Point(
                    'esp_wifi_tries', // name of the measurement
                    (double)$body["wifi"]["tries"],
                    $tags,
                    [], // optional additional fields
                    $time // Time precision has to be set to seconds!
                );
            }

            if (isset($body["sleep"]["bc"])){
                $points[] = new InfluxDB\Point(
                    'esp_sleep_bootcount', // name of the measurement
                    (double)$body["sleep"]["bc"],
                    $tags,
                    [], // optional additional fields
                    $time // Time precision has to be set to seconds!
                );
            }

            if (isset($body["sleep"]["wifi_failed_before"])){
                $points[] = new InfluxDB\Point(
                    'esp_wifi_failed_before', // name of the measurement
                    (double)$body["sleep"]["wifi_failed_before"],
                    $tags,
                    [], // optional additional fields
                    $time // Time precision has to be set to seconds!
                );
            }

            if (isset($body["sleep"]["http_failed_before"])){
                $points[] = new InfluxDB\Point(
                    'esp_http_failed_before', // name of the measurement
                    (double)$body["sleep"]["http_failed_before"],
                    $tags,
                    [], // optional additional fields
                    $time // Time precision has to be set to seconds!
                );
            }
            
            if (isset($body["hw"]["heap_free"]) && isset($body["hw"]["heap_size"])){
                $points[] = new InfluxDB\Point(
                    'esp_hw_heap_free', // name of the measurement
                    (double)$body["hw"]["heap_free"],
                    $tags,
                    [], // optional additional fields
                    $time // Time precision has to be set to seconds!
                );

                $points[] = new InfluxDB\Point(
                    'esp_hw_heap_size', // name of the measurement
                    (double)$body["hw"]["heap_size"],
                    $tags,
                    [], // optional additional fields
                    $time // Time precision has to be set to seconds!
                );
            }

            if (isset($body["sensors"])){
                // whitlisting, don't want any surprices... 
                $sensors = ["co2","tvoc","temperature","pressure","humidity","light","pm25","pm10"];
                foreach ($sensors as $sensor){
                    if (isset($body["sensors"][$sensor]) && is_numeric($body["sensors"][$sensor])){
                        //if ($sensor == "light") $body["sensors"][$sensor] = (double)$body["sensors"][$sensor];
                        $points[] = new InfluxDB\Point(
                            $sensor, // name of the measurement
                            (double)$body["sensors"][$sensor],
                            $tags,
                            [], // optional additional fields
                            $time // Time precision has to be set to seconds!
                        );
                    }
                }

                // new workaround: Array of sensors values, testing with temp
                if (isset($body["sensors"]["temperatureMulti"])){
                    foreach ($body["sensors"]["temperatureMulti"] as $measurement){
                        $mtags = $tags; 
                        $mtags["sensorID"] = $measurement["id"];
                        $points[] = new InfluxDB\Point(
                            "temperature", // name of the measurement
                            (double)$measurement["temp"],
                            $mtags,
                            [], // optional additional fields
                            $time // Time precision has to be set to seconds!
                        );
                    }
                }

            }
            

            if (isset($body["battery"]) && isset($body["battery"]["raw"])){
                $points[] = new InfluxDB\Point(
                    'esp_battery_raw', // name of the measurement
                    (double)$body["battery"]["raw"],
                    $tags,
                    [], // optional additional fields
                    $time // Time precision has to be set to seconds!
                );
            }

            try {
                if (isset($body["sensors"])){
                    $sensorsAndValues = [
                        "SDS011" => [
                            "P1" => "pm10",
                            "P2" => "pm25",
                        ],
                        "BME280" => [
                            "temperature" => "temperature",
                            "humidity" => "humidity",
                            "pressure" => "pressure"
                        ]
                    ]; 
                    
                    foreach ($sensorsAndValues as $sensor => $sensorcfg){
                        $values = []; 
                        foreach ($sensorcfg as $value_type => $sensorname){
                            if (isset($body["sensors"][$sensorname]) && is_numeric($body["sensors"][$sensorname])){
                                $values[] = array("value_type" => $value_type, "value" => strval($body["sensors"][$sensorname]));
                            }
                        }
                        if (sizeof($values) > 0){
                            senddataToSensorCommunity($body["device"], $sensor, $values, $body["sketch"]["v"]);
                        }
                    }
                }
            } catch (Exception $e){

            }

            /*$points = array(
                new InfluxDB\Point(
                    'voltage', // name of the measurement
                    (double)($device->powermeter->voltage/1000), // the measurement value
                    ['device' => (string)$device->name, 'product' => (string)$device->attributes()->productname], // optional tags
                    [], // optional additional fields
                    time() // Time precision has to be set to seconds!
                )
            );*/
            foreach ($influx_dbs as $dbname){
                $database = $client->selectDB($dbname);
                try {
                    $result = $database->writePoints($points, InfluxDB\Database::PRECISION_SECONDS);
                } catch (Exception $e){}
                
            }
            
        }
    }
?>