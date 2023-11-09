<?php

class fppEEPROM {
    const FIXEDFORMATREAD  = 'A6format/A26capeName/A10capeVersion/A16capeSerial';
    const FIXEDFORMATWRITE = 'a6a26a10a16';
    const FIXEDLENGTH = 58;
    const SIG_UNCONFIRMED = -2;
    const SIG_INVALID = -1;
    const SIG_UNSIGNED = 0;
    const SIG_SIGNED = 1;
    const LOC_ANY = '0';
    const LOC_EEPROM = '1';
    const LOC_FILE = '2';

    private $format = 'FPP02';
    private $capeName = '';
    private $capeVersion = '';
    private $capeSerial = '';
    private $deviceSerial = '';
    private $signingKey = '';

    private $dataLength = 0;
    private $data = '';
    private $varData = '';
    private $variables = Array();
    private $signedVars = Array();
    private $sigStatus = self::SIG_UNSIGNED;
    private $keyDir = '../fp/keys/';

    function parseData($rawData) {
        $this->data = $rawData;

        $result = unpack(self::FIXEDFORMATREAD, $this->data);

        $this->format = $result['format'];
        $this->capeName = $result['capeName'];
        $this->capeVersion = $result['capeVersion'];
        $this->capeSerial = $result['capeSerial'];

        $this->varData = substr($this->data, self::FIXEDLENGTH);

        $offset = 0;
        $len = intval(unpack('A6', $this->varData)[1]);

        while ($len > 0) {
            $offset += 6;

            $elem = Array();
            $elem['length'] = $len;

            $code = intval(unpack('A2', substr($this->varData, $offset))[1]);
            $offset += 2;
            $elem['code'] = $code;

            switch ($code) {
                case 0:
                case 1:
                case 2:
                case 3:
                        $filename = unpack('A64', substr($this->varData, $offset))[1];
                        $offset += 64;

                        $elem['destination'] = $filename;
                        $elem['fileData'] = unpack("C$len", substr($this->varData, $offset));
                        break;
                case 96:
                        $elem['code'] = -1;
                        $this->capeSerial = unpack('A16', substr($this->varData, $offset))[1];
                        $len -= 16;
                        $offset += 16;

                        $this->deviceSerial = unpack('A42', substr($this->varData, $offset))[1];
                        break;
                case 97:
                        $sKey = unpack('A12', substr($this->varData, $offset))[1];
                        $len -= 12;
                        $offset += 12;

                        $sValue = unpack("A$len", substr($this->varData, $offset))[1];

                        $elem['key'] = $sKey;
                        $elem['value'] = $sValue;
                        break;
                case 98:
                        $elem['location'] = unpack('A2', substr($this->varData, $offset))[1];
                        break;
                case 99:
                        $keyID = unpack('A6', substr($this->varData, $offset))[1];
                        $len -= 6;
                        $offset += 6;

                        $this->signingKey = $keyID;

                        $sig = substr($this->varData, $offset, $len);

                        $this->verifySignature($keyID, $sig, $offset + $len);
                        break;
            }

            if ($elem['code'] != -1) {
                if ($this->sigStatus == 0)
                    array_push($this->variables, $elem);
                else
                    array_push($this->signedVars, $elem);
            }

            $offset += $len;

            $len = intval(unpack('A6', substr($this->varData, $offset))[1]);
        }

        $this->dataLength = self::FIXEDLENGTH + $offset + 6;
        $this->data = substr($this->data, 0, $this->dataLength);

        return 1;
    }

