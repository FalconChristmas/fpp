<center><b>Email Setup</b></center>
<hr>
Outbound email requires a SMTP account to relay mail thru.  Nothing is stored in/on your mail server.  You can normally use your internet provider's SMTP servers by providing the authentication information for your email account.  If you prefer, you can also use a mail service such as Gmail.  Gmail accounts are free by going to <a href="https://gmail.com" target="new">Gmail.com</a>.  By routing thru Gmail you can go around ISPs that block outbound port 25 forcing you to route email thru them.<br /><br />
To send email from a script:<br />
<ol>
<li>Create your message to a temp file of your chosing</li>
<li>Execute the mail utility:  mail -s "This is the subject line" DESTINATION_EMAIL_ADDRESS < tempfile.txt</li>
<li>If you need to attach a file also you can use the -a attachment_file param</li>
</ol>
