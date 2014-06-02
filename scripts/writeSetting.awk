#!/usr/bin/awk -f

BEGIN { start = 0;
 
    if (ARGC < 4 || ARGC > 4) {
        print "awk -f writeSetting.awk <settingFile> setting=<settingName> value=<value>"
        exit 0;
    }
    
    split(ARGV[2], arg, "=");
    gsub(/ /, "", arg[1]);
    gsub(/ /, "", arg[2]);

    if (arg[1] != "setting")
    {
      print "awk -f writeSetting.awk <settingFile> setting=<settingName> value=<value>"
      exit 0;
    }
    setting = arg[2];

    split(ARGV[3], arg, "=");
    gsub(/ /, "", arg[1]);
    gsub(/ /, "", arg[2]);
    if (arg[1] != "value")
    {
      print "awk -f writeSetting.awk <settingFile> setting=<settingName> value=<value>"
      exit 0;
    }
    value = arg[2];

    file = ARGV[1];
    settingFound = 0;
}
 
{
    
    split($1, arg, "=");
    gsub(/ /, "", arg[1]);
    gsub(/ /, "", arg[2]);
    if (arg[1] == setting) 
    {
      settingFound = 1;
      printf("%s = %s\n",setting,value);
      next;
    }
    else
    {
      print $0
      next;
    }
}
 
END{
    if(settingFound != 1)
    {
      printf("\n%s = %s\n",setting,value);
    }
    exit 0;
}