    function generateData() {
        $result = "";

        $tdata = pack(self::FIXEDFORMATWRITE, $this->format, $this->capeName, $this->capeVersion, $this->capeSerial);

        $result .= $tdata;

        $signing = 0;
        $sigData = '';

        for ($v = 0; $v < 2; $v++) {
            $vars = Array();
            if ($v == 1) {
                $vars = $this->signedVars;

                if ($this->deviceSerial != '') {
                    # Generate data for Cape and Device Serial Numbers
                    $len = 16 + 42;
                    $sigData = pack("a6a2a16a42", $len, 96, $this->capeSerial, $this->deviceSerial);
                }
            } else {
                $vars = $this->variables;
            }

            for ($i = 0; $i < count($vars); $i++) {
                $e = $vars[$i];
                $code = '' . $e['code'];

                switch ($vars[$i]['code']) {
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                            if (isset($e['source'])) {
                                printf( "Packaging %s from %s\n", $e['destination'], $e['source']);
                                $source = $vars[$i]['source'];
                                $tempfile = './tempfile.' . getmypid();
                                # Creating a new eeprom, so need to package up the file
                                if ($vars[$i]['code'] == 0) { # flat file
                                    system("cp $source $tempfile");
                                } else if ($vars[$i]['code'] == 1) { # zip
                                    system("(cd $source && zip -9r - ./) > $tempfile");
                                } else if ($vars[$i]['code'] == 2) { # tar.gz
                                    system("(cd $source && tar cvzf - ./) > $tempfile");
                                } else if ($vars[$i]['code'] == 3) { # tar.bz2
                                    system("(cd $source && tar cvjf - ./) > $tempfile");
                                }
                                printf( "\n");

                                $len = filesize($tempfile);
                                $fdata = file_get_contents($tempfile);
                                unlink($tempfile);
                                $vars[$i]['length'] = $len;
                            } else if (isset($e['length'])) {
                                # We are re-writing an eeprom so just use the pre-existing file data
                                $len = '' . $e['length'];
                                $fdata = pack("C$len", ...$e['fileData']);
                            }

                            $tdata = pack("a6a2a64a$len", $len, $code, $e['destination'], $fdata);

                            if ($v == 1) {
                                $sigData .= $tdata;
                            } else {
                                $result .= $tdata;
                            }
                            break;
                    case 97:
                            # Regenerate data for any setting key/value pairs
                            $tlen = strlen($e['value']);
                            $len = 12 + $tlen;
                            $tdata = pack("a6a2a12a$tlen", $len, $code, $e['key'], $e['value']);

                            if ($v == 1) {
                                $sigData .= $tdata;
                            } else {
                                $result .= $tdata;
                            }
                            break;
                    case 98:
                            # Regenerate data for location
                            $len = 2;

                            if ($e['location'] == self::LOC_FILE) {
                                // If location is 'file', force to 'any' so can also be written to an EEPROM
                                $tdata = pack('a6a2a2', $len, $code, self::LOC_ANY);
                            } else {
                                $tdata = pack('a6a2a2', $len, $code, $e['location']);
                            }

                            if ($v == 1) {
                                $sigData .= $tdata;
                            } else {
                                $result .= $tdata;
                            }
                            break;
                }
            }

            // Store any changes
            if ($v == 1) {
                $this->signedVars = $vars;
            } else {
                $this->variables = $vars;
            }
        }

        $tdata = pack('a6', '0');
        if ($this->signingKey != '') {
            $sigData .= $tdata;

            $pkey = openssl_pkey_get_private(file_get_contents($this->keyDir . $this->signingKey . '.pem'));

            $signature = '';
            openssl_sign($sigData, $signature, $pkey, OPENSSL_ALGO_SHA256);

            openssl_pkey_free($pkey);

            $len = 6 + strlen($signature);
            $slen = strlen($signature);
            $code = 99;
            $tdata = pack("a6a2a6a$slen", $len, $code, $this->signingKey, $signature);
            $result .= $tdata;

            # now write out the signed data
            $result .= $sigData;
        } else {
            $result .= $tdata;
        }

        return $result;
    }

