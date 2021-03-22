<?php
    include '/var/www/home/ESPRoomSensors.php';

    // grab them from RZSensors.ino.esp32.bin?

    $source_file = '/var/www/rz/RZSensors/RZSensors.ino.esp32.bin';
    if (file_exists($source_file) && filemtime($source_file) < time()-30){
        $sourcecode = file_get_contents('/var/www/rz/RZSensors/RZSensors.ino');
        if (preg_match('~const char\* VERSION = "([^"]+)";~', $sourcecode, $matches)){
            rename($source_file, __DIR__.'/firmwares/RZSensors/'.$matches[1].'.bin');
        }
    }
    
    if (file_exists(__DIR__.'/firmwares/')){
        if (isset($_REQUEST["hostname"])){
            // device known
            $device = $_REQUEST["hostname"];
            if (isset($_CONFIG["RZ"]["ESPSensors"]["device2sensor"][$device])){
                $sensor = $_CONFIG["RZ"]["ESPSensors"]["device2sensor"][$device]; 
                if (isset($sensor["fw_channel"])){
                    if (file_exists(__DIR__.'/firmwares/'.$sensor["fw_channel"])){
                        $channelpath = __DIR__.'/firmwares/'.$sensor["fw_channel"].'/';
                        $channeldir = scandir($channelpath);

                        $sentversion = null; 
                        if (isset($_REQUEST["v"]) && !empty($_REQUEST["v"])){
                            $sentversion = $_REQUEST["v"];
                        }

                        $max_version = null; 
                        $max_filename = null; 
                        foreach ($channeldir as $file){
                            if (preg_match("~^[0-9]{4}-[0-9]{2}-[0-9]{2}[a-z0-9_-]*\.bin$~", $file)){
                                if ($file > $max_version){
                                    $max_version = $file; 
                                    $max_filename = $channelpath.$file; 
                                }
                            }
                        }

                        if ($max_version){
                            $max_version = str_replace(".bin", "", $max_version);
                            if (!$sentversion || $sentversion < $max_version){
                                http_response_code(200);
                                $fp = fopen($max_filename, 'rb');
                                header("Content-Type: application/octet-stream");
                                header("Content-Length: " . filesize($max_filename));

                                // dump the picture and stop the script
                                fpassthru($fp);
                                exit;
                            }
                        } else {
                            echo "Cannot find any valid updates.";
                            http_response_code(404);  
                        }
                    } else {
                        echo "unknown channel"; 
                        http_response_code(500);  
                    }
                } else {
                    echo "sensor found, but no channel defined.";
                    http_response_code(500);  
                }
            } else {
                echo "device unknown";
                http_response_code(403);  
            }
        } else {
            echo "hostname not set";
            http_response_code(400);  
        }
    } else {
        echo "firmware folder not found";
        http_response_code(500);  
    }

    // no one should end here
    http_response_code(404);  
?>