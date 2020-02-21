<center><b>Email Setup</b></center>
<hr>
Outbound email requires a Gmail account to relay mail thru.  Nothing is stored in/on your Gmail account.  Gmail accounts are free by going to <a href="https://gmail.com" target="new">Gmail.com</a>.  By routing thru Gmail we go around when ISPs block outbound port 25 making you route email thru them.<br /><br />
To send email from a script:<br />
<ol>
<li>Create your message to a temp file of your chosing</li>
<li>Execute the mail utility:  mail -s "This is the subject line" root@localhost < tempfile.txt</li>
<li>If you need to attach a file also you can use the -a attachment_file param</li>
</ol>
All outbound email via scripts needs to be sent to <b>root@localhost</b>.