    function verifySignature($key, $sig, $offset) {
        $origOffset = $offset;

        $len = intval(unpack('A6', substr($this->varData, $offset))[1]);
        $offset += 6;

        while ($len > 0) {
            $type = intval(unpack('A2', substr($this->varData, $offset))[1]);
            $offset += 2;

            if ($type < 50) {
                $offset += 64;
            }

            $offset += $len;

            $len = intval(unpack('A6', substr($this->varData, $offset))[1]);
            $offset += 6;
        }

        $signedData = substr($this->varData, $origOffset, $offset - $origOffset);

        if (!file_exists($this->keyDir . $key . '_pub.pem')) {
            $this->sigStatus = self::SIG_UNCONFIRMED;
            return 0;
        }

        $pkey = openssl_pkey_get_public(file_get_contents($this->keyDir . $key . '_pub.pem'));

        $result = 0;
        $this->sigStatus = self::SIG_INVALID;
        if (openssl_verify($signedData, $sig, $pkey, OPENSSL_ALGO_SHA256)) {
            $result = 1;
            $this->sigStatus = self::SIG_SIGNED;
        }

        openssl_pkey_free($pkey);

        return $result;
    }

    function setLocation($location) {
        $signed = ($this->signingKey != '');

        $item = Array();
        $item['code'] = 98;

        if ($location == 'any')
            $item['location'] = self::LOC_ANY;
        else if ($location == 'eeprom')
            $item['location'] = self::LOC_EEPROM;
        else if ($location == 'file')
            $item['location'] = self::LOC_FILE;
        else
            return 0;

        $this->insertVariable($item, $signed);

        return 1;
    }

    function insertFile($source, $destination) {
        $signed = ($this->signingKey != '');

        $item = Array();
        $item['source'] = $source;
        $item['destination'] = $destination;

        if ((preg_match('/.tbz2$/i', $destination)) ||
            (preg_match('/.tar.bz2$/i', $destination))) {
            $item['code'] = 3;
        } else if ((preg_match('/.tgz$/i', $destination)) ||
            (preg_match('/.tar.gz$/i', $destination))) {
            $item['code'] = 2;
        } else if (preg_match('/.zip$/i', $destination)) {
            $item['code'] = 1;
        } else {
            $item['code'] = 0;
        }

        $this->insertVariable($item, $signed);
    }

    function insertVariable($item, $signed = true) {
        if ($signed)
            array_push($this->signedVars, $item);
        else
            array_push($this->variables, $item);
    }

    function replaceVariable($key, $value, $signed = true) {
        if ($signed) {
            for ($i = 0; $i < count($this->signedVars); $i++) {
                if (($this->signedVars[$i]['code'] == 97) && ($this->signedVars[$i]['key'] == $key)) {
                    $this->signedVars[$i]['value'] = '' . $value;
                    return;
                }
            }
        } else {
            for ($i = 0; $i < count($this->variables); $i++) {
                if (($this->variables[$i]['code'] == 97) && ($this->variables[$i]['key'] == $key)) {
                    $this->variables[$i]['value'] = '' . $value;
                    return;
                }
            }
        }

        $item = Array();
        $item['code'] = 97;
        $item['key'] = $key;
        $item['value'] = '' . $value;

        $this->insertVariable($item, $signed);
    }

    function getSetting($key) {
        for ($i = 0; $i < count($this->variables); $i++) {
            if (($this->variables[$i]['code'] == 97) && ($this->variables[$i]['key'] == $key)) {
                return $this->variables[$i]['value'];
            }
        }

        return '';
    }

    function read($file) {
        if (!$fh = fopen($file, 'rb'))
            return 0;

        $data = fread($fh, 32768);

        fclose($fh);

        return $this->parseData($data);
    }

    function write($file) {
        $eepromData = $this->generateData();

        if (!$fh = fopen($file, 'wb'))
            return 0;

        fwrite($fh, $eepromData);

        fclose($fh);
    }

