#!/usr/bin/python

# Must do apt-get install python-daemon
import socket
import sys
import os
import time
import signal
import daemon
import lockfile
import subprocess

from time import sleep
from Adafruit_CharLCDPlate import Adafruit_CharLCDPlate

class fppLCD():

  def __init__(self,directory):
    self.THIS_DIRECTORY = directory

    self.BACK_COLOR = 0
    self.PLAYLIST_DIRECTORY = "/home/pi/media/playlists/"
    self.SETTINGS_FILE = "/home/pi/media/settings"

    self.DIRECTION_LEFT = 0
    self.DIRECTION_RIGHT = 1
    self.MAX_CHARS = 16
    self.MAX_STATUS_TIMEOUT = 120
    self.NORMAL_STATUS_TIMEOUT = 3
    self.FPPD_PLAYER_MODE = "0"
    self.FPPD_BRIDGE_MODE = "1"
    self.maxStatusUpdateCount = 5
    self.statusUpdateCounter= 5
    self.FPPD_STATUS_STOPPED = -1
    self.FPPD_STATUS_RUNNING = 1
    self.FPPDstatus = 0
    self.FPPDplayerStatus = 0;
    # Status Constants
    self.STATUS_IDLE = "0"
    self.STATUS_PLAYING = "1"
    self.STATUS_STOPPING_GRACEFULLY = "2"
    
    self.STATUS_INX_PLAYER_MODE = 0
    self.STATUS_INX_STATUS = 1
    self.STATUS_INX_NEXT_PLAY_LIST = 3
    self.STATUS_INX_CURRENT_PLAY_LIST = 3
    self.STATUS_INX_NEXT_TIME = 4
    self.STATUS_INX_PL_TYPE = 4
    self.STATUS_INX_SEQ_NAME = 5
    self.STATUS_INX_SONG_NAME = 6
    self.STATUS_INX_SECS_ELASPED = 9
    self.STATUS_INX_SECS_REMAINING = 10
    
    
    # Text to display
    self.line1=""
    self.line2=""
    # Rotation Variables
    self.rotateEnable = 0
    self.tempLine1=""
    self.tempLine2=""
    self.rotatePositon1=0
    self.rotatePositon2=0
    self.rotateSpeed=0
    self.rotateCount=0
    self.rotateMaxCount=3
    self.fixedLocation1=0
    self.fixedLocation2=0

    self.lcd = Adafruit_CharLCDPlate()
    self.lcd.noBlink()
    self.lcd.noCursor()
    self.lcd.clear()
    
    self.sequenceCount=0;
    self.sequences = ()
    
    self.playlists = ()
    self.selectedPlaylistIndex = 0
    self.MenuIndex=0;
    self.SubmenuIndex=0;

    self.MENU_INX_STATUS = 0
    self.MENU_INX_STOP_SEQUENCE = 1
    self.MENU_INX_PLAYLISTS = 2
    self.MENU_INX_PLAYLIST_ENTRIES = 3
    self.MENU_INX__SHUTDOWN_REBOOT = 4
    self.MENU_INX_BACKCOLOR = 5
    self.COLOR_WHITE = 6

    #Color
    self.BackgroundColor = 0
    self.MENU_COLOR_COUNT = 7
    self.previousBtn = -1
    
    self.colors=(("Red            ",self.lcd.RED),
                 ("Green          ",self.lcd.GREEN),
                 ("Blue           ",self.lcd.BLUE),
                 ("Yellow         ",self.lcd.YELLOW),
                 ("Teal           ",self.lcd.TEAL),
                 ("Violet         ",self.lcd.VIOLET),
                 ("White          ",self.lcd.WHITE))
    
  def Initialize(self):
    strColorIndex = self.readSetting("piLCD_BackColor")
    if strColorIndex.isdigit():
      self.BackgroundColor = int(strColorIndex);
    else:
      self.BackgroundColor = self.COLOR_WHITE
    
    self.SelectColor(self.BackgroundColor);
    return 
    
  def SendCommand(self,command):
    count =0;
    max_timeout = 10;
    #buf=""
    # Create a UDS socket
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    sock.bind("/tmp/FPP" + str(os.getpid()))
    sock.settimeout(1)
    # Connect the socket to the port where the server is listening
    server_address = "/tmp/FPPD"
    #print >>sys.stderr, 'connecting to %s' % server_address
    try:
        sock.connect(server_address)
        sock.send(command)
        buf = sock.recv(1024)
        sock.close()
        os.unlink("/tmp/FPP" + str(os.getpid()));
        
    except socket.error, msg:
        #print >>sys.stderr, msg
        sock.close()
        os.unlink("/tmp/FPP" + str(os.getpid()));
        return "false"

    return buf;
  
  def StartFPPD(self):
    self.SetStatusTimeOut(self.NORMAL_STATUS_TIMEOUT);
    fppdStartScript = "sudo " + os.path.dirname(self.THIS_DIRECTORY) + "/fppd_start"
    print >>sys.stderr, fppdStartScript
    os.system(fppdStartScript)
    return;
  
  def StopFPPD(self):
    fppdStopScript = "sudo " + os.path.dirname(self.THIS_DIRECTORY) + "/fppd_stop"
    #print >>sys.stderr, fppdStopScript
    os.system(fppdStopScript)
    return;

  def SetStatusTimeOut(self,timeout):
    self.maxStatusUpdateCount = timeout
    self.statusUpdateCounter = 0
    return;
    
  def ProcessLeft(self):
    self.SubmenuIndex = 0
    self.SetStatusTimeOut(self.MAX_STATUS_TIMEOUT);
    if self.previousBtn == self.lcd.LEFT:
      return
    
    if self.MenuIndex == self.MENU_INX_STATUS:
      self.MenuIndex = self.MENU_INX_BACKCOLOR
      self.SubmenuIndex = fpplcd.BACK_COLOR
      self.DisplayMenuBackColor()
    elif self.MenuIndex == self.MENU_INX__SHUTDOWN_REBOOT:
      self.SetStatusTimeOut(self.NORMAL_STATUS_TIMEOUT);
      self.MenuIndex = self.MENU_INX_STATUS
      self.UpdateStatus()
    elif self.MenuIndex == self.MENU_INX_STOP_SEQUENCE:
      self.SetStatusTimeOut(self.NORMAL_STATUS_TIMEOUT);
      self.MenuIndex = self.MENU_INX_STATUS
      self.UpdateStatus()
    elif self.MenuIndex == self.MENU_INX_BACKCOLOR:
      self.MenuIndex = self.MENU_INX__SHUTDOWN_REBOOT
      self.DisplayShutdownMenu()
    elif self.MenuIndex == self.MENU_INX_PLAYLIST_ENTRIES:
      self.MenuIndex = self.MENU_INX_PLAYLISTS
      self.SubmenuIndex = self.selectedPlaylistIndex
      self.DisplayPlaylists()
   
    self.previousBtn = self.lcd.LEFT
    return;
  
  def ProcessRight(self):
    self.SubmenuIndex = 0
    self.SetStatusTimeOut(self.MAX_STATUS_TIMEOUT);
    if self.previousBtn == self.lcd.RIGHT:
      return
    if self.MenuIndex ==self.MENU_INX_STATUS:
      self.MenuIndex = self.MENU_INX__SHUTDOWN_REBOOT
      self.DisplayShutdownMenu()
    elif self.MenuIndex == self.MENU_INX__SHUTDOWN_REBOOT:
      self.MenuIndex = self.MENU_INX_BACKCOLOR
      self.SubmenuIndex = fpplcd.BACK_COLOR
      self.DisplayMenuBackColor()
    elif self.MenuIndex == self.MENU_INX_BACKCOLOR:
      self.SetStatusTimeOut(self.NORMAL_STATUS_TIMEOUT);
      self.MenuIndex = self.MENU_INX_STATUS
      self.UpdateStatus()
    elif self.MenuIndex == self.MENU_INX_STOP_SEQUENCE:
      self.SetStatusTimeOut(self.NORMAL_STATUS_TIMEOUT);
      self.MenuIndex = self.MENU_INX_STATUS
      self.UpdateStatus()
    elif self.MenuIndex == self.MENU_INX_PLAYLIST_ENTRIES:
      self.MenuIndex = self.MENU_INX_PLAYLISTS
      self.SubmenuIndex = self.selectedPlaylistIndex
      self.DisplayPlaylists()

    self.previousBtn = self.lcd.RIGHT
    return;
    
  def ProcessUp(self):
    self.SetStatusTimeOut(self.MAX_STATUS_TIMEOUT);
    if self.previousBtn == self.lcd.DOWN:
      return
    
    self.SubmenuIndex = self.SubmenuIndex - 1
    if self.MenuIndex == self.MENU_INX_STOP_SEQUENCE:
      self.DisplayStopSequence()
    elif self.MenuIndex == self.MENU_INX__SHUTDOWN_REBOOT:
      self.DisplayShutdownMenu()
    elif self.MenuIndex == self.MENU_INX_PLAYLISTS:
      self.DisplayPlaylists()
    elif self.MenuIndex == self.MENU_INX_BACKCOLOR:
      self.DisplayMenuBackColor()
    elif self.MenuIndex == self.MENU_INX_PLAYLIST_ENTRIES:
      self.DisplayPlaylistEntries()
    elif self.MenuIndex == self.MENU_INX_STATUS:
      self.SetStatusTimeOut(self.NORMAL_STATUS_TIMEOUT);
      self.UpdateStatus()
     
    self.previousBtn = self.lcd.UP
    return;
    
    
  def ProcessDown(self):
    self.SetStatusTimeOut(self.MAX_STATUS_TIMEOUT);
    if self.previousBtn == self.lcd.DOWN:
      return
    
    self.SubmenuIndex = self.SubmenuIndex + 1
    if self.MenuIndex == self.MENU_INX_STOP_SEQUENCE:
      self.DisplayStopSequence()
    elif self.MenuIndex == self.MENU_INX__SHUTDOWN_REBOOT:
      self.DisplayShutdownMenu()
    elif self.MenuIndex == self.MENU_INX_PLAYLISTS:
      self.DisplayPlaylists()
    elif self.MenuIndex == self.MENU_INX_BACKCOLOR:
      self.DisplayMenuBackColor()
    elif self.MenuIndex == self.MENU_INX_STATUS:
      self.SetStatusTimeOut(self.NORMAL_STATUS_TIMEOUT);
      self.UpdateStatus()
    elif self.MenuIndex == self.MENU_INX_PLAYLIST_ENTRIES:
      self.DisplayPlaylistEntries()
		
    self.previousBtn = self.lcd.DOWN
    return;

  def ProcessSelect(self):
    self.SetStatusTimeOut(self.MAX_STATUS_TIMEOUT);
    if self.previousBtn == self.lcd.SELECT:
      return
    if self.MenuIndex == self.MENU_INX_STATUS:
      if self.FPPDstatus == self.FPPD_STATUS_STOPPED:
        self.StartFPPD()
      elif self.FPPDplayerStatus == self.STATUS_IDLE:
        self.MenuIndex = self.MENU_INX_PLAYLISTS
        self.SubmenuIndex = 0
        self.DisplayPlaylists()
      elif self.FPPDplayerStatus == self.STATUS_PLAYING:
        self.MenuIndex = self.MENU_INX_STOP_SEQUENCE
        self.SubmenuIndex = 0
        self.DisplayStopSequence()
    elif self.MenuIndex == self.MENU_INX_STOP_SEQUENCE:
      self.StopSequence()
    elif self.MenuIndex == self.MENU_INX_PLAYLISTS:
      self.MenuIndex = self.MENU_INX_PLAYLIST_ENTRIES
      self.DisplayPlaylistEntries()
    elif self.MenuIndex == self.MENU_INX_PLAYLIST_ENTRIES:
      self.StartPlaylist()
    elif self.MenuIndex == self.MENU_INX_BACKCOLOR:
      self.BackgroundColor = self.SubmenuIndex
      self.SelectColor(1)

    self.previousBtn = self.lcd.SELECT
    return;

  def CheckButtons(self):
    if self.lcd.buttonPressed(self.lcd.LEFT):
      self.ProcessLeft()
    elif self.lcd.buttonPressed(self.lcd.RIGHT):
      self.ProcessRight()
    elif self.lcd.buttonPressed(self.lcd.UP):
      self.ProcessUp()
    elif self.lcd.buttonPressed(self.lcd.DOWN):
      self.ProcessDown()
    elif self.lcd.buttonPressed(self.lcd.SELECT):
      self.ProcessSelect()
    else:
      self.previousBtn = -1

    return;
    
  #################  Rotate Functions  ############################
    
  def SetFixedLocation(self,lineNumber,fixedLocation):
    if lineNumber == 0:
      self.fixedLocation1 = fixedLocation
    else:
      self.fixedLocation2 = fixedLocation

    self.ResetRotatePosition(lineNumber);
    return;
      
  def ResetRotatePosition(self,lineNumber):
    if lineNumber == 0:
      self.rotatePositon1 = self.fixedLocation1;
      #print "Resetting pos:" + str(self.fixedLocation1)
    else:  
      self.rotatePositon2 = self.fixedLocation2;
    return;
 
  # Speed of rotate value 1-10
  def SetRotateSpeed(self,speed):
    self.rotateSpeed = speed
    return;

  def RotateEnabled(self,enable):
    self.rotateCount = 0
    if enable==0:
      self.ResetRotatePosition(0)
      self.ResetRotatePosition(1)
      self.rotateEnabled = 0;
    else:
      self.rotateEnabled = 1;
    return;

  def CheckRotate(self):
    self.rotateCount = self.rotateCount +1;
    if self.rotateCount == self.rotateMaxCount:
      self.rotateCount = 0
      self.RotateText()
    return;
    
  def RotateText(self):
    if self.rotateEnable != 1:
      return;

    if len(self.line1) > 16:
      if len(self.line1) - self.rotatePositon1 < 16:
         endPosition = len(self.line1)
      else:
         endPosition = self.rotatePositon1 + 16
        
      self.tempLine1 = self.line1[:self.fixedLocation1] + self.line1[self.rotatePositon1:endPosition ] + "\n"
      self.rotatePositon1 = self.rotatePositon1 +1
      if self.rotatePositon1 == len(self.line1):
        self.ResetRotatePosition(0)
    else:
      self.tempLine1 =  self.line1 + "\n"
      
    if len(self.line2) > 16:
      if len(self.line2) - self.rotatePositon2 < 16:
         endPosition = len(self.line2)
      else:
         endPosition = self.rotatePositon2 + 16
        
      self.tempLine2 = self.line2[:self.fixedLocation2] + self.line2[self.rotatePositon2:endPosition ]
      self.rotatePositon2 = self.rotatePositon2 +1
      if self.rotatePositon2 == len(self.line2):
        self.ResetRotatePosition(1)
    else:
      self.tempLine2 =  self.line2

    self.UpdateDisplay()
    return
    
  def UpdateStatus(self):
    self.MenuIndex = self.MENU_INX_STATUS
    self.SetStatusTimeOut(self.NORMAL_STATUS_TIMEOUT)
    self.statusUpdateCounter = 0;
    result = self.SendCommand("s")
    #print result
    if result == "false" :
      self.FPPDstatus = self.FPPD_STATUS_STOPPED
      self.DisplayFPPDstopped()
    else:
      results = result.split(",")
      self.FPPDstatus = self.FPPD_STATUS_RUNNING
      self.FPPDplayerStatus = results[self.STATUS_INX_STATUS]
      if results[self.STATUS_INX_PLAYER_MODE] == self.FPPD_PLAYER_MODE: 
        if results[self.STATUS_INX_STATUS] == self.STATUS_IDLE:
          self.DisplayStatusIdle(results[self.STATUS_INX_NEXT_PLAY_LIST],
                           results[self.STATUS_INX_NEXT_TIME])
        elif results[self.STATUS_INX_STATUS] == self.STATUS_PLAYING or      \
             results[self.STATUS_INX_STATUS] == self.STATUS_STOPPING_GRACEFULLY:
          self.DisplayStatusPlaying(results[self.STATUS_INX_STATUS],results[self.STATUS_INX_CURRENT_PLAY_LIST],results[self.STATUS_INX_PL_TYPE],results[self.STATUS_INX_SEQ_NAME],results[self.STATUS_INX_SONG_NAME],results[self.STATUS_INX_SECS_ELASPED],results[self.STATUS_INX_SECS_REMAINING])
    
    return;

  def DisplayFPPDstopped(self):
    self.line1 = "FPPD: Stopped   "
    self.rotateEnable = 1
    self.fixedLocation2 = 0;
    self.line2 = " [Sel] to Start "
    self.UpdateDisplay()
    return
    
  def DisplayStatusPlaying(self,playingStatus,currentPlaylist,playlistType,seqName,songName,secsElapsed,secsRemaining):
    iSecsElapsed = int(secsElapsed)
    iSecsRemaining = int(secsRemaining)
    mElapsed = int(iSecsElapsed/60)
    sElapsed = int(iSecsElapsed%60)
    mRemaining = int(iSecsRemaining/60)
    sRemaining = int(iSecsRemaining%60)
    if playingStatus == self.STATUS_PLAYING:
      self.line1 = "Playing    " +  str(mElapsed).zfill(2) + ":" + str(sElapsed).zfill(2) #+ "  " +  
    else:
      self.line1 = "Stopping   " +  str(mRemaining).zfill(2) + ":" + str(sRemaining).zfill(2) #+ "  " +  


    if self.SubmenuIndex > 6:
      self.SubmenuIndex = 0
    else:
      self.SubmenuIndex = self.SubmenuIndex + 1
    
    if self.SubmenuIndex < 4:
      # currentPlaylist is the full path
      playListPath, playlist = os.path.split(currentPlaylist)
      self.line2 = self.MakeStringWithLength(playlist,16,1);
    else:
      if len(seqName) > 0:
        self.line2 = self.MakeStringWithLength(self.RemoveExtensionFromFilename(seqName),16,1);
      else:
        self.line2 = self.MakeStringWithLength(self.RemoveExtensionFromFilename(songName),16,1);
        
    self.UpdateDisplay()
    return;
  
  # Display the status when in idle mode. I use SubmenuIndex as a counter
  # to switch between time of next show and show name  
  def DisplayStatusIdle(self,nextPlaylist,nextTime):
    self.line1 = "Idle    " + time.strftime("%H:%M:%S")
    next = nextTime.split("@");
    if self.SubmenuIndex > 6:
      self.SubmenuIndex = 0
    else:
      self.SubmenuIndex = self.SubmenuIndex + 1
    
    if self.SubmenuIndex < 4:
      self.line2 = "Next " + str(nextTime)[:3]  + " @" + str(next[1])[:6]
    else:
      self.line2 = self.MakeStringWithLength(nextPlaylist,16,1)
      
    self.UpdateDisplay()
    return;
 
  def DisplayStopSequence(self):
    if self.SubmenuIndex < 0:
      self.SubmenuIndex = 1
      
    self.SubmenuIndex = self.SubmenuIndex%2
    self.line1 = "Stop Sequence   "
    if self.SubmenuIndex == 0:
      self.line2 = self.MakeStringWithLength("1. Now",16,0)
    else:
      self.line2 = self.MakeStringWithLength("2. Gracefully",16,0)
      
    self.UpdateDisplay()
    return
 
  def DisplayPlaylists(self):
    self.line1 = self.MakeStringWithLength("Playlists",16,0)
    if self.SubmenuIndex == 0:
      self.playlists = self.GetPlaylists()
    
    if len(self.playlists) == 0:
      self.line2 = self.MakeStringWithLength("No Playlists!",16,0)
      self.UpdateDisplay()
      return
    if self.SubmenuIndex < 0:
      self.SubmenuIndex = len(self.playlists) - 1

    self.SubmenuIndex = self.SubmenuIndex%len(self.playlists)
    self.line2 = self.MakeStringWithLength(str(self.SubmenuIndex+1) + ". " + self.playlists[self.SubmenuIndex],16,0)
    self.selectedPlaylistIndex = self.SubmenuIndex
    self.GetSequences()
    self.UpdateDisplay()
    return
    
  def DisplayPlaylistEntries(self):
    #print self.playlists
    #print self.selectedPlaylistIndex
    self.line1 = self.MakeStringWithLength(self.playlists[self.selectedPlaylistIndex],16,0)
    
    if self.sequenceCount == 0:
      self.line2 = self.MakeStringWithLength("No Entries!",16,0)
      self.UpdateDisplay()
      return
    if self.SubmenuIndex < 0:
      self.SubmenuIndex = self.sequenceCount - 1

    #print "before SubmenuIndex = " + str(self.SubmenuIndex)
    self.SubmenuIndex = self.SubmenuIndex%self.sequenceCount
    #print "after SubmenuIndex = " + str(self.SubmenuIndex)
    self.line2 = self.MakeStringWithLength(str(self.SubmenuIndex+1) + ". " + self.sequences[self.SubmenuIndex],16,0)
    self.UpdateDisplay()
    return
    
  def DisplayShutdownMenu(self):
    if self.SubmenuIndex < 0:
      self.SubmenuIndex = 1
      
    self.SubmenuIndex = self.SubmenuIndex%2
    self.line1 = self.MakeStringWithLength("Shutdown/Reboot",16,0)
    if self.SubmenuIndex == 0:
      self.line2 = self.MakeStringWithLength("1. Shutdown",16,0)
    else:
      self.line2 = self.MakeStringWithLength("2. Reboot",16,0)
     
    self.UpdateDisplay()
    return

  def DisplayMenuBackColor(self):
    self.SubmenuIndex = self.SubmenuIndex % self.MENU_COLOR_COUNT
    self.lcd.clear()
    if self.BackgroundColor == self.SubmenuIndex:
      selected = "*"
    else:
      selected = ""
      
    self.line1 = "Background Color"
    self.line2 =  selected + self.colors[self.SubmenuIndex][0]
    
    self.UpdateDisplay()
    return;  
    
  def SelectColor(self,updateSetting):
    self.lcd.backlight(self.colors[self.BackgroundColor][1])
    if updateSetting==1:
      self.writeSetting("piLCD_BackColor",str(self.BackgroundColor))

    self.DisplayMenuBackColor()
    return;

  def writeSetting(self,setting,value):
    writeScript = os.path.dirname(self.THIS_DIRECTORY) + "/writeSetting.awk"
    command = "awk -f " + writeScript + " " + self.SETTINGS_FILE + " setting=" + setting + " value=" + value
    settingsText = subprocess.check_output(command, shell=True) 
    file = open(self.SETTINGS_FILE, "w")
    file.write(settingsText)
    file.close();
    return
      
  def readSetting(self,setting):
    readScript = os.path.dirname(self.THIS_DIRECTORY) + "/readSetting.awk"
    command = "awk -f " + readScript + " " + self.SETTINGS_FILE + " setting=" + setting 
    setting = subprocess.check_output(command, shell=True) 
    return setting


  def GetPlaylists(self):
    return os.listdir(self.PLAYLIST_DIRECTORY)
    
  def UpdateDisplay(self):
    self.lcd.home();
    self.lcd.message(self.line1 + "\n" + self.line2);    
    return 
    
  def RemoveExtensionFromFilename(self,fileName):
    file = fileName.split(".")
    return str(file[0])
    return
    
  def MakeStringWithLength(self,str,length,center):
    spaceCount = length - len(str)
    spaces = "                "
    if len(str) > length:
      return str
    else:
      if center == 0:
        return str + spaces[:spaceCount]
      else:
        return spaces[:spaceCount/2] + str + spaces[:(spaceCount/2)+1]

  def StopSequence(self):
    self.SetStatusTimeOut(self.NORMAL_STATUS_TIMEOUT)
    if self.SubmenuIndex == 0:
      result = self.SendCommand("d")
    else:
      result = self.SendCommand("S")
      
    return
    
  def StartPlaylist(self):
    if self.FPPDplayerStatus == self.STATUS_PLAYING or self.FPPDplayerStatus == self.STATUS_STOPPING_GRACEFULLY:
      self.StopSequence()
      self.line1 = self.MakeStringWithLength("Starting",16,0)
      self.line2 = self.MakeStringWithLength("",16,0)
      self.UpdateDisplay()
      sleep(2)
      
    self.SetStatusTimeOut(self.NORMAL_STATUS_TIMEOUT)
    if self.SubmenuIndex < self.sequenceCount:
      command = "p," + self.playlists[self.selectedPlaylistIndex] + "," + str(self.SubmenuIndex) + ","
      self.SendCommand(command)
    return

  def RebootPi(self):
    self.line1 = self.MakeStringWithLength("Rebooting Pi",16,1)
    self.line2 = self.MakeStringWithLength(" ",16,1)
    self.UpdateDisplay()
    os.system("sudo shutdown -r now")
    return

  def ShutdownPi(self):
    self.line1 = self.MakeStringWithLength("Shutting Down Pi",16,1)
    self.line2 = self.MakeStringWithLength(" ",16,1)
    self.UpdateDisplay()
    os.system("sudo shutdown -h now")
    return


  def GetSequences(self):
    playListFile = self.PLAYLIST_DIRECTORY + self.playlists[self.selectedPlaylistIndex]
    self.sequenceCount = 0
    self.sequences = []
    with open(playListFile) as seqs:
      for seq in seqs:
        arrSeq = seq.split(",")
        if arrSeq[0]=="e":
          text = self.MakeStringWithLength("Event - " + arrSeq[1],16,0)
        elif arrSeq[0] == "p":
          text = self.MakeStringWithLength("Pause - " + arrSeq[1] + "s",16,0)
        else:
          text = self.MakeStringWithLength(self.RemoveExtensionFromFilename(arrSeq[1]),16,0)
        
        if self.sequenceCount != 0:
          self.sequences.append(text)
        
        self.sequenceCount = self.sequenceCount + 1

    # Subtract one because first line in file is not playlist entry      
    self.sequenceCount = self.sequenceCount - 1
    return
        
 # Clear display and show greeting, pause 1 sec
 
#context = daemon.DaemonContext(
#    working_directory='/home/pi',
#    umask=0o002,
#    pidfile=lockfile.FileLock('/home/pi'),
#)

THIS_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
context = daemon.DaemonContext()

context.open()
with context:
  sleep(1)
  fpplcd = fppLCD(THIS_DIRECTORY)
  fpplcd.Initialize()

  while True:
    fpplcd.CheckButtons()
    fpplcd.CheckRotate()
    sleep(.10)
    fpplcd.statusUpdateCounter = fpplcd.statusUpdateCounter + 1
    if fpplcd.statusUpdateCounter > fpplcd.maxStatusUpdateCount:
      fpplcd.UpdateStatus()
      