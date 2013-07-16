sudo ps cax | grep fppd > /dev/null
if [ $? -eq 0 ]; then
 echo "true"
else
 echo "false"
fi