    function clone($infile, $outfile, $serial) {
        if (!$fh = fopen($infile, 'rb'))
            return 0;

        $data = fread($fh, 32768);

        fclose($fh);

        $this->parseData($data);

        if (($this->sigStatus == self::SIG_SIGNED) &&
            ($this->signingKey == 'fp')) {
            echo "ERROR: You can not change the Cape Serial Number on an EEPROM signed using the 'fp' key.\n";
            exit(0);
        }

        $newData = substr($data, 0, 42);
        $newData .= pack('a16', $serial);
        $newData .= substr($data, 58);

        if (!$fh = fopen($outfile, 'wb'))
            return 0;

        fwrite($fh, $newData);

        fclose($fh);
    }

    function sigStatusText() {
        switch ($this->sigStatus) {
            case self::SIG_UNCONFIRMED: return "Unconfirmed";
            case self::SIG_INVALID:     return "INVALID!";
            case self::SIG_UNSIGNED:    return "Unsigned";
            case self::SIG_SIGNED:      return "Signed";
        }

        return "Unknown";
    }

    function dumpInfo() {
        printf( "------------ EEPROM Info ------------\n");
        printf( "Cape Name         : %s\n", $this->capeName);
        printf( "Cape Version      : %s\n", $this->capeVersion);
        printf( "Cape Serial #     : %s\n", $this->capeSerial);

        if ($this->deviceSerial != '')
            printf( "Device Serial #   : %s\n", $this->deviceSerial);

        printf( "Key Signature     : %s\n", $this->sigStatusText());

        if ($this->signingKey != '') {
            printf( "Signing Key       : %s\n", $this->signingKey);
        }

        for ($v = 0; $v < 2; $v++) {
            $vars = Array();
            if ($v == 1) {
                $vars = $this->signedVars;
                if (count($vars))
                    printf( "Signed ExtraData  :\n");
            } else {
                $vars = $this->variables;
                if (count($vars))
                    printf( "Unsigned ExtraData:\n");
            }

            for ($i = 0; $i < count($vars); $i++) {
                $e = $vars[$i];

                switch ($e['code']) {
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                            printf( "  File (Type %d):\n", $e['code']);
                            printf( "    Name: %s\n", $e['destination']);
                            printf( "    Size: %s\n", $e['length']);
                            break;
                    case 97:
                            printf( "  Variable:\n");
                            printf( "    Key  : %s\n", $e['key']);
                            printf( "    Value: %s\n", $e['value']);
                            break;
                    case 98:
                            printf( "  EEPROM Location: %s\n", ($e['location'] == self::LOC_ANY) ? 'Any' : (($e['location'] == self::LOC_EEPROM) ? 'EEPROM' : 'File'));
                            break;
                }
            }
        }
    }

