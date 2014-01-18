#!/usr/bin/awk -f
BEGIN { start = 0;
 
    if (ARGC < 3 || ARGC > 3) {
        print "awk -f readSetting.awk <settingName> value"
        exit 0;
    }
    
    split(ARGV[2], arg, "=");
    if (arg[1] != "setting")
    {
      print "awk -f readSetting.awk <settingName> value"
      exit 0;
    }

    file = ARGV[1];
    setting = arg[2];
    settingFound = 0;
}
 
{
    if ($1 == setting) 
    {
      settingFound = 1;
      split($0,arrSetting,"=");
      #Trim leading and trailing space
      gsub (/^ */, "", arrSetting[2]);
      gsub (/ *$/, "", arrSetting[2]);
      print(arrSetting[2]);
      exit 0
    }
}
 
END{
    if (settingFound != 1)
    { 
      print("Not Found");
    }
    exit 0;
}