    function dumpTable() {

        printf( "<table>\n");
        printf( "<tr><th align='left'>Format:</th><td>%s</td></tr>\n", $this->format);
        printf( "<tr><th align='left'>Cape Name:</th><td>%s</td></tr>\n", $this->capeName);
        printf( "<tr><th align='left'>Cape Version:</th><td>%s</td></tr>\n", $this->capeVersion);
        printf( "<tr><th align='left'>Cape Serial:</th><td>%s</td></tr>\n", $this->capeSerial);

        if ($this->deviceSerial != '')
            printf( "<tr><th align='left'>Device Serial:</th><td>%s</td></tr>\n", $this->deviceSerial);

        printf( "<tr><th align='left'>Signing Status:</th><td>%s</td></tr>\n", $this->sigStatusText());

        if ($this->signingKey != '')
            printf( "<tr><th align='left'>Signature Key ID:</th><td>%s</td></tr>\n", $this->signingKey);

        $indent = '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;';
        for ($v = 0; $v < 2; $v++) {
            $vars = Array();
            if ($v == 1) {
                $vars = $this->signedVars;
                if (count($vars))
                    echo "<tr><td><b>Signed Variables</b></td></tr>\n";
            } else {
                $vars = $this->variables;
                if (count($vars))
                    echo "<tr><td><b>Variables</b></td></tr>\n";
            }

            for ($i = 0; $i < count($vars); $i++) {
                $e = $vars[$i];

                switch ($vars[$i]['code']) {
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                            printf( "<tr><th align='left'>%sVar Data %02d len %d:</th><td><b>File</b></td></tr>\n", $indent, $e['code'], $e['length']);
                            printf( "<tr><th align='right'>%sFilename:</th><td>%s</td></tr>\n", $indent, $e['destination']);
                            break;
                    case 97:
                            printf( "<tr><th align='left'>%sVar Data %02d len %d:</th><td><b>Setting</b></td></tr>\n", $indent, $e['code'], strlen($e['value']));
                            printf( "<tr><th align='right'>%sSetting Key:</th><td>%s</td></tr>\n", $indent, $e['key']);
                            printf( "<tr><th align='right'>%sSetting Value:</th><td>%s</td></tr>\n", $indent, $e['value']);
                            break;
                    case 98:
                            printf( "<tr><th align='left'>%sVar Data %02d len %d:</th><td><b>Location</b></td></tr>\n", $indent, $e['code'], $e['length']);
                            printf( "<tr><th align='right'>%sLocation:</th><td>%s</td></tr>\n", $indent, ($e['location'] == self::LOC_ANY) ? 'Any' : (($e['location'] == self::LOC_EEPROM) ? 'EEPROM' : 'File'));
                            break;
                }
            }
        }

        echo "</table>\n";
    }

    function extractFiles($dir) {
        for ($v = 0; $v < 2; $v++) {
            $vars = Array();
            if ($v == 1) {
                $vars = $this->signedVars;

                if ($this->deviceSerial != '') {
                    # Generate data for Cape and Device Serial Numbers
                    $len = 16 + 42;
                    $sigData = pack("a6a2a16a42", $len, 96, $this->capeSerial, $this->deviceSerial);
                }
            } else {
                $vars = $this->variables;
            }

            for ($i = 0; $i < count($vars); $i++) {
                $e = $vars[$i];
                $code = '' . $e['code'];

                switch ($vars[$i]['code']) {
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                            $filename = $e['destination'];
                            $subdir = dirname($filename);
                            $file = basename($filename);
                            system("mkdir -p $dir/$subdir");

                            printf( "- Writing %s/%s\n", $dir, $filename);

                            $len = $e['length'];
                            $fdata = pack("C$len", ...$e['fileData']);
                            file_put_contents("$dir/$filename", $fdata);
                            break;
                }
            }
        }
    }

    function setKeyDir($dir) {
        if (!preg_match('/\/$/', $dir))
            $dir .= '/';

        $this->keyDir = $dir;
    }

    function setCapeName($name) {
        $this->capeName = $name;
    }

    function setCapeVersion($vers) {
        $this->capeVersion = $vers;
    }

    function setCapeSerial($sn) {
        $this->capeSerial = $sn;
    }

    function setDeviceSerial($sn) {
        $this->deviceSerial = $sn;
    }

    function setSigningKey($id) {
        $this->signingKey = $id;
    }

    function checkSignature() {
        return $this->sigStatus;
    }

    function getSize() {
        return $this->dataLength;
    }

    function getData() {
        return $this->data;
    }

    function getCapeName() {
        return $this->capeName;
    }

    function getCapeVersion() {
        return $this->capeVersion;
    }

    function getCapeSerial() {
        return $this->capeSerial;
    }

    function getDeviceSerial() {
        return $this->deviceSerial;
    }

    function getVariable($key) {
        $values = array();

        for ($v = 0; $v < 2; $v++) {
            $vars = Array();
            if ($v == 1) {
                $vars = $this->signedVars;
            } else {
                $vars = $this->variables;
            }

            for ($i = 0; $i < count($vars); $i++) {
                $e = $vars[$i];

                if (($e['code'] == 97) && ($e['key'] == $key))
                    $values[$e['value']] = 1;
            }
        }

        return array_keys($values);
    }

}

?>
